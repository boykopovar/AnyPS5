import {actions, canonicalValue, defaults, mouseValues, typesForAction, wheelValues} from './mapping.js';
import {keyNames} from './keys.js';

export const settings = [
    {name: 'LeftStickDeadzone', kind: 'percent'},
    {name: 'RightStickDeadzone', kind: 'percent'},
    {name: 'MouseSensitivity', kind: 'sensitivity'},
    {name: 'MouseVerticalScale', minimum: 0.1, maximum: 10},
    {name: 'MouseCurve', minimum: 0.2, maximum: 5},
    {name: 'MouseDeadzoneOffset', minimum: 0, maximum: 0.9},
    {name: 'MouseMaxSpeed', minimum: 0.1, maximum: 10},
    {name: 'MouseSmoothingMs', minimum: 0, maximum: 500},
    {name: 'MouseDecayMs', minimum: 1, maximum: 1000},
    {name: 'StickModifierScale', minimum: 0.05, maximum: 1},
    {name: 'KeyboardRampMs', minimum: 0, maximum: 2000},
    {name: 'TriggerRampMs', minimum: 0, maximum: 2000},
    {name: 'MouseStickTarget', kind: 'stick'}
];

const numberPattern = /^-?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?$/u;

export function settingValue(setting, value) {
    const text = typeof value === 'string' ? value.trim() : '';
    if (setting.kind === 'percent') {
        if (!/^-?\d+$/u.test(text) || Number(text) < 0 || Number(text) > 90) return `${setting.name} must be a whole percentage from 0 to 90.`;
        return '';
    }
    const parsed = numberPattern.test(text) ? Number(text) : undefined;
    if (setting.kind === 'stick') {
        const target = text.toUpperCase();
        if (target !== 'LEFT' && target !== 'RIGHT') return `${setting.name} must be LEFT or RIGHT.`;
        return '';
    }
    if (setting.kind === 'sensitivity') {
        if (parsed === undefined || !Number.isFinite(parsed) || !(parsed > 0) || parsed > 10) return `${setting.name} must be a number greater than 0 and at most 10.`;
        return '';
    }
    if (parsed === undefined || !Number.isFinite(parsed) || parsed < setting.minimum || parsed > setting.maximum) {
        return `${setting.name} must be a number from ${setting.minimum} to ${setting.maximum}.`;
    }
    return '';
}

export function findSetting(name) {
    const canonical = canonicalValue(name, settings.map(setting => setting.name));
    return canonical === undefined ? undefined : settings.find(setting => setting.name === canonical);
}

export function validateSettings(configuration) {
    const errors = [];
    const accepted = [];
    const seen = new Set();
    if (!Array.isArray(configuration)) return {errors: [{index: 0, message: 'Settings must be a list of names and values.'}], settings: accepted};
    for (const [index, item] of configuration.entries()) {
        const setting = findSetting(item?.name);
        if (setting === undefined) {
            errors.push({index, message: `${item?.name ?? 'Setting'}: unknown input setting.`});
            continue;
        }
        const failure = settingValue(setting, item?.value);
        if (seen.has(setting.name)) {
            errors.push({index, message: `${setting.name} is set more than once.`});
        } else if (failure) {
            errors.push({index, message: failure});
        } else {
            seen.add(setting.name);
            accepted.push({name: setting.name, value: item.value.trim()});
        }
    }
    return {errors, settings: accepted};
}

export function validate(entries) {
    const errors = [];
    const warnings = [];
    const bindings = [];
    const seen = new Set();
    if (!Array.isArray(entries)) return {errors: [{message: 'Configuration must be a list of bindings.'}], warnings, bindings};
    entries.forEach((entry, index) => {
        const action = canonicalValue(entry?.action, actions);
        const type = canonicalValue(entry?.type, ['KEY', 'MOUSE', 'WHEEL']);
        let message;
        if (!action) message = 'Unknown or empty action.';
        else if (!type) message = 'Input type must be KEY, MOUSE or WHEEL.';
        else if (!typesForAction(action).includes(type)) message = `${action} cannot use ${type}.`;
        const choices = type === 'KEY' ? keyNames : type === 'MOUSE' ? mouseValues : wheelValues;
        const value = canonicalValue(entry?.value, choices);
        if (!message && !value) message = `Unknown or empty ${type} value.`;
        if (!message && /[#;\r\n]/u.test(value)) message = 'This SDL key name cannot be represented in the current INI comment syntax.';
        if (message) {
            errors.push({index, message: `${entry?.action || 'Binding'}: ${message}`});
            return;
        }
        const binding = {action, type, value};
        const identity = `${action}:${type}:${value}`;
        if (seen.has(identity)) warnings.push({index, message: `${action}: duplicate ${type}:${value} will be omitted.`});
        else bindings.push(binding);
        seen.add(identity);
    });
    const configured = new Set(bindings.map(binding => binding.action));
    const sources = new Map();
    const effective = bindings.map(binding => ({action: binding.action, source: `${binding.type}:${binding.value}`, builtin: false}));
    for (const action of actions) {
        if (!configured.has(action)) {
            for (const source of defaults[action]) effective.push({action, source, builtin: true});
        }
    }
    for (const binding of effective) {
        const owners = sources.get(binding.source) || [];
        owners.push(binding);
        sources.set(binding.source, owners);
    }
    for (const [source, owners] of sources) {
        if (owners.length > 1 && owners.some(owner => !owner.builtin)) {
            warnings.push({message: `${source} controls ${owners.map(owner => `${owner.action}${owner.builtin ? ' (built-in)' : ''}`).join(' and ')}.`});
        }
    }
    return {errors, warnings, bindings};
}

export function serialize(entries, configuration = []) {
    const result = validate(entries);
    if (result.errors.length) throw new Error(result.errors.map(error => error.message).join('\n'));
    const checked = validateSettings(configuration);
    if (checked.errors.length) throw new Error(checked.errors.map(error => error.message).join('\n'));
    const lines = [];
    for (const action of actions) {
        for (const binding of result.bindings.filter(binding => binding.action === action)) {
            lines.push(`${action} = ${binding.type}:${binding.value}`);
        }
    }
    for (const setting of settings) {
        const found = checked.settings.find(item => item.name === setting.name);
        if (found) lines.push(`${found.name} = ${found.value}`);
    }
    return lines.length ? `${lines.join('\n')}\n` : '';
}

export function parseIni(text) {
    if (typeof text !== 'string') throw new TypeError('INI content must be text.');
    const entries = [];
    const configuration = [];
    const errors = [];
    const warnings = [];
    const identities = new Set();
    text.replace(/^\uFEFF/u, '').split('\n').forEach((line, index) => {
        const content = line.split(/[#;]/u, 1)[0].trim();
        if (!content) return;
        const separator = content.indexOf('=');
        const source = content.slice(separator + 1);
        const setting = findSetting(content.slice(0, separator));
        if (separator !== -1 && setting !== undefined) {
            if (configuration.some(item => item.name === setting.name)) {
                errors.push({line: index + 1, message: `${setting.name} is set more than once.`});
            } else {
                const failure = settingValue(setting, source);
                if (failure) errors.push({line: index + 1, message: failure});
                else configuration.push({name: setting.name, value: source.trim()});
            }
            return;
        }
        const colon = source.indexOf(':');
        if (separator === -1 || colon === -1) {
            errors.push({line: index + 1, message: 'Expected Action = TYPE:VALUE.'});
            return;
        }
        const entry = {action: content.slice(0, separator), type: source.slice(0, colon), value: source.slice(colon + 1)};
        const result = validate([entry]);
        if (result.errors.length) {
            errors.push(...result.errors.map(error => ({line: index + 1, message: error.message})));
            return;
        }
        const binding = result.bindings[0];
        const identity = JSON.stringify(binding);
        if (identities.has(identity)) warnings.push({line: index + 1, message: 'Duplicate binding ignored.'});
        else entries.push(binding);
        identities.add(identity);
    });
    return {entries, settings: configuration, errors, warnings};
}
