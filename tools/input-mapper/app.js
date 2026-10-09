(() => {
    const groups = [
        ['Face buttons', [['Cross', 'Cross', '✕'], ['Circle', 'Circle', '○'], ['Triangle', 'Triangle', '△'], ['Square', 'Square', '□']]],
        ['Shoulders and triggers', [['L1', 'L1'], ['R1', 'R1'], ['L2', 'L2'], ['R2', 'R2']]],
        ['D-pad', [['Up', 'Up', '↑'], ['Down', 'Down', '↓'], ['Left', 'Left', '←'], ['Right', 'Right', '→']]],
        ['Left stick', [['LeftStickUp', 'Up', '↑'], ['LeftStickDown', 'Down', '↓'], ['LeftStickLeft', 'Left', '←'], ['LeftStickRight', 'Right', '→'], ['L3', 'Press', '●']]],
        ['Right stick', [['RightStickUp', 'Up', '↑'], ['RightStickDown', 'Down', '↓'], ['RightStickLeft', 'Left', '←'], ['RightStickRight', 'Right', '→'], ['R3', 'Press', '●']]],
        ['Touchpad', [['TouchLeft', 'Left side'], ['TouchRight', 'Right side']]],
        ['Options and toggles', [['Options', 'Options'], ['ToggleMouse', 'Mouse look'], ['ToggleFullscreen', 'Fullscreen']]]
    ];

    const element = (id) => document.getElementById(id);
    const groupsView = element('groups');
    const preview = element('preview');
    const summary = element('summary');
    const status = element('status');
    const problems = element('problems');
    const download = element('download');
    const copy = element('copy');
    const fileInput = element('file');
    const dialog = element('capture');
    const captureTitle = element('capture-title');
    const captureZone = element('capture-zone');
    const captureHelp = element('capture-help');
    const captureMessage = element('capture-message');
    const nameForm = element('capture-name');
    const nameInput = element('key-name');

    const builtIn = InputMapper.defaults();
    const actionsByName = new Map(InputMapper.actions.map((action) => [action.name, action]));
    let bindings = InputMapper.defaults();
    let fileErrors = [];
    let openedName = '';
    let captureAction = null;
    let pendingControl = null;

    function create(tag, className, text) {
        const node = document.createElement(tag);
        if (className) node.className = className;
        if (text !== undefined) node.textContent = text;
        return node;
    }

    function capitalize(text) {
        return text.charAt(0).toUpperCase() + text.slice(1);
    }

    function bindingLabel(binding) {
        if (binding.type === 'KEY') return InputMapper.keyName(binding.key);
        if (binding.type === 'MOUSE') return 'Mouse ' + binding.button;
        return 'Wheel ' + binding.direction;
    }

    function announce(text) {
        status.textContent = text;
    }

    function removable(name) {
        return bindings.get(name).length > 1 || builtIn.get(name).length === 0;
    }

    function renderAction(name, label, glyph, shared) {
        const changed = InputMapper.changed(bindings, name);
        const item = create('li', changed ? 'action changed' : 'action');
        const head = create('div', 'action-head');
        if (glyph) head.append(create('span', 'glyph glyph-' + name, glyph));
        head.append(create('span', 'action-label', label));
        if (label !== name) head.append(create('span', 'key-name', name));
        if (changed) {
            const reset = create('button', 'reset', 'Reset');
            reset.type = 'button';
            reset.dataset.focus = 'reset-' + name;
            reset.setAttribute('aria-label', `Reset ${name} to the built-in bindings`);
            reset.addEventListener('click', () => {
                bindings.set(name, builtIn.get(name).map((binding) => ({ ...binding })));
                update(`Reset ${name}.`, 'add-' + name);
            });
            head.append(reset);
        }
        item.append(head);

        const list = create('ul', 'bindings');
        const current = bindings.get(name);
        if (current.length === 0) list.append(create('li', 'empty', 'Not bound'));
        current.forEach((binding, index) => {
            const chip = create('li', 'binding');
            const text = bindingLabel(binding);
            chip.append(create('span', '', text));
            const others = (shared.get(InputMapper.format(binding)) || []).filter((other) => other !== name);
            if (others.length !== 0) {
                chip.classList.add('conflict');
                chip.title = 'Also bound to ' + others.join(', ');
            }
            const remove = create('button', '', '×');
            remove.type = 'button';
            remove.dataset.focus = `remove-${name}-${index}`;
            remove.setAttribute('aria-label', `Remove ${text} from ${name}`);
            if (removable(name)) {
                remove.addEventListener('click', () => {
                    current.splice(index, 1);
                    update(`Removed ${text} from ${name}.`, 'add-' + name);
                });
            } else {
                remove.disabled = true;
                remove.title = `The file cannot leave ${name} without an input. Add another one first.`;
            }
            chip.append(remove);
            list.append(chip);
        });
        const add = create('button', 'add', '+ Add');
        add.type = 'button';
        add.dataset.focus = 'add-' + name;
        add.setAttribute('aria-label', 'Add an input to ' + name);
        add.addEventListener('click', () => openCapture(name));
        const addItem = create('li');
        addItem.append(add);
        list.append(addItem);
        item.append(list);
        return item;
    }

    function renderProblems(conflicts) {
        problems.replaceChildren();
        if (fileErrors.length !== 0) {
            const box = create('div', 'problem error');
            box.append(create('p', '', `AnyPS5 would refuse ${openedName} and stop at line ${fileErrors[0].line}. These lines were left out here:`));
            const list = create('ul');
            for (const error of fileErrors) list.append(create('li', '', `Line ${error.line}: ${error.reason}`));
            box.append(list);
            problems.append(box);
        }
        if (conflicts.length !== 0) {
            const box = create('div', 'problem warning');
            box.append(create('p', '', 'These inputs press several actions at once:'));
            const list = create('ul');
            for (const entry of conflicts) list.append(create('li', '', `${bindingLabel(entry.binding)}: ${entry.actions.join(', ')}`));
            box.append(list);
            problems.append(box);
        }
    }

    function render(focusKey) {
        const conflicts = InputMapper.conflicts(bindings);
        const shared = new Map(conflicts.map((entry) => [InputMapper.format(entry.binding), entry.actions]));

        const sections = groups.map(([title, actions]) => {
            const section = create('section', 'group');
            section.append(create('h2', '', title));
            const list = create('ul');
            for (const [name, label, glyph] of actions) list.append(renderAction(name, label, glyph, shared));
            section.append(list);
            return section;
        });
        groupsView.replaceChildren(...sections);

        const text = InputMapper.serialize(bindings);
        const count = InputMapper.actions.filter((action) => InputMapper.changed(bindings, action.name)).length;
        preview.classList.toggle('placeholder', text === '');
        preview.textContent = text || 'No changes. Without this file AnyPS5 uses its built-in bindings.';
        summary.textContent = count === 0 ? 'Built-in bindings' : `${count} ${count === 1 ? 'action' : 'actions'} changed`;
        download.disabled = text === '';
        copy.disabled = text === '';
        renderProblems(conflicts);

        if (focusKey) {
            const target = groupsView.querySelector(`[data-focus="${focusKey}"]`);
            if (target) target.focus();
        }
    }

    function update(message, focusKey) {
        render(focusKey);
        announce(message);
    }

    function openCapture(name) {
        const action = actionsByName.get(name);
        captureAction = name;
        captureTitle.textContent = 'Add an input to ' + name;
        captureHelp.textContent = name === 'ToggleFullscreen'
            ? 'Keyboard keys only'
            : action.button ? 'or click here with a mouse button, or scroll the wheel' : 'or click here with a mouse button';
        captureMessage.textContent = '';
        nameInput.value = '';
        dialog.showModal();
        captureZone.focus();
    }

    function closeCapture() {
        if (pendingControl) clearTimeout(pendingControl);
        pendingControl = null;
        if (dialog.open) dialog.close();
    }

    function commit(binding) {
        if (!dialog.open) return;
        const name = captureAction;
        const problem = InputMapper.problem(name, binding);
        if (problem) {
            captureMessage.textContent = capitalize(problem) + '.';
            return;
        }
        const current = bindings.get(name);
        if (current.some((existing) => InputMapper.same(existing, binding))) {
            captureMessage.textContent = `${bindingLabel(binding)} is already bound to ${name}.`;
            return;
        }
        current.push(binding);
        closeCapture();
        update(`Added ${bindingLabel(binding)} to ${name}.`, 'add-' + name);
    }

    function commitCode(code) {
        const found = InputMapper.keyFromCode(code);
        if (!found) {
            captureMessage.textContent = 'This key cannot be identified in the browser. Type its SDL key name below.';
            return;
        }
        commit({ type: 'KEY', key: found.key });
    }

    captureZone.addEventListener('keydown', (event) => {
        event.preventDefault();
        event.stopPropagation();
        if (event.repeat) return;
        if (pendingControl) {
            clearTimeout(pendingControl);
            pendingControl = null;
            commitCode(event.code === 'AltRight' ? 'AltRight' : 'ControlLeft');
            return;
        }
        if (event.code === 'Escape') {
            closeCapture();
            return;
        }
        if (event.code === 'ControlLeft') {
            pendingControl = setTimeout(() => {
                pendingControl = null;
                commitCode('ControlLeft');
            }, 50);
            return;
        }
        commitCode(event.code);
    });

    captureZone.addEventListener('mousedown', (event) => {
        event.preventDefault();
        if (document.activeElement !== captureZone) {
            captureZone.focus();
            captureMessage.textContent = '';
            return;
        }
        const button = InputMapper.mouseButtons[event.button];
        if (button) commit({ type: 'MOUSE', button });
    });

    for (const type of ['mouseup', 'auxclick', 'contextmenu']) {
        captureZone.addEventListener(type, (event) => event.preventDefault());
    }

    captureZone.addEventListener('wheel', (event) => {
        event.preventDefault();
        if (event.deltaY !== 0) commit({ type: 'WHEEL', direction: event.deltaY < 0 ? 'Up' : 'Down' });
    }, { passive: false });

    nameForm.addEventListener('submit', (event) => {
        event.preventDefault();
        const value = nameInput.value.trim();
        if (!value) return;
        const key = InputMapper.keyFromName(value);
        if (!key) {
            captureMessage.textContent = `Unknown SDL key name '${value}'.`;
            return;
        }
        commit({ type: 'KEY', key });
    });

    element('capture-cancel').addEventListener('click', closeCapture);

    dialog.addEventListener('close', () => {
        if (dialog.open) return;
        if (pendingControl) clearTimeout(pendingControl);
        pendingControl = null;
        const target = groupsView.querySelector(`[data-focus="add-${captureAction}"]`);
        if (target) target.focus();
    });

    download.addEventListener('click', () => {
        const url = URL.createObjectURL(new Blob([InputMapper.serialize(bindings)], { type: 'text/plain' }));
        const link = create('a');
        link.href = url;
        link.download = 'anyps5-input.ini';
        link.click();
        setTimeout(() => URL.revokeObjectURL(url));
        announce('Saved anyps5-input.ini.');
    });

    copy.addEventListener('click', async () => {
        try {
            await navigator.clipboard.writeText(InputMapper.serialize(bindings));
            announce('Copied the file contents.');
        } catch {
            getSelection().selectAllChildren(preview);
            announce('Copy the selected text with Ctrl+C.');
        }
    });

    async function openFile(file) {
        const result = InputMapper.parse(await file.text());
        bindings = result.bindings;
        fileErrors = result.errors;
        openedName = file.name;
        update(result.errors.length === 0 ? `Opened ${file.name}.` : `Opened ${file.name} with ${result.errors.length} rejected ${result.errors.length === 1 ? 'line' : 'lines'}.`);
    }

    element('open').addEventListener('click', () => fileInput.click());

    fileInput.addEventListener('change', () => {
        if (fileInput.files.length !== 0) openFile(fileInput.files[0]);
        fileInput.value = '';
    });

    element('reset-all').addEventListener('click', () => {
        const changed = InputMapper.actions.some((action) => InputMapper.changed(bindings, action.name));
        if (changed && !confirm('Reset every action to the built-in bindings?')) return;
        bindings = InputMapper.defaults();
        fileErrors = [];
        update('Reset all actions.');
    });

    document.addEventListener('dragover', (event) => {
        if (!event.dataTransfer.types.includes('Files')) return;
        event.preventDefault();
        document.body.classList.add('dragging');
    });

    document.addEventListener('dragleave', (event) => {
        if (event.relatedTarget === null) document.body.classList.remove('dragging');
    });

    document.addEventListener('drop', (event) => {
        document.body.classList.remove('dragging');
        if (event.dataTransfer.files.length === 0) return;
        event.preventDefault();
        openFile(event.dataTransfer.files[0]);
    });

    const names = element('key-names');
    for (const [key, , name] of InputMapper.scancodes) {
        if (InputMapper.problem('Cross', { type: 'KEY', key }) === '') names.append(new Option(name));
    }

    render();
})();
