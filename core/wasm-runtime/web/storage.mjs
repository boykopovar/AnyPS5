function describe(error) {
  return 'anyps5 storage: ' + (error.message || error);
}

export function openStorage(fs, idbfs, paths, dependencies, log) {
  let state = 'loading';
  let syncing = false;
  let again = false;
  const sync = () => {
    if (syncing) {
      again = true;
      return;
    }
    syncing = true;
    fs.syncfs(false, (error) => {
      log(error ? describe(error) : 'anyps5 storage: saved');
      syncing = false;
      if (again) {
        again = false;
        sync();
      }
    });
  };
  for (const path of paths) {
    fs.mkdir(path);
    fs.mount(idbfs, {}, path);
  }
  dependencies.add('aps5-storage');
  fs.syncfs(true, (error) => {
    if (error) log(describe(error));
    const waiting = again;
    again = false;
    state = error ? 'failed' : 'ready';
    dependencies.remove('aps5-storage');
    if (waiting && state === 'ready') sync();
    if (waiting && state === 'failed') log('anyps5 storage: not saved, the stored saves were not loaded');
  });
  return {
    commit() {
      if (state === 'loading') again = true;
      else if (state === 'failed') log('anyps5 storage: not saved, the stored saves were not loaded');
      else sync();
    },
  };
}
