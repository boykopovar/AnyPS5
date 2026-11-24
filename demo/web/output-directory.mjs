import { translate } from "./i18n.mjs";

export class OutputDirectoryTransaction {
  #state = "open";
  #discard;
  #locale;

  constructor(parent, name, directory, locale = "en") {
    this.parent = parent;
    this.name = name;
    this.directory = directory;
    this.#locale = locale;
  }

  static create(parent, name, locks, locale = "en", isCurrent = () => true) {
    return locks.request("anyps5-output-directory", async () => {
      if (!isCurrent()) return null;
      try {
        await parent.getDirectoryHandle(name);
      } catch (error) {
        if (error.name === "TypeMismatchError") throw new Error(translate(locale, "outputPathCollision", { name }));
        if (error.name !== "NotFoundError") throw error;
        if (!isCurrent()) return null;
        const directory = await parent.getDirectoryHandle(name, { create: true });
        const transaction = new OutputDirectoryTransaction(parent, name, directory, locale);
        if (isCurrent()) return transaction;
        await transaction.discard();
        return null;
      }
      throw new Error(translate(locale, "folderAlreadyExists", { name }));
    });
  }

  commit() {
    if (this.#state !== "open") throw new Error(translate(this.#locale, "transactionInactive"));
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

export async function abortAndDiscardOutput(transaction, sink, locale = "en") {
  const failures = [];
  if (sink) {
    try { await sink.abort(); }
    catch (error) { failures.push(translate(locale, "discardFileFailed", { message: error instanceof Error ? error.message : String(error) })); }
  }
  if (transaction) {
    try { await transaction.discard(); }
    catch (error) { failures.push(translate(locale, "discardFolderFailed", { name: transaction.name, message: error instanceof Error ? error.message : String(error) })); }
  }
  return failures;
}
