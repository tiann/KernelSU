let callbackCounter = 0;
function getUniqueCallbackName() {
  return `_callback_${Date.now()}_${callbackCounter++}`;
}

function exec(command, options) {
  if (typeof options === "undefined") {
    options = {};
  }

  return new Promise((resolve, reject) => {
    // Generate a unique callback function name
    const callbackFuncName = getUniqueCallbackName();

    // Define the success callback function
    window[callbackFuncName] = (stdout, stderr) => {
      resolve({ stdout, stderr });
      cleanup(callbackFuncName);
    };

    // Define the failure callback function
    const errorFuncName = callbackFuncName + "_error";
    window[errorFuncName] = (error) => {
      reject(error);
      cleanup(callbackFuncName, errorFuncName);
    };

    // Cleanup function to remove the callbacks
    function cleanup(successName, errorName = successName) {
      delete window[successName];
      if (errorName) {
        delete window[errorName];
      }
    }

    try {
      ksu.exec(
        command,
        JSON.stringify(options),
        callbackFuncName,
        errorFuncName
      );
    } catch (error) {
      reject(error);
      cleanup(callbackFuncName, errorFuncName);
    }
  });
}
