const byId = id => document.getElementById(id);
let state = null, view = 'library', authenticated = false, refreshing = false, submitting = false, scanSignature = '', gamesSignature = '', detailsId = '';
async function api(path, value) {
    const response = await fetch(path, value === undefined ? {} : {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(value)});
    const result = await response.json();
    if (!response.ok) { const error = new Error(result.error || response.statusText); error.status = response.status; throw error; }
    return result;
}
function text(tag, value, className) {
    const item = document.createElement(tag);
    item.textContent = value;
    if (className) item.className = className;
    return item;
}
function button(label, handler, disabled = false, className = '') {
    const item = text('button', label, className); item.type = 'button'; item.disabled = disabled; item.onclick = handler; return item;
}
function unavailable() { return !authenticated || !state?.online || state.busy || submitting; }
function launchPending() { return state?.commands?.some(command => command.verb === 'launch' && ['queued', 'delivered'].includes(command.status)); }
function navigate(next) {
    view = next;
    byId('engine-panel').hidden = byId('engine-panel').dataset.configured === 'false' || (authenticated && view !== 'settings');
    for (const [name, id] of [['library', 'library'], ['folders', 'folders-view'], ['settings', 'settings-view']]) byId(id).hidden = !authenticated || view !== name;
    document.querySelectorAll('[data-view]').forEach(item => { item.classList.toggle('selected', item.dataset.view === view); item.setAttribute('aria-current', item.dataset.view === view ? 'page' : 'false'); });
}
function openAdd() {
    if (!authenticated) { byId('token').focus(); return; }
    byId('add-dialog').showModal(); renderScan();
}
async function action(name, id = '') {
    if (submitting) return;
    submitting = true; byId('error').textContent = ''; byId('scan-error').textContent = '';
    renderGames(true);
    renderScan();
    try { await api('/api/action', {action: name, id}); if (name === 'prepare_scan') byId('add-dialog').close(); }
    catch (error) { (byId('add-dialog').open ? byId('scan-error') : byId('error')).textContent = error.message; }
    finally { submitting = false; await refresh(); }
}
function renderGames(force = false) {
    if (!state) return;
    const signature = JSON.stringify([state.games, unavailable(), launchPending(), byId('search').value, byId('sort').value]);
    if (!force && gamesSignature === signature) return;
    gamesSignature = signature;
    const focused = document.activeElement?.dataset?.focus;
    const games = [...state.games].filter(game => (game.title + ' ' + game.title_id).toLocaleLowerCase().includes(byId('search').value.toLocaleLowerCase()));
    games.sort((a, b) => (byId('sort').value === 'availability' ? Number(b.available) - Number(a.available) : 0) || a.title.localeCompare(b.title));
    byId('count').textContent = state.games.length.toString().padStart(2, '0');
    byId('games').replaceChildren();
    for (const game of games) {
        const card = text('article', '', 'game');
        const cover = text('div', '', 'cover');
        cover.append(text('span', game.title.split(/\s+/).map(word => word[0]).slice(0, 2).join(''), 'monogram'));
        if (game.artwork_url) { const image = document.createElement('img'); image.src = game.artwork_url; image.alt = ''; image.loading = 'lazy'; image.onerror = () => image.remove(); cover.append(image); }
        cover.append(text('span', game.title, 'cover-title'));
        const body = text('div', '', 'game-body');
        body.append(text('h3', game.title), text('span', game.uncertain ? 'Previous session needs confirmation' : game.running ? 'Running on Windows' : game.available === false ? 'Folder or launch file missing' : game.exit_code ? 'Exited with code ' + game.exit_code + ' - Review logs' : 'Available \u00b7 Gameplay unverified', 'badge'));
        const row = text('div', '', 'row');
        const pending = state.commands.some(command => command.verb === 'launch' && command.arg === game.id && ['queued', 'delivered'].includes(command.status));
        const play = button(pending ? 'Launch requested' : game.running ? '\u25cf Running' : '\u25b7 Play', () => action('launch', game.id), unavailable() || game.available === false || state.games.some(item => item.running || item.uncertain) || launchPending(), 'primary');
        play.dataset.focus = game.id + '-play';
        const details = button('Details', () => showDetails(game.id), false, 'details-button'); details.dataset.focus = game.id + '-details';
        row.append(play, details); body.append(row); card.append(cover, body); byId('games').append(card);
    }
    if (!games.length) {
        const empty = text('div', '', 'empty');
        empty.append(text('h2', state.games.length ? 'No games match your search' : 'A home for your games'), text('p', state.games.length ? 'Try a different title or title ID.' : 'Link a prepared Windows game folder to get started. Your files, runtime and saves stay right where they are.', 'muted'));
        if (!state.games.length) empty.append(button('\u002b Add your first folder', openAdd, unavailable(), 'primary'));
        byId('games').append(empty);
    }
    if (focused) document.querySelector(`[data-focus="${CSS.escape(focused)}"]`)?.focus();
}
function showDetails(id) {
    detailsId = id; renderDetails(); if (!byId('details').open) byId('details').showModal();
}
function renderDetails() {
    const game = state?.games.find(item => item.id === detailsId); if (!game) return;
    const content = byId('detail-content');
    const signature = JSON.stringify([game, state.commands, state.notes, unavailable()]);
    if (content.dataset.signature === signature) return;
    content.dataset.signature = signature; content.replaceChildren();
    content.append(text('h2', game.title), text('p', (game.title_id || 'No title ID') + ' \u00b7 ' + (game.uncertain ? 'Session status unknown after helper restart' : game.running ? 'Process tree running' : game.available === false ? 'Missing folder or launch file' : 'Available to launch')));
    content.append(text('p', 'Gameplay is unverified. File presence and a successful exit do not establish compatibility. Existing launch settings and game-local logging are preserved.', 'muted'));
    for (const [label, value] of [['Working folder', game.working_directory], ['Launch file', game.executable], ['Compatibility record', game.status]]) content.append(text('h3', label), text('p', value || 'Unverified', 'path'));
    if (game.uncertain) content.append(text('p', 'The helper restarted and cannot confirm the previous game closed. Check your Windows desktop before starting another game.'), button('I have closed this game', () => action('acknowledge_exit', game.id), unavailable()));
    content.append(text('h3', 'Session results'));
    if (game.exit_code !== null && game.exit_code !== undefined) content.append(text('p', 'Last process tree completed. Wrapper exit code: ' + game.exit_code), text('p', game.log_directory, 'path'));

    const launches = state.commands.filter(command => command.verb === 'launch');
    content.append(text('pre', launches.map(command => command.status + (command.result ? ' \u00b7 ' + command.result : '')).join('\n') || 'No recent launch requests.'));
    content.append(text('pre', (state.notes || []).join('\n')));
}
function renderScan() {
    let scan = state?.scan || {active: false, results: [], message: ''};
    const queued = state?.commands?.some(command => ['scan', 'rescan'].includes(command.verb) && ['queued', 'delivered'].includes(command.status));
    if (queued) scan = {active: true, visited: 0, current: 'Waiting for the Windows helper to start this scan', results: []};
    const blocked = unavailable() || state?.preparation?.active;
    byId('scan-button').disabled = blocked || scan.active;
    byId('pick').disabled = blocked || scan.active;
    byId('cancel-scan').hidden = !scan.active; byId('cancel-scan').disabled = blocked;
    byId('scan-progress').textContent = scan.active ? `Scanning \u00b7 ${scan.visited} folders visited\n${scan.current}` : scan.message || 'Scan never executes discovered files. We stop at game assets and skip directory links.';
    const results = scan.results || [];
    const selectable = results.filter(item => item.prepared && !item.registered);
    const preparation = results.filter(item => !item.prepared && !item.registered);
    if (!scan.active && results.length) {
        byId('scan-progress').textContent = selectable.length
            ? `Scan complete. ${selectable.length} new Windows ${selectable.length === 1 ? 'game is' : 'games are'} ready to add. Select the games you want below.`
            : preparation.length
                ? `Scan complete. ${preparation.length} new ${preparation.length === 1 ? 'game needs' : 'games need'} Windows preparation before adding. No new launch-ready games were found.`
                : 'Scan complete. All launch-ready games found are already in your library.';
        if (preparation.length) byId('scan-progress').textContent += '\nChoose Prepare & add to create and validate a separate Windows copy. Original files stay unchanged. Encrypted packages require extracted, decrypted input.';
    }
    if (!state?.online) byId('scan-progress').textContent = 'Windows helper disconnected. Run Start-AnyPS5.cmd on your desktop; this panel will reconnect automatically.';
    else if (state.busy) byId('scan-progress').textContent = state.message;
    const signature = JSON.stringify([scan.results, blocked, state?.preparation?.supported]);
    if (scanSignature !== signature) {
        const previous = new Map([...byId('results').querySelectorAll('input')].map(input => [input.value, input.checked]));
        scanSignature = signature; byId('results').replaceChildren();
        for (const item of scan.results || []) {
            const row = text('div', '', 'result');
            const label = text('label', '');
            if (item.prepared && !item.registered) {
                const input = document.createElement('input');
                input.type = 'checkbox'; input.value = item.id; input.id = 'result-' + item.id; input.checked = previous.get(item.id) ?? true;
                label.htmlFor = input.id; row.append(input);
            }
            label.append(text('b', item.title), text('small', [item.title_id, item.registered ? 'Already in library \u00b7 Settings preserved' : !item.prepared ? 'Needs Windows preparation' : item.reason].filter(Boolean).join(' \u00b7 ')), text('small', item.path));
            row.append(label);
            if (!item.prepared && !item.registered) {
                row.append(button('Prepare & add', () => action('prepare_scan', item.id), blocked || scan.active || !state?.preparation?.supported, 'primary'));
                if (!state?.preparation?.supported) row.append(text('small', 'Install the latest engine bundle and desktop helper to enable preparation.'));
            }
            byId('results').append(row);
        }
    }
    byId('add-selected').hidden = !selectable.length;
    byId('add-selected').disabled = blocked || scan.active;
}
function renderPreparation() {
    const preparation = state?.preparation;
    const storage = byId('storage-path');
    if (preparation?.root && storage.dataset.root !== preparation.root && document.activeElement !== storage) {
        storage.value = preparation.root; storage.dataset.root = preparation.root;
    }
    byId('save-storage').disabled = unavailable() || preparation?.active || !preparation?.storage_supported;
    byId('storage-space').textContent = !preparation?.storage_supported ? 'Update the desktop helper to choose storage.' : preparation.free_bytes == null ? 'Storage unavailable. Reconnect the drive or choose another folder.' : (preparation.free_bytes / 1000000000).toFixed(1) + ' GB available. Preparation needs a complete copy, matching runtime and conversion workspace.';
    const panel = byId('preparation-panel');
    panel.hidden = !authenticated || !preparation || preparation.status === 'idle';
    if (panel.hidden) return;
    const detail = preparation.detail || {};
    byId('preparation-title').textContent = (preparation.active ? 'Preparing ' : 'Preparation: ') + preparation.title;
    byId('preparation-status').textContent = (preparation.active ? [detail.phase, detail.message] : [preparation.message, preparation.status === 'failed' ? detail.message : '']).filter(Boolean).join(' \u00b7 ');
    byId('preparation-output').textContent = preparation.output;
    const progress = byId('preparation-progress');
    progress.max = detail.total_bytes || 1;
    if (detail.total_bytes) progress.value = Math.min(detail.completed_bytes || 0, detail.total_bytes); else progress.removeAttribute('value');
    progress.hidden = !preparation.active;
    byId('cancel-preparation').hidden = !preparation.active;
    byId('cancel-preparation').disabled = unavailable();
    byId('preparation-log').disabled = unavailable();
}
function renderFolders() {
    const signature = JSON.stringify([state.folders, unavailable(), state.preparation?.active]);
    if (byId('folders').dataset.signature === signature) return;
    byId('folders').dataset.signature = signature; byId('folders').replaceChildren();
    for (const folder of state.folders) {
        const row = text('article', '', 'folder'), info = text('div', '');
        info.append(text('b', folder.exists ? 'Connected folder' : 'Folder unavailable'), text('p', folder.path, 'path'));
        if (!folder.exists) info.append(text('p', 'Reconnect the drive, then rescan. Your registered games remain in the library.', 'muted'));
        row.append(info, button('Remove root', () => { if (confirm('Remove this discovery root? Game files, saves and existing library entries will be retained.')) action('remove_root', folder.id); }, unavailable() || state.preparation?.active)); byId('folders').append(row);
    }
    if (!state.folders.length) byId('folders').append(text('p', 'No folders linked yet. Add a folder to start your collection.', 'muted'));
}
async function refresh() {
    if (refreshing) return; refreshing = true;
    try {
        state = await api('/api/library'); authenticated = true;
        byId('pairing').hidden = true; navigate(view);
        byId('connection').textContent = state.online ? 'Desktop connected' : 'Desktop disconnected';
        byId('helper-status').textContent = state.online ? state.message : 'Helper offline. Run Start-AnyPS5.cmd on your signed-in desktop. The library stays available while it reconnects.';
        byId('notice').hidden = state.online && !state.busy;
        byId('notice').textContent = state.online ? state.message : 'Your Windows helper is disconnected. Start it with Start-AnyPS5.cmd. Games and settings are retained; Play will return when it reconnects.';
        byId('rescan').disabled = unavailable() || state.scan?.active || state.preparation?.active || !state.folders.length;
        renderGames(); renderFolders(); renderScan(); renderPreparation();
        if (byId('details').open) renderDetails();
        byId('notes').textContent = (state.notes || []).join('\n\n');
        byId('commands').replaceChildren(...state.commands.map(command => text('p', command.verb + ' \u00b7 ' + command.status + (command.result ? ' \u00b7 ' + command.result : ''), 'path')));
        const latest = state.commands[0];
        const failure = latest && ['failed', 'unknown'].includes(latest.status) ? latest : null;
        if (failure && byId('error').dataset.command !== failure.id) { byId('error').dataset.command = failure.id; byId('error').textContent = failure.result; if (byId('add-dialog').open) byId('scan-error').textContent = failure.result; }
        if (!failure && byId('error').dataset.command) { byId('error').textContent = ''; delete byId('error').dataset.command; }
    } catch (error) {
        if (error.status === 401) { authenticated = false; byId('preparation-panel').hidden = true; byId('pairing').hidden = false; navigate(view); byId('connection').textContent = 'Pair this browser'; }
        else { byId('notice').hidden = false; byId('notice').textContent = 'The local app is reconnecting. Check Docker Desktop and run Start-AnyPS5.cmd if needed. Your library is retained.'; if (state) state.online = false; renderGames(); renderScan(); }
    } finally { refreshing = false; }
}
byId('login').onsubmit = async event => {
    event.preventDefault();
    try { await api('/api/login', {token: byId('token').value.trim()}); byId('token').value = ''; byId('error').textContent = ''; await refresh(); }
    catch (error) { byId('error').textContent = error.message; }
};
byId('scan-form').onsubmit = event => { event.preventDefault(); action('scan', byId('folder-path').value); };
byId('storage-form').onsubmit = event => { event.preventDefault(); action('set_preparation_root', byId('storage-path').value); };
byId('cancel-preparation').onclick = () => action('cancel_preparation');
byId('preparation-log').onclick = () => action('open_preparation_log');
byId('pick').onclick = () => action('pick_folder');
byId('rescan').onclick = () => { openAdd(); action('rescan'); };
byId('cancel-scan').onclick = () => action('cancel_scan');
byId('add-selected').onclick = () => {
    const ids = [...byId('results').querySelectorAll('input:checked:not(:disabled)')].map(input => input.value);
    if (ids.length) action('add_scan', ids.join(',')); else byId('scan-error').textContent = 'Select at least one prepared game.';
};
byId('disconnect').onclick = async () => { await api('/api/logout', {}); byId('details').close(); byId('add-dialog').close(); await refresh(); };
byId('search').oninput = () => renderGames(); byId('sort').onchange = () => renderGames();
document.querySelectorAll('.add-folder').forEach(item => item.onclick = openAdd);
document.querySelectorAll('[data-view]').forEach(item => item.onclick = () => navigate(item.dataset.view));
document.querySelectorAll('.close').forEach(item => item.onclick = () => item.closest('dialog').close());
let lastPoll = 0;
refresh(); setInterval(() => { if (byId('add-dialog').open || Date.now() - lastPoll > 2000) { lastPoll = Date.now(); refresh(); } }, 250);

async function refreshEngine() {
    try {
        const response = await fetch('/engine/status', {cache: 'no-store'});
        const engine = await response.json();
        byId('engine-panel').dataset.configured = String(engine.status !== 'not_configured');
        if (engine.status === 'not_configured') byId('setup-instructions').textContent = 'Keep Docker Desktop running. Run Start-AnyPS5.cmd once to install the desktop helper and start the app.';
        byId('engine-panel').hidden = engine.status === 'not_configured' || (authenticated && view !== 'settings');
        byId('engine-download').hidden = engine.status !== 'ready';
        byId('engine-status').textContent = engine.status === 'ready'
            ? `Ready for Windows x86-64 · ${engine.library_count} runtime libraries · Source ${engine.source.fingerprint.slice(0, 20)} · Bundle ${engine.id.slice(0, 12)}`
            : engine.message || 'Preparing the Windows engine. Check docker compose logs engine.';
        if (engine.status === 'ready' && authenticated && state.online && state.helper_revision !== 'source-' + engine.source.fingerprint.slice(0, 20)) {
            byId('engine-status').textContent += ' · Desktop helper update available. Close games and run the downloaded installer.';
        }
    } catch {
        byId('engine-panel').hidden = false;
        byId('engine-status').textContent = 'Engine status is unavailable. Check Docker Desktop and docker compose logs.';
    }
}
refreshEngine(); setInterval(refreshEngine, 15000);
