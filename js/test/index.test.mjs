import assert from "node:assert/strict";
import { after, before, describe, it } from "node:test";

// Bridge expects `window` for exec/spawn callback registration (browser WebView).
globalThis.window = globalThis;

function baseKsu() {
  return {
    exec() {},
    spawn() {},
    fullScreen() {},
    enableEdgeToEdge() {},
    toast() {},
    moduleInfo() {},
    listPackages() {
      return "[]";
    },
    getPackagesInfo() {
      return "[]";
    },
    exit() {},
  };
}

describe("kernelsu module WebUI bridge", () => {
  let listPackages;
  let getPackagesInfo;
  let exec;

  before(async () => {
    globalThis.ksu = baseKsu();
    ({ listPackages, getPackagesInfo, exec } = await import("../index.js"));
  });

  after(() => {
    delete globalThis.ksu;
  });

  it("listPackages returns parsed array on valid JSON", () => {
    globalThis.ksu.listPackages = () => '["a.pkg","b.pkg"]';
    assert.deepEqual(listPackages("user"), ["a.pkg", "b.pkg"]);
  });

  it("listPackages returns empty array when JSON.parse throws", () => {
    globalThis.ksu.listPackages = () => "not-json";
    assert.deepEqual(listPackages("user"), []);
  });

  it("getPackagesInfo stringifies non-string packages argument", () => {
    let seen;
    globalThis.ksu.getPackagesInfo = (json) => {
      seen = json;
      return "[]";
    };
    getPackagesInfo(["x", "y"]);
    assert.equal(seen, '["x","y"]');
  });

  it("getPackagesInfo returns empty array when JSON.parse throws", () => {
    globalThis.ksu.getPackagesInfo = () => "@@@";
    assert.deepEqual(getPackagesInfo(["a"]), []);
  });

  it("exec resolves with stdout and removes callback from window", async () => {
    globalThis.ksu.exec = (command, optionsJson, callbackFuncName) => {
      assert.equal(command, "id");
      assert.ok(typeof callbackFuncName === "string");
      assert.ok(callbackFuncName.startsWith("exec_callback_"));
      queueMicrotask(() => {
        globalThis.window[callbackFuncName](0, "uid=0", "");
      });
    };

    const result = await exec("id", { cwd: "/data" });
    assert.equal(result.errno, 0);
    assert.equal(result.stdout, "uid=0");
    assert.equal(result.stderr, "");
  });
});
