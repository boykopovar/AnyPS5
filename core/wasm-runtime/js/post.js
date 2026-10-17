if (!ENVIRONMENT_IS_PTHREAD) {
  const loadWasmModuleToWorker = PThread.loadWasmModuleToWorker;
  PThread.loadWasmModuleToWorker = (worker) => {
    const postMessage = worker.postMessage.bind(worker);
    worker.postMessage = (message, ...rest) => {
      if (message && message.wasmModule) message.aps5GameModule = globalThis.aps5GameModule;
      return postMessage(message, ...rest);
    };
    return loadWasmModuleToWorker(worker);
  };
}
