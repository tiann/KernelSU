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

### enableEdgeToEdge

Request the WebView to set padding to 0 or safeDrawing insets

- tips: this is disabled by default but if you request resource from `internal/insets.css`, this will be enabled automatically.
- To get insets value and enable this automatically, you can
  - add `@import "https://mui.kernelsu.org/internal/insets.css";` in css OR
  - add `<link rel="stylesheet" type="text/css" href="/internal/insets.css" />` in html.

```javascript
import { enableEdgeToEdge } from 'kernelsu';
enableEdgeToEdge(true);
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

### io

The exported `io` object provides file system operations with root access. It acts as a small facade over the WebUI file APIs, exposing Java-IO-style factory methods such as `io.File()`, `io.FileInputStream()`, `io.FileOutputStream()`, and `io.RandomAccessFile()`.

```javascript
import { io } from 'kernelsu';
```

#### io.File

Represents a file or directory path. All operations execute via root shell.

**Error handling**: Methods return default values (false, empty arrays, -1, etc.) on error.
Check return values carefully. For example, `exists()` returns false both when a file doesn't
exist and when permission is denied. Use multiple checks to disambiguate (e.g., check parent
directory permissions if a file operation fails).

```javascript
const file = io.File('/data/adb/modules');

if (file.exists()) {
    console.log(file.listFiles());
}
```

##### Methods

- `exists(): boolean` — Check if path exists
- `isFile(): boolean` — Check if path is a regular file
- `isDirectory(): boolean` — Check if path is a directory
- `canRead(): boolean` — Check read permission
- `canWrite(): boolean` — Check write permission
- `canExecute(): boolean` — Check execute permission
- `createNewFile(): boolean` — Create a new empty file
- `delete(): boolean` — Delete file or empty directory
- `deleteRecursive(): boolean` — Recursively delete directory
- `mkdir(): boolean` — Create directory
- `mkdirs(): boolean` — Create directory and all parent directories
- `renameTo(destPath: string): boolean` — Rename or move file
- `list(): string[]` — List filenames in directory
- `listFiles(): string[]` — List full paths of files in directory
- `length(): number` — File size in bytes
- `lastModified(): number` — Last modified timestamp (ms)
- `setLastModified(time: number): boolean` — Set last modified time
- `getAbsolutePath(): string` — Absolute path
- `getCanonicalPath(): string` — Canonical path (resolves symlinks)
- `getParent(): string | null` — Parent directory path
- `getPath(): string` — Path string
- `getName(): string` — File/directory name
- `isHidden(): boolean` — Check if hidden
- `isBlock(): boolean` — Check if block device
- `isCharacter(): boolean` — Check if character device
- `isSymlink(): boolean` — Check if symbolic link
- `createNewSymlink(target: string): boolean` — Create symbolic link
- `createNewLink(existing: string): boolean` — Create hard link
- `clear(): boolean` — Truncate file to zero length
- `setReadOnly(): boolean` — Set read-only
- `setReadable(readable: boolean, ownerOnly: boolean): boolean` — Set read permission
- `setWritable(writable: boolean, ownerOnly: boolean): boolean` — Set write permission
- `setExecutable(executable: boolean, ownerOnly: boolean): boolean` — Set execute permission
- `getFreeSpace(): number` — Free space on partition
- `getTotalSpace(): number` — Total space on partition
- `getUsableSpace(): number` — Usable space on partition
- `newInputStream(): string` — Open input stream, returns stream ID
- `newOutputStream(append?: boolean): string` — Open output stream, returns stream ID

#### io.FileInputStream

Read file contents as base64-encoded chunks.

**Error handling**: Methods return empty string or 0 on error. Always check `open()` return
value before using the stream ID. Close streams when done to avoid resource leaks.

```javascript
const reader = io.FileInputStream();
const id = reader.open('/data/adb/ksu.log');

if (!id) {
    console.error('Failed to open file');
} else {
    let chunk;
    while ((chunk = reader.read(id)) !== '') {
        console.log(atob(chunk));
    }
    reader.close(id);
}
```

##### Methods

- `open(path: string): string` — Open file for reading, returns stream ID
- `read(id: string): string` — Read chunk (up to 8KB), returns base64 string (empty on EOF)
- `read(id: string, maxBytes: number): string` — Read up to maxBytes, returns base64 string
- `available(id: string): number` — Estimated bytes available
- `close(id: string): boolean` — Close stream

#### io.FileOutputStream

Write file contents from base64-encoded data.

**Error handling**: Methods return false on error. Always check `open()` and `write()` return
values. Close streams when done to avoid resource leaks.

```javascript
const writer = io.FileOutputStream();
const id = writer.open('/data/adb/output.txt');

if (!id) {
    console.error('Failed to open file for writing');
} else {
    if (!writer.write(id, btoa('Hello, World!'))) {
        console.error('Write failed');
    }
    writer.close(id);
}
```

##### Methods

- `open(path: string, append?: boolean): string` — Open file for writing, returns stream ID
- `write(id: string, base64: string): boolean` — Write base64 data. For large sequential writes, prefer large chunks (for example 1-2 MiB) to reduce WebView bridge overhead.
- `writeByte(id: string, b: number): boolean` — Write single byte
- `flush(id: string): boolean` — Flush buffer
- `close(id: string): boolean` — Close stream

#### io.RandomAccessFile

Random access file I/O with seek support. Uses `dd` under the hood, so each operation has overhead. Prefer `FileInputStream`/`FileOutputStream` for sequential access.

**Error handling**: Methods return default values on error (0, -1, false, empty string, null).
Always check `open()` return value before using the file ID. Close files when done to avoid
resource leaks.

```javascript
const raf = io.RandomAccessFile();
const id = raf.open('/data/adb/data.bin', 'rw');

if (!id) {
    console.error('Failed to open file');
} else {
    raf.seek(id, 1024);
    raf.writeInt(id, 42);
    raf.seek(id, 1024);
    console.log(raf.readInt(id));
    
    raf.close(id);
}
```

##### Methods

- `open(path: string, mode: string): string` — Open file with mode (`"r"`, `"rw"`, etc.), returns file ID
- `read(id: string): number` — Read single byte (0-255, or -1 on EOF)
- `readBytes(id: string, len: number): string` — Read bytes, returns base64 string
- `readBoolean(id: string): boolean` — Read boolean
- `readByte(id: string): number` — Read signed byte
- `readInt(id: string): number` — Read 32-bit integer
- `readLong(id: string): number` — Read 64-bit integer
- `readShort(id: string): number` — Read 16-bit short
- `readFloat(id: string): number` — Read 32-bit float
- `readDouble(id: string): number` — Read 64-bit double
- `readUTF(id: string): string` — Read UTF string
- `readLine(id: string): string | null` — Read line
- `write(id: string, b: number): void` — Write single byte
- `writeBase64(id: string, data: string): void` — Write base64 data
- `writeBoolean(id: string, v: boolean): void` — Write boolean
- `writeByte(id: string, v: number): void` — Write byte
- `writeInt(id: string, v: number): void` — Write 32-bit integer
- `writeLong(id: string, v: number): void` — Write 64-bit integer
- `writeShort(id: string, v: number): void` — Write 16-bit short
- `writeFloat(id: string, v: number): void` — Write 32-bit float
- `writeDouble(id: string, v: number): void` — Write 64-bit double
- `writeUTF(id: string, str: string): void` — Write UTF string
- `seek(id: string, pos: number): boolean` — Set file pointer position
- `getFilePointer(id: string): number` — Get current file pointer position
- `length(id: string): number` — Get file length
- `setLength(id: string, newLength: number): boolean` — Truncate or extend file
- `close(id: string): boolean` — Close file
