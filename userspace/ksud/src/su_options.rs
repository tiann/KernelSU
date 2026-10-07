//! Magisk-compatible su option parsing, without changing shell argument boundaries.
use anyhow::{Result, bail};

#[derive(Debug, Default, PartialEq, Eq)]
// These are independent command-line switches, rather than mutually exclusive states.
#[allow(clippy::struct_excessive_bools)]
pub struct SuOptions {
    pub uid: Option<u32>,
    pub gids: Vec<u32>,
    pub shell: Option<String>,
    pub shell_args: Vec<String>,
    pub context: Option<String>,
    pub target_pid: Option<i32>,
    pub preserve_env: bool,
    pub login: bool,
    pub no_wrapper: bool,
    pub no_new_privs: bool,
    pub help: bool,
    pub version: bool,
    pub version_code: bool,
}

fn number(value: &str, kind: &str) -> Result<u32> {
    if value.is_empty() || !value.bytes().all(|byte| byte.is_ascii_digit()) {
        bail!("Invalid {kind}: {value}");
    }
    let value = value.parse::<u32>()?;
    if value > i32::MAX as u32 {
        bail!("Invalid {kind}: {value}");
    }
    Ok(value)
}

impl SuOptions {
    pub fn parse(args: &[String], resolve_user: impl Fn(&str) -> Option<u32>) -> Result<Self> {
        let mut options = Self::default();
        let mut index = 0;
        'options: while let Some(original) = args.get(index) {
            let arg = match original.as_str() {
                "-mm" => "-M",
                "-cn" | "-z" => "-Z",
                arg => arg,
            };
            if arg == "--" {
                index += 1;
                break;
            }
            if !arg.starts_with('-') || arg == "-" {
                break;
            }
            let mut long_value = None;
            let flags = if let Some(long) = arg.strip_prefix("--") {
                let (name, value) = long
                    .split_once('=')
                    .map_or((long, None), |(name, value)| (name, Some(value)));
                long_value = value;
                match name {
                    "help" => "h",
                    "version" => "v",
                    "preserve-environment" => "p",
                    "login" => "l",
                    "shell" => "s",
                    "group" => "g",
                    "supp-group" => "G",
                    "context" => "Z",
                    "target" => "t",
                    "mount-master" => "M",
                    "no-wrapper" => "W",
                    "ksu-no-new-privs" => "N",
                    // KSU inherits its caller's TTY; it needs no remote PTY allocation.
                    "interactive" => "i",
                    _ => break,
                }
            } else {
                &arg[1..]
            };
            let mut flags = flags.char_indices().peekable();
            while let Some((position, flag)) = flags.next() {
                let takes_value = matches!(flag, 's' | 'g' | 'G' | 'Z' | 't');
                if !takes_value && long_value.is_some() {
                    bail!("Option {original} does not take an argument");
                }
                let value = if takes_value {
                    Some(if let Some(value) = long_value {
                        value
                    } else if flags.peek().is_some() {
                        // An attached short option value consumes the rest of the token.
                        &arg[position + 2..]
                    } else {
                        index += 1;
                        args.get(index).ok_or_else(|| {
                            anyhow::anyhow!("Option {original} requires an argument")
                        })?
                    })
                } else {
                    None
                };
                match (flag, value) {
                    ('h', _) => {
                        options.help = true;
                        return Ok(options);
                    }
                    ('v', _) => {
                        options.version = true;
                        return Ok(options);
                    }
                    ('V', _) => {
                        options.version_code = true;
                        return Ok(options);
                    }
                    ('p' | 'm', _) => options.preserve_env = true,
                    ('l', _) => options.login = true,
                    ('W', _) => options.no_wrapper = true,
                    ('N', _) if arg.starts_with("--") => options.no_new_privs = true,
                    ('i', _) => {}
                    ('s', Some(value)) => options.shell = Some(value.to_owned()),
                    ('Z', Some(value)) => options.context = Some(value.to_owned()),
                    ('g' | 'G', Some(value)) => {
                        let gid = number(value, "GID")?;
                        if flag == 'g' && !options.gids.is_empty() {
                            let old_primary = std::mem::replace(&mut options.gids[0], gid);
                            options.gids.push(old_primary);
                        } else {
                            options.gids.push(gid);
                        }
                    }
                    ('M' | 't', value) => {
                        if options.target_pid.is_some() {
                            bail!("Can't use -M and -t at the same time or specify a target twice");
                        }
                        options.target_pid =
                            Some(value.map_or(Ok(0), |value| number(value, "PID"))? as i32);
                    }
                    _ => break 'options,
                }
                if takes_value {
                    break;
                }
            }
            index += 1;
        }
        if args.get(index).is_some_and(|arg| arg == "-") {
            options.login = true;
            index += 1;
        }
        if let Some(user) = args.get(index)
            && let Some(uid) = resolve_user(user)
        {
            options.uid = Some(uid);
            index += 1;
        }
        options.shell_args = args[index..].to_vec();
        // Match Magisk's legacy `su -c command arg...` compatibility rule.
        // Quoted command strings keep their shell $0/$1/... arguments intact.
        if options.shell_args.len() >= 3
            && options.shell_args[0] == "-c"
            && !options.shell_args[1].contains(' ')
        {
            options.shell_args = vec!["-c".to_owned(), options.shell_args[1..].join(" ")];
        }
        Ok(options)
    }
}
