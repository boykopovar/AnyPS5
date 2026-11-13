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

  static async create(parent, name, locale = "en") {
    try {
      await parent.getDirectoryHandle(name);
    } catch (error) {
      if (error.name !== "NotFoundError") throw error;
      const directory = await parent.getDirectoryHandle(name, { create: true });
      return new OutputDirectoryTransaction(parent, name, directory, locale);
    }
    throw new Error(translate(locale, "folderAlreadyExists", { name }));
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
