# Library for KernelSU's module WebUI

## Install

```sh
yarn add kernelsu
```

## API

### exec

Spawns a **root** shell and runs a command within that shell, returning a Promise that resolves with the `stdout` and `stderr` outputs upon completion.

- `command` `<string>` The command to run, with space-separated arguments.
- `options` `<Object>`
  - `cwd` - Current working directory of the child process.
  - `env` - Environment key-value pairs.

```javascript
import { exec } from 'kernelsu';

const { errno, stdout, stderr } = await exec('ls -l', { cwd: '/tmp' });
if (errno === 0) {
    // success
    console.log(stdout);
}
```

### spawn

Spawns a new process using the given `command` in **root** shell, with command-line arguments in `args`. If omitted, `args` defaults to an empty array.

Returns a `ChildProcess` instance. Instances of `ChildProcess` represent spawned child processes.

- `command` `<string>` The command to run.
- `args` `<string[]>` List of string arguments.
- `options` `<Object>`:
  - `cwd` `<string>` - Current working directory of the child process.
  - `env` `<Object>` - Environment key-value pairs.

Example of running `ls -lh /data`, capturing `stdout`, `stderr`, and the exit code:

```javascript
import { spawn } from 'kernelsu';

const ls = spawn('ls', ['-lh', '/data']);

ls.stdout.on('data', (data) => {
  console.log(`stdout: ${data}`);
});

ls.stderr.on('data', (data) => {
  console.log(`stderr: ${data}`);
});

ls.on('exit', (code) => {
  console.log(`child process exited with code ${code}`);
});
```

#### ChildProcess

##### Event 'exit'

- `code` `<number>` The exit code if the child process exited on its own.

The `'exit'` event is emitted when the child process ends. If the process exits, `code` contains the final exit code; otherwise, it is null.

##### Event 'error'

- `err` `<Error>` The error.

The `'error'` event is emitted whenever:

- The process could not be spawned.
- The process could not be killed.

##### `stdout`

A `Readable Stream` that represents the child process's `stdout`.

```javascript
const subprocess = spawn('ls');

subprocess.stdout.on('data', (data) => {
  console.log(`Received chunk ${data}`);
});
```

#### `stderr`

A `Readable Stream` that represents the child process's `stderr`.

### fullScreen

Request the WebView enter/exit full screen.

```javascript
import { fullScreen } from 'kernelsu';
fullScreen(true);
```

### enableInsets

Request the WebView to set padding to 0 or system bar insets

- tips: this is disabled by default but if you request resource from `internal/insets.css`, this will be enabled automatically.
- To get insets value and enable this automatically, you can
  - add `@import "https://mui.kernelsu.org/internal/insets.css";` in css OR
  - add `<link rel="stylesheet" type="text/css" href="/internal/insets.css" />` in html.

```javascript
import { enableInsets } from 'kernelsu';
enableInsets(true);
```

### toast

Show a toast message.

```javascript
import { toast } from 'kernelsu';
toast('Hello, world!');
```

### moduleInfo

Get module info.

```javascript
import { moduleInfo } from 'kernelsu';
// print moduleId in console
console.log(moduleInfo());
```

### listPackages

List installed packages.

Returns an array of package names.

- `type` `<string>` The type of packages to list: "user", "system", or "all".

```javascript
import { listPackages } from 'kernelsu';
// list user packages
const packages = listPackages("user");
```

- tips: when `listPackages` api is available, you can use ksu://icon/{packageName} to get app icon.

``` javascript
img.src = "ksu://icon/" + packageName;
```

### getPackagesInfo

Get information for a list of packages.

Returns an array of `PackagesInfo` objects.

- `packages` `<string[]>` The list of package names.

```javascript
import { getPackagesInfo } from 'kernelsu';
const packages = getPackagesInfo(['com.android.settings', 'com.android.shell']);
```

#### PackagesInfo

An object contains:

- `packageName` `<string>` Package name of the application.
- `versionName` `<string>` Version of the application.
- `versionCode` `<number>` Version code of the application.
- `appLabel` `<string>` Display name of the application.
- `isSystem` `<boolean>` Whether the application is a system app.
- `uid` `<number>` UID of the application.

### exit

Exit the current WebUI activity.

```javascript
import { exit } from 'kernelsu';
exit();
```
