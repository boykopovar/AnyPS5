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

async function setup(t, name) {
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
      getElementById: get,
      createElement: () => new Element(),
      querySelector: selector => {
        assert.equal(selector, 'input[name="target"]:checked');
        return { value: "linux" };
      }
    },
    window: { isSecureContext: true, Worker, WebAssembly, ReadableStream, WritableStream, crypto: { subtle: {} } },
    Worker,
    fetch: async () => ({
      ok: true,
      json: async () => ({ tag: "test-release", platforms: { linux: { expandedBytes: 1024 }, windows: { expandedBytes: 1024 } } })
    })
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
