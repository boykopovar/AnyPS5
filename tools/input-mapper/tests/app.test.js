import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {setImmediate} from 'node:timers/promises';
import {settings as settingDefinitions} from '../validation.js';

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

test('editing only the profile destination requires confirmation before reset', async t => {
    const app = await setup(t, 'reset-dirty-profile-destination');
    const binding = 'Cross = KEY:F\n';
    await app.importFile('anyps5-input.PPSA00001.ini', binding);
    const titleId = app.get('profile-title-id');
    titleId.value = 'PPSA00002';
    titleId.dispatchEvent(new Event('input'));
    assert.equal(app.get('preview-name').textContent, 'anyps5-input.PPSA00002.ini');

    app.get('reset').click();
    await setImmediate();
    assert.equal(app.dialogOpens(), 1);
    assert.equal(titleId.value, 'PPSA00002');
    assert.equal(app.get('preview').value, binding);

    app.setDialogDecision('discard');
    app.get('reset').click();
    await setImmediate();
    assert.equal(app.dialogOpens(), 2);
    assert.equal(titleId.value, '');
    assert.equal(app.get('preview').value, '');
    assert.equal(window.dispatchEvent(new Event('beforeunload', {cancelable: true})), true);
});

test('copying the mapping leaves an edited profile destination unsaved', async t => {
    const app = await setup(t, 'copy-dirty-profile-destination');
    const binding = 'Cross = KEY:F\n';
    await app.importFile('anyps5-input.ini', binding);
    const titleId = app.get('profile-title-id');
    titleId.value = 'PPSA00002';
    titleId.dispatchEvent(new Event('input'));
    let copied;
    navigator.clipboard.writeText = async text => { copied = text; };
    app.get('copy').click();
    await setImmediate();
    assert.equal(copied, binding);
    assert.equal(window.dispatchEvent(new Event('beforeunload', {cancelable: true})), false);

    const replacement = 'Square = KEY:J\n';
    await app.importFile('anyps5-input.PPSA00003.ini', replacement);
    assert.equal(app.dialogOpens(), 1);
    assert.equal(titleId.value, 'PPSA00002');
    assert.equal(app.get('preview').value, binding);

    app.setDialogDecision('discard');
    await app.importFile('anyps5-input.PPSA00003.ini', replacement);
    assert.equal(app.dialogOpens(), 2);
    assert.equal(titleId.value, 'PPSA00003');
    assert.equal(app.get('preview').value, replacement);
    assert.equal(window.dispatchEvent(new Event('beforeunload', {cancelable: true})), true);
});

test('importing the destination already entered applies without a discard prompt', async t => {
    const app = await setup(t, 'import-matches-edited-profile-destination');
    await app.importFile('anyps5-input.PPSA00001.ini', 'Cross = KEY:F\n');
    const titleId = app.get('profile-title-id');
    titleId.value = 'PPSA00002';
    titleId.dispatchEvent(new Event('input'));
    assert.equal(window.dispatchEvent(new Event('beforeunload', {cancelable: true})), false);

    const replacement = 'Square = KEY:J\n';
    await app.importFile('anyps5-input.PPSA00002.ini', replacement);
    assert.equal(app.dialogOpens(), 0);
    assert.equal(titleId.value, 'PPSA00002');
    assert.equal(app.get('preview').value, replacement);
    assert.equal(window.dispatchEvent(new Event('beforeunload', {cancelable: true})), true);
});

test('importing a custom filename preserves the dirty profile destination', async t => {
    const app = await setup(t, 'import-keeps-dirty-profile-destination');
    await app.importFile('anyps5-input.PPSA00001.ini', 'Cross = KEY:F\n');
    const titleId = app.get('profile-title-id');
    titleId.value = 'PPSA00002';
    titleId.dispatchEvent(new Event('input'));

    const replacement = 'Square = KEY:J\n';
    await app.importFile('custom.ini', replacement);
    assert.equal(app.dialogOpens(), 0);
    assert.equal(titleId.value, 'PPSA00002');
    assert.equal(app.get('preview').value, replacement);
    assert.equal(window.dispatchEvent(new Event('beforeunload', {cancelable: true})), false);

    titleId.value = 'PPSA00001';
    titleId.dispatchEvent(new Event('input'));
    assert.equal(window.dispatchEvent(new Event('beforeunload', {cancelable: true})), true);
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

function inputSetting(app, name) {
    const row = app.get('input-setting-values').children.find(item => item.querySelector('span').textContent === name);
    assert.ok(row, name);
    return row.querySelector('input') ?? row.querySelector('select');
}

function editInputSetting(app, name, value) {
    const field = inputSetting(app, name);
    field.value = value;
    field.dispatchEvent(new Event(field.tagName === 'SELECT' ? 'change' : 'input'));
}

test('settings can be added, validated, and removed without touching bindings', async t => {
    const app = await setup(t, 'edit-input-settings');
    assert.deepEqual(app.get('input-setting-values').children.map(row => row.querySelector('span').textContent),
        settingDefinitions.map(setting => setting.name));
    const original = 'Cross = KEY:J\nSDLButton:b = Cross\n';
    await app.importFile('anyps5-input.ini', original);
    editInputSetting(app, 'LeftStickDeadzone', '12');
    assert.equal(app.get('preview').value, 'Cross = KEY:J\nLeftStickDeadzone = 12\nSDLButton:b = Cross\n');
    editInputSetting(app, 'LeftStickDeadzone', '91');
    assert.equal(app.get('preview').value, '');
    assert.equal(app.get('copy').disabled, true);
    assert.equal(app.get('download').disabled, true);
    assert.equal(inputSetting(app, 'LeftStickDeadzone').getAttribute('aria-invalid'), 'true');
    assert.match(app.get('validation').querySelector('li').textContent, /whole percentage from 0 to 90/u);
    editInputSetting(app, 'LeftStickDeadzone', '');
    assert.equal(inputSetting(app, 'LeftStickDeadzone').getAttribute('aria-invalid'), 'false');
    assert.equal(app.get('copy').disabled, false);
    assert.equal(app.get('download').disabled, false);
    assert.equal(app.get('preview').value, original);
});

test('settings preserve imported spellings and keep edits across locale changes', async t => {
    const app = await setup(t, 'localized-input-settings');
    const original = 'LeftStickDeadzone = -00\nMouseSensitivity = 1e0\nMouseStickTarget = RIGHT\nToggleActions = r2 ,cross, R2\n';
    await app.importFile('anyps5-input.PPSA01234.ini', original);
    assert.equal(app.get('profile-title-id').value, 'PPSA01234');
    assert.equal(inputSetting(app, 'MouseSensitivity').value, '1e0');
    assert.equal(inputSetting(app, 'MouseStickTarget').value, 'Right');
    assert.equal(app.get('preview').value, original);
    editInputSetting(app, 'MouseStickTarget', 'Left');
    editInputSetting(app, 'ToggleActions', 'R2, L1');
    const select = app.get('language-control').querySelector('select');
    select.value = 'ar-EG';
    select.dispatchEvent(new Event('change'));
    assert.equal(document.documentElement.dir, 'rtl');
    assert.equal(inputSetting(app, 'MouseSensitivity').dir, 'ltr');
    assert.equal(inputSetting(app, 'MouseStickTarget').value, 'Left');
    assert.equal(inputSetting(app, 'ToggleActions').value, 'R2, L1');
    const expected = 'LeftStickDeadzone = -00\nMouseSensitivity = 1e0\nMouseStickTarget = Left\nToggleActions = R2, L1\n';
    assert.equal(app.get('preview').value, expected);
    let copied = '';
    navigator.clipboard.writeText = async text => { copied = text; };
    app.get('copy').click();
    await setImmediate();
    assert.equal(copied, expected);
});

test('setting hints give the accepted range and built-in value in the interface language', async t => {
    const app = await setup(t, 'input-setting-hints');
    const hint = name => {
        const row = app.get('input-setting-values').children.find(item => item.querySelector('span').textContent === name);
        return row.children.find(child => child.className === 'hint');
    };
    assert.equal(hint('MouseCurve').textContent, '\u20660.2–5\u2069 · Built-in: 1');
    assert.equal(inputSetting(app, 'MouseCurve').getAttribute('aria-describedby'), `${hint('MouseCurve').id} input-settings-help`);
    const curveName = app.get('input-setting-values').children.find(item => item.querySelector('span').textContent === 'MouseCurve').querySelector('span');
    assert.equal(inputSetting(app, 'MouseCurve').getAttribute('aria-labelledby'), curveName.id);
    assert.equal(hint('MouseSensitivity').textContent, '\u20660 < x ≤ 10\u2069 · Built-in: 1');
    for (const [value, invalid] of [['0', 'true'], ['0.001', 'false'], ['10', 'false'], ['10.001', 'true']]) {
        editInputSetting(app, 'MouseSensitivity', value);
        assert.equal(inputSetting(app, 'MouseSensitivity').getAttribute('aria-invalid'), invalid, value);
    }
    editInputSetting(app, 'MouseSensitivity', '');
    assert.equal(hint('MouseStickTarget').textContent, 'Built-in: Right');
    assert.equal(hint('ToggleActions'), undefined);
    assert.equal(inputSetting(app, 'ToggleActions').getAttribute('aria-describedby'), 'input-settings-help');
    const select = app.get('language-control').querySelector('select');
    select.value = 'fr';
    select.dispatchEvent(new Event('change'));
    assert.equal(hint('LeftStickDeadzone').textContent, '\u20660–90\u2069 · Par défaut : 0');
});

test('reset ignores restored settings order but prompts for an invalid draft', async t => {
    const app = await setup(t, 'restored-input-settings');
    const original = 'LeftStickDeadzone = 12\nMouseSensitivity = 2\n';
    await app.importFile('anyps5-input.ini', original);

    editInputSetting(app, 'LeftStickDeadzone', '91');
    app.get('reset').click();
    await setImmediate();
    assert.equal(app.dialogOpens(), 1);
    assert.equal(inputSetting(app, 'LeftStickDeadzone').value, '91');

    editInputSetting(app, 'LeftStickDeadzone', '');
    editInputSetting(app, 'LeftStickDeadzone', '12');
    assert.equal(app.get('preview').value, original);
    app.get('reset').click();
    await setImmediate();
    assert.equal(app.dialogOpens(), 1);
    assert.equal(app.get('preview').value, '');
});

test('invalid imports and canceled resets preserve edited settings', async t => {
    const app = await setup(t, 'input-settings-reset');
    editInputSetting(app, 'MouseDecayMs', '50');
    await app.importFile('anyps5-input.ini', 'MouseDecayMs = 0\n');
    assert.equal(app.get('import-report').hidden, false);
    assert.equal(inputSetting(app, 'MouseDecayMs').value, '50');
    assert.equal(app.get('preview').value, 'MouseDecayMs = 50\n');
    app.get('reset').click();
    await setImmediate();
    assert.equal(app.dialogOpens(), 1);
    assert.equal(inputSetting(app, 'MouseDecayMs').value, '50');
    app.setDialogDecision('discard');
    app.get('reset').click();
    await setImmediate();
    assert.equal(inputSetting(app, 'MouseDecayMs').value, '');
    assert.equal(app.get('preview').value, '');
});
