import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { test } from "node:test";
import { apply, current, locales, messages, set, translate } from "../i18n.mjs";

function placeholders(text) {
  return [...text.matchAll(/\{(\w+)\}/gu)].map(match => match[1]).sort();
}

class FakeElement {
  constructor(dataset = {}) {
    this.dataset = dataset;
    this.textContent = "";
    this.attributes = {};
  }
  setAttribute(name, value) { this.attributes[name] = value; }
}

function stubGlobal(name, value, t) {
  const original = Object.getOwnPropertyDescriptor(globalThis, name);
  Object.defineProperty(globalThis, name, { value, configurable: true, writable: true });
  t.after(() => {
    if (original) Object.defineProperty(globalThis, name, original);
    else delete globalThis[name];
  });
}

function stubDocument(elements) {
  return {
    documentElement: { lang: "", dir: "" },
    querySelectorAll(selector) {
      if (selector === "[data-i18n]") return elements.filter(el => el.dataset.i18n !== undefined);
      if (selector === "[data-i18n-attr]") return elements.filter(el => el.dataset.i18nAttr !== undefined);
      return [];
    }
  };
}

test("every shipped locale has exactly the English keys", () => {
  const englishKeys = Object.keys(messages.en).sort();
  assert.ok(englishKeys.length > 0);
  for (const { code } of locales) {
    if (code === "en") continue;
    assert.deepEqual(Object.keys(messages[code]).sort(), englishKeys, `${code} has missing or extra keys compared to English`);
  }
});

test("placeholders match the English source for every key and locale", () => {
  for (const { code } of locales) {
    if (code === "en") continue;
    for (const key of Object.keys(messages.en)) {
      assert.deepEqual(placeholders(messages[code][key]), placeholders(messages.en[key]), `${code}.${key} placeholders differ from English`);
    }
  }
});

test("index.html has no visible text literal without data-i18n or data-i18n-attr", () => {
  const html = readFileSync(new URL("../index.html", import.meta.url), "utf8");
  const body = html.slice(html.indexOf("<body>"));
  const whitelist = new Set(["AnyPS5", "GitHub", "Discord", "sce_sys/", "sce_module/", "sce_modules/", "prx/", "Windows", "Linux", ".exe", ".elf", "ZIP", "↗", "◐"]);
  const withLetters = /[A-Za-zÀ-ÖØ-öø-ÿ]/u;
  const tokens = [...body.matchAll(/<[^>]+>|[^<]+/gu)].map(match => match[0]);
  let lastOpenTag = "";
  for (const token of tokens) {
    if (token.startsWith("<")) {
      if (!token.startsWith("</")) lastOpenTag = token;
      continue;
    }
    const trimmed = token.replace(/&amp;/gu, "&").replace(/&nbsp;/gu, " ").trim();
    if (!trimmed || !withLetters.test(trimmed) || whitelist.has(trimmed)) continue;
    assert.ok(/data-i18n(?:-attr)?=/u.test(lastOpenTag), `Untranslated text "${trimmed}" after ${lastOpenTag}`);
  }
});

test("translate returns German, falls back to English for unknown keys and unknown locales", () => {
  assert.equal(translate("de", "cancelButton"), messages.de.cancelButton);
  assert.notEqual(messages.de.cancelButton, messages.en.cancelButton);
  assert.equal(translate("de", "__does_not_exist__"), "__does_not_exist__");
  assert.equal(translate("fr", "cancelButton"), messages.en.cancelButton);
});

test("translate substitutes placeholders", () => {
  assert.equal(translate("en", "unsafeZipPath", { name: "evil.txt" }), "Unsafe ZIP path: evil.txt");
  assert.equal(translate("de", "unsafeZipPath", { name: "evil.txt" }), messages.de.unsafeZipPath.replace("{name}", "evil.txt"));
});

test("apply sets lang, dir, text content and attributes for a locale", t => {
  const textNode = new FakeElement({ i18n: "cancelButton" });
  const attrNode = new FakeElement({ i18nAttr: "aria-label:themeToggle" });
  const fakeDocument = stubDocument([textNode, attrNode]);
  stubGlobal("document", fakeDocument, t);

  apply("de");

  assert.equal(fakeDocument.documentElement.lang, "de");
  assert.equal(fakeDocument.documentElement.dir, "ltr");
  assert.equal(textNode.textContent, messages.de.cancelButton);
  assert.equal(attrNode.attributes["aria-label"], messages.de.themeToggle);
});

test("apply sets dir=rtl for an rtl locale without shipping one", t => {
  const fakeDocument = stubDocument([]);
  stubGlobal("document", fakeDocument, t);
  locales.push({ code: "test-rtl", name: "Test RTL", direction: "rtl" });
  t.after(() => { locales.pop(); });

  apply("test-rtl");

  assert.equal(fakeDocument.documentElement.lang, "test-rtl");
  assert.equal(fakeDocument.documentElement.dir, "rtl");
});

test("a saved locale choice is applied at start", t => {
  const store = new Map([["anyps5.converter.locale", "de"]]);
  stubGlobal("localStorage", { getItem: key => store.get(key) ?? null, setItem: (key, value) => { store.set(key, value); } }, t);
  const fakeDocument = stubDocument([]);
  stubGlobal("document", fakeDocument, t);

  assert.equal(current(), "de");
  apply(current());
  assert.equal(fakeDocument.documentElement.lang, "de");
});

test("without a saved choice the default locale is English", t => {
  stubGlobal("localStorage", { getItem: () => null, setItem: () => {} }, t);
  assert.equal(current(), "en");
});

test("set() persists the locale to localStorage and applies it", t => {
  const store = new Map();
  stubGlobal("localStorage", { getItem: key => store.get(key) ?? null, setItem: (key, value) => { store.set(key, value); } }, t);
  const fakeDocument = stubDocument([]);
  stubGlobal("document", fakeDocument, t);

  set("de");

  assert.equal(store.get("anyps5.converter.locale"), "de");
  assert.equal(current(), "de");
  assert.equal(fakeDocument.documentElement.lang, "de");
});
