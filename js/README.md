# Library for KernelSU's module WebUI

## Install

```sh
yarn add kernelsu
```

## API

### exec

Execute a command in the **root** shell.

options:

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
