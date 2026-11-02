# AGENTS.md

## Regola principale: un solo worktree per progetto

Tutti gli agenti, subagent e orchestratori devono riutilizzare l'unico checkout/worktree canonico del progetto. Prima di lavorare verificare `git worktree list`, branch e `git status --short`; usare il percorso concordato nel Plan. Non creare altri worktree, cloni isolati o copie dei sorgenti per task, review, pubblicazione o build senza una richiesta esplicita dell'utente. Questa regola prevale sulle precedenti istruzioni di isolamento automatico.

Coordinare la proprieta dei file e serializzare le modifiche incompatibili; non cambiare branch durante il lavoro di altri agenti. Preservare modifiche locali e operazioni in corso: niente reset, stash o cancellazioni per ottenere un checkout pulito. Build, log e artefatti possono avere directory separate senza duplicare il checkout Git. I worktree storici non vanno cancellati automaticamente.

AnyPS5 relinks PS5 executables into native Linux or Windows programs that run against reimplemented system libraries; it is not an emulator. The rules are [CONTRIBUTING.md](CONTRIBUTING.md) and [CONVENTIONS](docs/dev/CONVENTIONS.md); these are the ones agents break.

- No comments in code, except `#endif` and namespace ends.
- An export you cannot implement calls `NotImplemented_nid_no_patch(__func__)`; never return a made-up success.
- Implement the function for every input. Never special-case a value, ID or path seen in one title's trace; if the general behaviour is unknown, throw and record it in `docs/dev/debt/<library>.md`.
- New third-party code is a submodule at `3rdparty/<name>`: add its `.gitmodules` entry (`git submodule add <url> 3rdparty/<name>`) and build it from source with CMake. Never `find_package` or copy sources in.
- Known gaps go in `docs/dev/debt/<library>.md`; notes, reports and logs go in the pull request. Add no other files.
- Commit subjects and PR titles are Conventional Commits.
- PR body: copy [the template](.github/pull_request_template.md), keep every line, tick every box, and fill `What`, `Tested`, `Depends on` (#N or none), `Fixes` (Fixes #N or none), `Unblocks` (title ID and discussion, or none), `GPUs tested` (AGC, shader recompiler, VideoOut; otherwise n/a) and `AI-assisted` (yes or no).
- Before pushing run `python3 tools/check_conventions.py --base origin/main`; before opening run `python3 tools/pr_overlap.py --branch <branch>`.
- Build and test: [BUILD](docs/dev/BUILD.md); a relinker-only change builds with `-DANYPS5_RELINKER_ONLY=ON`.
- A speed claim shows `tools/trace_digest.py` summaries of `main` and of the change from the same machine and scene.
- AGC, shader recompiler and VideoOut changes: run the same focused tests on `main` and the change with `ANYPS5_REQUIRE_VULKAN=1`, `ANYPS5_NO_SHADER_CACHE=1` and `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` using `ctest --test-dir build -R <pattern> -V` from the same machine and GPU. Confirm neither log contains `was not found but was requested by env var VK_INSTANCE_LAYERS`, then compare the logs and confirm the change adds no new `VUID-` validation errors (`ctest` can report `Passed` when validation errors are logged).
