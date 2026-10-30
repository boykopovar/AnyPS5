import {actions, canonicalValue, defaults, mouseValues, typesForAction, wheelValues} from './mapping.js';
import {keyNames} from './keys.js';

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

export function serialize(entries) {
    const result = validate(entries);
    if (result.errors.length) throw new Error(result.errors.map(error => error.message).join('\n'));
    const lines = [];
    for (const action of actions) {
        for (const binding of result.bindings.filter(binding => binding.action === action)) {
            lines.push(`${action} = ${binding.type}:${binding.value}`);
        }
    }
    return lines.length ? `${lines.join('\n')}\n` : '';
}

export function parseIni(text) {
    if (typeof text !== 'string') throw new TypeError('INI content must be text.');
    const entries = [];
    const errors = [];
    const warnings = [];
    const identities = new Set();
    text.replace(/^\uFEFF/u, '').split('\n').forEach((line, index) => {
        const content = line.split(/[#;]/u, 1)[0].trim();
        if (!content) return;
        const separator = content.indexOf('=');
        const source = content.slice(separator + 1);
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
    return {entries, errors, warnings};
}
