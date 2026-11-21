import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {setImmediate} from 'node:timers/promises';

class MockElement extends EventTarget {
    children = [];
    dataset = {};
    attributes = new Map();
    style = {setProperty() {}, removeProperty() {}};
    className = '';
    value = '';
    textContent = '';
    disabled = false;
    hidden = false;
    focused = false;
    classList = {toggle() {}};

    constructor(tag = 'div', hidden = false) {
        super();
        this.tagName = tag.toUpperCase();
        this.hidden = hidden;
    }

    setAttribute(name, value) { this.attributes.set(name, String(value)); }
    getAttribute(name) { return this.attributes.get(name); }
    removeAttribute(name) { this.attributes.delete(name); }
    append(...nodes) {
        for (const node of nodes) {
            if (typeof node === 'object' && node) {
                node.parentElement = this;
                this.children.push(node);
            }
        }
    }
    replaceChildren(...nodes) {
        this.children = [];
        this.append(...nodes);
    }
    focus() {
        this.focused = true;
        document.activeElement = this;
    }
    click() { this.dispatchEvent(new Event('click')); }
    closest(selector) {
        for (let node = this; node; node = node.parentElement) {
            if (matches(node, selector)) return node;
        }
        return null;
    }
    querySelectorAll(selector) {
        const parts = selector.trim().split(/\s+/u);
        let nodes = [this];
        for (const part of parts) {
            const next = [];
            const walk = node => {
                for (const child of node.children) {
                    if (matches(child, part)) next.push(child);
                    walk(child);
                }
            };
            for (const node of nodes) walk(node);
            nodes = next;
        }
        return nodes;
    }
    querySelector(selector) { return this.querySelectorAll(selector)[0] ?? null; }
}

function matches(node, selector) {
    if (selector.startsWith('.')) return node.className.split(/\s+/u).includes(selector.slice(1));
    if (selector === '[aria-invalid="true"]') return node.getAttribute('aria-invalid') === 'true';
    const attr = /^\[data-action="([^"]+)"\]$/u.exec(selector);
    if (attr) return node.dataset.action === attr[1];
    return node.tagName === selector.toUpperCase();
}

async function setup(t, name) {
    const html = readFileSync(new URL('../index.html', import.meta.url), 'utf8');
    const elements = new Map([...html.matchAll(/<(\w+)[^>]+\bid="([^"]+)"[^>]*>/gu)].map(match => [
        match[2], new MockElement(match[1], /\shidden(?:\s|>)/u.test(match[0]))
    ]));
    const get = id => {
        if (!elements.has(id)) elements.set(id, new MockElement('div'));
        return elements.get(id);
    };
    const root = new MockElement('body');
    for (const node of elements.values()) root.append(node);
    let dialogDecision = 'cancel';
    let dialogOpens = 0;
    const replaceDialog = get('replace-dialog');
    replaceDialog.showModal = () => {
        dialogOpens++;
        replaceDialog.returnValue = dialogDecision;
        queueMicrotask(() => replaceDialog.dispatchEvent(new Event('close')));
    };
    get('capture-dialog').close = () => {};
    get('picker-dialog').close = () => {};
    const globals = {
        document: {
            documentElement: {lang: '', dir: 'ltr'},
            activeElement: null,
            getElementById: get,
            createElement: tag => new MockElement(tag),
            querySelector: selector => root.querySelector(selector),
            querySelectorAll: selector => selector.startsWith('[data-i18n') || selector === '[data-view]' ? [] : root.querySelectorAll(selector)
        },
        window: Object.assign(new EventTarget(), {scrollTo() {}}),
        location: {hash: ''},
        history: {pushState() {}},
        localStorage: {getItem() { return null; }, setItem() {}},
        setInterval: () => 0
    };
    for (const [key, value] of Object.entries(globals)) {
        const original = Object.getOwnPropertyDescriptor(globalThis, key);
        Object.defineProperty(globalThis, key, {value, configurable: true, writable: true});
        t.after(() => {
            if (original) Object.defineProperty(globalThis, key, original);
            else delete globalThis[key];
        });
    }
    await import(new URL(`../app.js?${name}`, import.meta.url));
    return {
        get,
        setDialogDecision(value) { dialogDecision = value; },
        dialogOpens: () => dialogOpens,
        async importFile(filename, text) {
            const bytes = new TextEncoder().encode(text);
            const file = {name: filename, size: bytes.byteLength, arrayBuffer: async () => bytes.buffer};
            const input = get('file');
            input.files = [file];
            input.dispatchEvent(new Event('change'));
            await setImmediate();
        }
    };
}

test('cancelling replacement keeps the previous import report hidden and preserves unsaved edits', async t => {
    const app = await setup(t, 'cancel-hidden');
    app.get('mappings').querySelector('.add-binding').click();
    assert.equal(app.get('import-report').hidden, true);
    app.setDialogDecision('cancel');
    await app.importFile('other.ini', 'Cross = KEY:Space\nCross = KEY:Space\n');
    assert.equal(app.dialogOpens(), 1);
    assert.equal(app.get('import-report').hidden, true);
    assert.equal(app.get('import-report').children.length, 0);
    assert.equal(app.get('status').textContent, '');
    assert.equal(app.get('preview').value, '');
});

test('cancelling replacement preserves an earlier import report until replacement is confirmed', async t => {
    const app = await setup(t, 'cancel-existing');
    app.get('mappings').querySelector('.add-binding').click();
    await app.importFile('broken.ini', 'NotAnAction = KEY:Space\n');
    assert.equal(app.dialogOpens(), 0);
    assert.equal(app.get('import-report').hidden, false);
    assert.equal(app.get('import-report').children[0].textContent, 'Import rejected');
    assert.equal(app.get('import-report').focused, true);

    app.setDialogDecision('cancel');
    await app.importFile('valid.ini', 'Cross = KEY:Space\n');
    assert.equal(app.dialogOpens(), 1);
    assert.equal(app.get('import-report').children[0].textContent, 'Import rejected');
    assert.equal(app.get('preview').value, '');

    app.setDialogDecision('discard');
    await app.importFile('valid.ini', 'Cross = KEY:Space\n');
    assert.equal(app.dialogOpens(), 2);
    assert.equal(app.get('import-report').children[0].textContent, 'Import read successfully');
    assert.equal(app.get('preview').value, 'Cross = KEY:Space\n');
});

test('replacing a built-in search result keeps the new binding reachable until it has a value', async t => {
    const app = await setup(t, 'search-add');
    const search = app.get('search');
    search.value = 'F12';
    search.dispatchEvent(new Event('input'));
    const mappings = app.get('mappings');
    mappings.querySelector('[data-action="Screenshot"] .add-binding').click();
    const action = mappings.querySelector('[data-action="Screenshot"]');
    const value = action.querySelectorAll('select')[1];
    assert.equal(action.hidden, false);
    assert.equal(action.parentElement.hidden, false);
    assert.equal(document.activeElement, action.querySelectorAll('.dropdown-trigger')[1]);
    assert.equal(search.value, 'F12');
    assert.equal(app.get('download').disabled, true);

    value.value = 'K';
    value.dispatchEvent(new Event('change'));
    assert.equal(action.hidden, true);
    assert.equal(document.activeElement, search);
    assert.equal(app.get('download').disabled, false);
    assert.equal(app.get('preview').value, 'Screenshot = KEY:K\n');
});

test('a chord search result stays visible while changing its buttons in the open picker', async t => {
    const app = await setup(t, 'search-edit-chord');
    await app.importFile('input.ini', 'Screenshot = PAD:L1+R1\n');
    const search = app.get('search');
    search.value = 'PAD:L1+R1';
    search.dispatchEvent(new Event('input'));
    const action = app.get('mappings').querySelector('[data-action="Screenshot"]');
    const picker = action.querySelector('.chord-picker');
    const r1 = picker.querySelectorAll('input').find(input => input.value === 'R1');
    picker.open = true;
    r1.focus();
    r1.checked = false;
    picker.querySelector('.chord-buttons').dispatchEvent(new Event('change'));
    assert.equal(action.hidden, false);
    assert.equal(app.get('preview').value, 'Screenshot = PAD:L1\n');

    picker.querySelector('summary').focus();
    picker.open = false;
    picker.dispatchEvent(new Event('toggle'));
    assert.equal(action.hidden, true);
    assert.equal(document.activeElement, search);
});
