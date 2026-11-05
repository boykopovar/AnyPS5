# Local Development Environment Check — AnyPS5

Run date: 2026-11-05
Working directory: `C:\Users\alfas\Desktop\REA`

## Tool availability

| Tool | Installed? | Version / notes | Authentication |
|------|-----------|-----------------|----------------|
| Git | **Yes** | `git version 2.52.0.windows.1` | Configured: `user.name=AlfasMP`, `user.email=alfasmp1@gmail.com` |
| GitHub CLI (`gh`) | **No** | `gh: command not found` | N/A — not installed |
| Python | **Yes** | `Python 3.13.5` (via `python`; `python3` alias is NOT on PATH) | N/A |

## What needs to be installed

1. **GitHub CLI (`gh`)** — required for automating PR submissions from the command line.
   - Windows: `winget install GitHub.cli` or download from https://github.com/cli/cli
   - After installing, authenticate: `gh auth login`
   - Verify: `gh auth status`

2. **`python3` on PATH** — the repo scripts are invoked as `python3 tools/...`. On this Windows machine only `python` is available, so either:
   - add Python's install directory + `Scripts` to PATH and ensure a `python3.exe` (or `python3`) shim exists, or
   - invoke the scripts with `python tools/...` until the `python3` alias is in place.

## Git repository state

- The directory is **not** a git repository (`git rev-parse --is-inside-work-tree` fails).
- Git is configured with a name and email, so once you run `git init` and add a remote, commits will be attributed correctly.

## Files added by this setup

- `tools/auto_stub.py` — parses missing PS5 `sce` function declarations and emits C++ PRX stubs.
- `tools/check_conventions.py` — verifies generated stubs against the project's C++ coding conventions.
- `tools/missing_sce_functions.txt` — sample input of missing `sce` declarations.
- `.opencode.json` — local OpenCode config enabling filesystem + command execution tools for PR automation.
