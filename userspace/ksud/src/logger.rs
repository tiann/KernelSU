use android_logger::{AndroidLogger, Config};
use log::{Level, LevelFilter, Log, Metadata, Record};
use std::sync::OnceLock;
use std::sync::atomic::{AtomicUsize, Ordering};

static LOGGER: OnceLock<MyLogger> = OnceLock::new();
static ANDROID_LOG_MAX_LEVEL: AtomicUsize = AtomicUsize::new(level_filter_to_usize(LevelFilter::Off));
static STDIO_LOG_MAX_LEVEL: AtomicUsize = AtomicUsize::new(level_filter_to_usize(LevelFilter::Off));

pub struct MyLogger {
    android: AndroidLogger,
}

impl MyLogger {
    fn new(config: Config) -> Self {
        Self {
            android: AndroidLogger::new(config),
        }
    }

    fn println_enabled(level: Level) -> bool {
        level_filter_to_usize(level.to_level_filter()) <= STDIO_LOG_MAX_LEVEL.load(Ordering::Relaxed)
    }
}

impl Log for MyLogger {
    fn enabled(&self, metadata: &Metadata) -> bool {
        self.android.enabled(metadata) || Self::println_enabled(metadata.level())
    }

    fn log(&self, record: &Record) {
        if !self.enabled(record.metadata()) {
            return;
        }

        if self.android.enabled(record.metadata()) {
            self.android.log(record);
        }

        if Self::println_enabled(record.level()) {
            println!("[{}] {}", record.level(), record.args());
        }
    }

    fn flush(&self) {
        self.android.flush();
    }
}

pub fn init_once(config: Config, android_max_level: LevelFilter) {
    ANDROID_LOG_MAX_LEVEL.store(level_filter_to_usize(android_max_level), Ordering::Relaxed);

    let logger = LOGGER.get_or_init(|| MyLogger::new(config));

    if let Err(err) = log::set_logger(logger) {
        log::debug!("logger: log::set_logger failed: {err}");
    } else {
        update_global_max_level();
    }
}

pub fn set_stdio_log_max_level(level: LevelFilter) {
    STDIO_LOG_MAX_LEVEL.store(level_filter_to_usize(level), Ordering::Relaxed);
    update_global_max_level();
}

fn update_global_max_level() {
    let max_level = ANDROID_LOG_MAX_LEVEL
        .load(Ordering::Relaxed)
        .max(STDIO_LOG_MAX_LEVEL.load(Ordering::Relaxed));
    log::set_max_level(level_filter_from_usize(max_level));
}

const fn level_filter_to_usize(level: LevelFilter) -> usize {
    match level {
        LevelFilter::Off => 0,
        LevelFilter::Error => 1,
        LevelFilter::Warn => 2,
        LevelFilter::Info => 3,
        LevelFilter::Debug => 4,
        LevelFilter::Trace => 5,
    }
}

const fn level_filter_from_usize(level: usize) -> LevelFilter {
    match level {
        0 => LevelFilter::Off,
        1 => LevelFilter::Error,
        2 => LevelFilter::Warn,
        3 => LevelFilter::Info,
        4 => LevelFilter::Debug,
        _ => LevelFilter::Trace,
    }
}
