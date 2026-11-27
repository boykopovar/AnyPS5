import test from 'node:test';
import assert from 'node:assert/strict';

const eventTarget = () => ({addEventListener() {}});
const element = () => Object.assign(new EventTarget(), {
    children: [],
    attributes: new Map(),
    style: {setProperty() {}, removeProperty() {}},
    value: '',
    focused: false,
    isConnected: true,
    append(...children) { this.children.push(...children); },
    replaceChildren() { this.children = []; },
    setAttribute(name, value) { this.attributes.set(name, value); },
    focus() { this.focused = true; },
    scrollIntoView() {}
});
const dialog = Object.assign(element(), {
    open: false,
    showModal() { this.open = true; },
    close() { this.open = false; }
});
const nodes = new Map([['picker-dialog', dialog]]);
globalThis.document = {
    getElementById(id) {
        if (!nodes.has(id)) nodes.set(id, element());
        return nodes.get(id);
    },
    createElement: element
};
globalThis.window = eventTarget();
globalThis.matchMedia = () => ({matches: true});

const {choose, createDropdown, normalizeSearch} = await import('../dropdown.js');

test('dropdown search keeps technical key names locale independent', () => {
    assert.equal('I'.toLocaleLowerCase('tr'), '\u0131');
    assert.equal(normalizeSearch('I'), 'i');
    assert.equal(normalizeSearch('i'), 'i');
    assert.ok(normalizeSearch('KEY:I').includes(normalizeSearch('i')));
});

test('language names match Latin searches without accents', () => {
    for (const [label, query] of [['Español', 'espanol'], ['Français', 'francais'],
                                 ['Türkçe', 'TURKCE'], ['Português', 'portugues'],
                                 ['Tiếng Việt', 'tieng viet'], ['İstanbul', 'istanbul']]) {
        assert.ok(normalizeSearch(label).includes(normalizeSearch(query)), `${query}: ${label}`);
    }
    assert.equal(normalizeSearch('FRANÇAIS'), normalizeSearch('franc\u0327ais'));
    assert.equal(normalizeSearch('français'), normalizeSearch('francais'));
});

test('search folding preserves non-Latin letters and technical punctuation', () => {
    for (const value of ['日本語', 'ば', 'は', 'فارسی', 'العَرَبِيَّة', 'KEY:Keypad +', 'KEY:[']) {
        assert.equal(normalizeSearch(value), value.toLowerCase());
    }
    assert.notEqual(normalizeSearch('ば'), normalizeSearch('は'));
});

test('choosing the selected type preserves the binding until the type changes', () => {
    const select = new EventTarget();
    select.value = 'KEY';
    const control = {select, setValue(value) { select.value = value; }};
    let binding = 'J';
    let changes = 0;
    select.addEventListener('change', () => {
        changes++;
        binding = '';
    });

    choose(control, 'KEY');
    assert.equal(binding, 'J');
    assert.equal(changes, 0);
    choose(control, 'MOUSE');
    assert.equal(select.value, 'MOUSE');
    assert.equal(binding, '');
    assert.equal(changes, 1);
});

test('the picker lets an IME handle keys during composition', () => {
    const japanese = '日本語';
    const control = createDropdown({label: 'Language', choices: [japanese, 'English'], value: 'English', text: {}});
    control.trigger.dispatchEvent(new Event('click'));
    const search = document.getElementById('picker-search');
    search.value = japanese;
    search.dispatchEvent(new Event('input'));

    const keydown = (key, isComposing) => {
        const event = Object.assign(new Event('keydown', {cancelable: true}), {key, isComposing});
        Object.defineProperty(event, 'target', {value: search});
        dialog.dispatchEvent(event);
        return event;
    };

    assert.equal(keydown('ArrowDown', true).defaultPrevented, false);
    assert.equal(keydown('Enter', true).defaultPrevented, false);
    assert.equal(dialog.open, true);
    assert.equal(control.select.value, 'English');

    assert.equal(keydown('Enter', false).defaultPrevented, true);
    assert.equal(dialog.open, false);
    assert.equal(control.select.value, japanese);
    dialog.dispatchEvent(new Event('close'));
});

test('a delayed close event leaves the reopened picker active', () => {
    const makeControl = label => createDropdown({label, choices: ['X', 'J'], value: 'X', text: {}});
    const first = makeControl('Cross');
    const second = makeControl('Circle');
    first.trigger.dispatchEvent(new Event('click'));
    choose(first, 'J');
    second.trigger.dispatchEvent(new Event('click'));
    dialog.dispatchEvent(new Event('close'));

    assert.equal(first.trigger.attributes.get('aria-expanded'), 'false');
    assert.equal(second.trigger.attributes.get('aria-expanded'), 'true');
    assert.equal(second.trigger.focused, false);
    const pickerOptions = document.getElementById('picker-options');
    pickerOptions.children[1].dispatchEvent(new Event('click'));
    dialog.dispatchEvent(new Event('close'));

    assert.equal(first.select.value, 'J');
    assert.equal(second.select.value, 'J');
    assert.equal(second.trigger.attributes.get('aria-expanded'), 'false');
    assert.equal(second.trigger.focused, true);
});
