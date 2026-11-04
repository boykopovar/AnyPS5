import test from 'node:test';
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {once} from 'node:events';
import net from 'node:net';
import {fileURLToPath} from 'node:url';
import {setTimeout as delay} from 'node:timers/promises';

async function startServer(t, ephemeral = false) {
    const reservation = net.createServer();
    reservation.listen(0, '127.0.0.1');
    await once(reservation, 'listening');
    const port = reservation.address().port;
    await new Promise((resolve, reject) => reservation.close(error => error ? reject(error) : resolve()));
    const child = spawn(process.execPath, [fileURLToPath(new URL('../serve.js', import.meta.url))], {
        env: {...process.env, PORT: ephemeral ? '0' : String(port)}, stdio: ['ignore', 'pipe', 'pipe']
    });
    let stderr = '';
    child.stderr.setEncoding('utf8').on('data', text => { stderr += text; });
    t.after(async () => {
        if (child.exitCode === null && child.signalCode === null) {
            const exited = once(child, 'exit', {signal: AbortSignal.timeout(3000)});
            child.kill();
            await exited;
        }
    });
    let ready = '';
    child.stdout.setEncoding('utf8').on('data', text => { ready += text; });
    for (let attempts = 0; attempts < 500; attempts++) {
        if (ready.includes('Input mapper:')) {
            const match = /Input mapper: http:\/\/127\.0\.0\.1:(\d+)/u.exec(ready);
            assert.ok(match, ready);
            const actual = Number(match[1]);
            assert.ok(actual > 0 && actual <= 65535, ready);
            if (!ephemeral) assert.equal(actual, port);
            return {child, port: actual, stderr: () => stderr};
        }
        if (child.exitCode !== null) throw new Error(stderr || `server exited ${child.exitCode}`);
        await delay(10);
    }
    throw new Error('server did not become ready');
}

test('a dynamically assigned port is reported as a usable server address', {timeout: 15000}, async t => {
    const server = await startServer(t, true);
    assert.match(await rawRequest(server.port, '/'), /^HTTP\/1\.1 200 /u);
    assert.match(await rawRequest(server.port, '/mapping.js', 'HEAD'), /^HTTP\/1\.1 200 /u);
    assert.equal(server.child.exitCode, null);
    assert.equal(server.stderr(), '');
});

async function rawRequest(port, target, method = 'GET') {
    return new Promise((resolve, reject) => {
        const socket = net.createConnection({host: '127.0.0.1', port});
        let data = '';
        socket.setEncoding('utf8');
        socket.setTimeout(3000, () => socket.destroy(new Error('HTTP request timed out')));
        socket.on('error', error => error.code === 'ECONNRESET' ? resolve(data) : reject(error));
        socket.on('data', chunk => { data += chunk; });
        socket.on('end', () => resolve(data));
        socket.on('connect', () => socket.write(`${method} ${target} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`));
    });
}

test('malformed request targets return 400 and leave the server serving requests', {timeout: 15000}, async t => {
    const server = await startServer(t);
    for (const target of ['http://[', 'http://[::1', 'http://example.invalid:bad/']) {
        const response = await rawRequest(server.port, target);
        assert.match(response, /^HTTP\/1\.1 400 /u);
        assert.equal(server.child.exitCode, null);
        const healthy = await rawRequest(server.port, '/');
        assert.match(healthy, /^HTTP\/1\.1 200 /u);
        assert.match(healthy, /<!doctype html>/iu);
    }
    assert.equal(server.stderr(), '');
});

test('server retains GET HEAD allowlist and method behavior', {timeout: 15000}, async t => {
    const {port} = await startServer(t);
    assert.match(await rawRequest(port, '/mapping.js?cache=1'), /^HTTP\/1\.1 200 /u);
    const head = await rawRequest(port, '/', 'HEAD');
    assert.match(head, /^HTTP\/1\.1 200 /u);
    assert.equal(head.split('\r\n\r\n')[1], '');
    assert.match(await rawRequest(port, '/missing.js'), /^HTTP\/1\.1 404 /u);
    assert.match(await rawRequest(port, '/package.json'), /^HTTP\/1\.1 404 /u);
    assert.match(await rawRequest(port, '/', 'POST'), /^HTTP\/1\.1 405 /u);
});

test('invalid configured ports fail without leaking a stack trace', {timeout: 15000}, async () => {
    for (const invalidPort of ['abc', '-1', '65536', '1.5', ' ', '\t']) {
        const child = spawn(process.execPath, [fileURLToPath(new URL('../serve.js', import.meta.url))], {
            env: {...process.env, PORT: invalidPort}, stdio: ['ignore', 'pipe', 'pipe']
        });
        let stdout = '';
        let stderr = '';
        child.stdout.setEncoding('utf8').on('data', chunk => { stdout += chunk; });
        child.stderr.setEncoding('utf8').on('data', chunk => { stderr += chunk; });
        const [code] = await once(child, 'exit');
        assert.equal(code, 1);
        assert.equal(stdout, '');
        assert.match(stderr, /options\.port should be >= 0 and < 65536/u);
        assert.doesNotMatch(stderr, /RangeError|ERR_SOCKET_BAD_PORT|^\s+at\s+/mu);
    }
});
