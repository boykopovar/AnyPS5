export class OutputDirectoryTransaction {
  #state = "open";
  #discard;

  constructor(parent, name, directory) {
    this.parent = parent;
    this.name = name;
    this.directory = directory;
  }

  static async create(parent, name) {
    try {
      await parent.getDirectoryHandle(name);
    } catch (error) {
      if (error.name !== "NotFoundError") throw error;
      const directory = await parent.getDirectoryHandle(name, { create: true });
      return new OutputDirectoryTransaction(parent, name, directory);
    }
    throw new Error(`The folder ${name} already exists. Choose another destination or rename the existing folder.`);
  }

  commit() {
    if (this.#state !== "open") throw new Error("Output directory transaction is no longer active");
    this.#state = "committed";
  }

  discard() {
    if (this.#state === "discarding") return this.#discard;
    if (this.#state !== "open") return Promise.resolve();
    this.#state = "discarding";
    this.#discard = Promise.resolve().then(() => this.parent.removeEntry(this.name, { recursive: true })).then(
      () => { this.#state = "discarded"; },
      error => { this.#state = "open"; throw error; }
    );
    return this.#discard;
  }
}

export async function abortAndDiscardOutput(transaction, sink) {
  const failures = [];
  if (sink) {
    try { await sink.abort(); }
    catch (error) { failures.push(`Could not discard the output file: ${error instanceof Error ? error.message : String(error)}`); }
  }
  if (transaction) {
    try { await transaction.discard(); }
    catch (error) { failures.push(`Could not remove incomplete output folder ${transaction.name}/: ${error instanceof Error ? error.message : String(error)}`); }
  }
  return failures;
}
