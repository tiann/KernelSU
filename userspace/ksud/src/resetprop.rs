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
    disable_help_subcommand = true,
    after_help = "Arguments:\n  NAME   Property name.\n  VALUE  Property value (for set or wait-for-value)."
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

    /// Rebuild a property area by SELinux context name, or all property areas if name is not given.
    #[arg(short = 'c', long = "rebuild", alias = "compact")]
    rebuild: bool,

    /// Show SELinux context when listing properties, or if -c is used, rebuild the property area containing the property NAME.
    #[arg(short = 'Z')]
    show_context: bool,

    /// Force rebuild all property areas, should be used with `-c` . Without this flag set, only abnormal property areas will be rebuilt.
    #[arg(long = "force")]
    force: bool,

    #[arg(
        allow_hyphen_values = true,
        trailing_var_arg = true,
        num_args = 0..=2,
        hide = true,
    )]
    arguments: Vec<String>,
}

impl Args {
    fn name(&self) -> Option<&String> {
        self.arguments.first()
    }

    fn value(&self) -> Option<&String> {
        self.arguments.get(1)
    }
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
        rebuild: false,
    };

    // Validate: at most one special mode
    let special_modes = u8::from(cli.wait) + u8::from(cli.delete) + u8::from(cli.file.is_some());
    if special_modes > 1 {
        bail!("multiple operation modes detected");
    }

    if cli.rebuild && !(special_modes == 0 || cli.delete) {
        bail!("Only -d can be used with -c");
    }

    // -w: wait mode
    if cli.wait {
        let name = cli.name().context("--wait requires a property name")?;
        let timeout = cli.timeout.map(Duration::from_secs_f64);
        let ok = rp
            .wait(name, cli.value().map(std::string::String::as_str), timeout)
            .context("wait failed")?;
        if !ok {
            return Err(WaitTimeoutError {
                name: name.to_owned(),
            }
            .into());
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
        let name = cli.name().context("--delete requires a property name")?;
        let deleted = rp.delete(name).context("delete failed")?;
        if !deleted {
            bail!("{name} not found");
        }
        if !cli.rebuild {
            return Ok(());
        }
    }

    if cli.rebuild {
        if let Some(name) = cli.name() {
            let ctx = if cli.show_context || cli.delete {
                sys_prop::get_context(name)?
            } else {
                name.to_owned()
            };
            rp.rebuild(&ctx)?;
        } else if !rp.rebuild_all(cli.force)? {
            eprintln!("Something wrong happened, see log for detail.");
            std::process::exit(1);
        }
        return Ok(());
    }

    let name = cli.name();
    let value = cli.value();

    match (name, value) {
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
        rebuild: false,
    };

    let file = File::open(path).with_context(|| format!("Failed to open {}", path.display()))?;
    let reader = BufReader::new(file);
    rp.load_props(reader.lines())
        .with_context(|| format!("Failed to load properties from {}", path.display()))?;

    info!("Loaded system.prop from {}", path.display());
    Ok(())
}
