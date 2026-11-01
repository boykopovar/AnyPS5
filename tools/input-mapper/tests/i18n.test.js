import test from 'node:test';
import assert from 'node:assert/strict';
import {diagnostic, initialLocale, locales, messages, translate} from '../i18n.js';
import {parseIni, serialize} from '../validation.js';

test('every selectable language supplies the main interface and every FAQ', () => {
    assert.equal(locales[0].code, 'ar-EG');
    assert.equal(new Set(locales.map(item => item.code)).size, locales.length);
    const required = ['controls', 'configuration', 'help', 'settings', 'language', 'close', 'intro', 'search', 'import', 'reset', 'select', 'add', 'capture', 'remove', 'download', 'copy', 'backControls', 'helpIntro', 'configIntro'];
    for (let index = 0; index < 6; index++) required.push(`group${index}`);
    for (let index = 0; index < 4; index++) required.push(`faq${index}Question`, `faq${index}Answer`);
    for (const locale of locales) {
        assert.doesNotThrow(() => new Intl.DateTimeFormat(locale.code));
        assert.equal(locale.direction, ['ar-EG', 'ur', 'fa'].includes(locale.code) ? 'rtl' : 'ltr');
        for (const key of required) assert.ok(messages[locale.code][key], `${locale.code}: ${key}`);
    }
});

test('first visits use English while valid saved language preferences are retained', () => {
    for (const saved of [undefined, null, '', 'unsupported']) assert.equal(initialLocale(saved), 'en');
    for (const locale of locales) assert.equal(initialLocale(locale.code), locale.code);
});

test('language substitution preserves canonical inputs and exported INI bytes', () => {
    const source = 'Cross = KEY:F\nCross = KEY:Space\nToggleMouse = MOUSE:X1\n';
    const entries = parseIni(source).entries;
    for (const locale of locales) {
        assert.ok(translate(locale.code, 'builtin', {inputs: 'KEY:F, KEY:Space'}).includes('KEY:F, KEY:Space'));
        assert.equal(serialize(entries), source);
    }
    assert.equal(translate('ar-EG', 'captured', {value: 'F', action: 'Cross'}), 'تم تسجيل F لـ Cross.');
    assert.equal(translate('unsupported', 'help'), 'Help');
});

test('Egyptian diagnostics retain action tokens and unknown technical details', () => {
    assert.equal(diagnostic('ar-EG', 'Cross: Unknown or empty KEY value.'), 'Cross: قيمة KEY مش معروفة أو فاضية.');
    assert.equal(diagnostic('ar-EG', 'Unexpected diagnostic'), 'Unexpected diagnostic');
    assert.equal(diagnostic('fr', 'Cross: Unknown or empty KEY value.'), 'Cross: Unknown or empty KEY value.');
});
