# Contributing

## Code

- Follow the [coding conventions](docs/dev/CONVENTIONS.md): naming, no comments except [technical debt](docs/dev/TechnicalDebt.md), Conventional Commits.
- Every function either does exactly what it is supposed to or throws. Unimplemented exports call `NotImplemented_nid_no_patch(__func__)` (see [libSceAudioIn](core/libs/prx/libSceAudioIn/Export.cpp)).
- Pull requests that add or change shader instruction semantics say where they come from: measured on hardware (which GPU and what was checked, e.g. with the [hardware oracle](docs/dev/HW_ORACLE.md)) or the exact source (ISA section, LLVM, ACO or Mesa file). Reviewers check the semantics on hardware. Cases the source doesn't settle throw, and behaviour not verified on hardware is recorded in [technical debt](docs/dev/TechnicalDebt.md).
- A silent stub is allowed only when it unblocks a title and only affects the UI; add it to [silent stubs](docs/dev/TechnicalDebt.md#silent-stubs).
- Missing imports fail at startup with a clear error; don't replace them with fallbacks that keep running.
- Implement the general behaviour of a function, not what one title happens to need.
- Don't add replacements for modules that titles ship themselves in `sce_module/`, `sce_modules/` or `prx/`: engine or middleware modules (Cohtml, FMOD, GOG Galaxy) and SDK libraries that only wrap other system libraries (NpCppWebApi over NpWebApi2). The relinker converts and loads the title's own module, and a host library with the same name is left out of `DT_NEEDED`. Only system libraries, which reach the kernel or the hardware, are reimplemented in [core/libs/prx](core/libs/prx).
- Avoid non-standard extensions (`__attribute__`, etc.) where standard C++ is enough. Helper symbols that must not become NIDs use the `_nid_no_patch` or `_nid_no_patch_cut` suffix.
- The relinker uses only the C++20 standard library.
- Third-party code is added as a submodule under `3rdparty/` and built from source, not found on the system.
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

This manual run also builds and tests the relinker on Linux, Windows and macOS, regardless of the author's previous contributions or the changed files. The relinker matrix also runs automatically when a pull request targeting `main` is opened, updated or reopened, if its author already has a commit in `main`; full builds never run automatically for pull requests. All build jobs build the selected pull request merged into the current `main` (GitHub's merge commit), so a pull request that no longer merges cleanly must be rebased first. New commits require another manual run for full builds.

The Conventions check runs automatically for every contributor, including first-time contributors. It uses the base branch checker and reads pull request Git objects without executing pull request code. The Conventions check runs on every pull request and fails when a rule on this page is broken. It accepts code comments only when the pull request also changes [TechnicalDebt](docs/dev/TechnicalDebt.md), and only UTF-8 text files. Run it locally before pushing:

```
python3 tools/check_conventions.py --base origin/main
```

## Branches and pull requests

The [pull request template](.github/pull_request_template.md) is the checklist for these rules.

- One branch per topic, based on current `main`. Follow-up work goes in a new pull request, not into an open one.
- Before starting, check that no open pull request already implements the same functions.
- Keep the branch up to date with `main` and resolve conflicts yourself; rebasing and force-pushing is fine.
- If a pull request needs another one first, say so in the description (`Depends on #N`).
- Run the tests before opening the pull request and describe what was tested (OS, title or homebrew).
- Investigation notes, reports, screenshots and logs go in the pull request, not in the repository. Images for documentation go in the [gist](https://gist.github.com/boykopovar/0e53f2e1426f29ecd41e3b51540b8a90) comments.
- Say whether the change was written with AI assistance. The author of the pull request is responsible for every line of it.

## Documentation

Text in the repository is dry, strict and concise. Record known gaps, unknown NIDs and signatures in [technical debt](docs/dev/TechnicalDebt.md) instead of new files.
