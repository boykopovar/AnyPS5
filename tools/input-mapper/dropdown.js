const dialog = document.getElementById('picker-dialog');
const title = document.getElementById('picker-title');
const search = document.getElementById('picker-search');
const list = document.getElementById('picker-options');
const empty = document.getElementById('picker-empty');
const close = document.getElementById('picker-close');
let active;
let visible = [];
let focused = 0;

function renderOptions() {
    const query = search.value.trim().toLocaleLowerCase();
    visible = active.choices.filter(item => `${item.label} ${item.value}`.toLocaleLowerCase().includes(query));
    list.replaceChildren();
    const exactMatch = query ? visible.findIndex(item => item.label.toLocaleLowerCase() === query || item.value.toLocaleLowerCase() === query) : -1;
    focused = Math.max(0, exactMatch >= 0 ? exactMatch : visible.findIndex(item => item.value === active.select.value));
    visible.forEach((item, index) => {
        const option = document.createElement('button');
        option.type = 'button';
        option.className = 'picker-option';
        option.setAttribute('role', 'option');
        option.setAttribute('aria-selected', String(item.value === active.select.value));
        option.tabIndex = index === focused ? 0 : -1;
        option.textContent = item.label;
        if (item.lang) option.lang = item.lang;
        option.dir = item.direction || active.direction;
        option.addEventListener('click', () => choose(item));
        option.addEventListener('focus', () => { focused = index; });
        list.append(option);
    });
    empty.hidden = visible.length !== 0;
    if (dialog.open) list.children[focused]?.scrollIntoView({block: 'nearest'});
}

function choose(item) {
    const current = active;
    current.setValue(item.value);
    dialog.close();
    current.select.dispatchEvent(new Event('change', {bubbles: true}));
}

function focusOption(index) {
    if (!visible.length) return;
    focused = (index + visible.length) % visible.length;
    [...list.children].forEach((option, position) => { option.tabIndex = position === focused ? 0 : -1; });
    list.children[focused].focus({preventScroll: true});
    list.children[focused].scrollIntoView({block: 'nearest'});
}

function positionPicker() {
    if (!dialog.open || !active) return;
    if (matchMedia('(max-width: 600px)').matches) {
        for (const name of ['top', 'left', 'width']) dialog.style.removeProperty(name);
        return;
    }
    const rect = active.trigger.getBoundingClientRect();
    const width = Math.min(Math.max(rect.width, 320), innerWidth - 32);
    dialog.style.width = `${width}px`;
    const height = dialog.getBoundingClientRect().height;
    const preferred = rect.bottom + 10;
    const top = preferred + height <= innerHeight - 16 ? preferred : Math.max(16, rect.top - height - 10);
    const left = active.direction === 'rtl' ? rect.right - width : rect.left;
    dialog.style.top = `${Math.min(top, Math.max(16, innerHeight - height - 16))}px`;
    dialog.style.left = `${Math.max(16, Math.min(left, innerWidth - width - 16))}px`;
}

function openPicker(control) {
    active = control;
    title.textContent = control.label;
    list.setAttribute('aria-label', control.label);
    search.value = '';
    search.placeholder = control.text.search;
    search.setAttribute('aria-label', control.text.search);
    close.textContent = control.text.close;
    empty.textContent = control.text.empty;
    dialog.style.setProperty('--picker-height', `${Math.min(480, 136 + Math.min(control.choices.length, 8) * 46)}px`);
    renderOptions();
    control.trigger.setAttribute('aria-expanded', 'true');
    dialog.showModal();
    positionPicker();
    list.children[focused]?.scrollIntoView({block: 'nearest'});
    search.focus();
}

search.addEventListener('input', renderOptions);
dialog.addEventListener('keydown', event => {
    if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
        event.preventDefault();
        const inSearch = event.target === search;
        focusOption(inSearch ? focused : focused + (event.key === 'ArrowDown' ? 1 : -1));
    } else if (event.target !== search && (event.key === 'Home' || event.key === 'End')) {
        event.preventDefault();
        focusOption(event.key === 'Home' ? 0 : visible.length - 1);
    } else if (event.target === search && event.key === 'Enter' && visible.length) {
        event.preventDefault();
        choose(visible[focused]);
    }
});
close.addEventListener('click', () => dialog.close());
dialog.addEventListener('click', event => {
    if (event.target !== dialog) return;
    const rect = dialog.getBoundingClientRect();
    if (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom) dialog.close();
});
dialog.addEventListener('close', () => {
    const current = active;
    active = undefined;
    current?.trigger.setAttribute('aria-expanded', 'false');
    if (current?.trigger.isConnected) current.trigger.focus({preventScroll: true});
});
window.addEventListener('resize', positionPicker);

export function createDropdown({label, choices, value, placeholder, text, trigger, direction = 'ltr'}) {
    const wrapper = document.createElement('div');
    wrapper.className = 'dropdown';
    const select = document.createElement('select');
    select.hidden = true;
    const providedTrigger = Boolean(trigger);
    const control = trigger || document.createElement('button');
    control.type = 'button';
    if (!providedTrigger) {
        control.className = 'dropdown-trigger';
        control.setAttribute('role', 'combobox');
    }
    control.setAttribute('aria-label', label);
    control.setAttribute('aria-haspopup', 'dialog');
    control.setAttribute('aria-controls', 'picker-dialog');
    control.setAttribute('aria-expanded', 'false');
    const selected = document.createElement('span');
    selected.className = 'dropdown-value';
    if (!providedTrigger) control.append(selected);
    const api = {
        element: wrapper, select, trigger: control, label, direction, text, choices: [],
        setValue(next) {
            select.value = next;
            const choice = api.choices.find(item => item.value === select.value);
            if (!providedTrigger) selected.textContent = choice?.label || placeholder;
            if (!providedTrigger) control.title = choice?.label || placeholder;
        },
        setChoices(next) {
            const previous = select.value;
            api.choices = next.map(item => typeof item === 'string' ? {value: item, label: item} : item);
            select.replaceChildren();
            for (const item of api.choices) {
                const option = document.createElement('option');
                option.value = item.value;
                option.textContent = item.label;
                select.append(option);
            }
            api.setValue(previous);
        },
        focus() { control.focus(); }
    };
    api.setChoices(choices);
    api.setValue(value);
    select.addEventListener('change', () => api.setValue(select.value));
    control.addEventListener('click', () => openPicker(api));
    control.addEventListener('keydown', event => {
        if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
            event.preventDefault();
            openPicker(api);
        }
    });
    wrapper.append(select);
    if (!providedTrigger) wrapper.append(control);
    return api;
}
