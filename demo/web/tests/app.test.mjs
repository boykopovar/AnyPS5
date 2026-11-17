import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { setImmediate } from "node:timers/promises";
import { test } from "node:test";

class Element {
  children = [];
  listeners = new Map();
  value = "";
  checked = false;
  disabled = false;
  textContent = "";

  constructor(hidden = false) { this.hidden = hidden; }
  addEventListener(type, listener) { this.listeners.set(type, listener); }
  removeAttribute(name) { delete this[name]; }
  append(...children) { this.children.push(...children); }
  replaceChildren(...children) { this.children = children; }
}

async function setup(t, name, overrides = {}) {
  const html = readFileSync(new URL("../index.html", import.meta.url), "utf8");
  const elements = new Map([...html.matchAll(/<[^>]+\bid="([^"]+)"[^>]*>/gu)].map(match => [
    match[1], new Element(/\shidden(?:\s|>)/u.test(match[0]))
  ]));
  const get = id => {
    assert.ok(elements.has(id), `Missing element: ${id}`);
    return elements.get(id);
  };
  const options = new Map(["disk", "zip", "memory"].map(value => [`[value="${value}"]`, new Element()]));
  get("save").querySelector = selector => options.get(selector);
  get("folder").webkitdirectory = true;
  const workers = [];
  class Worker {
    messages = [];
    terminated = false;
    constructor() { workers.push(this); }
    postMessage(message) { this.messages.push(message); }
    terminate() { this.terminated = true; }
  }
  const globals = {
    document: {
      documentElement: { lang: "", dir: "" },
      getElementById: get,
      createElement: () => new Element(),
      createTextNode: text => ({ textContent: text }),
      querySelector: selector => {
        assert.equal(selector, 'input[name="target"]:checked');
        return { value: "linux" };
      },
      querySelectorAll: () => []
    },
    window: { isSecureContext: true, Worker, WebAssembly, ReadableStream, WritableStream, crypto: { subtle: {} }, ...overrides },
    Worker,
    fetch: overrides.fetch ?? (async () => ({
      ok: true,
      json: async () => ({ tag: "test-release", platforms: { linux: { expandedBytes: 1024 }, windows: { expandedBytes: 1024 } } })
    }))
  };
  for (const [key, value] of Object.entries(globals)) {
    const original = Object.getOwnPropertyDescriptor(globalThis, key);
    Object.defineProperty(globalThis, key, { value, configurable: true, writable: true });
    t.after(() => {
      if (original) Object.defineProperty(globalThis, key, original);
      else delete globalThis[key];
    });
  }
  await import(new URL(`../app.mjs?${name}`, import.meta.url));
  await setImmediate();
  return { get, workers };
}

async function inspect(worker, count) {
  for (let index = 0; index < count; index++) {
    await worker.onmessage({ data: {
      type: "inspected", index,
      summary: { files: 1, totalBytes: 1024, modules: 0, selectedBytes: 1024, windowsSelectedBytes: 1024, warnings: ["Test warning"] }
    } });
  }
  await worker.onmessage({ data: { type: "inspection-complete" } });
}

for (const stage of ["constructor", "postMessage"]) {
  test(`A worker ${stage} failure restores selection and permits retry`, async t => {
    const { get, workers } = await setup(t, `startup-${stage}`);
    const OriginalWorker = globalThis.Worker;
    const message = stage === "constructor" ? "Worker creation denied" : "Input could not be cloned";
    globalThis.Worker = class extends OriginalWorker {
      constructor() {
        if (stage === "constructor") throw new DOMException(message, "SecurityError");
        super();
      }
      postMessage() { throw new DOMException(message, "DataCloneError"); }
    };
    get("file").files = [{ name: "first.zip" }];
    get("file").onchange();
    await setImmediate();
    globalThis.Worker = OriginalWorker;

    assert.equal(get("error").hidden, false);
    assert.equal(get("error").children[0].textContent, message);
    assert.equal(get("status").hidden, true);
    assert.equal(get("cancel").hidden, true);
    assert.equal(get("drop").hidden, false);
    for (const id of ["choose", "choose-folder", "change", "target", "save", "intel"]) {
      assert.equal(get(id).disabled, false, id);
    }
    assert.equal(get("convert").disabled, true);
    assert.equal(workers.length, stage === "constructor" ? 0 : 1);
    for (const worker of workers) assert.equal(worker.terminated, true);

    get("file").files = [{ name: "replacement.zip" }];
    get("file").onchange();
    const replacement = workers.at(-1);
    assert.deepEqual(replacement.messages[0].inputs.map(input => input.name), ["replacement.zip"]);
    await inspect(replacement, 1);
    assert.equal(get("error").hidden, true);
    assert.equal(get("archive-name").textContent, "replacement.zip");
    get("acknowledge").checked = true;
    get("acknowledge").listeners.get("change")();
    assert.equal(get("convert").disabled, false);
  });
}

for (const kind of ["zip", "batch", "folder"]) {
  test(`Change resets an inspected ${kind} and permits a new conversion`, async t => {
    const { get, workers } = await setup(t, kind);
    assert.equal(get("convert").disabled, true);
    if (kind === "folder") {
      get("folder").files = [{ name: "eboot.bin", webkitRelativePath: "app/eboot.bin" }];
      get("folder").onchange();
    } else {
      get("file").files = kind === "batch" ? [{ name: "first.zip" }, { name: "second.zip" }] : [{ name: "first.zip" }];
      get("file").onchange();
    }
    const first = workers[0];
    assert.equal(first.messages[0].type, "inspect");
    assert.equal(first.messages[0].inputs.length, kind === "batch" ? 2 : 1);
    get("change").onclick();
    assert.equal(first.terminated, false);
    await inspect(first, first.messages[0].inputs.length);
    assert.equal(get("selection").hidden, false);
    assert.equal(get("drop").hidden, true);
    assert.equal(get("warnings").hidden, false);
    assert.equal(get("app-list").children.length, first.messages[0].inputs.length);
    assert.equal(get("app-list").hidden, kind !== "batch");
    get("acknowledge").checked = true;
    get("acknowledge").listeners.get("change")();
    assert.equal(get("convert").disabled, false);

    get("change").onclick();
    assert.equal(first.terminated, true);
    assert.equal(get("file").value, "");
    assert.equal(get("folder").value, "");
    assert.equal(get("selection").hidden, true);
    assert.equal(get("warnings").hidden, true);
    assert.equal(get("app-list").hidden, true);
    assert.equal(get("app-list").children.length, 0);
    assert.equal(get("drop").hidden, false);
    assert.equal(get("acknowledge").checked, false);
    assert.equal(get("convert").disabled, true);
    get("acknowledge").checked = true;
    get("acknowledge").listeners.get("change")();
    assert.equal(get("convert").disabled, true);
    get("change").onclick();

    get("file").files = [{ name: "replacement.zip" }];
    get("file").onchange();
    assert.equal(workers.length, 2);
    const replacement = workers[1];
    assert.deepEqual(replacement.messages[0].inputs.map(input => input.name), ["replacement.zip"]);
    assert.equal(get("convert").disabled, true);
    await inspect(replacement, 1);
    assert.equal(get("archive-name").textContent, "replacement.zip");
    assert.equal(get("app-list").children.length, 1);
    assert.equal(get("app-list").hidden, true);
    assert.equal(get("convert").disabled, true);
    get("acknowledge").checked = true;
    get("acknowledge").listeners.get("change")();
    assert.equal(get("convert").disabled, false);
    await get("convert").onclick();
    assert.equal(replacement.messages.at(-1).type, "convert");
    assert.equal(replacement.terminated, false);
  });
}

function deferred() {
  let resolve;
  let reject;
  const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
  return { promise, resolve, reject };
}

async function selected(get, workers, name) {
  get("file").files = [{ name }];
  get("file").onchange();
  const current = workers.at(-1);
  await inspect(current, 1);
  get("acknowledge").checked = true;
  get("acknowledge").listeners.get("change")();
  return current;
}

for (const outcome of ["error", "handle", "writable", "directory"]) {
  test(`a cancelled ${outcome} callback cannot change the replacement worker`, async t => {
    const pending = deferred();
    let removed = 0;
    let aborted = 0;
    let opened = 0;
    const handle = { async createWritable() { opened++; return outcome === "writable" ? pending.promise : { async abort() { aborted++; } }; } };
    const directory = {
      async getFileHandle() { return outcome === "writable" ? handle : pending.promise; },
      async getDirectoryHandle() { return pending.promise; }
    };
    const parent = {
      async getDirectoryHandle(name, options) {
        if (!options?.create) throw Object.assign(new Error("missing"), { name: "NotFoundError" });
        return directory;
      },
      async removeEntry() { removed++; }
    };
    const { get, workers } = await setup(t, `cancel-${outcome}`, { showDirectoryPicker: async () => parent });
    const old = await selected(get, workers, "old.zip");
    get("save").value = "disk";
    get("save").listeners.get("change")();
    await get("convert").onclick();
    const request = old.onmessage({ data: outcome === "directory" ? { type: "directory", name: "nested/" } : { type: "open-file", name: "old.bin" } });
    await setImmediate();
    await get("cancel").onclick();
    get("file").files = [{ name: "replacement.zip" }];
    get("file").onchange();
    const replacement = workers.at(-1);
    if (outcome === "error") pending.reject(new Error("old request failed"));
    else if (outcome === "writable") pending.resolve({ async abort() { aborted++; } });
    else pending.resolve(outcome === "handle" ? handle : directory);
    await request;
    assert.equal(removed, 1);
    assert.equal(replacement.terminated, false);
    assert.deepEqual(replacement.messages.map(message => message.type), ["inspect"]);
    assert.equal(opened, outcome === "writable" ? 1 : 0);
    assert.equal(aborted, outcome === "writable" ? 1 : 0);
    await old.onmessage({ data: { type: "progress", message: "stale progress" } });
    await old.onerror({ message: "stale worker failure" });
    assert.equal(replacement.terminated, false);
    assert.equal(get("log").textContent, "");
    await inspect(replacement, 1);
    assert.equal(get("archive-name").textContent, "replacement.zip");
  });
}

for (const type of ["write", "close-file", "complete"]) {
  for (const reject of [false, true]) {
    test(`late ${type} ${reject ? "failure" : "success"} cannot acknowledge or complete a new job`, async t => {
      const pending = deferred();
      const sink = { write: () => pending.promise, close: () => pending.promise, async abort() {} };
      const { get, workers } = await setup(t, `io-${type}-${reject}`, { showSaveFilePicker: async () => ({ createWritable: async () => sink }) });
      const old = await selected(get, workers, "old.zip");
      get("save").value = "zip";
      get("save").listeners.get("change")();
      await get("convert").onclick();
      const request = old.onmessage({ data: { type, bytes: new Uint8Array([1]), mode: "zip", outputName: "old.elf", size: 1 } });
      await get("cancel").onclick();
      get("file").files = [{ name: "replacement.zip" }];
      get("file").onchange();
      const replacement = workers.at(-1);
      if (reject) pending.reject(new Error("old I/O failed"));
      else pending.resolve();
      await request;
      assert.equal(replacement.terminated, false);
      assert.deepEqual(replacement.messages.map(message => message.type), ["inspect"]);
      assert.equal(get("done").hidden, true);
      assert.equal(get("convert").disabled, true);
    });
  }
}

for (const stage of ["picker", "writable", "root"]) {
  test(`a late ${stage} result cleans only the obsolete output`, async t => {
    const pending = deferred();
    let aborted = 0;
    let removed = 0;
    let opened = 0;
    const handle = { async createWritable() { opened++; return stage === "writable" ? pending.promise : { async abort() { aborted++; } }; } };
    const parent = {
      async getDirectoryHandle(name, options) {
        if (!options?.create) throw Object.assign(new Error("missing"), { name: "NotFoundError" });
        return pending.promise;
      },
      async removeEntry() { removed++; }
    };
    const { get, workers } = await setup(t, `late-${stage}`, {
      showSaveFilePicker: async () => stage === "picker" ? pending.promise : handle,
      showDirectoryPicker: async () => parent
    });
    const old = await selected(get, workers, "old.zip");
    get("save").value = stage === "root" ? "disk" : "zip";
    get("save").listeners.get("change")();
    const converting = get("convert").onclick();
    await setImmediate();
    await old.onerror({ message: "old worker stopped" });
    get("file").files = [{ name: "replacement.zip" }];
    get("file").onchange();
    const replacement = workers.at(-1);
    pending.resolve(stage === "picker" ? handle : stage === "writable" ? { async abort() { aborted++; } } : {});
    await converting;
    assert.equal(replacement.terminated, false);
    assert.deepEqual(replacement.messages.map(message => message.type), ["inspect"]);
    assert.equal(opened, stage === "writable" ? 1 : 0);
    assert.equal(aborted, stage === "writable" ? 1 : 0);
    assert.equal(removed, stage === "root" ? 1 : 0);
  });
}

test("a visible error follows a language switch", async t => {
  const { get } = await setup(t, "locale-error", { fetch: async () => ({ ok: false, status: 404 }) });
  assert.equal(get("error").hidden, false);
  assert.equal(get("error").children[0].textContent, "Cannot load release information (404). release.json is generated during the site build, so it is missing when serving demo/web directly.");
  get("locale").onclick();
  assert.equal(document.documentElement.lang, "de");
  assert.equal(get("error").children[0].textContent, "Release-Informationen können nicht geladen werden (404). release.json wird während des Site-Builds erzeugt und fehlt deshalb, wenn demo/web direkt bereitgestellt wird.");
  get("locale").onclick();
  assert.equal(document.documentElement.lang, "ar");
  assert.equal(document.documentElement.dir, "rtl");
  assert.match(get("error").children[0].textContent, /\p{Script=Arabic}.*404/u);
  get("locale").onclick();
  assert.equal(document.documentElement.lang, "en");
  assert.equal(document.documentElement.dir, "ltr");
  assert.equal(get("error").children[0].textContent, "Cannot load release information (404). release.json is generated during the site build, so it is missing when serving demo/web directly.");
});

test("a browser that denies direct ZIP writes explains how to continue", async t => {
  const denied = Object.assign(new Error("createWritable blocked"), { name: "NotAllowedError" });
  const { get, workers } = await setup(t, "write-denied", {
    showSaveFilePicker: async () => ({ createWritable: async () => { throw denied; } })
  });
  assert.equal(get("save").value, "zip");
  await selected(get, workers, "first.zip");
  assert.equal(get("convert").disabled, false);
  await get("convert").onclick();
  const message = get("error").children[0]?.textContent ?? get("error").textContent;
  assert.match(message, /desktop Chrome or Edge/);
  assert.doesNotMatch(message, /createWritable/);
  assert.equal(get("selection").hidden, true);
  assert.equal(get("convert").disabled, true);
});

test("cancelling the ZIP picker does not report a failure", async t => {
  const { get, workers } = await setup(t, "write-cancelled", {
    showSaveFilePicker: async () => { throw Object.assign(new Error("cancelled"), { name: "AbortError" }); }
  });
  await selected(get, workers, "first.zip");
  await get("convert").onclick();
  assert.equal(get("error").hidden, true);
  assert.equal(get("selection").hidden, false);
  assert.equal(get("convert").disabled, false);
});
