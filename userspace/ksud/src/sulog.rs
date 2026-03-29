use anyhow::{Context, Result, bail, ensure};
use chrono::Local;
use std::fmt::Write as FmtWrite;
use std::fs::{self, DirBuilder, File, OpenOptions, Permissions};
use std::io::{self, ErrorKind, LineWriter, Read, Seek, Write};
use std::mem::size_of;
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd, RawFd};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt, PermissionsExt};
use std::os::unix::process::CommandExt;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::thread;
use std::time::Duration;

use crate::{defs, ksucalls, utils};

const KSU_EVENT_QUEUE_TYPE_DROPPED: u16 = u16::MAX;
const KSU_EVENT_RECORD_FLAG_INTERNAL: u16 = 1;
const TASK_COMM_LEN: usize = 16;
const READ_BUF_SIZE: usize = 8192;
const SULOGD_RESTART_DELAY: Duration = Duration::from_secs(3);
const SULOG_DIR_MODE: u32 = 0o700;
const SULOG_FILE_MODE: u32 = 0o600;

#[repr(C, packed)]
#[derive(Clone, Copy, Debug)]
struct EventRecordHeader {
    record_type: u16,
    flags: u16,
    payload_len: u32,
    seq: u64,
    ts_ns: u64,
}

#[repr(C, packed)]
#[derive(Clone, Copy, Debug)]
struct DroppedInfo {
    dropped: u64,
    first_seq: u64,
    last_seq: u64,
}

#[repr(C, packed)]
#[derive(Clone, Copy, Debug)]
struct SulogEventHeader {
    version: u16,
    event_type: u16,
    retval: i32,
    pid: u32,
    tgid: u32,
    ppid: u32,
    uid: u32,
    euid: u32,
    comm: [u8; TASK_COMM_LEN],
    filename_len: u32,
    argv_len: u32,
}

#[derive(Clone, Debug)]
struct SulogEvent {
    version: u16,
    event_type: u16,
    retval: i32,
    pid: u32,
    tgid: u32,
    ppid: u32,
    uid: u32,
    euid: u32,
    comm: String,
    file: String,
    argv: String,
}

enum ReadState {
    Drained,
    Closed,
}

enum SessionExitReason {
    FdClosed,
    EpollHangup,
}

struct PidFileGuard {
    _lock_file: File,
    state: PidFileState,
}

struct DailyLogWriter {
    current_day: String,
    writer: LineWriter<File>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
struct PidFileState {
    pid: i32,
    start_time_ticks: u64,
    boot_id: String,
}

impl EventRecordHeader {
    fn parse(bytes: &[u8]) -> Result<Self> {
        read_packed_struct(bytes)
    }
}

impl DroppedInfo {
    fn parse(bytes: &[u8]) -> Result<Self> {
        read_packed_struct(bytes)
    }
}

impl SulogEventHeader {
    fn parse(bytes: &[u8]) -> Result<Self> {
        read_packed_struct(bytes)
    }
}

impl SulogEvent {
    fn parse(payload: &[u8]) -> Result<Self> {
        let header = SulogEventHeader::parse(payload)?;
        let fixed_len = size_of::<SulogEventHeader>();
        let comm = parse_c_string(&header.comm);
        let filename_len =
            usize::try_from(header.filename_len).context("filename length overflow")?;
        let argv_len = usize::try_from(header.argv_len).context("argv length overflow")?;
        let version = header.version;
        let event_type = header.event_type;
        let retval = header.retval;
        let process_id = header.pid;
        let tgid = header.tgid;
        let parent_process_id = header.ppid;
        let uid = header.uid;
        let euid = header.euid;

        let variable_len = filename_len
            .checked_add(argv_len)
            .context("sulog variable payload length overflow")?;
        let total_len = fixed_len
            .checked_add(variable_len)
            .context("sulog event length overflow")?;
        ensure!(
            total_len == payload.len(),
            "sulog payload length mismatch: expected {total_len}, got {}",
            payload.len()
        );

        let file = parse_c_string(&payload[fixed_len..fixed_len + filename_len]);
        let argv =
            parse_c_string(&payload[fixed_len + filename_len..fixed_len + filename_len + argv_len]);

        Ok(Self {
            version,
            event_type,
            retval,
            pid: process_id,
            tgid,
            ppid: parent_process_id,
            uid,
            euid,
            comm,
            file,
            argv,
        })
    }

    const fn event_name(&self) -> &'static str {
        match self.event_type {
            1 => "root_execve",
            2 => "sucompat",
            3 => "ioctl_grant_root",
            _ => "unknown",
        }
    }
}

impl Drop for PidFileGuard {
    fn drop(&mut self) {
        if read_pid_file().ok().flatten() == Some(self.state.clone()) {
            let _ = fs::remove_file(defs::SULOGD_PID_PATH);
        }
    }
}

impl PidFileGuard {
    fn acquire() -> Result<Option<Self>> {
        ensure_private_dir_exists(Path::new(defs::WORKING_DIR))?;
        let state = current_pid_file_state()?;
        let mut file = open_pid_file(true)?;

        if try_lock_pid_file(&file)? {
            write_pid_file_state(&mut file, &state)
                .with_context(|| format!("failed to write {}", defs::SULOGD_PID_PATH))?;
            file.sync_all()
                .with_context(|| format!("failed to sync {}", defs::SULOGD_PID_PATH))?;
            return Ok(Some(Self {
                _lock_file: file,
                state,
            }));
        }

        match read_pid_file_state_from_file(&mut file)? {
            Some(existing) => {
                log::info!("sulogd already running with pid={}", existing.pid);
            }
            None => {
                log::warn!("sulogd lock is held but pid metadata is unavailable");
            }
        }
        Ok(None)
    }
}

impl DailyLogWriter {
    fn open() -> Result<Self> {
        ensure_private_dir_exists(Path::new(defs::LOG_DIR))?;
        let current_day = current_log_day();
        let path = daily_log_path(&current_day);
        let writer = open_line_writer(&path)?;
        Ok(Self {
            current_day,
            writer,
        })
    }

    fn rotate_if_needed(&mut self) -> Result<()> {
        let current_day = current_log_day();
        if current_day == self.current_day {
            return Ok(());
        }

        let path = daily_log_path(&current_day);
        self.writer = open_line_writer(&path)?;
        self.current_day = current_day;
        Ok(())
    }
}

fn parse_c_string(bytes: &[u8]) -> String {
    let end = bytes
        .iter()
        .position(|byte| *byte == 0)
        .unwrap_or(bytes.len());
    String::from_utf8_lossy(&bytes[..end]).into_owned()
}

fn read_packed_struct<T: Copy>(bytes: &[u8]) -> Result<T> {
    ensure!(bytes.len() >= size_of::<T>(), "truncated packed struct");
    let ptr = bytes.as_ptr().cast::<T>();
    Ok(unsafe { std::ptr::read_unaligned(ptr) })
}

fn current_log_day() -> String {
    Local::now().format("%Y-%m-%d").to_string()
}

fn daily_log_path(day: &str) -> PathBuf {
    Path::new(defs::LOG_DIR).join(format!("sulog-{day}.log"))
}

fn ensure_private_dir_exists(path: &Path) -> Result<()> {
    DirBuilder::new()
        .recursive(true)
        .mode(SULOG_DIR_MODE)
        .create(path)
        .with_context(|| format!("failed to create {}", path.display()))?;
    fs::set_permissions(path, Permissions::from_mode(SULOG_DIR_MODE))
        .with_context(|| format!("failed to chmod {} to {:o}", path.display(), SULOG_DIR_MODE))?;
    Ok(())
}

fn open_line_writer(path: &Path) -> Result<LineWriter<File>> {
    let file = OpenOptions::new()
        .create(true)
        .append(true)
        .mode(SULOG_FILE_MODE)
        .open(path)
        .with_context(|| format!("failed to open {}", path.display()))?;
    file.set_permissions(Permissions::from_mode(SULOG_FILE_MODE))
        .with_context(|| {
            format!(
                "failed to chmod {} to {:o}",
                path.display(),
                SULOG_FILE_MODE
            )
        })?;
    Ok(LineWriter::new(file))
}

fn read_boot_id() -> Result<String> {
    let boot_id = fs::read_to_string("/proc/sys/kernel/random/boot_id")
        .context("failed to read /proc/sys/kernel/random/boot_id")?;
    Ok(boot_id.trim().to_string())
}

fn read_pid_file() -> Result<Option<PidFileState>> {
    let pid_text = match fs::read_to_string(defs::SULOGD_PID_PATH) {
        Ok(pid_text) => pid_text,
        Err(err) if err.kind() == ErrorKind::NotFound => return Ok(None),
        Err(err) => {
            return Err(err).with_context(|| format!("failed to read {}", defs::SULOGD_PID_PATH));
        }
    };

    Ok(parse_pid_file_state(&pid_text))
}

fn open_pid_file(create: bool) -> Result<File> {
    let file = OpenOptions::new()
        .read(true)
        .write(true)
        .create(create)
        .mode(SULOG_FILE_MODE)
        .open(defs::SULOGD_PID_PATH)
        .with_context(|| format!("failed to open {}", defs::SULOGD_PID_PATH))?;
    file.set_permissions(Permissions::from_mode(SULOG_FILE_MODE))
        .with_context(|| {
            format!(
                "failed to chmod {} to {:o}",
                defs::SULOGD_PID_PATH,
                SULOG_FILE_MODE
            )
        })?;
    Ok(file)
}

fn try_lock_pid_file(file: &File) -> io::Result<bool> {
    let ret = unsafe { libc::flock(file.as_raw_fd(), libc::LOCK_EX | libc::LOCK_NB) };
    if ret == 0 {
        return Ok(true);
    }

    let err = io::Error::last_os_error();
    if let Some(code) = err.raw_os_error()
        && (code == libc::EWOULDBLOCK || code == libc::EAGAIN)
    {
        return Ok(false);
    }

    if err.kind() == ErrorKind::WouldBlock {
        return Ok(false);
    }

    Err(err)
}

fn pid_is_alive(pid: i32) -> bool {
    if pid <= 0 {
        return false;
    }

    let ret = unsafe { libc::kill(pid, 0) };
    ret == 0 || io::Error::last_os_error().raw_os_error() == Some(libc::EPERM)
}

fn parse_pid_file_state(pid_text: &str) -> Option<PidFileState> {
    let mut fields = pid_text.split_whitespace();
    let pid = fields.next()?.parse::<i32>().ok()?;
    let start_time_ticks = fields.next()?.parse::<u64>().ok()?;
    let boot_id = fields.next()?.to_string();
    if pid <= 0 || boot_id.is_empty() {
        return None;
    }
    Some(PidFileState {
        pid,
        start_time_ticks,
        boot_id,
    })
}

fn write_pid_file_state(file: &mut File, state: &PidFileState) -> io::Result<()> {
    file.set_len(0)?;
    file.rewind()?;
    write!(
        file,
        "{} {} {}",
        state.pid, state.start_time_ticks, state.boot_id
    )
}

fn read_pid_file_state_from_file(file: &mut File) -> Result<Option<PidFileState>> {
    file.rewind()?;
    let mut pid_text = String::new();
    file.read_to_string(&mut pid_text)?;
    Ok(parse_pid_file_state(&pid_text))
}

fn read_process_start_time_ticks(pid: i32) -> Result<Option<u64>> {
    if pid <= 0 {
        return Ok(None);
    }

    let stat_path = format!("/proc/{pid}/stat");
    let stat_text = match fs::read_to_string(&stat_path) {
        Ok(stat_text) => stat_text,
        Err(err) if err.kind() == ErrorKind::NotFound => return Ok(None),
        Err(err) => return Err(err).with_context(|| format!("failed to read {stat_path}")),
    };

    let (_, rest) = stat_text
        .rsplit_once(") ")
        .context("failed to parse /proc stat comm field")?;
    let fields: Vec<&str> = rest.split_whitespace().collect();
    let start_time = fields
        .get(19)
        .context("failed to read start_time from /proc stat")?
        .parse::<u64>()
        .context("failed to parse start_time from /proc stat")?;
    Ok(Some(start_time))
}

fn current_pid_file_state() -> Result<PidFileState> {
    let pid = i32::try_from(std::process::id()).context("invalid current pid")?;
    let start_time_ticks =
        read_process_start_time_ticks(pid)?.context("failed to get current process start time")?;
    let boot_id = read_boot_id()?;
    Ok(PidFileState {
        pid,
        start_time_ticks,
        boot_id,
    })
}

fn pid_file_state_matches_live_process(state: &PidFileState) -> bool {
    if !pid_is_alive(state.pid) {
        return false;
    }

    let Ok(current_boot_id) = read_boot_id() else {
        return false;
    };
    if current_boot_id != state.boot_id {
        return false;
    }

    matches!(
        read_process_start_time_ticks(state.pid),
        Ok(Some(start_time_ticks)) if start_time_ticks == state.start_time_ticks
    )
}

fn escape_field(value: &str) -> String {
    let mut escaped = String::with_capacity(value.len());
    for ch in value.chars() {
        match ch {
            '\\' => escaped.push_str("\\\\"),
            '"' => escaped.push_str("\\\""),
            '\n' => escaped.push_str("\\n"),
            '\r' => escaped.push_str("\\r"),
            '\t' => escaped.push_str("\\t"),
            ch if ch.is_control() => {
                let code = u32::from(ch);
                let _ = write!(escaped, "\\x{code:02x}");
            }
            ch => escaped.push(ch),
        }
    }
    escaped
}

fn format_event_line(header: &EventRecordHeader, event: &SulogEvent) -> String {
    let ts_ns = header.ts_ns;
    let seq = header.seq;
    let version = event.version;
    let retval = event.retval;
    let process_id = event.pid;
    let tgid = event.tgid;
    let parent_process_id = event.ppid;
    let uid = event.uid;
    let euid = event.euid;
    format!(
        "ts_ns={} seq={} type={} version={} retval={} pid={} tgid={} ppid={} uid={} euid={} comm=\"{}\" file=\"{}\" argv=\"{}\"",
        ts_ns,
        seq,
        event.event_name(),
        version,
        retval,
        process_id,
        tgid,
        parent_process_id,
        uid,
        euid,
        escape_field(&event.comm),
        escape_field(&event.file),
        escape_field(&event.argv),
    )
}

fn format_dropped_line(header: &EventRecordHeader, info: &DroppedInfo) -> String {
    let ts_ns = header.ts_ns;
    let seq = header.seq;
    let dropped = info.dropped;
    let first_seq = info.first_seq;
    let last_seq = info.last_seq;
    format!(
        "ts_ns={ts_ns} seq={seq} type=dropped dropped={dropped} first_seq={first_seq} last_seq={last_seq}"
    )
}

fn write_log_line(writer: &mut DailyLogWriter, line: &str) -> io::Result<()> {
    writer.rotate_if_needed().map_err(io::Error::other)?;
    writer.writer.write_all(line.as_bytes())?;
    writer.writer.write_all(b"\n")?;
    writer.writer.flush()?;
    Ok(())
}

fn format_record_line(header: EventRecordHeader, payload: &[u8]) -> Result<String> {
    if header.record_type == KSU_EVENT_QUEUE_TYPE_DROPPED {
        ensure!(
            header.flags & KSU_EVENT_RECORD_FLAG_INTERNAL != 0,
            "dropped record missing internal flag"
        );
        let info = DroppedInfo::parse(payload)?;
        return Ok(format_dropped_line(&header, &info));
    }

    let event = SulogEvent::parse(payload)?;
    Ok(format_event_line(&header, &event))
}

fn handle_readable(fd: RawFd, writer: &mut DailyLogWriter) -> Result<ReadState> {
    let mut buf = [0u8; READ_BUF_SIZE];

    loop {
        let read_len =
            unsafe { libc::read(fd, buf.as_mut_ptr().cast::<libc::c_void>(), buf.len()) };
        if read_len < 0 {
            let err = io::Error::last_os_error();
            if err.raw_os_error() == Some(libc::EINTR) {
                continue;
            }
            if matches!(err.kind(), ErrorKind::WouldBlock)
                || err.raw_os_error() == Some(libc::EAGAIN)
            {
                return Ok(ReadState::Drained);
            }
            return Err(err).context("failed to read sulog event queue");
        }

        if read_len == 0 {
            return Ok(ReadState::Closed);
        }

        let read_len = usize::try_from(read_len).context("negative sulog read length")?;
        let mut offset = 0usize;

        while offset < read_len {
            let remaining = read_len - offset;
            if remaining < size_of::<EventRecordHeader>() {
                log::warn!("dropping truncated sulog frame header: {remaining} bytes remaining");
                break;
            }

            let header =
                EventRecordHeader::parse(&buf[offset..offset + size_of::<EventRecordHeader>()])?;
            let payload_len = match usize::try_from(header.payload_len) {
                Ok(payload_len) => payload_len,
                Err(err) => {
                    let seq = header.seq;
                    log::warn!(
                        "dropping sulog frame with invalid payload length at seq={seq}: {err:#}"
                    );
                    break;
                }
            };
            let Some(frame_len) = size_of::<EventRecordHeader>().checked_add(payload_len) else {
                let seq = header.seq;
                log::warn!("dropping sulog frame with overflowing length at seq={seq}");
                break;
            };
            if remaining < frame_len {
                log::warn!(
                    "dropping truncated sulog frame payload: need {frame_len}, got {remaining}"
                );
                break;
            }

            let payload = &buf[offset + size_of::<EventRecordHeader>()..offset + frame_len];
            match format_record_line(header, payload) {
                Ok(line) => {
                    write_log_line(writer, &line).context("failed to write sulog line")?;
                }
                Err(err) => {
                    let seq = header.seq;
                    let record_type = header.record_type;
                    log::warn!(
                        "dropping malformed sulog record seq={seq} type={record_type}: {err:#}"
                    );
                }
            }
            offset += frame_len;
        }
    }
}

pub fn open_sulog_fd() -> io::Result<OwnedFd> {
    let fd = ksucalls::get_sulog_fd()?;
    let flags = unsafe { libc::fcntl(fd, libc::F_GETFL) };
    if flags < 0 {
        let err = io::Error::last_os_error();
        let _ = unsafe { libc::close(fd) };
        return Err(err);
    }

    if unsafe { libc::fcntl(fd, libc::F_SETFL, flags | libc::O_NONBLOCK) } < 0 {
        let err = io::Error::last_os_error();
        let _ = unsafe { libc::close(fd) };
        return Err(err);
    }

    Ok(unsafe { OwnedFd::from_raw_fd(fd) })
}

fn write_session_marker(
    writer: &mut DailyLogWriter,
    boot_id: &str,
    restart_count: u64,
) -> Result<()> {
    let line = if restart_count == 0 {
        format!("type=daemon_start boot_id=\"{}\"", escape_field(boot_id))
    } else {
        format!(
            "type=daemon_restart boot_id=\"{}\" restart={restart_count}",
            escape_field(boot_id)
        )
    };
    write_log_line(writer, &line).context("failed to write sulogd session marker")
}

fn run_sulog_session(restart_count: u64) -> Result<SessionExitReason> {
    let sulog_fd = open_sulog_fd().context("failed to open sulog fd")?;
    let mut writer = DailyLogWriter::open()?;
    let boot_id = read_boot_id()?;

    let epoll_raw = unsafe { libc::epoll_create1(libc::EPOLL_CLOEXEC) };
    if epoll_raw < 0 {
        bail!("failed to create epoll fd: {}", io::Error::last_os_error());
    }
    let epoll_fd = unsafe { OwnedFd::from_raw_fd(epoll_raw) };

    let mut event = libc::epoll_event {
        events: u32::try_from(libc::EPOLLIN | libc::EPOLLERR | libc::EPOLLHUP)
            .context("invalid epoll event mask")?,
        u64: u64::try_from(sulog_fd.as_raw_fd()).context("invalid sulog fd")?,
    };
    let ctl_ret = unsafe {
        libc::epoll_ctl(
            epoll_fd.as_raw_fd(),
            libc::EPOLL_CTL_ADD,
            sulog_fd.as_raw_fd(),
            &raw mut event,
        )
    };
    if ctl_ret < 0 {
        bail!(
            "failed to register sulog fd to epoll: {}",
            io::Error::last_os_error()
        );
    }

    log::info!("sulogd session started, boot_id={boot_id}, restart={restart_count}");
    write_session_marker(&mut writer, &boot_id, restart_count)?;

    let mut events = [libc::epoll_event { events: 0, u64: 0 }; 4];
    loop {
        let ready = unsafe {
            libc::epoll_wait(
                epoll_fd.as_raw_fd(),
                events.as_mut_ptr(),
                i32::try_from(events.len()).context("too many epoll events")?,
                -1,
            )
        };
        if ready < 0 {
            let err = io::Error::last_os_error();
            if err.raw_os_error() == Some(libc::EINTR) {
                continue;
            }
            return Err(err).context("epoll_wait failed for sulogd");
        }

        let ready = usize::try_from(ready).context("invalid epoll ready count")?;
        for ready_event in &events[..ready] {
            let event_mask = ready_event.events;
            if event_mask & u32::try_from(libc::EPOLLIN).context("invalid EPOLLIN")? != 0 {
                match handle_readable(sulog_fd.as_raw_fd(), &mut writer)? {
                    ReadState::Drained => {}
                    ReadState::Closed => {
                        log::warn!("sulog fd closed");
                        return Ok(SessionExitReason::FdClosed);
                    }
                }
            }

            let hup_mask =
                u32::try_from(libc::EPOLLERR | libc::EPOLLHUP).context("invalid EPOLLHUP mask")?;
            if event_mask & hup_mask != 0 {
                match handle_readable(sulog_fd.as_raw_fd(), &mut writer)? {
                    ReadState::Drained | ReadState::Closed => {}
                }
                log::warn!("sulog epoll hangup");
                return Ok(SessionExitReason::EpollHangup);
            }
        }
    }
}

pub fn run_sulogd() -> Result<()> {
    let Some(_pid_guard) = PidFileGuard::acquire()? else {
        return Ok(());
    };

    let mut restart_count = 0u64;
    loop {
        match run_sulog_session(restart_count) {
            Ok(SessionExitReason::FdClosed) => {
                log::warn!(
                    "restarting sulogd session after fd close in {}s",
                    SULOGD_RESTART_DELAY.as_secs()
                );
            }
            Ok(SessionExitReason::EpollHangup) => {
                log::warn!(
                    "restarting sulogd session after hangup in {}s",
                    SULOGD_RESTART_DELAY.as_secs()
                );
            }
            Err(err) => {
                log::warn!(
                    "sulogd session failed: {err:#}; restarting in {}s",
                    SULOGD_RESTART_DELAY.as_secs()
                );
            }
        }

        restart_count = restart_count.saturating_add(1);
        thread::sleep(SULOGD_RESTART_DELAY);
    }
}

pub fn spawn_sulogd() -> Result<()> {
    if let Some(existing) = read_pid_file()?
        && pid_file_state_matches_live_process(&existing)
    {
        log::info!(
            "sulogd already running with pid={}, skipping spawn",
            existing.pid
        );
        return Ok(());
    }

    let current_exe = std::env::current_exe().context("failed to resolve current ksud path")?;
    let mut command = Command::new(current_exe);
    command
        .arg("sulogd")
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .current_dir("/")
        .process_group(0);

    unsafe {
        command.pre_exec(|| {
            utils::switch_cgroups();
            Ok(())
        });
    }

    let child = command.spawn().context("failed to spawn sulogd")?;
    log::info!("spawned sulogd pid={}", child.id());
    Ok(())
}

pub fn ensure_sulogd_running() -> Result<()> {
    spawn_sulogd()
}
