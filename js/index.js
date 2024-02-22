let callbackCounter = 0;
function getUniqueCallbackName() {
  return `_callback_${Date.now()}_${callbackCounter++}`;
}

export function exec(command, options) {
  if (typeof options === "undefined") {
    options = {};
  }

  return new Promise((resolve, reject) => {
    // Generate a unique callback function name
    const callbackFuncName = getUniqueCallbackName();

    // Define the success callback function
    window[callbackFuncName] = (errno, stdout, stderr) => {
      resolve({ errno, stdout, stderr });
      cleanup(callbackFuncName);
    };

    function cleanup(successName) {
      delete window[successName];
    }

    try {
      ksu.exec(
        command,
        JSON.stringify(options),
        callbackFuncName,
      );
    } catch (error) {
      reject(error);
      cleanup(callbackFuncName);
    }
  });
}

export function fullScreen(isFullScreen) {
  ksu.fullScreen(isFullScreen);
}

export function toast(message) {
  ksu.toast(message);
}
