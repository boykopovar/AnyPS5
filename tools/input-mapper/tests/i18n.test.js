import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {diagnostic, initialLocale, locales, messages, translate} from '../i18n.js';
import {captureKey} from '../capture.js';
import {parseIni, serialize} from '../validation.js';

test('every selectable language supplies the primary interface and English fallbacks are complete', () => {
    assert.equal(locales[0].code, 'ar-EG');
    assert.equal(new Set(locales.map(item => item.code)).size, locales.length);
    const required = ['controls', 'configuration', 'help', 'settings', 'language', 'close', 'intro', 'search', 'import', 'reset', 'select', 'add', 'capture', 'remove', 'download', 'copy', 'backControls', 'helpIntro', 'configIntro', 'controllerButtons', 'noOverride'];
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
    assert.equal(diagnostic('pt-BR', 'ToggleActions has an empty action name.'), 'ToggleActions contém um nome de ação vazio.');
    assert.equal(diagnostic('pt-BR', "ToggleActions: unknown action 'Jump'."), "ToggleActions: ação 'Jump' desconhecida.");
    assert.equal(diagnostic('pt-BR', "ToggleActions only lists pad buttons, not 'LeftStickUp'."), "ToggleActions aceita apenas botões do controle, não 'LeftStickUp'.");
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
    assert.equal(diagnostic('fr', 'ToggleActions has an empty action name.'), 'ToggleActions contient un nom d’action vide.');
    assert.equal(diagnostic('fr', "ToggleActions: unknown action 'Jump'."), "ToggleActions : action 'Jump' inconnue.");
    assert.equal(diagnostic('fr', "ToggleActions only lists pad buttons, not 'LeftStickUp'."), "ToggleActions n’accepte que des boutons de manette, pas 'LeftStickUp'.");
    assert.equal(diagnostic('fr', captureKey({key: 'Dead', code: 'BracketRight'}).error), 'Les touches mortes et de composition sont ambiguës. Choisissez une touche SDL dans la liste.');
    assert.equal(diagnostic('fr', 'Unexpected diagnostic'), 'Unexpected diagnostic');
    assert.equal(diagnostic('fr', 'Le fichier INI doit faire moins de 1 MiB.'), 'Le fichier INI doit faire moins de 1 MiB.');
});

test('Spanish diagnostics keep action, setting and type tokens', () => {
    assert.equal(diagnostic('es', 'Cross: Unknown or empty KEY value.'), 'Cross: Valor KEY desconocido o vacío.');
    assert.equal(diagnostic('es', 'Cross: duplicate KEY:F will be omitted.'), 'Cross: se omitirá la entrada duplicada KEY:F.');
    assert.equal(diagnostic('es', 'Cross cannot use WHEEL.'), 'Cross no puede usar WHEEL.');
    assert.equal(diagnostic('es', 'MouseSensitivity must be a number greater than 0 and at most 10.'), 'MouseSensitivity debe ser un número mayor que 0 y como máximo 10.');
    assert.equal(diagnostic('es', 'StickModifierScale must be a number from 0.05 to 1.'), 'StickModifierScale debe ser un número de 0.05 a 1.');
    assert.equal(diagnostic('es', 'LeftStickDeadzone must be a whole percentage from 0 to 90.'), 'LeftStickDeadzone debe ser un porcentaje entero de 0 a 90.');
    assert.equal(diagnostic('es', 'MouseStickTarget must be LEFT or RIGHT.'), 'MouseStickTarget debe ser LEFT o RIGHT.');
    assert.equal(diagnostic('es', 'MadeUp: unknown input setting.'), 'MadeUp: ajuste de entrada desconocido.');
    assert.equal(diagnostic('es', 'MouseSensitivity is set more than once.'), 'MouseSensitivity está definido más de una vez.');
    assert.equal(diagnostic('es', 'SDLButton:a: expected a PlayStation button or None.'), 'SDLButton:a: el valor debe ser un botón de PlayStation o None.');
    assert.equal(diagnostic('es', 'SDLButton:a is set more than once.'), 'SDLButton:a está definido más de una vez.');
    assert.equal(diagnostic('es', "Unknown SDL controller button 'leftx'."), "Botón de mando SDL desconocido: 'leftx'.");
    assert.equal(diagnostic('es', 'Binding: Unknown or empty action.'), 'Entrada: Acción desconocida o vacía.');
    assert.equal(diagnostic('es', 'KEY:C controls TogglePerformanceOverlay and Circle (built-in).'), 'KEY:C controla TogglePerformanceOverlay y Circle (predeterminado).');
    assert.equal(diagnostic('es', 'Expected Action = TYPE:VALUE.'), 'Formato esperado: Action = TYPE:VALUE.');
    assert.equal(diagnostic('es', 'Duplicate binding ignored.'), 'Entrada duplicada ignorada.');
    assert.equal(diagnostic('es', 'ToggleActions has an empty action name.'), 'ToggleActions contiene un nombre de acción vacío.');
    assert.equal(diagnostic('es', "ToggleActions: unknown action 'Jump'."), "ToggleActions: acción 'Jump' desconocida.");
    assert.equal(diagnostic('es', "ToggleActions only lists pad buttons, not 'LeftStickUp'."), "ToggleActions solo admite botones del mando, no 'LeftStickUp'.");
    assert.equal(diagnostic('es', captureKey({key: 'Dead', code: 'BracketRight'}).error), 'Las teclas muertas y de composición son ambiguas. Elige una tecla SDL de la lista.');
    assert.equal(diagnostic('es', 'Unexpected diagnostic'), 'Unexpected diagnostic');
});

test('every diagnostic of an invalid INI reads in Spanish', () => {
    const source = [
        'Cross = KEY:NotAKey', 'Jump = KEY:A', 'Cross = KEY:F', 'Cross = KEY:F', 'Circle = KEY:Q', 'Square = KEY:Q',
        'garbage line', 'Square = BOGUS:1', 'MouseSensitivity = 50', 'MouseCurve = 2', 'MouseCurve = 3',
        'LeftStickDeadzone = 95', 'StickModifierScale = 3', 'MouseStickTarget = UP', 'ToggleActions = R2,,L1',
        'SDLButton:a = Jump', 'SDLButton:b = Cross', 'SDLButton:b = Circle', 'SDLButton:leftx = Cross',
    ].join('\n');
    const {errors, warnings} = parseIni(source);
    const reported = [...errors, ...warnings].map(item => item.message);
    assert.ok(reported.length >= 14);
    const english = /\b(must|Unknown|unknown|cannot|duplicate|Duplicate|expected|Expected|controls|built-in|more than once|only lists|has an empty)\b/u;
    for (const message of reported) {
        const translated = diagnostic('es', message);
        assert.notEqual(translated, message, message);
        assert.doesNotMatch(translated, english, `${message} -> ${translated}`);
    }
});

test('inherited property names fall back to the original message', () => {
    for (const locale of ['xx', 'constructor', 'toString', '__proto__', 'hasOwnProperty']) {
        assert.equal(diagnostic(locale, 'Unexpected diagnostic'), 'Unexpected diagnostic');
    }
});

test('Spanish and Portuguese translate the per-game profile field', () => {
    for (const code of ['es', 'pt-BR']) {
        for (const key of ['profileTitleId', 'profileTitleIdHelp', 'profileTitleIdError']) {
            assert.equal(typeof messages[code][key], 'string', `${code}: ${key}`);
            assert.notEqual(messages[code][key], messages.en[key], `${code}: ${key}`);
        }
    }
});

test('every translated title ID error states the native title ID length limit', () => {
    const source = readFileSync(new URL('../../../core/libs/prx/libScePad/include/InputMapping.hpp', import.meta.url), 'utf8');
    const [, limit] = source.match(/MaxTitleIdLength = (\d+);/u);
    for (const locale of [{code: 'en'}, ...locales]) {
        const message = messages[locale.code]?.profileTitleIdError;
        if (message === undefined) continue;
        const spellings = [limit, new Intl.NumberFormat(locale.code).format(Number(limit))];
        assert.ok(spellings.some(spelling => message.includes(spelling)), `${locale.code}: ${message}`);
    }
});
