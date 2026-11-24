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
        navigator: {clipboard: {}},
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
        async importFile(filename, text, pendingRead) {
            const bytes = new TextEncoder().encode(text);
            const file = {
                name: filename,
                size: bytes.byteLength,
                async arrayBuffer() {
                    await pendingRead;
                    return bytes.buffer;
                }
            };
            const input = get('file');
            input.files = [file];
            input.dispatchEvent(new Event('change'));
            await setImmediate();
        }
    };
}

for (const outcome of ['valid', 'invalid', 'failed']) {
    test(`a late ${outcome} read cannot affect a newer import`, async t => {
        const app = await setup(t, `stale-import-${outcome}`);
        const read = Promise.withResolvers();
        const first = outcome === 'invalid' ? 'NotAnAction = KEY:F\n' : 'Cross = KEY:F\n';
        await app.importFile('first.ini', first, read.promise);
        await app.importFile('second.ini', 'Cross = KEY:J\n');
        assert.equal(app.get('preview').value, 'Cross = KEY:J\n');
        const status = app.get('status').textContent;

        if (outcome === 'failed') read.reject(new Error('File is unavailable'));
        else read.resolve();
        await setImmediate();

        assert.equal(app.get('preview').value, 'Cross = KEY:J\n');
        assert.equal(app.get('status').textContent, status);
        assert.equal(app.get('import-report').children[0].textContent, 'Import read successfully');
        assert.equal(app.dialogOpens(), 0);
    });
}

test('a delayed close cannot answer the confirmation for a newer import', async t => {
    const app = await setup(t, 'stale-confirmation');
    app.get('mappings').querySelector('.add-binding').click();
    const confirmation = app.get('replace-dialog');
    confirmation.showModal = () => { confirmation.open = true; };

    await app.importFile('first.ini', 'Cross = KEY:F\n');
    assert.equal(confirmation.open, true);
    const laterRead = Promise.withResolvers();
    await app.importFile('second.ini', 'Square = KEY:J\n', laterRead.promise);
    confirmation.open = false;
    confirmation.returnValue = 'cancel';
    laterRead.resolve();
    await setImmediate();
    assert.equal(confirmation.open, true);
    confirmation.dispatchEvent(new Event('close'));
    await setImmediate();
    assert.equal(app.get('preview').value, '');

    confirmation.open = false;
    confirmation.returnValue = 'discard';
    confirmation.dispatchEvent(new Event('close'));
    await setImmediate();
    assert.equal(app.get('preview').value, 'Square = KEY:J\n');
});

test('a late import cannot restore a configuration after reset', async t => {
    const app = await setup(t, 'stale-import-after-reset');
    await app.importFile('first.ini', 'Cross = KEY:F\n');
    const read = Promise.withResolvers();
    await app.importFile('pending.ini', 'Cross = KEY:J\n', read.promise);
    app.get('reset').click();
    await setImmediate();
    assert.equal(app.get('preview').value, '');

    read.resolve();
    await setImmediate();

    assert.equal(app.get('preview').value, '');
    assert.equal(app.get('import-report').hidden, true);
});

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

test('the import report is named by its translated heading', async t => {
    const app = await setup(t, 'report-name');
    const html = readFileSync(new URL('../index.html', import.meta.url), 'utf8');
    const section = /<section\b[^>]*\bid="import-report"[^>]*>/u.exec(html)[0];
    assert.doesNotMatch(section, /\saria-label=/u);
    const labelledBy = /\saria-labelledby="([^"]+)"/u.exec(section)?.[1];
    assert.ok(labelledBy);
    for (const [text, heading] of [['NotAnAction = KEY:Space\n', 'Import rejected'], ['Cross = KEY:Space\n', 'Import read successfully']]) {
        await app.importFile('report.ini', text);
        const title = app.get('import-report').children[0];
        assert.equal(title.tagName, 'H2');
        assert.equal(title.textContent, heading);
        assert.equal(title.id, labelledBy);
    }
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

test('successful imports use the destination from a recognized profile filename', async t => {
    const app = await setup(t, 'import-profile');
    const titleId = app.get('profile-title-id');
    const preview = app.get('preview-name');
    const binding = 'Cross = KEY:X\n';
    await app.importFile('anyps5-input.PPSA00001.ini', binding);
    assert.equal(titleId.value, 'PPSA00001');
    assert.equal(preview.textContent, 'anyps5-input.PPSA00001.ini');

    await app.importFile('anyps5-input.PPSA00002.ini', 'Cross = invalid\n');
    assert.equal(titleId.value, 'PPSA00001');
    await app.importFile('anyps5-input.PPSA00003.backup.ini', binding);
    assert.equal(titleId.value, 'PPSA00001');

    app.get('mappings').querySelector('.add-binding').click();
    await app.importFile('anyps5-input.PPSA00004.ini', binding);
    assert.equal(app.dialogOpens(), 1);
    assert.equal(titleId.value, 'PPSA00001');

    app.setDialogDecision('discard');
    await app.importFile('anyps5-input.ini', binding);
    assert.equal(titleId.value, '');
    assert.equal(preview.textContent, 'anyps5-input.ini');
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

for (const result of ['success', 'failure']) {
    test(`a late clipboard ${result} leaves a newer import saved`, async t => {
        const app = await setup(t, `late-copy-${result}`);
        const clipboard = Promise.withResolvers();
        const writes = [];
        navigator.clipboard.writeText = text => {
            writes.push(text);
            return clipboard.promise;
        };
        const preview = app.get('preview');
        preview.select = () => {};
        await app.importFile('first.ini', 'Cross = KEY:F\n');
        app.get('copy').click();
        assert.deepEqual(writes, ['Cross = KEY:F\n']);
        await app.importFile('second.ini', 'Cross = KEY:J\n');
        const status = app.get('status').textContent;

        if (result === 'success') clipboard.resolve();
        else clipboard.reject(new Error('Clipboard unavailable'));
        await setImmediate();

        assert.equal(preview.value, 'Cross = KEY:J\n');
        assert.equal(app.get('status').textContent, status);
        assert.equal(preview.focused, false);
        assert.equal(window.dispatchEvent(new Event('beforeunload', {cancelable: true})), true);
    });
}

test('a late clipboard failure leaves focus on a newer import report', async t => {
    const app = await setup(t, 'late-copy-report');
    const clipboard = Promise.withResolvers();
    navigator.clipboard.writeText = () => clipboard.promise;
    const preview = app.get('preview');
    preview.select = () => {};
    await app.importFile('first.ini', 'Cross = KEY:F\n');
    app.get('copy').click();
    await app.importFile('broken.ini', 'NotAnAction = KEY:Space\n');
    const status = app.get('status').textContent;

    clipboard.reject(new Error('Clipboard unavailable'));
    await setImmediate();

    assert.equal(document.activeElement, app.get('import-report'));
    assert.equal(preview.focused, false);
    assert.equal(app.get('status').textContent, status);
});

for (const [failure, detail, size, read] of [
    ['size', 'INI file must be smaller than 1 MiB.', 1024 * 1024 + 1, () => assert.fail('Unexpected file read')],
    ['read', 'The file could not be read.', 1, () => { throw new Error('The file could not be read.'); }],
    ['encoding', 'The file must be valid UTF-8 text.', 1, () => Uint8Array.of(0xff).buffer]
]) {
    for (const result of ['success', 'failure']) {
        test(`a late clipboard ${result} preserves an import ${failure} error`, async t => {
            const app = await setup(t, `late-copy-import-${failure}-${result}`);
            const clipboard = Promise.withResolvers();
            navigator.clipboard.writeText = () => clipboard.promise;
            const preview = app.get('preview');
            preview.select = () => {};
            await app.importFile('first.ini', 'Cross = KEY:F\n');
            app.get('copy').click();
            const input = app.get('file');
            input.files = [{name: 'second.ini', size, arrayBuffer: read}];
            input.dispatchEvent(new Event('change'));
            await setImmediate();
            assert.equal(app.get('status').textContent, `Import failed: ${detail}`);

            if (result === 'success') clipboard.resolve();
            else clipboard.reject(new Error('Clipboard unavailable'));
            await setImmediate();

            assert.equal(app.get('status').textContent, `Import failed: ${detail}`);
            assert.equal(preview.focused, false);
            assert.equal(preview.value, 'Cross = KEY:F\n');
        });
    }
}

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

test('controller overrides can be added, edited, disabled and inherited alongside imported settings', async t => {
    const app = await setup(t, 'controller-remaps');
    const control = name => {
        const row = app.get('controller-remaps').children.find(item => item.querySelector('span').textContent === `SDLButton:${name}`);
        assert.ok(row, name);
        return row.querySelector('select');
    };
    const assign = (name, destination) => {
        const select = control(name);
        select.value = destination;
        select.dispatchEvent(new Event('change'));
    };
    const settings = 'Square = KEY:J\nMouseSensitivity = 2\n';
    await app.importFile('game.ini', settings + 'SDLButton:b = Cross\n');
    assert.equal(control('b').value, 'Cross');
    assert.equal(control('a').value, '');
    assign('a', 'Circle');
    assign('b', 'None');
    assert.equal(app.get('preview').value, settings + 'SDLButton:b = None\nSDLButton:a = Circle\n');
    assign('b', '');
    assert.equal(app.get('preview').value, settings + 'SDLButton:a = Circle\n');
    await app.importFile('empty.ini', '');
    assert.equal(app.dialogOpens(), 1);
    assert.equal(control('a').value, 'Circle');
    app.setDialogDecision('discard');
    app.get('reset').click();
    await setImmediate();
    assert.equal(control('a').value, '');
    assert.equal(app.get('preview').value, '');
});
