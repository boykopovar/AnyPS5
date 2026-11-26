import {actions, actionsForInput, asciiTrim, canonicalValue, chordButtons, defaults, groups, inputFilename, inputTitleId, mouseValues, typesForAction, wheelValues} from './mapping.js';
import {exportableKeys} from './keys.js';
import {captureKey} from './capture.js';
import {controllerDestinations, mouseStickTargets, parseIni, sdlControllerButtons, serialize, settings as settingDefinitions, validate, validateSettings} from './validation.js';
import {diagnostic, initialLocale, locales, translate} from './i18n.js';
import {createDropdown} from './dropdown.js';

const byId = id => document.getElementById(id);
let entries = [];
let settings = [];
let controllerRemaps = [];
const configurationText = () => serialize(entries, settings, controllerRemaps);
const settingValue = name => settings.find(setting => setting.name === name)?.value ?? '';
const snapshot = () => JSON.stringify([
    entries, settingDefinitions.map(({name}) => settingValue(name)), controllerRemaps
]);
let savedConfiguration = snapshot();
let replacementRequest = 0;
let captureTarget;
let captureButton;
let selectedGroup = groups[0].name;
let locale = initialLocale();
let languageControl;
const bindingControls = new WeakMap();
const settingControls = new Map();
let activeView = ['controls', 'configuration', 'help'].includes(location.hash.slice(1)) ? location.hash.slice(1) : 'controls';
let currentStatus;
let saveRequest = 0;
let importReport;
const dialog = byId('capture-dialog');
try {
    const savedLocale = localStorage.getItem('anyps5-controls-language');
    locale = initialLocale(savedLocale);
} catch {}
const t = (key, values) => translate(locale, key, values);
const groupLabel = name => t(`group${groups.findIndex(group => group.name === name)}`);

function setStatus(key, values = {}) {
    currentStatus = {key, values};
    byId('status').textContent = t(key, values);
}

function showView(view, focus = true, updateHash = true) {
    activeView = view;
    const controls = view === 'controls';
    byId('categories').hidden = !controls;
    byId('toolbar').hidden = !controls;
    byId('mappings').hidden = !controls;
    byId('configuration').hidden = view !== 'configuration';
    byId('help').hidden = view !== 'help';
    byId('validation').hidden = view === 'help';
    byId('status').hidden = view === 'help';
    byId('import-report').hidden = view === 'help' || !importReport;
    byId('view-intro').textContent = t(controls ? 'intro' : view === 'help' ? 'helpIntro' : 'configIntro');
    if (controls) filterActions();
    else byId('group-title').textContent = t(view);
    for (const link of document.querySelectorAll('[data-view]')) {
        const current = link.dataset.view === view;
        link.classList.toggle('current', current);
        if (current) link.setAttribute('aria-current', 'page');
        else link.removeAttribute('aria-current');
    }
    if (updateHash && location.hash !== `#${view}`) history.pushState(null, '', `#${view}`);
    if (focus) {
        byId('content').focus({preventScroll: true});
        window.scrollTo({top: 0, behavior: 'auto'});
    }
}

function renderHelp() {
    const opened = [...byId('help').querySelectorAll('details')].map(item => item.open);
    byId('help').replaceChildren();
    for (let index = 0; index < 4; index++) {
        const question = element('details', undefined, 'faq');
        question.open = opened[index] || false;
        question.append(element('summary', t(`faq${index}Question`)), element('p', t(`faq${index}Answer`)));
        byId('help').append(question);
    }
}

function renderImportReport() {
    if (!importReport) return;
    const report = byId('import-report');
    const heading = element('h2', t(importReport.errors.length ? 'importRejected' : 'importRead'));
    heading.id = 'import-report-title';
    report.replaceChildren(heading);
    const messages = [...importReport.errors, ...importReport.warnings];
    if (messages.length) {
        const list = element('ul');
        for (const item of messages) list.append(element('li', t('line', {line: item.line, message: diagnostic(locale, item.message)})));
        report.append(list);
    }
    if (importReport.errors.length) report.append(element('p', t('preserved'), 'error'));
    report.hidden = false;
}

function applyLocale() {
    document.documentElement.lang = locale;
    document.documentElement.dir = locales.find(item => item.code === locale).direction;
    for (const node of document.querySelectorAll('[data-i18n]')) node.textContent = t(node.dataset.i18n);
    for (const node of document.querySelectorAll('[data-i18n-label]')) node.setAttribute('aria-label', t(node.dataset.i18nLabel));
    for (const node of document.querySelectorAll('[data-i18n-placeholder]')) node.placeholder = t(node.dataset.i18nPlaceholder);
    languageControl.label = t('language');
    languageControl.direction = document.documentElement.dir;
    languageControl.text = dropdownText();
    languageControl.setValue(locale);
    byId('settings').title = t('language');
    byId('categories').replaceChildren();
    renderCategories();
    renderHelp();
    renderControllerRemaps();
    renderInputSettings();
    render();
    renderImportReport();
    if (currentStatus) setStatus(currentStatus.key, currentStatus.values);
    showView(activeView, false, false);
    updateClock();
}

function element(tag, text, className) {
    const node = document.createElement(tag);
    if (text !== undefined) node.textContent = text;
    if (className) node.className = className;
    return node;
}

function isDirty() {
    return snapshot() !== savedConfiguration;
}

async function confirmReplacement() {
    if (!isDirty()) return true;
    const confirmation = byId('replace-dialog');
    confirmation.returnValue = 'cancel';
    return new Promise(resolve => {
        function finish() {
            if (confirmation.open) return;
            confirmation.removeEventListener('close', finish);
            resolve(confirmation.returnValue === 'discard');
        }
        confirmation.addEventListener('close', finish);
        confirmation.showModal();
    });
}

function valueChoices(type) {
    return type === 'KEY' ? exportableKeys : type === 'MOUSE' ? mouseValues : type === 'WHEEL' ? wheelValues : [];
}

function dropdownText() {
    return {search: t('searchOptions'), close: t('close'), empty: t('noOptions')};
}

function selectControl(label, choices, value) {
    return createDropdown({label, choices: [{value: '', label: t('select')}, ...choices], value, placeholder: t('select'), text: dropdownText()});
}

function button(text, label, click) {
    const control = element('button', text);
    control.type = 'button';
    control.setAttribute('aria-label', label);
    control.addEventListener('click', click);
    return control;
}

function renderCategories() {
    const icons = ['face-icon', 'shoulder-icon', 'direction-icon', 'stick-icon', 'stick-icon', 'touch-icon'];
    const symbols = ['△ ○ × □', 'L2 R2', '', 'L', 'R', ''];
    groups.forEach((group, index) => {
        const category = button('', groupLabel(group.name), () => {
            selectedGroup = group.name;
            byId('search').value = '';
            filterActions();
        });
        category.className = 'category';
        category.dataset.group = group.name;
        const icon = element('span', undefined, 'category-icon');
        icon.setAttribute('aria-hidden', 'true');
        const mark = element('span', undefined, icons[index]);
        if (index === 0) {
            for (const symbol of symbols[index].split(' ')) mark.append(element('span', symbol));
        } else mark.textContent = symbols[index];
        icon.append(mark);
        category.append(icon, element('span', t(`group${index}`), 'category-label'));
        category.addEventListener('keydown', event => {
            const direction = document.documentElement.dir === 'rtl' ? -1 : 1;
            const offset = event.key === 'ArrowRight' ? direction : event.key === 'ArrowLeft' ? -direction : 0;
            if (!offset && event.key !== 'Home' && event.key !== 'End') return;
            event.preventDefault();
            const next = event.key === 'Home' ? 0 : event.key === 'End' ? groups.length - 1 : (index + offset + groups.length) % groups.length;
            const target = byId('categories').children[next];
            target.click();
            target.focus();
            target.scrollIntoView({block: 'nearest', inline: 'nearest'});
        });
        byId('categories').append(category);
    });
}

function renderChordControl(entry, label) {
    const control = element('details', undefined, 'chord-picker');
    const summary = element('summary');
    const options = element('div', undefined, 'chord-buttons');
    const selection = new Set(entry.value.split('+'));
    const checkboxes = chordButtons.map(name => {
        const option = element('label');
        const checkbox = element('input');
        checkbox.type = 'checkbox';
        checkbox.value = name;
        checkbox.checked = selection.has(name);
        option.append(checkbox, name);
        options.append(option);
        return checkbox;
    });
    const updateSummary = () => {
        summary.textContent = entry.value || t('select');
        summary.setAttribute('aria-label', `${label} (${summary.textContent})`);
    };
    options.addEventListener('change', () => {
        entry.value = checkboxes.filter(checkbox => checkbox.checked).map(checkbox => checkbox.value).join('+');
        updateSummary();
        updateOutput();
    });
    control.dir = 'ltr';
    control.append(summary, options);
    control.addEventListener('toggle', filterActions);
    updateSummary();
    return control;
}

function renderBindingValue(entry, label) {
    if (entry.type === 'PAD') {
        bindingControls.delete(entry);
        return renderChordControl(entry, label);
    }
    const control = selectControl(label, valueChoices(entry.type), entry.value);
    control.element.dir = 'ltr';
    control.select.addEventListener('change', () => {
        entry.value = control.select.value;
        updateOutput();
    });
    bindingControls.set(entry, control);
    return control.element;
}

function renderBinding(entry, position) {
    const row = element('div', undefined, 'binding');
    const label = t('bindingLabel', {action: entry.action, number: position + 1});
    const type = selectControl(t('typeLabel', {binding: label}), typesForAction(entry.action), entry.type);
    const valueLabel = t('valueLabel', {binding: label});
    let value = renderBindingValue(entry, valueLabel);
    type.element.dir = 'ltr';
    const capture = button(t('capture'), t('captureLabel', {binding: label}), () => {
        captureTarget = entry;
        captureButton = capture;
        byId('capture-title').textContent = t('captureTitle', {action: entry.action});
        byId('capture-status').textContent = '';
        dialog.showModal();
    });
    capture.disabled = entry.type !== 'KEY';
    const remove = button(t('remove'), t('removeLabel', {binding: label}), () => {
        entries = entries.filter(binding => binding !== entry);
        render();
        const action = document.querySelector(`[data-action="${entry.action}"]`);
        const remaining = action.querySelector('.dropdown-trigger') || action.querySelector('.add-binding');
        (action.hidden ? byId('search') : remaining).focus();
    });
    type.select.addEventListener('change', () => {
        if (entry.type === type.select.value) return;
        entry.type = type.select.value;
        entry.value = '';
        const next = renderBindingValue(entry, valueLabel);
        value.replaceWith(next);
        value = next;
        capture.disabled = entry.type !== 'KEY';
        updateOutput();
    });
    row.append(type.element, value, capture, remove);
    return row;
}

function renderControllerRemaps() {
    byId('controller-remaps').replaceChildren(...sdlControllerButtons.map(button => {
        const row = element('label');
        const source = element('span', `SDLButton:${button}`);
        source.dir = 'ltr';
        const select = element('select');
        for (const destination of ['', ...controllerDestinations]) {
            const option = element('option', destination || t('noOverride'));
            option.value = destination;
            select.append(option);
        }
        select.value = controllerRemaps.find(remap => remap.button === button)?.destination || '';
        select.addEventListener('change', () => {
            const destination = select.value;
            const remap = controllerRemaps.find(item => item.button === button);
            if (!destination) controllerRemaps = controllerRemaps.filter(item => item.button !== button);
            else if (remap) remap.destination = destination;
            else controllerRemaps.push({button, destination});
            updateOutput();
        });
        row.append(source, select);
        return row;
    }));
}

function renderInputSettings() {
    settingControls.clear();
    byId('input-setting-values').replaceChildren(...settingDefinitions.map(({name, kind, minimum, minimumExclusive, maximum, builtin}) => {
        const choices = kind === 'stickTarget' ? mouseStickTargets : undefined;
        const row = element('label');
        const label = element('span', name);
        label.id = `setting-${name}-label`;
        label.dir = 'ltr';
        const range = minimumExclusive ? `${minimum} < x ≤ ${maximum}` : `${minimum}–${maximum}`;
        const hint = element('span', [minimum === undefined ? '' : `\u2066${range}\u2069`, builtin === undefined ? '' : t('builtin', {inputs: builtin})].filter(Boolean).join(' · '));
        hint.className = 'hint';
        hint.id = `setting-${name}-hint`;
        const control = element(choices ? 'select' : 'input');
        const value = settingValue(name);
        control.dir = 'ltr';
        control.setAttribute('aria-labelledby', label.id);
        control.setAttribute('aria-describedby', hint.textContent ? `${hint.id} input-settings-help` : 'input-settings-help');
        control.setAttribute('aria-errormessage', 'validation');
        if (choices) {
            for (const choice of ['', ...choices]) {
                const option = element('option', choice || t('inheritSetting'));
                option.value = choice;
                control.append(option);
            }
            control.value = canonicalValue(value, choices) ?? '';
        } else {
            control.type = 'text';
            control.autocomplete = 'off';
            control.spellcheck = false;
            control.placeholder = kind === 'padButtons' ? 'R2, L1' : t('inheritSetting');
            if (kind !== 'padButtons') control.inputMode = kind === 'percent' ? 'numeric' : 'decimal';
            control.value = value;
        }
        control.addEventListener(choices ? 'change' : 'input', () => {
            const next = asciiTrim(control.value);
            const current = settings.findIndex(setting => setting.name === name);
            if (current === -1) {
                if (next) settings.push({name, value: next});
            } else if (next) settings[current].value = next;
            else settings.splice(current, 1);
            updateOutput();
        });
        settingControls.set(name, control);
        row.append(label, control);
        if (hint.textContent) row.append(hint);
        return row;
    }));
}

function render(focusEntry) {
    const container = byId('mappings');
    container.replaceChildren();
    for (const group of groups) {
        const section = element('section', undefined, 'group');
        section.dataset.group = group.name;
        section.append(element('h2', groupLabel(group.name)));
        for (const action of group.actions) {
            const article = element('div', undefined, 'action');
            article.dataset.action = action;
            const heading = element('div', undefined, 'action-heading');
            const add = button(t('add'), t('addLabel', {action}), () => {
                const entry = {action, type: 'KEY', value: ''};
                entries.push(entry);
                render(entry);
            });
            add.className = 'add-binding';
            const title = element('div', undefined, 'action-title');
            const symbols = {Cross: '×', Circle: '○', Triangle: '△', Square: '□', Up: '↑', Right: '→', Down: '↓', Left: '←'};
            if (symbols[action]) {
                const symbol = element('span', symbols[action], 'action-symbol');
                symbol.setAttribute('aria-hidden', 'true');
                title.append(symbol);
            }
            const actionName = element('h3', action);
            actionName.dir = 'ltr';
            title.append(actionName);
            heading.append(title, add);
            article.append(heading);
            const configured = entries.filter(entry => entry.action === action);
            article.append(element('p', configured.length ? t('custom') : t('builtin', {inputs: defaults[action].join(', ') || t('noDefault')}), 'hint'));
            configured.forEach((entry, position) => article.append(renderBinding(entry, position)));
            section.append(article);
        }
        container.append(section);
    }
    bindingControls.get(focusEntry)?.focus();
    updateOutput();
}

function filterActions() {
    const searchField = byId('search');
    const search = searchField.value.trim().toLowerCase();
    const focused = document.activeElement;
    const focusedAction = focused?.closest('.action');
    const inputActions = actionsForInput(search, entries);
    if (focused?.closest('.binding')?.querySelector('[aria-invalid="true"]') || focused?.closest('.chord-picker')?.open) {
        inputActions.add(focusedAction.dataset.action);
    }
    if (activeView === 'controls') byId('group-title').textContent = search ? t('search') : groupLabel(selectedGroup);
    for (const category of byId('categories').children) {
        category.setAttribute('aria-pressed', String(!search && category.dataset.group === selectedGroup));
    }
    let visible = 0;
    for (const section of document.querySelectorAll('.group')) {
        let matches = 0;
        for (const action of section.querySelectorAll('.action')) {
            const label = `${section.dataset.group} ${groupLabel(section.dataset.group)} ${action.dataset.action}`.toLowerCase();
            action.hidden = search ? !label.includes(search) && !inputActions.has(action.dataset.action) : section.dataset.group !== selectedGroup;
            if (!action.hidden) matches++;
        }
        section.hidden = matches === 0;
        visible += matches;
    }
    let empty = byId('no-results');
    if (!empty) {
        empty = element('p', t('noResults'), 'hint');
        empty.id = 'no-results';
        byId('mappings').append(empty);
    }
    empty.hidden = visible !== 0;
    if (focusedAction?.hidden) searchField.focus({preventScroll: true});
}

function updateOutput() {
    const result = validate(entries);
    const settingErrors = validateSettings(settings).errors;
    const valid = result.errors.length === 0 && settingErrors.length === 0;
    byId('preview').value = valid ? configurationText() : '';
    byId('count').textContent = `${new Set(result.bindings.map(binding => binding.action)).size} / ${actions.length}`;
    const titleId = byId('profile-title-id');
    const filename = inputFilename(titleId.value);
    titleId.setAttribute('aria-invalid', String(filename === undefined));
    byId('profile-title-id-error').hidden = filename !== undefined;
    byId('preview-name').textContent = filename ?? t('configuration');
    byId('download').disabled = !valid || filename === undefined;
    byId('copy').disabled = !valid;
    const report = byId('validation');
    report.replaceChildren();
    for (const [name, messages] of [['error', [...result.errors, ...settingErrors]], ['warning', result.warnings]]) {
        if (!messages.length) continue;
        const list = element('ul', undefined, name);
        for (const item of messages) list.append(element('li', diagnostic(locale, item.message)));
        report.append(list);
    }
    const rows = [...document.querySelectorAll('.binding')];
    const renderedEntries = groups.flatMap(group => group.actions.flatMap(action => entries.filter(entry => entry.action === action)));
    rows.forEach((row, index) => {
        const invalid = result.errors.some(error => entries[error.index] === renderedEntries[index]);
        for (const control of row.querySelectorAll('.dropdown-trigger')) control.setAttribute('aria-invalid', String(invalid));
    });
    const invalidSettings = new Set(settingErrors.map(error => settings[error.index].name));
    for (const [name, control] of settingControls) control.setAttribute('aria-invalid', String(invalidSettings.has(name)));
    filterActions();
}

function cancelCapture(message) {
    captureTarget = undefined;
    dialog.close();
    captureButton?.focus();
    if (message) setStatus(message);
}

byId('cancel-capture').addEventListener('click', () => cancelCapture('captureCancelled'));
dialog.addEventListener('cancel', event => {
    event.preventDefault();
    cancelCapture('captureCancelled');
});
window.addEventListener('blur', () => {
    if (captureTarget) cancelCapture('captureBlur');
});
window.addEventListener('keydown', event => {
    if (!captureTarget) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    if (event.code === 'Escape' || event.key === 'Escape') {
        cancelCapture('captureCancelled');
        return;
    }
    if (event.repeat) return;
    const result = captureKey(event);
    if (result.error) {
        byId('capture-status').textContent = diagnostic(locale, result.error);
        return;
    }
    const target = captureTarget;
    target.value = result.value;
    cancelCapture();
    bindingControls.get(target)?.setValue(result.value);
    updateOutput();
    setStatus('captured', {value: result.value, action: target.action});
}, true);

byId('search').addEventListener('input', filterActions);
byId('profile-title-id').addEventListener('input', updateOutput);
byId('reset').addEventListener('click', async () => {
    const request = ++replacementRequest;
    if (!await confirmReplacement() || request !== replacementRequest) return;
    entries = [];
    settings = [];
    controllerRemaps = [];
    savedConfiguration = snapshot();
    importReport = undefined;
    saveRequest++;
    byId('import-report').hidden = true;
    byId('profile-title-id').value = '';
    renderControllerRemaps();
    renderInputSettings();
    render();
    setStatus('resetDone');
});
byId('import').addEventListener('click', () => byId('file').click());
byId('file').addEventListener('change', async event => {
    const file = event.target.files[0];
    if (!file) return;
    event.target.value = '';
    const request = ++replacementRequest;
    try {
        if (file.size > 1024 * 1024) throw new Error(t('fileTooLarge'));
        const bytes = await file.arrayBuffer();
        if (request !== replacementRequest) return;
        const text = new TextDecoder('utf-8', {fatal: true}).decode(bytes);
        const result = parseIni(text);
        const report = byId('import-report');
        if (result.errors.length) {
            importReport = result;
            saveRequest++;
            renderImportReport();
            report.focus();
            return;
        }
        if (!await confirmReplacement() || request !== replacementRequest) return;
        importReport = result;
        saveRequest++;
        renderImportReport();
        const titleId = inputTitleId(file.name);
        if (titleId !== undefined) byId('profile-title-id').value = titleId;
        entries = result.entries;
        settings = result.settings;
        controllerRemaps = result.controllerRemaps;
        savedConfiguration = snapshot();
        renderControllerRemaps();
        renderInputSettings();
        render();
        setStatus('imported', {name: file.name});
    } catch (error) {
        if (request !== replacementRequest) return;
        saveRequest++;
        setStatus('importFailed', {message: error instanceof TypeError ? t('invalidUtf8') : diagnostic(locale, error.message)});
    }
});
byId('download').addEventListener('click', () => {
    const content = configurationText();
    const url = URL.createObjectURL(new Blob([content], {type: 'text/plain;charset=utf-8'}));
    const link = element('a');
    const filename = inputFilename(byId('profile-title-id').value);
    if (filename === undefined) {
        URL.revokeObjectURL(url);
        return;
    }
    link.href = url;
    link.download = filename;
    document.body.append(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    savedConfiguration = snapshot();
    saveRequest++;
    setStatus('downloaded');
});
byId('copy').addEventListener('click', async () => {
    const request = ++saveRequest;
    try {
        const content = configurationText();
        const copiedConfiguration = snapshot();
        await navigator.clipboard.writeText(content);
        if (request !== saveRequest) return;
        savedConfiguration = copiedConfiguration;
        setStatus('copied');
    } catch {
        if (request !== saveRequest) return;
        byId('preview').focus();
        byId('preview').select();
        setStatus('clipboardFailed');
    }
});
window.addEventListener('beforeunload', event => {
    if (!isDirty()) return;
    event.preventDefault();
    event.returnValue = '';
});
byId('focus-search').addEventListener('click', () => {
    showView('controls');
    byId('search').focus();
});
for (const link of document.querySelectorAll('[data-view]')) {
    link.addEventListener('click', event => {
        event.preventDefault();
        showView(link.dataset.view);
    });
}
byId('back-controls').addEventListener('click', () => showView('controls'));
function restoreView() {
    const view = location.hash.slice(1);
    if (['controls', 'configuration', 'help'].includes(view)) showView(view, false, false);
    else if (!view) showView('controls', false, false);
}
window.addEventListener('popstate', restoreView);
window.addEventListener('hashchange', restoreView);
languageControl = createDropdown({
    label: t('language'), choices: locales.map(item => ({value: item.code, label: item.name, lang: item.code, direction: item.direction})),
    value: locale, placeholder: t('language'), text: dropdownText(), trigger: byId('settings'), direction: locales.find(item => item.code === locale).direction
});
byId('language-control').append(languageControl.element);
languageControl.select.addEventListener('change', event => {
    locale = event.target.value;
    try { localStorage.setItem('anyps5-controls-language', locale); } catch {}
    applyLocale();
});
function updateClock() {
    const now = new Date();
    byId('clock').textContent = new Intl.DateTimeFormat(locale, {hour: '2-digit', minute: '2-digit'}).format(now);
    byId('clock').dateTime = now.toISOString();
}
updateClock();
setInterval(updateClock, 60000);
applyLocale();
