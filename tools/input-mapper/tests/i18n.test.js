import test from 'node:test';
import assert from 'node:assert/strict';
import {diagnostic, initialLocale, locales, messages, translate} from '../i18n.js';
import {captureKey} from '../capture.js';
import {parseIni, serialize} from '../validation.js';

test('every selectable language supplies the primary interface and English fallbacks are complete', () => {
    assert.equal(locales[0].code, 'ar-EG');
    assert.equal(new Set(locales.map(item => item.code)).size, locales.length);
    const required = ['controls', 'configuration', 'help', 'settings', 'language', 'close', 'intro', 'search', 'import', 'reset', 'select', 'add', 'capture', 'remove', 'download', 'copy', 'backControls', 'helpIntro', 'configIntro'];
    for (let index = 0; index < 6; index++) required.push(`group${index}`);
    for (let index = 0; index < 4; index++) required.push(`faq${index}Question`, `faq${index}Answer`);
    for (const [key, message] of Object.entries(messages.en)) {
        assert.equal(typeof message, 'string', `en: ${key}`);
        assert.notEqual(message.trim(), '', `en: ${key}`);
    }
    for (const locale of locales) {
        assert.doesNotThrow(() => new Intl.DateTimeFormat(locale.code));
        assert.equal(locale.direction, ['ar-EG', 'ur', 'fa'].includes(locale.code) ? 'rtl' : 'ltr');
        assert.ok(messages[locale.code], locale.code);
        for (const key of required) {
            assert.equal(typeof messages[locale.code][key], 'string', `${locale.code}: ${key}`);
            assert.notEqual(messages[locale.code][key].trim(), '', `${locale.code}: ${key}`);
        }
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

test('translated diagnostics retain action tokens and unknown technical details', () => {
    assert.equal(diagnostic('ar-EG', 'Cross: Unknown or empty KEY value.'), 'Cross: قيمة KEY مش معروفة أو فاضية.');
    assert.equal(diagnostic('sv', 'Cross: Unknown or empty KEY value.'), 'Cross: Okänt eller tomt KEY-värde.');
    assert.equal(diagnostic('ar-EG', 'Unexpected diagnostic'), 'Unexpected diagnostic');
    assert.equal(diagnostic('sv', 'Unexpected diagnostic'), 'Unexpected diagnostic');
    assert.equal(diagnostic('ar-EG', 'Composition and dead keys are ambiguous. Select an SDL key from the list.'), 'زرار تركيب الحروف مش واضح في التسجيل. اختار زرار SDL من القائمة.');
    assert.equal(diagnostic('fr', 'Cross: Unknown or empty KEY value.'), 'Cross : Valeur KEY inconnue ou vide.');
});

test('translated controller diagnostics preserve button names and None', () => {
    assert.equal(diagnostic('sv', "Unknown SDL controller button 'leftx'."), "Okänd SDL-handkontrollknapp 'leftx'.");
    assert.equal(diagnostic('sv', 'SDLButton:a is set more than once.'), 'SDLButton:a anges mer än en gång.');
    assert.equal(diagnostic('pt-BR', 'SDLButton:a: expected a PlayStation button or None.'), 'SDLButton:a: O valor deve ser um botão do PlayStation ou None.');
    assert.equal(diagnostic('ar-EG', 'SDLButton:a: expected a PlayStation button or None.'), 'SDLButton:a: القيمة لازم تكون زرار PlayStation أو None.');
});

test('capture diagnostics are fully translated', () => {
    const message = captureKey({key: 'Dead', code: 'BracketRight'}).error;
    assert.equal(diagnostic('sv', message), 'Döda tangenter och kombinationstangenter är tvetydiga. Välj en SDL-tangent i listan.');
    assert.equal(diagnostic('ar-EG', message), 'زرار تركيب الحروف مش واضح في التسجيل. اختار زرار SDL من القائمة.');
});

test('Egyptian diagnostics translate setting refusals with their names and ranges', () => {
    assert.equal(diagnostic('ar-EG', 'MouseSensitivity is set more than once.'), 'MouseSensitivity متكرر ومش لازم يتحدد أكتر من مرة.');
    assert.equal(diagnostic('ar-EG', 'LeftStickDeadzone must be a whole percentage from 0 to 90.'), 'LeftStickDeadzone لازم يكون رقم صحيح من 0 لـ 90.');
    assert.equal(diagnostic('ar-EG', 'MouseSensitivity must be a number greater than 0 and at most 10.'), 'MouseSensitivity لازم يكون رقم أكبر من 0 وبحد أقصى 10.');
    assert.equal(diagnostic('ar-EG', 'MouseCurve must be a number from 0.2 to 5.'), 'MouseCurve لازم يكون رقم من 0.2 لـ 5.');
    assert.equal(diagnostic('ar-EG', 'MadeUp: unknown input setting.'), 'الإعداد MadeUp مش معروف.');
});

test('Portuguese diagnostics keep action and type tokens', () => {
    assert.equal(diagnostic('pt-BR', 'Cross: Unknown or empty KEY value.'), 'Cross: Valor KEY desconhecido ou vazio.');
    assert.equal(diagnostic('pt-BR', 'Cross: duplicate KEY:F will be omitted.'), 'Cross: duplicado KEY:F será omitido.');
    assert.equal(diagnostic('pt-BR', 'Composition and dead keys are ambiguous. Select an SDL key from the list.'), 'A composição e as teclas mortas são ambíguas. Escolha uma tecla SDL na lista.');
    assert.equal(diagnostic('pt-BR', 'Unexpected diagnostic'), 'Unexpected diagnostic');
});

test('French diagnostics keep action, setting and type tokens', () => {
    assert.equal(diagnostic('fr', 'Cross: Unknown or empty KEY value.'), 'Cross : Valeur KEY inconnue ou vide.');
    assert.equal(diagnostic('fr', 'Cross : Unknown or empty KEY value.'), 'Cross : Valeur KEY inconnue ou vide.');
    assert.equal(diagnostic('fr', 'Cross: duplicate KEY:F will be omitted.'), 'Cross : l’entrée KEY:F en double sera omise.');
    assert.equal(diagnostic('fr', 'MouseSensitivity must be a number greater than 0 and at most 10.'), 'MouseSensitivity doit être un nombre supérieur à 0 et inférieur ou égal à 10.');
    assert.equal(diagnostic('fr', 'StickModifierScale must be a number from 0.05 to 1.'), 'StickModifierScale doit être un nombre de 0.05 à 1.');
    assert.equal(diagnostic('fr', 'SDLButton:a: expected a PlayStation button or None.'), 'SDLButton:a : la valeur doit être un bouton PlayStation ou None.');
    assert.equal(diagnostic('fr', 'SDLButton:a is set more than once.'), 'SDLButton:a est défini plus d’une fois.');
    assert.equal(diagnostic('fr', 'Binding: Unknown or empty action.'), 'Entrée : Action inconnue ou vide.');
    assert.equal(diagnostic('fr', 'KEY:C controls TogglePerformanceOverlay and Circle (built-in).'), 'KEY:C déclenche TogglePerformanceOverlay et Circle (par défaut).');
    assert.equal(diagnostic('fr', 'KEY:Keypad : controls Triangle and L1.'), 'KEY:Keypad : déclenche Triangle et L1.');
    assert.equal(diagnostic('fr', 'Expected Action = TYPE:VALUE.'), 'Format attendu : Action = TYPE:VALUE.');
    assert.equal(diagnostic('fr', captureKey({key: 'Dead', code: 'BracketRight'}).error), 'Les touches mortes et de composition sont ambiguës. Choisissez une touche SDL dans la liste.');
    assert.equal(diagnostic('fr', 'Unexpected diagnostic'), 'Unexpected diagnostic');
    assert.equal(diagnostic('fr', 'Le fichier INI doit faire moins de 1 MiB.'), 'Le fichier INI doit faire moins de 1 MiB.');
});

test('inherited property names fall back to the original message', () => {
    for (const locale of ['xx', 'constructor', 'toString', '__proto__', 'hasOwnProperty']) {
        assert.equal(diagnostic(locale, 'Unexpected diagnostic'), 'Unexpected diagnostic');
    }
});
