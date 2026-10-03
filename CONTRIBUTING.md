# Contributing

## Code

- Follow the [coding conventions](docs/dev/CONVENTIONS.md): naming, no comments except [technical debt](docs/dev/TechnicalDebt.md), Conventional Commits.
- Every function either does exactly what it is supposed to or throws. Unimplemented exports call `NotImplemented_nid_no_patch(__func__)` (see [libSceAudioIn](core/libs/prx/libSceAudioIn/Export.cpp)).
- A silent stub is allowed only when it unblocks a title and only affects the UI; add it to [silent stubs](docs/dev/TechnicalDebt.md#silent-stubs).
- Missing imports fail at startup with a clear error; don't replace them with fallbacks that keep running.
- Implement the general behaviour of a function, not what one title happens to need.
- Avoid non-standard extensions (`__attribute__`, etc.) where standard C++ is enough. Helper symbols that must not become NIDs use the `_nid_no_patch` or `_nid_no_patch_cut` suffix.
- The relinker uses only the C++20 standard library.
- Third-party code is added as a submodule under `3rdparty/` and built from source, not found on the system.
- Don't add tests that only check that a symbol is exported: a missing export already fails at startup.

## Build and test

Toolchains are listed in the [README](README.md#build).

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Python 3 is optional; without it some relinker tests are not registered.

The Conventions check runs on every pull request and fails when a rule on this page is broken. Run it locally before pushing:

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
