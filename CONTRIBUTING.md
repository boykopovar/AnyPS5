# Contributing

## Code

- Follow the [coding conventions](docs/dev/CONVENTIONS.md): naming, no comments except [technical debt](docs/dev/TechnicalDebt.md), Conventional Commits.
- Every function either does exactly what it is supposed to or throws. Unimplemented exports call `NotImplemented_nid_no_patch(__func__)` (see [libSceAudioIn](core/libs/prx/libSceAudioIn/Export.cpp)).
- Pull requests that add or change shader instruction semantics say where they come from: measured on hardware (which GPU and what was checked, e.g. with the [hardware oracle](docs/dev/HW_ORACLE.md)) or the exact source (ISA section, LLVM, ACO or Mesa file). Reviewers check the semantics on hardware. Cases the source doesn't settle throw, and behaviour not verified on hardware is recorded in [technical debt](docs/dev/TechnicalDebt.md).
- An export's signature or behaviour taken from a title's call sites or from a library's disassembly follows [reading titles and libraries](docs/dev/REVERSE_ENGINEERING.md): a call site settles the arguments, not what the library does.
- A silent stub is allowed only when it unblocks a title and only affects the UI; add it under Silent stubs in its library's [technical debt](docs/dev/debt/) file.
- Missing imports fail at startup with a clear error; don't replace them with fallbacks that keep running.
- Implement the general behaviour of a function, not what one title happens to need.
- Don't add replacements for modules that titles ship themselves in `sce_module/`, `sce_modules/` or `prx/`: engine or middleware modules (Cohtml, FMOD, GOG Galaxy) and SDK libraries that only wrap other system libraries. The relinker converts and loads the title's own module, and a host library with the same name is left out of `DT_NEEDED`. Only system libraries, which reach the kernel or the hardware, are reimplemented in [core/libs/prx](core/libs/prx). A host copy may still exist for a wrapped library that some dumps lack entirely, like libSceNpCppWebApi: the title's own module wins whenever one is shipped, so it is only ever consulted by titles that ship nothing.
- Avoid non-standard extensions (`__attribute__`, etc.) where standard C++ is enough. Helper symbols that must not become NIDs use the `_nid_no_patch` or `_nid_no_patch_cut` suffix.
- The relinker uses only the C++20 standard library.
- Third-party code is added as a submodule under `3rdparty/` and built from source, not found on the system. With `ANYPS5_USE_SYSTEM_DEPS=ON` the packages packaged by the distributions (SDL2, freetype, Vulkan-Headers, SPIRV-Headers, glslang and SPIRV-Tools) come from the system instead; that resolution lives in [cmake/SystemDeps.cmake](cmake/SystemDeps.cmake) and the default build never reads the system.
- Don't add tests that only check that a symbol is exported: a missing export already fails at startup.
- Each test finishes in 30 s on the CI runners, Vulkan tests included (Linux runs them on lavapipe), counting the first run with no shader cache. ctest runs in parallel there, so a slow test competes for the cores and times out on some runs and not others. Split or shrink a test that gets close instead of raising its timeout. CI reports every test over 30 s as a warning on the pull request.

## Build and test

Toolchains are listed in the [build instructions](docs/dev/BUILD.md).

For relinker changes, use the [relinker-only build](docs/dev/BUILD.md#relinker-only) to build and test without third-party submodules, system libraries or a GPU. It also works on macOS. Changes to system libraries or shaders still need the full build and relevant runtime tests.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Python 3 is optional; without it some relinker tests are not registered.

Every push to `main`, including a merged pull request, automatically runs full Linux and Windows builds and tests and the relinker matrix on Linux, Windows and macOS, using the pushed commit without contributor checks.

For pull requests, full Linux and Windows builds and tests run only when a collaborator with write access selects Actions > Build > Run workflow on `main` and enters an open pull request number targeting `main`. A collaborator can start the same manual run from the command line, replacing `1234` with the pull request number:

```
gh workflow run build.yml --repo boykopovar/AnyPS5 --ref main --field pr=1234
```

This manual run also builds and tests the relinker on Linux, Windows and macOS when the pull request changes a file the relinker-only build uses, regardless of the author's previous contributions. The relinker matrix also runs automatically when a pull request targeting `main` is opened, updated or reopened, if its author already has a commit in `main` and it changes a file the relinker-only build uses; full builds never run automatically for pull requests. Every job that runs automatically for pull requests runs only when the pull request changes its files: a `paths` filter on the `pull_request` trigger, or `needs: eligibility` with an `if` on the files output it sets in `build.yml`. The Conventions check fails a workflow change that adds a job without one. All build jobs build the selected pull request merged into the current `main` (GitHub's merge commit), so a pull request that no longer merges cleanly must be rebased first. New commits require another manual run for full builds.

With `--field full=false`, the manual run skips the full builds and runs only the relinker matrix on Linux, Windows and macOS, if the pull request changes a file the relinker-only build uses, such as the Python tools and their tests in `tools/`.

The Conventions check runs automatically for every contributor, including first-time contributors. It uses the checker from the current tip of the base branch, so rules added after a pull request was opened apply to its next run, and reads pull request Git objects without executing pull request code. The Conventions check runs on every pull request and fails when a rule on this page is broken. It accepts code comments only when the pull request also changes [technical debt](docs/dev/TechnicalDebt.md), the index or a file in `docs/dev/debt/`, and only UTF-8 text files. Run it locally before pushing:

```
python3 tools/check_conventions.py --base origin/main
```

## Branches and pull requests

The [pull request template](.github/pull_request_template.md) is the checklist for these rules.

- One branch per topic, based on current `main`. Follow-up work goes in a new pull request, not into an open one.
- Open pull requests per author are limited by how many of theirs are merged into `main`: 1 with none merged, 20 with at least 1, no limit from 20. A pull request opened or reopened over the limit is closed automatically; reopen it once another one is merged or closed.
- Before starting, check that no open pull request already implements the same functions.
- Keep the branch up to date with `main` and resolve conflicts yourself; rebasing and force-pushing is fine.
- A pull request that conflicts with `main` and has no activity for 7 days is labeled `stale` with a reminder. It is not closed.
- If a pull request needs another one first, say so on the checklist line (`Depends on: #N`; `none` otherwise).
- If a pull request fixes a reported issue, write `Fixes #N` on its checklist line (`none` otherwise). GitHub closes the issue on merge, and the `fix error` label is added within the hour.
- If a pull request fixes a stop or a wrong result seen on a title, name it on the checklist line with the title ID and its [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) discussion (`Unblocks: PPSA01325 #3232`; `none` otherwise). Reviewers review these first. Such a pull request gets the `unblocks title` label.
- For GPU changes, list each tested GPU's vendor, model, driver and OS on the checklist line. CI runs GPU tests only on lavapipe, so reviewers may ask for a run on another vendor before merging.
- Fixes a title needs go to `main` as pull requests. A Game bring-up discussion lists the open pull requests and local changes the title still needs; update the list when one is merged, closed or replaced.
- The PR overlap comment lists open pull requests that change the same lines or exports; agree with their authors which one goes first and declare it with `Depends on`.
- Run the tests before opening the pull request and describe what was tested (OS, title or homebrew).
- A pull request that claims a change in speed shows the summary lines of `main` and of the change, from the same machine and scene, and attaches the traces, captured as the [Performance report](.github/ISSUE_TEMPLATE/performance.yml) issue form describes. A slow scene is reported with the same form. A pull request titled `perf(...)` or `perf:` is labeled `perf`, and `perf numbers missing` until its description holds two valid summary lines with different commits.
- Changes to the AGC driver, the shader recompiler or VideoOut run the same focused tests on `main` and the change with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`, `ANYPS5_NO_SHADER_CACHE=1` and `ANYPS5_REQUIRE_VULKAN=1` using `ctest -V` from the same machine and GPU, confirm neither log reports `was not found but was requested by env var VK_INSTANCE_LAYERS`, confirm the change introduces no new `VUID-` validation errors compared with `main`, and name each GPU the GPU tests ran on (vendor, model, driver, OS) on the `GPUs tested` checklist line. CI runs these tests only on lavapipe, and hardware drivers fail where lavapipe and other vendors pass, so a reviewer can ask for a run on another vendor before merging.
- Investigation notes, reports, screenshots and logs go in the pull request, not in the repository. Images for documentation go in the [gist](https://gist.github.com/boykopovar/0e53f2e1426f29ecd41e3b51540b8a90) comments.
- Say whether the change was written with AI assistance. The author of the pull request is responsible for every line of it.

## Documentation

Text in the repository is dry, strict and concise. Record known gaps, unknown NIDs and signatures in [technical debt](docs/dev/TechnicalDebt.md) instead of new files. An entry states the gap first (what differs from the console, what is unverified, what throws) and describes the implemented behaviour only as far as the gap needs it. A pull request that closes a gap removes its entry, or cuts it to what is still open, instead of rewriting it to describe the corrected behaviour. An entry goes in `docs/dev/debt/<name>.md`, named after the first `core/` path it links (`<name>` for `core/libs/prx/<name>/...`, otherwise for `core/<name>/...`), under the same `###` sections as the index; entries that link neither stay in the index. A pull request whose `TechnicalDebt.md` change conflicts after the split rebases and runs `python3 tools/debt_split.py resolve` during the stopped rebase, which moves its added, edited and removed entries to their files; `python3 tools/debt_split.py split` moves entries left in the index.
