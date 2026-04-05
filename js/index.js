let callbackCounter = 0;
function getUniqueCallbackName(prefix) {
  return `${prefix}_callback_${Date.now()}_${callbackCounter++}`;
}

export function exec(command, options) {
  if (typeof options === "undefined") {
    options = {};
  }

  return new Promise((resolve, reject) => {
    // Generate a unique callback function name
    const callbackFuncName = getUniqueCallbackName("exec");

    // Define the success callback function
    window[callbackFuncName] = (errno, stdout, stderr) => {
      resolve({ errno, stdout, stderr });
      cleanup(callbackFuncName);
    };

    function cleanup(successName) {
      delete window[successName];
    }

    try {
      ksu.exec(command, JSON.stringify(options), callbackFuncName);
    } catch (error) {
      reject(error);
      cleanup(callbackFuncName);
    }
  });
}

function Stdio() {
    this.listeners = {};
  }
  
  Stdio.prototype.on = function (event, listener) {
    if (!this.listeners[event]) {
      this.listeners[event] = [];
    }
    this.listeners[event].push(listener);
  };
  
  Stdio.prototype.emit = function (event, ...args) {
    if (this.listeners[event]) {
      this.listeners[event].forEach((listener) => listener(...args));
    }
  };
  
  function ChildProcess() {
    this.listeners = {};
    this.stdin = new Stdio();
    this.stdout = new Stdio();
    this.stderr = new Stdio();
  }
  
  ChildProcess.prototype.on = function (event, listener) {
    if (!this.listeners[event]) {
      this.listeners[event] = [];
    }
    this.listeners[event].push(listener);
  };
  
  ChildProcess.prototype.emit = function (event, ...args) {
    if (this.listeners[event]) {
      this.listeners[event].forEach((listener) => listener(...args));
    }
  };
  
  export function spawn(command, args, options) {
    if (typeof args === "undefined") {
      args = [];
    } else if (!(args instanceof Array)) {
        // allow for (command, options) signature
        options = args;
        args = [];
    }
    
    if (typeof options === "undefined") {
      options = {};
    }
  
    const child = new ChildProcess();
    const childCallbackName = getUniqueCallbackName("spawn");
    window[childCallbackName] = child;
  
    function cleanup(name) {
      delete window[name];
    }

    child.on("exit", code => {
        cleanup(childCallbackName);
    });

    try {
      ksu.spawn(
        command,
        JSON.stringify(args),
        JSON.stringify(options),
        childCallbackName
      );
    } catch (error) {
      child.emit("error", error);
      cleanup(childCallbackName);
    }
    return child;
  }

export function fullScreen(isFullScreen) {
  ksu.fullScreen(isFullScreen);
}

export function enableEdgeToEdge(enable) {
  ksu.enableEdgeToEdge(enable);
}

export function toast(message) {
  ksu.toast(message);
}

export function moduleInfo() {
  return ksu.moduleInfo();
}

export function listPackages(type) {
  try {
    return JSON.parse(ksu.listPackages(type));
  } catch (error) {
    return [];
  }
}

export function getPackagesInfo(packages) {
  try {
    if (typeof packages !== "string") {
      packages = JSON.stringify(packages);
    }
    return JSON.parse(ksu.getPackagesInfo(packages));
  } catch (error) {
    return [];
  }
}

export function exit() {
  ksu.exit();
}

export const io = {
  File(path) {
    return ksu.io().File(path);
  },

  FileInputStream() {
    const impl = ksu.io().FileInputStream();
    return {
      open(path) { return impl.open(path); },
      read(id, maxBytes) { return impl.read(id, maxBytes); },
      available(id) { return impl.available(id); },
      close(id) { return impl.close(id); },
    };
  },

  FileOutputStream() {
    const impl = ksu.io().FileOutputStream();
    return {
      open(path, append) { return impl.open(path, append); },
      write(id, data) { return impl.write(id, data); },
      writeByte(id, b) { return impl.writeByte(id, b); },
      flush(id) { return impl.flush(id); },
      close(id) { return impl.close(id); },
    };
  },

  RandomAccessFile() {
    const impl = ksu.io().RandomAccessFile();
    return {
      open(path, mode) { return impl.open(path, mode); },
      read(id) { return impl.read(id); },
      readBytes(id, len) { return impl.readBytes(id, len); },
      readBoolean(id) { return impl.readBoolean(id); },
      readByte(id) { return impl.readByte(id); },
      readInt(id) { return impl.readInt(id); },
      readLong(id) { return impl.readLong(id); },
      readShort(id) { return impl.readShort(id); },
      readFloat(id) { return impl.readFloat(id); },
      readDouble(id) { return impl.readDouble(id); },
      readUTF(id) { return impl.readUTF(id); },
      readLine(id) { return impl.readLine(id); },
      write(id, b) { impl.write(id, b); },
      writeBase64(id, data) { impl.writeBase64(id, data); },
      writeBoolean(id, v) { impl.writeBoolean(id, v); },
      writeByte(id, v) { impl.writeByte(id, v); },
      writeInt(id, v) { impl.writeInt(id, v); },
      writeLong(id, v) { impl.writeLong(id, v); },
      writeShort(id, v) { impl.writeShort(id, v); },
      writeFloat(id, v) { impl.writeFloat(id, v); },
      writeDouble(id, v) { impl.writeDouble(id, v); },
      writeUTF(id, str) { impl.writeUTF(id, str); },
      seek(id, pos) { return impl.seek(id, pos); },
      getFilePointer(id) { return impl.getFilePointer(id); },
      length(id) { return impl.length(id); },
      setLength(id, newLength) { return impl.setLength(id, newLength); },
      close(id) { return impl.close(id); },
    };
  },
};
