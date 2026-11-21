import test from 'node:test';
import assert from 'node:assert/strict';

const eventTarget = () => ({addEventListener() {}});
const dialog = {...eventTarget(), close() {}};
globalThis.document = {getElementById: id => id === 'picker-dialog' ? dialog : eventTarget()};
globalThis.window = eventTarget();

const {choose, normalizeSearch} = await import('../dropdown.js');

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
