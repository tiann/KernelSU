use anyhow::{Context, Result, bail};
use clap::Parser;
use clap::error::ErrorKind;
use log::info;
use prop_rs_android::resetprop::ResetProp;
use prop_rs_android::sys_prop;
use std::fmt;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::path::Path;
use std::time::Duration;

#[derive(Debug)]
struct WaitTimeoutError {
    name: String,
}

impl fmt::Display for WaitTimeoutError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "timeout waiting for {}", self.name)
    }
}

impl std::error::Error for WaitTimeoutError {}

/// Magisk-compatible Android system property tool.
#[derive(Parser)]
#[command(
    name = "resetprop",
    version,
    about = "Magisk-compatible system property tool",
    disable_help_subcommand = true
)]
#[allow(clippy::struct_excessive_bools)]
struct Args {
    /// Skip property_service (force direct mmap operation).
    #[arg(short = 'n', long = "skip-svc")]
    skip_svc: bool,

    /// Also operate on persistent property storage (persist.* files).
    #[arg(short = 'p', long = "persistent")]
    persistent: bool,

    /// Only read persistent properties from storage.
    #[arg(short = 'P')]
    persist_only: bool,

    /// Delete the named property.
    #[arg(short = 'd', long = "delete")]
    delete: bool,

    /// Verbose output.
    #[arg(short = 'v', long = "verbose")]
    verbose: bool,

    /// Wait for a property to exist or change from a given value to another value.
    #[arg(short = 'w', long = "wait")]
    wait: bool,

    /// Timeout in seconds for --wait (default: wait forever).
    #[arg(long = "timeout")]
    timeout: Option<f64>,

    /// Load and set properties from FILE.
    #[arg(short = 'f', long = "file")]
    file: Option<String>,

    /// Compact property area memory (reclaim holes left by deleted properties).
    /// Optionally pass a SELinux context name to compact only that area.
    #[arg(short = 'c', long = "compact")]
    compact: bool,

    /// Show SELinux context when listing properties.
    #[arg(short = 'Z')]
    show_context: bool,

    /// Property name.
    name: Option<String>,

    /// Property value (for set or wait-for-value).
    value: Option<String>,
}

pub fn resetprop_main(args: &[String]) -> ! {
    if let Err(err) = run_from_args(args) {
        let code = if err.downcast_ref::<WaitTimeoutError>().is_some() {
            2
        } else {
            1
        };
        eprintln!("resetprop: {err:#}");
        std::process::exit(code);
    }
    std::process::exit(0);
}

/// Entry point for resetprop multicall and subcommand.
///
/// `args` should include argv[0] (the program name).
fn run_from_args(args: &[String]) -> Result<()> {
    let cli = match Args::try_parse_from(args) {
        Ok(cli) => cli,
        Err(err) => {
            if matches!(
                err.kind(),
                ErrorKind::DisplayHelp | ErrorKind::DisplayVersion
            ) {
                err.print()?;
                return Ok(());
            }
            return Err(anyhow::anyhow!("{err}"));
        }
    };

    sys_prop::init().context("Failed to initialize system property API")?;

    let rp = ResetProp {
        skip_svc: cli.skip_svc,
        persistent: cli.persistent,
        persist_only: cli.persist_only,
        verbose: cli.verbose,
        show_context: cli.show_context,
    };

    // Validate: at most one special mode
    let special_modes = u8::from(cli.wait)
        + u8::from(cli.delete)
        + u8::from(cli.compact)
        + u8::from(cli.file.is_some());
    if special_modes > 1 {
        bail!("multiple operation modes detected");
    }

    // -w: wait mode
    if cli.wait {
        let name = cli
            .name
            .as_deref()
            .context("--wait requires a property name")?;
        let timeout = cli.timeout.map(Duration::from_secs_f64);
        let ok = rp
            .wait(name, cli.value.as_deref(), timeout)
            .context("wait failed")?;
        if !ok {
            return Err(WaitTimeoutError {
                name: name.to_owned(),
            }
            .into());
        }
        return Ok(());
    }

    // -c: compact property area memory
    // When a positional argument is given, treat it as a SELinux context name.
    if cli.compact {
        let context = cli.name.as_deref();
        let compacted = sys_prop::compact(context).context("compact failed")?;
        if !compacted {
            bail!("nothing to compact");
        }
        return Ok(());
    }

    // -f: load from file
    if let Some(path) = &cli.file {
        let file = File::open(path).with_context(|| format!("Failed to open {path}"))?;
        let reader = BufReader::new(file);
        rp.load_props(reader.lines())
            .context("Failed to load properties from file")?;
        return Ok(());
    }

    // -d: delete
    if cli.delete {
        let name = cli
            .name
            .as_deref()
            .context("--delete requires a property name")?;
        let deleted = rp.delete(name).context("delete failed")?;
        if !deleted {
            bail!("{name} not found");
        }
        return Ok(());
    }

    match (&cli.name, &cli.value) {
        // resetprop name value (set)
        (Some(name), Some(value)) => {
            rp.set(name, value)
                .with_context(|| format!("Failed to set {name}"))?;
        }

        // resetprop name (get)
        (Some(name), None) => match rp.get(name) {
            Some(val) => println!("{val}"),
            None => bail!("{name} not found"),
        },

        // resetprop (list all)
        (None, None) => {
            let props = rp.list_all().context("Failed to list properties")?;
            for (name, value) in &props {
                println!("[{name}]: [{value}]");
            }
        }

        // resetprop <no name> <value> — invalid
        (None, Some(_)) => {
            bail!("property name is required");
        }
    }

    Ok(())
}

/// Load system.prop file using internal resetprop API.
///
/// Equivalent to `resetprop -n --file <path>`.
pub fn load_system_prop_file(path: &Path) -> Result<()> {
    sys_prop::init().context("Failed to initialize system property API")?;

    let rp = ResetProp {
        skip_svc: true,
        persistent: false,
        persist_only: false,
        verbose: false,
        show_context: false,
    };

    let file = File::open(path).with_context(|| format!("Failed to open {}", path.display()))?;
    let reader = BufReader::new(file);
    rp.load_props(reader.lines())
        .with_context(|| format!("Failed to load properties from {}", path.display()))?;

    info!("Loaded system.prop from {}", path.display());
    Ok(())
}
