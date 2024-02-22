# Library for KernelSU's module WebUI

## Install

```sh
yarn add kernelsu
```

## API

### exec

Spawns a **root** shell and runs a command within that shell, passing the `stdout` and `stderr` to a Promise when complete.

- `command` `<string>` The command to run, with space-separated arguments.
- `options` `<Object>`
  - `cwd` - Current working directory of the child process
  - `env` - Environment key-value pairs

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

Returns a `ChildProcess`, Instances of the ChildProcess represent spawned child processes.

- `command` `<string>` The command to run.
- `args` `<string[]>` List of string arguments.
- `options` `<Object>`:
  - `cwd` `<string>` - Current working directory of the child process
  - `env` `<Object>` - Environment key-value pairs

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

- `code` `<number>` The exit code if the child exited on its own.

The `'exit'` event is emitted after the child process ends. If the process exited, `code` is the final exit code of the process, otherwise null

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

### toast

Show a toast message.

```javascript
import { toast } from 'kernelsu';
toast('Hello, world!');
```
