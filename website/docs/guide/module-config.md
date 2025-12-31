# Module Configuration

KernelSU provides a built-in configuration system that allows modules to store persistent or temporary key-value settings. Configurations are stored in a binary format at `/data/adb/ksu/module_configs/<module_id>/` with the following characteristics:

## Configuration Types

- **Persist Config** (`persist.config`): Survives reboots and persists until explicitly deleted or the module is uninstalled
- **Temp Config** (`tmp.config`): Automatically cleared during the post-fs-data stage on every boot

When reading configurations, temporary values take priority over persistent values for the same key.

## Using Configuration in Module Scripts

All module scripts (`post-fs-data.sh`, `service.sh`, `boot-completed.sh`, etc.) run with the `KSU_MODULE` environment variable set to the module ID. You can use the `ksud module config` commands to manage your module's configuration:

```bash
# Get a configuration value
value=$(ksud module config get my_setting)

# Set a persistent configuration value
ksud module config set my_setting "some value"

# Set a temporary configuration value (cleared on reboot)
ksud module config set --temp runtime_state "active"

# Set value from stdin (useful for multiline or complex data)
ksud module config set my_key <<EOF
multiline
text value
EOF

# Or pipe from command
echo "value" | ksud module config set my_key

# Explicit stdin flag
cat file.json | ksud module config set json_data --stdin

# List all configuration entries (merged persist + temp)
ksud module config list

# Delete a configuration entry
ksud module config delete my_setting

# Delete a temporary configuration entry
ksud module config delete --temp runtime_state

# Clear all persistent configurations
ksud module config clear

# Clear all temporary configurations
ksud module config clear --temp
```

## Validation Limits

The configuration system enforces the following limits:

- **Maximum key length**: 256 bytes
- **Maximum value length**: 1MB (1048576 bytes)
- **Maximum config entries**: 32 per module
- **Key format**: Must match `^[a-zA-Z][a-zA-Z0-9._-]+$` (same as module ID)
  - Must start with a letter (a-zA-Z)
  - Can contain letters, numbers, dots (`.`), underscores (`_`), or hyphens (`-`)
  - Minimum length: 2 characters
- **Value format**: No restrictions - can contain any UTF-8 characters including newlines, control characters, etc.
  - Stored in binary format with length prefix, ensuring safe handling of all data

## Lifecycle

- **On boot**: All temporary configurations are cleared during the post-fs-data stage
- **On module uninstall**: All configurations (both persist and temp) are removed automatically
- Configurations are stored in a binary format with magic number `0x4b53554d` ("KSUM") and version validation

## Use Cases

The configuration system is ideal for:

- **User preferences**: Store module settings that users configure through WebUI or action scripts
- **Feature flags**: Enable/disable module features without reinstalling
- **Runtime state**: Track temporary state that should reset on reboot (use temp config)
- **Installation settings**: Remember choices made during module installation
- **Complex data**: Store JSON, multiline text, Base64 encoded data, or any structured content (up to 1MB)

::: tip BEST PRACTICES
- Use persistent configs for user preferences that should survive reboots
- Use temporary configs for runtime state or feature toggles that should reset on boot
- Validate configuration values in your scripts before using them
- Use the `ksud module config list` command to debug configuration issues
:::

## Advanced Features

The module configuration system provides special configuration keys for advanced use cases:

### Overriding Module Description {#overriding-module-description}

You can dynamically override the `description` field from `module.prop` by setting the `override.description` configuration key:

```bash
# Override module description
ksud module config set override.description "Custom description shown in the manager"
```

When the module list is retrieved, if the `override.description` config exists, it will replace the original description from `module.prop`. This is useful for:
- Displaying dynamic status information in the module description
- Showing runtime configuration details to users
- Updating description based on module state without reinstalling

### Declaring Managed Features

Modules can declare which KernelSU features they manage using the `manage.<feature>` configuration pattern. The supported features correspond to KernelSU's internal `FeatureId` enum:

**Supported Features:**
- `su_compat` - SU compatibility mode
- `kernel_umount` - Kernel automatic unmount

```bash
# Declare that this module manages SU compatibility and enables it
ksud module config set manage.su_compat true

# Declare that this module manages kernel unmount and disables it
ksud module config set manage.kernel_umount false

# Remove feature management (module no longer controls this feature)
ksud module config delete manage.su_compat
```

**How it works:**
- The presence of a `manage.<feature>` key indicates the module is managing that feature
- The value indicates the desired state: `true`/`1` for enabled, `false`/`0` (or any other value) for disabled
- To stop managing a feature, delete the configuration key entirely

Managed features are exposed through the module list API as a `managedFeatures` field (comma-separated string). This allows:
- KernelSU manager to detect which modules manage which KernelSU features
- Prevention of conflicts when multiple modules try to manage the same feature
- Better coordination between modules and core KernelSU functionality

::: warning SUPPORTED FEATURES ONLY
Only use the predefined feature names listed above (`su_compat`, `kernel_umount`). These correspond to actual KernelSU internal features. Using other feature names will not cause errors but serves no functional purpose.
:::
