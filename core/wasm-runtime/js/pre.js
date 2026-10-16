if (ENVIRONMENT_IS_PTHREAD) {
  const receive = (data) => {
    if (data && data.aps5GameModule) globalThis.aps5GameModule = data.aps5GameModule;
  };
  if (ENVIRONMENT_IS_NODE) {
    require('worker_threads').parentPort.on('message', receive);
    process.on('uncaughtException', (error) => {
      const detail = error instanceof WebAssembly.Exception && typeof getExceptionMessage === 'function' ? getExceptionMessage(error).join(': ') : (error && error.stack) || String(error);
      process.stderr.write('anyps5 runtime: uncaught exception in a worker: ' + detail + '\n');
    });
  } else {
    self.addEventListener('message', (event) => receive(event.data));
  }
} else if (!globalThis.aps5GameModule) {
  if (!ENVIRONMENT_IS_NODE) throw new Error('set globalThis.aps5GameModule to the compiled game module before loading the runtime');
  const path = process.argv[2];
  if (!path) throw new Error('usage: node anyps5-runtime.js <game.wasm> [--call <nid> [arguments...]]... [-- guest arguments...]');
  globalThis.aps5GameModule = new WebAssembly.Module(require('fs').readFileSync(path));
}
