import test from 'node:test';
import assert from 'node:assert/strict';

const eventTarget = () => ({addEventListener() {}});
globalThis.document = {getElementById: eventTarget};
globalThis.window = eventTarget();

const {normalizeSearch} = await import('../dropdown.js');

test('dropdown search keeps technical key names locale independent', () => {
    assert.equal('I'.toLocaleLowerCase('tr'), '\u0131');
    assert.equal(normalizeSearch('I'), 'i');
    assert.equal(normalizeSearch('i'), 'i');
    assert.ok(normalizeSearch('KEY:I').includes(normalizeSearch('i')));
});
