import {actions, defaults, groups, mouseValues, typesForAction, wheelValues} from './mapping.js';
import {exportableKeys} from './keys.js';
import {captureKey} from './capture.js';
import {parseIni, serialize, validate} from './validation.js';
import {diagnostic, initialLocale, locales, translate} from './i18n.js';
import {createDropdown} from './dropdown.js';

const byId = id => document.getElementById(id);
let entries = [];
let savedConfiguration = '[]';
let captureTarget;
let captureButton;
let selectedGroup = groups[0].name;
let locale = initialLocale();
let languageControl;
const bindingControls = new WeakMap();
let activeView = ['controls', 'configuration', 'help'].includes(location.hash.slice(1)) ? location.hash.slice(1) : 'controls';
let currentStatus;
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
    report.replaceChildren(element('h2', t(importReport.errors.length ? 'importRejected' : 'importRead')));
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
    return JSON.stringify(entries) !== savedConfiguration;
}

async function confirmReplacement() {
    if (!isDirty()) return true;
    const confirmation = byId('replace-dialog');
    confirmation.returnValue = 'cancel';
    return new Promise(resolve => {
        confirmation.addEventListener('close', () => resolve(confirmation.returnValue === 'discard'), {once: true});
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

function renderBinding(entry, position) {
    const row = element('div', undefined, 'binding');
    const label = t('bindingLabel', {action: entry.action, number: position + 1});
    const type = selectControl(t('typeLabel', {binding: label}), typesForAction(entry.action), entry.type);
    const value = selectControl(t('valueLabel', {binding: label}), valueChoices(entry.type), entry.value);
    type.element.dir = 'ltr';
    value.element.dir = 'ltr';
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
        const remaining = document.querySelector(`[data-action="${entry.action}"] .dropdown-trigger`);
        (remaining || document.querySelector(`[data-action="${entry.action}"] .add-binding`)).focus();
    });
    type.select.addEventListener('change', () => {
        entry.type = type.select.value;
        entry.value = '';
        value.setChoices([{value: '', label: t('select')}, ...valueChoices(entry.type)]);
        value.setValue('');
        capture.disabled = entry.type !== 'KEY';
        updateOutput();
    });
    value.select.addEventListener('change', () => {
        entry.value = value.select.value;
        updateOutput();
    });
    row.append(type.element, value.element, capture, remove);
    bindingControls.set(entry, value);
    return row;
}

function render() {
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
                entries.push({action, type: 'KEY', value: ''});
                render();
                const rows = document.querySelectorAll(`[data-action="${action}"] .binding`);
                rows[rows.length - 1].querySelectorAll('.dropdown-trigger')[1].focus();
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
    filterActions();
    updateOutput();
}

function filterActions() {
    const search = byId('search').value.trim().toLowerCase();
    if (activeView === 'controls') byId('group-title').textContent = search ? t('search') : groupLabel(selectedGroup);
    for (const category of byId('categories').children) {
        category.setAttribute('aria-pressed', String(!search && category.dataset.group === selectedGroup));
    }
    let visible = 0;
    for (const section of document.querySelectorAll('.group')) {
        let matches = 0;
        for (const action of section.querySelectorAll('.action')) {
            action.hidden = search ? !`${section.dataset.group} ${groupLabel(section.dataset.group)} ${action.dataset.action}`.toLowerCase().includes(search) : section.dataset.group !== selectedGroup;
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
}

function updateOutput() {
    const result = validate(entries);
    const valid = result.errors.length === 0;
    byId('preview').value = valid ? serialize(entries) : '';
    byId('count').textContent = `${new Set(result.bindings.map(binding => binding.action)).size} / ${actions.length}`;
    byId('download').disabled = !valid;
    byId('copy').disabled = !valid;
    const report = byId('validation');
    report.replaceChildren();
    for (const [name, messages] of [['error', result.errors], ['warning', result.warnings]]) {
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
byId('reset').addEventListener('click', async () => {
    if (!await confirmReplacement()) return;
    entries = [];
    savedConfiguration = '[]';
    importReport = undefined;
    byId('import-report').hidden = true;
    render();
    setStatus('resetDone');
});
byId('import').addEventListener('click', () => byId('file').click());
byId('file').addEventListener('change', async event => {
    const file = event.target.files[0];
    if (!file) return;
    event.target.value = '';
    try {
        if (file.size > 1024 * 1024) throw new Error(t('fileTooLarge'));
        const text = new TextDecoder('utf-8', {fatal: true}).decode(await file.arrayBuffer());
        const result = parseIni(text);
        const report = byId('import-report');
        importReport = result;
        renderImportReport();
        if (result.errors.length) {
            report.focus();
            return;
        }
        if (!await confirmReplacement()) return;
        entries = result.entries;
        savedConfiguration = JSON.stringify(entries);
        render();
        setStatus('imported', {name: file.name});
    } catch (error) {
        setStatus('importFailed', {message: error instanceof TypeError ? t('invalidUtf8') : error.message});
    }
});
byId('download').addEventListener('click', () => {
    const content = serialize(entries);
    const url = URL.createObjectURL(new Blob([content], {type: 'text/plain;charset=utf-8'}));
    const link = element('a');
    link.href = url;
    link.download = 'anyps5-input.ini';
    document.body.append(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    savedConfiguration = JSON.stringify(entries);
    setStatus('downloaded');
});
byId('copy').addEventListener('click', async () => {
    try {
        const content = serialize(entries);
        const copiedConfiguration = JSON.stringify(entries);
        await navigator.clipboard.writeText(content);
        savedConfiguration = copiedConfiguration;
        setStatus('copied');
    } catch {
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
