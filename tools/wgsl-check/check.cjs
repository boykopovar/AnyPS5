const { spawn } = require('node:child_process');
const { mkdtemp, readFile, writeFile, rm } = require('node:fs/promises');
const { tmpdir } = require('node:os');
const { join } = require('node:path');

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function endpoint(directory, browser) {
  for (let i = 0; i < 150; i++) {
    if (browser.launchError) throw browser.launchError;
    if (browser.exitCode !== null) throw new Error(`Chromium exited with ${browser.exitCode}`);
    try {
      return (await readFile(join(directory, 'DevToolsActivePort'), 'utf8')).split('\n')[0];
    } catch (error) {
      if (error.code !== 'ENOENT') throw error;
      await sleep(100);
    }
  }
  throw new Error('Chromium debug endpoint did not start');
}

async function evaluate(url, pageUrl, expression) {
  const socket = new WebSocket(url);
  try {
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('Chromium WebSocket timed out')), 10000);
      socket.onopen = () => { clearTimeout(timeout); resolve(); };
      socket.onerror = (error) => { clearTimeout(timeout); reject(error); };
    });
    return await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('WebGPU check timed out')), 30000);
      timeout.unref();
      const fail = (error) => { clearTimeout(timeout); reject(error); };
      let committed = false;
      let evaluated = false;
      socket.onerror = fail;
      socket.onmessage = (event) => {
        const response = JSON.parse(event.data);
        if (response.error) { fail(new Error(JSON.stringify(response.error))); return; }
        if (response.id === 1) {
          socket.send(JSON.stringify({ id: 2, method: 'Page.navigate', params: { url: pageUrl } }));
        } else if (response.id === 2 && response.result.errorText) {
          fail(new Error(response.result.errorText));
        } else if (response.method === 'Page.frameNavigated' && response.params.frame.url === pageUrl) {
          committed = true;
        } else if (response.method === 'Page.loadEventFired' && committed && !evaluated) {
          evaluated = true;
          socket.send(JSON.stringify({ id: 3, method: 'Runtime.evaluate', params: { expression, awaitPromise: true, returnByValue: true } }));
        } else if (response.id === 3) {
          clearTimeout(timeout);
          if (response.result.exceptionDetails) fail(new Error(JSON.stringify(response.result.exceptionDetails)));
          else resolve(response.result.result.value);
        }
      };
      socket.send(JSON.stringify({ id: 1, method: 'Page.enable' }));
    });
  } finally {
    socket.close();
  }
}

async function main(prefixes) {
  if (prefixes.length === 0) throw new Error('usage: node check.cjs <shader-prefix>...');
  const sources = await Promise.all(prefixes.map((prefix) => readFile(`${prefix}.cs.wgsl`, 'utf8')));
  const directory = await mkdtemp(join(tmpdir(), 'anyps5-wgsl-'));
  await writeFile(join(directory, 'blank.html'), '<!doctype html>');
  const browser = spawn(process.env.CHROMIUM || 'google-chrome', [
    '--headless=new', '--no-sandbox', '--enable-unsafe-webgpu', '--enable-features=Vulkan',
    '--use-angle=vulkan', '--use-vulkan=swiftshader', '--ignore-gpu-blocklist',
    '--disable-background-networking', '--disable-component-update', '--disable-sync', '--no-first-run',
    '--remote-debugging-port=0', `--user-data-dir=${directory}`, 'about:blank',
  ], { stdio: ['ignore', 'ignore', 'pipe'] });
  browser.once('error', (error) => { browser.launchError = error; });
  browser.stderr.pipe(process.stderr);
  try {
    const port = await endpoint(directory, browser);
    const target = await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: 'PUT' });
    if (!target.ok) throw new Error(`Chromium target: HTTP ${target.status}`);
    const { webSocketDebuggerUrl } = await target.json();
    if (!webSocketDebuggerUrl) throw new Error('Chromium did not provide a page endpoint');
    const expression = `(async () => {
      if (!isSecureContext || !navigator.gpu) throw new Error('WebGPU unavailable after loading ' + location.href);
      const adapter = await navigator.gpu.requestAdapter();
      if (!adapter) throw new Error('WebGPU adapter unavailable');
      const device = await adapter.requestDevice();
      const sources = ${JSON.stringify(sources)};
      for (let i = 0; i < sources.length; i++) {
        const code = sources[i].replaceAll('aps5_storage_format', 'rgba8unorm').replaceAll('override aps5_', 'const aps5_');
        const module = device.createShaderModule({ code });
        const info = await module.getCompilationInfo();
        const errors = info.messages.filter((message) => message.type === 'error');
        if (errors.length) throw new Error('shader ' + i + ': ' + errors.map((message) => message.message).join('; '));
        await device.createComputePipelineAsync({ layout: 'auto', compute: { module, entryPoint: 'main' } });
      }
      return sources.length;
    })()`;
    const count = await evaluate(webSocketDebuggerUrl, `file://${join(directory, 'blank.html')}`, expression);
    if (count !== sources.length) throw new Error('not all WGSL pipelines were checked');
    console.log(`WebGPU: ${count}/${sources.length} compute pipelines created`);
  } finally {
    browser.kill();
    await Promise.race([new Promise((resolve) => browser.once('exit', resolve)), sleep(3000)]);
    await rm(directory, { recursive: true, force: true, maxRetries: 10, retryDelay: 250 });
  }
}

main(process.argv.slice(2)).catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
