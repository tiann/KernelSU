use anyhow::{Context, Result, bail, ensure};
use chrono::{Days, Local, NaiveDate};
use std::fmt::Write as FmtWrite;
use std::fs::{self, DirBuilder, File, OpenOptions, Permissions};
use std::io::{self, ErrorKind, LineWriter, Write};
use std::mem::size_of;
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd, RawFd};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt, PermissionsExt};
use std::os::unix::process::CommandExt;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::thread;
use std::time::Duration;

use crate::ksu_uapi::ProcessTag;
use crate::{defs, ksucalls, module_config, utils};

const KSU_EVENT_QUEUE_TYPE_DROPPED: u16 = u16::MAX;
const KSU_EVENT_RECORD_FLAG_INTERNAL: u16 = 1;
const READ_BUF_SIZE: usize = 8192;
const SULOGD_RESTART_DELAY: Duration = Duration::from_secs(3);
const SULOG_DIR_MODE: u32 = 0o700;
const SULOG_FILE_MODE: u32 = 0o600;
pub const SULOG_CONFIG_MODULE_ID: &str = "internal.ksud.sulogd";
const SULOG_RETENTION_CONFIG_KEY: &str = "log.retention.days";
const SULOG_MAX_FILE_SIZE_CONFIG_KEY: &str = "log.max_file_size";
const DEFAULT_SULOG_RETENTION_DAYS: u64 = 3;
const DEFAULT_SULOG_MAX_FILE_SIZE: u64 = 10 * 1024 * 1024;

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

type SulogEventHeader = crate::ksu_uapi::ksu_sulog_event;

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
    process_tag: ProcessTag,
}

enum ReadState {
    Drained,
    Closed,
}

enum SessionExitReason {
    FdClosed,
    EpollHangup,
}

struct SulogdLockGuard {
    _lock_file: File,
}

struct DailyLogWriter {
    current_day: String,
    current_index: u32,
    current_size: u64,
    max_file_size: u64,
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
        let process_tag_name = parse_c_string(&header.process_tag_name);

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
            process_tag: ProcessTag {
                tag_type: header.process_tag_type,
                name: process_tag_name,
            },
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

impl SulogdLockGuard {
    fn acquire() -> Result<Option<Self>> {
        ensure_private_dir_exists(Path::new(defs::WORKING_DIR))?;
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .mode(SULOG_FILE_MODE)
            .open(defs::SULOGD_LOCK_PATH)
            .with_context(|| format!("failed to open {}", defs::SULOGD_LOCK_PATH))?;

        if try_lock_file(&file)? {
            return Ok(Some(Self { _lock_file: file }));
        }

        Ok(None)
    }
}

impl DailyLogWriter {
    fn open() -> Result<Self> {
        ensure_private_dir_exists(Path::new(defs::LOG_DIR))?;
        let config = ensure_sulog_config()?;
        cleanup_expired_logs(config.retention_days)?;
        let current_day = current_log_day();
        let (current_index, current_size, writer) =
            open_log_writer_for_day(&current_day, config.max_file_size)?;
        Ok(Self {
            current_day,
            current_index,
            current_size,
            max_file_size: config.max_file_size,
            writer,
        })
    }

    fn rotate_if_needed(&mut self, next_write_len: usize) -> Result<()> {
        let current_day = current_log_day();
        let next_write_len = u64::try_from(next_write_len).context("invalid log line length")?;
        if current_day != self.current_day {
            let config = ensure_sulog_config()?;
            cleanup_expired_logs(config.retention_days)?;
            let (current_index, current_size, writer) =
                open_log_writer_for_day(&current_day, config.max_file_size)?;
            self.writer = writer;
            self.current_day = current_day;
            self.current_index = current_index;
            self.current_size = current_size;
            self.max_file_size = config.max_file_size;
            return Ok(());
        }

        if self.current_size > 0
            && self.current_size.saturating_add(next_write_len) > self.max_file_size
        {
            self.current_index = self.current_index.saturating_add(1);
            let path = daily_log_path(&self.current_day, self.current_index);
            self.writer = open_line_writer(&path)?;
            self.current_size = 0;
        }
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

fn daily_log_path(day: &str, index: u32) -> PathBuf {
    let file_name = if index == 0 {
        format!("sulog-{day}.log")
    } else {
        format!("sulog-{day}-{index}.log")
    };
    Path::new(defs::LOG_DIR).join(file_name)
}

fn parse_retention_days(value: &str) -> Result<u64> {
    let days = value
        .trim()
        .parse::<u64>()
        .with_context(|| format!("invalid {SULOG_RETENTION_CONFIG_KEY} value: '{value}'"))?;
    ensure!(
        days > 0,
        "{SULOG_RETENTION_CONFIG_KEY} must be greater than 0"
    );
    Ok(days)
}

fn parse_max_file_size(value: &str) -> Result<u64> {
    let size = value
        .trim()
        .parse::<u64>()
        .with_context(|| format!("invalid {SULOG_MAX_FILE_SIZE_CONFIG_KEY} value: '{value}'"))?;
    ensure!(
        size > 0,
        "{SULOG_MAX_FILE_SIZE_CONFIG_KEY} must be greater than 0"
    );
    Ok(size)
}

#[derive(Clone, Copy, Debug)]
struct SulogConfig {
    retention_days: u64,
    max_file_size: u64,
}

fn ensure_config_value(key: &str, default_value: u64) -> Result<String> {
    let config = module_config::merge_configs(SULOG_CONFIG_MODULE_ID)?;
    if let Some(value) = config.get(key) {
        return Ok(value.clone());
    }

    let default_value = default_value.to_string();
    module_config::set_config_value(
        SULOG_CONFIG_MODULE_ID,
        key,
        &default_value,
        module_config::ConfigType::Persist,
    )?;
    Ok(default_value)
}

fn ensure_sulog_config() -> Result<SulogConfig> {
    let retention_days = parse_retention_days(&ensure_config_value(
        SULOG_RETENTION_CONFIG_KEY,
        DEFAULT_SULOG_RETENTION_DAYS,
    )?)?;
    let max_file_size = parse_max_file_size(&ensure_config_value(
        SULOG_MAX_FILE_SIZE_CONFIG_KEY,
        DEFAULT_SULOG_MAX_FILE_SIZE,
    )?)?;
    Ok(SulogConfig {
        retention_days,
        max_file_size,
    })
}

fn parse_log_date_from_path(path: &Path) -> Option<NaiveDate> {
    parse_log_name(path).map(|(date, _)| date)
}

fn parse_log_name(path: &Path) -> Option<(NaiveDate, u32)> {
    let file_name = path.file_name()?.to_str()?;
    let name = file_name.strip_prefix("sulog-")?.strip_suffix(".log")?;
    let (date_str, index) = if name.len() == "YYYY-MM-DD".len() {
        (name, 0)
    } else if let Some((date_str, index_str)) = name.rsplit_once('-') {
        if date_str.len() == "YYYY-MM-DD".len() && index_str.chars().all(|ch| ch.is_ascii_digit()) {
            (date_str, index_str.parse::<u32>().ok()?)
        } else {
            let (legacy_date_str, legacy_index_str) = name.rsplit_once('.')?;
            (legacy_date_str, legacy_index_str.parse::<u32>().ok()?)
        }
    } else {
        let (legacy_date_str, legacy_index_str) = name.rsplit_once('.')?;
        (legacy_date_str, legacy_index_str.parse::<u32>().ok()?)
    };
    Some((NaiveDate::parse_from_str(date_str, "%Y-%m-%d").ok()?, index))
}

fn cleanup_expired_logs(retention_days: u64) -> Result<()> {
    let log_dir = Path::new(defs::LOG_DIR);
    if !log_dir.exists() {
        return Ok(());
    }

    let today = Local::now().date_naive();
    let cutoff = today
        .checked_sub_days(Days::new(retention_days.saturating_sub(1)))
        .context("failed to compute sulog retention cutoff")?;

    for entry in
        fs::read_dir(log_dir).with_context(|| format!("failed to read {}", log_dir.display()))?
    {
        let entry = entry.with_context(|| format!("failed to read {}", log_dir.display()))?;
        let path = entry.path();
        let Some(log_date) = parse_log_date_from_path(&path) else {
            continue;
        };

        if log_date < cutoff {
            fs::remove_file(&path)
                .with_context(|| format!("failed to remove expired sulog {}", path.display()))?;
            log::info!(
                "removed expired sulog log {}, retention_days={retention_days}",
                path.display()
            );
        }
    }

    Ok(())
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

fn open_log_writer_for_day(day: &str, max_file_size: u64) -> Result<(u32, u64, LineWriter<File>)> {
    let mut highest_index = 0u32;
    let mut found = false;
    for entry in
        fs::read_dir(defs::LOG_DIR).with_context(|| format!("failed to read {}", defs::LOG_DIR))?
    {
        let entry = entry.with_context(|| format!("failed to read {}", defs::LOG_DIR))?;
        let path = entry.path();
        let Some((log_date, index)) = parse_log_name(&path) else {
            continue;
        };
        if log_date == NaiveDate::parse_from_str(day, "%Y-%m-%d").context("invalid current day")? {
            highest_index = highest_index.max(index);
            found = true;
        }
    }

    let mut index = if found { highest_index } else { 0 };
    let mut path = daily_log_path(day, index);
    let mut current_size = fs::metadata(&path).map_or(0, |meta| meta.len());
    if current_size >= max_file_size && current_size > 0 {
        index = index.saturating_add(1);
        path = daily_log_path(day, index);
        current_size = fs::metadata(&path).map_or(0, |meta| meta.len());
    }

    let writer = open_line_writer(&path)?;
    Ok((index, current_size, writer))
}

fn read_boot_id() -> Result<String> {
    let boot_id = fs::read_to_string("/proc/sys/kernel/random/boot_id")
        .context("failed to read /proc/sys/kernel/random/boot_id")?;
    Ok(boot_id.trim().to_string())
}

fn try_lock_file(file: &File) -> io::Result<bool> {
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
        "ts_ns={} seq={} type={} version={} retval={} pid={} tgid={} ppid={} uid={} euid={} comm=\"{}\" file=\"{}\" argv=\"{}\" tag_type=\"{}\" tag_name=\"{}\"",
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
        event.process_tag.tag_type,
        escape_field(&event.process_tag.name),
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
    let write_len = line
        .len()
        .checked_add(1)
        .ok_or_else(|| io::Error::other("sulog line length overflow"))?;
    writer
        .rotate_if_needed(write_len)
        .map_err(io::Error::other)?;
    writer.writer.write_all(line.as_bytes())?;
    writer.writer.write_all(b"\n")?;
    writer.writer.flush()?;
    writer.current_size = writer
        .current_size
        .saturating_add(u64::try_from(write_len).map_err(io::Error::other)?);
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
    let Some(_lock_guard) = SulogdLockGuard::acquire()? else {
        log::info!("sulogd lock is held, skipping start");
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
    if utils::create_daemon(true)? {
        let current_exe = std::env::current_exe().context("failed to resolve current ksud path")?;
        let mut command = Command::new(current_exe);
        command
            .arg("sulogd")
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .current_dir("/");

        Err(command.exec()).context("failed to exec sulogd")
    } else {
        Ok(())
    }
}

pub fn ensure_sulogd_running() -> Result<()> {
    spawn_sulogd()
}
