use anyhow::{Context, Result, bail, ensure};
use chrono::Local;
use std::fmt::Write as FmtWrite;
use std::fs::{self, File, OpenOptions};
use std::io::{self, ErrorKind, LineWriter, Write};
use std::mem::size_of;
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd, RawFd};
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

struct PidFileGuard {
    pid: i32,
}

struct DailyLogWriter {
    current_day: String,
    writer: LineWriter<File>,
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
        if read_pid_file().ok().flatten() == Some(self.pid) {
            let _ = fs::remove_file(defs::SULOGD_PID_PATH);
        }
    }
}

impl PidFileGuard {
    fn acquire() -> Result<Option<Self>> {
        utils::ensure_dir_exists(Path::new(defs::WORKING_DIR))?;
        let current_pid = i32::try_from(std::process::id()).context("invalid current pid")?;

        for _ in 0..2 {
            match OpenOptions::new()
                .write(true)
                .create_new(true)
                .open(defs::SULOGD_PID_PATH)
            {
                Ok(mut file) => {
                    write!(file, "{current_pid}")
                        .with_context(|| format!("failed to write {}", defs::SULOGD_PID_PATH))?;
                    file.sync_all()
                        .with_context(|| format!("failed to sync {}", defs::SULOGD_PID_PATH))?;
                    return Ok(Some(Self { pid: current_pid }));
                }
                Err(err) if err.kind() == ErrorKind::AlreadyExists => {
                    if let Some(pid) = read_pid_file()? {
                        if pid_is_alive(pid) {
                            log::info!("sulogd already running with pid={pid}");
                            return Ok(None);
                        }
                        log::warn!("removing stale sulog pid file for pid={pid}");
                    } else {
                        log::warn!("removing invalid sulog pid file");
                    }
                    remove_pid_file_if_exists()?;
                }
                Err(err) => {
                    return Err(err)
                        .with_context(|| format!("failed to open {}", defs::SULOGD_PID_PATH));
                }
            }
        }

        bail!("failed to acquire sulog pid file")
    }
}

impl DailyLogWriter {
    fn open() -> Result<Self> {
        utils::ensure_dir_exists(Path::new(defs::LOG_DIR))?;
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

fn open_line_writer(path: &Path) -> Result<LineWriter<File>> {
    let file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(path)
        .with_context(|| format!("failed to open {}", path.display()))?;
    Ok(LineWriter::new(file))
}

fn read_boot_id() -> Result<String> {
    let boot_id = fs::read_to_string("/proc/sys/kernel/random/boot_id")
        .context("failed to read /proc/sys/kernel/random/boot_id")?;
    Ok(boot_id.trim().to_string())
}

fn read_pid_file() -> Result<Option<i32>> {
    let pid_text = match fs::read_to_string(defs::SULOGD_PID_PATH) {
        Ok(pid_text) => pid_text,
        Err(err) if err.kind() == ErrorKind::NotFound => return Ok(None),
        Err(err) => {
            return Err(err).with_context(|| format!("failed to read {}", defs::SULOGD_PID_PATH));
        }
    };

    let pid = match pid_text.trim().parse::<i32>() {
        Ok(pid) if pid > 0 => Some(pid),
        _ => None,
    };
    Ok(pid)
}

fn remove_pid_file_if_exists() -> Result<()> {
    match fs::remove_file(defs::SULOGD_PID_PATH) {
        Ok(()) => Ok(()),
        Err(err) if err.kind() == ErrorKind::NotFound => Ok(()),
        Err(err) => Err(err).with_context(|| format!("failed to remove {}", defs::SULOGD_PID_PATH)),
    }
}

fn pid_is_alive(pid: i32) -> bool {
    if pid <= 0 {
        return false;
    }

    let ret = unsafe { libc::kill(pid, 0) };
    ret == 0 || io::Error::last_os_error().raw_os_error() == Some(libc::EPERM)
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

fn parse_record(
    header: EventRecordHeader,
    payload: &[u8],
    writer: &mut DailyLogWriter,
) -> Result<()> {
    if header.record_type == KSU_EVENT_QUEUE_TYPE_DROPPED {
        ensure!(
            header.flags & KSU_EVENT_RECORD_FLAG_INTERNAL != 0,
            "dropped record missing internal flag"
        );
        let info = DroppedInfo::parse(payload)?;
        write_log_line(writer, &format_dropped_line(&header, &info))
            .context("failed to write dropped sulog line")?;
        return Ok(());
    }

    let event = SulogEvent::parse(payload)?;
    write_log_line(writer, &format_event_line(&header, &event))
        .context("failed to write sulog line")?;
    Ok(())
}

fn handle_readable(fd: RawFd, writer: &mut DailyLogWriter) -> Result<ReadState> {
    let mut buf = [0u8; READ_BUF_SIZE];

    loop {
        let read_len =
            unsafe { libc::read(fd, buf.as_mut_ptr().cast::<libc::c_void>(), buf.len()) };
        if read_len < 0 {
            let err = io::Error::last_os_error();
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
            ensure!(
                read_len - offset >= size_of::<EventRecordHeader>(),
                "short sulog frame header: {} bytes remaining",
                read_len - offset
            );
            let header =
                EventRecordHeader::parse(&buf[offset..offset + size_of::<EventRecordHeader>()])?;
            let payload_len =
                usize::try_from(header.payload_len).context("sulog payload length overflow")?;
            let frame_len = size_of::<EventRecordHeader>()
                .checked_add(payload_len)
                .context("sulog frame length overflow")?;
            ensure!(
                read_len - offset >= frame_len,
                "short sulog frame payload: need {frame_len}, got {}",
                read_len - offset
            );

            parse_record(
                header,
                &buf[offset + size_of::<EventRecordHeader>()..offset + frame_len],
                writer,
            )?;
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

pub fn run_sulogd() -> Result<()> {
    let Some(_pid_guard) = PidFileGuard::acquire()? else {
        return Ok(());
    };
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

    log::info!("sulogd started, boot_id={boot_id}");
    write_log_line(
        &mut writer,
        &format!("type=daemon_start boot_id=\"{}\"", escape_field(&boot_id)),
    )
    .context("failed to write sulogd start marker")?;

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
                        log::info!("sulog fd closed");
                        return Ok(());
                    }
                }
            }

            let hup_mask =
                u32::try_from(libc::EPOLLERR | libc::EPOLLHUP).context("invalid EPOLLHUP mask")?;
            if event_mask & hup_mask != 0 {
                match handle_readable(sulog_fd.as_raw_fd(), &mut writer)? {
                    ReadState::Drained | ReadState::Closed => {}
                }
                log::info!("sulog epoll hangup");
                return Ok(());
            }
        }
    }
}

pub fn spawn_sulogd() -> Result<()> {
    if let Some(pid) = read_pid_file()?
        && pid_is_alive(pid)
    {
        log::info!("sulogd already running with pid={pid}, skipping spawn");
        return Ok(());
    }
    remove_pid_file_if_exists()?;

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

pub fn stop_sulogd() -> Result<()> {
    let Some(pid) = read_pid_file()? else {
        return Ok(());
    };

    if !pid_is_alive(pid) {
        remove_pid_file_if_exists()?;
        return Ok(());
    }

    log::info!("stopping sulogd pid={pid}");
    let term_ret = unsafe { libc::kill(pid, libc::SIGTERM) };
    if term_ret < 0 {
        let err = io::Error::last_os_error();
        if err.raw_os_error() != Some(libc::ESRCH) {
            return Err(err).context("failed to send SIGTERM to sulogd");
        }
    }

    for _ in 0..20 {
        if !pid_is_alive(pid) {
            remove_pid_file_if_exists()?;
            return Ok(());
        }
        thread::sleep(Duration::from_millis(100));
    }

    let kill_ret = unsafe { libc::kill(pid, libc::SIGKILL) };
    if kill_ret < 0 {
        let err = io::Error::last_os_error();
        if err.raw_os_error() != Some(libc::ESRCH) {
            return Err(err).context("failed to send SIGKILL to sulogd");
        }
    }

    for _ in 0..10 {
        if !pid_is_alive(pid) {
            remove_pid_file_if_exists()?;
            return Ok(());
        }
        thread::sleep(Duration::from_millis(100));
    }

    bail!("failed to stop sulogd pid={pid}")
}

pub fn sync_sulogd(enabled: bool) -> Result<()> {
    if enabled {
        spawn_sulogd()
    } else {
        stop_sulogd()
    }
}

pub fn sync_sulogd_with_kernel_feature() -> Result<()> {
    let feature_id = crate::feature::FeatureId::Sulog as u32;
    let (value, supported) =
        ksucalls::get_feature(feature_id).context("failed to read sulog feature state")?;
    if !supported {
        stop_sulogd()?;
        return Ok(());
    }
    sync_sulogd(value != 0)
}
