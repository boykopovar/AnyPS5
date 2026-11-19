# AnyPS5 with Docker

Build this fork's complete Windows x86-64 engine, native launcher and desktop helper in Linux containers, then start its local web launcher:

```powershell
docker compose up
```

Run this in the source folder containing `compose.yaml`. A source ZIP works without host Git or initialized submodules. Docker retrieves missing dependencies at the revisions in `tools/engine/dependencies.lock`. It builds the supplied source; it does not download a replacement AnyPS5 checkout or use an existing `b/` build.

## Requirements

Use Windows x86-64, Docker Desktop in Linux-container mode, Compose with `up --wait` support, and internet access for the first build. The tested versions are below; older releases have not been validated. No host compiler, CMake, Ninja, Python or Node installation is required. The builder runs pinned Windows WinLibs GCC 15.2, CMake 3.31.6, Ninja 1.12.1 and Python 3.13.3 under Wine 9 inside Linux. This preserves the project's required SysV exception handling; Ubuntu's native MinGW compiler failed that link probe.

The default parallelism is four jobs. For a smaller Docker allocation, set `$env:ANYPS5_BUILD_JOBS = '2'` before building. Builds need substantial free Docker disk space for sources, Wine/toolchains, intermediate objects and BuildKit cache. See the validation section for measured observations; they are observations on this machine, not minimum hardware guarantees.

Games execute on the Windows desktop. Docker does not run graphical games, download games, decrypt packages or establish compatibility with every title/GPU.

## First run

1. Extract this fork's source ZIP, or clone this fork. Open PowerShell in its source folder.
2. Run `docker compose up`. Leave the terminal open during the build. The `engine` service verifies and exports the engine, then exits with code **0**. The `web` service becomes **healthy**. An exited engine service is normal.
3. Open [http://127.0.0.1:8766](http://127.0.0.1:8766). Download the Windows engine and helper using the setup panel. Extract the complete ZIP to a writable folder and run **Install-Helper.cmd**. If asked, choose the source folder containing `compose.yaml`. Alternatively, run `Install-Helper.cmd` directly from that source folder after export.
4. The installer installs the helper for the current Windows user, registers sign-in startup, opens the browser and shows a short-lived pairing code. Enter it in the browser. Use `Pair-Browser.cmd` in the source or extracted bundle whenever another code is needed. A code expires in two minutes and works once.
5. Paste an Explorer folder path, including spaces or surrounding quotes, into **Add folder**. Scan, review, add selected games, then choose **Play**. The signed-in user's helper accesses the Windows path directly; do not translate it to a Linux path.

The generated installer checks the helper hash against the bundle manifest. It validates the new configuration before stopping an existing helper, refuses updates while its games are running, and retains `.previous` executable/configuration backups. Startup failure restores the previous installation. Keep downloaded code from a trusted local build; checksums detect corruption, not a malicious replacement of both code and manifest.

The browser never receives the permanent helper credential. The installer reads it from the selected local app folder. The local endpoint serves engine binaries without pairing so first-run setup can download them. No game data or credential is included in that archive. Web health requires a valid engine export, but does not require an online Windows helper.

Readiness results are cached for at most 30 seconds; export-pointer changes bypass that cache. Each download additionally checks the archive hash. If an export changes while verification is running, readiness temporarily fails until a consistent export can be verified.

## Prepared games and converter

A ready game has its converted executable, matching `libs`, `app0` resources and required launch settings. Keep the complete folder together, including saves and configuration. For an extracted game marked **Needs Windows preparation**, choose **Prepare & add** in its scan result. The bundled worker validates the engine, unwraps decrypted SELF images when needed, copies resources and the matching runtime, converts the executable, standard bundled modules and statically referenced PRX dependencies in other folders, then checks Windows imports before copying the remaining resources and registering the output. It never runs the game automatically.

Prepared copies default to `%LOCALAPPDATA%\AnyPS5\PreparedGames`, independently of the source folder. To use another drive, create a folder there, open **Settings > Preparation storage**, paste its Windows path and save. The setting applies to new copies; existing games and saves remain in place. The app shows available space, and a failed space check reports required, available and missing GB before resource copying. The helper caches its immutable engine under `%LOCALAPPDATA%\AnyPS5\DesktopHelper\engines`. No installed Python or compiler is needed. Progress, cancellation and **Open log on Windows** are available in the app. Cancellation or failure does not add a game; original games, saves and settings are untouched. A completed copy is available to launch, with gameplay still unverified.

Encrypted inputs and package extraction/decryption are unsupported. Extracted ELF64 x86-64 or decrypted SELF inputs need an unambiguous main executable. Companion x86-64 ELF/SELF executables are converted together with the main program and audited before registration. Package wrappers containing loose helpers are scanned for the nested game entry point. Executables sharing module directories, incompatible shared-module layouts and dynamically loaded external modules without static dependency declarations require manual preparation. Unsupported console syscall instructions remain a conversion error; automatic preparation does not bypass the syscall check. Automatic preparation currently limits each game to 250,000 resource files and main executables to 2 GiB. Insufficient disk space, unsupported conversion and missing runtime imports are reported as errors. A newer engine may be required for unsupported games; preparation cannot implement missing system functionality.

Use the generated `relinker.exe` and `libs` together, following [USAGE](USAGE.md), with supported extracted, decrypted input. Prepare into a **new** output folder. Keep the original input and existing working runtime intact. Renaming an input does not convert it. Rebuilding the engine never replaces runtime libraries inside registered games.

The bundle also includes the native `AnyPS5Launcher.exe` and `portable.flag`. It can manage a separate portable collection beside the bundle with Docker stopped. Existing `Start-Launcher.cmd` continues to use the original native build when present.

## Everyday commands

Start in the background, inspect status/logs, stop, or remove this stack's containers/network:

```powershell
docker compose up -d
docker compose ps -a
docker compose logs --tail=100
docker compose stop
docker compose down
```

All retain bind-mounted application state and external game folders. Ctrl+C stops an attached `up`. Windows game processes have a separate lifecycle; close them normally through the game. The helper retries while the backend is unavailable and reconnects when it returns.

After updating source, incorporate changes explicitly:

```powershell
docker compose up -d --build --wait
```

Plain `up` can reuse existing images. The rebuild uses unchanged build layers and an incremental compiler cache. UI assets are excluded from the engine context. Each completed engine has its own identity and directory. Install the newly generated helper when updating; existing prepared games keep their original runtime and settings. Metadata records a content fingerprint even for a ZIP; Git revision is recorded when available. Cleanliness is deliberately unknown with metadata-only Git context. Build times and PE timestamps mean byte-for-byte reproducibility is not claimed.

Independent engine/export and web image commands:

```powershell
docker compose build engine
docker compose run --rm engine
docker compose build web
docker compose up -d
```

Building the engine image compiles and verifies its payload. Running `engine` publishes it. Building `web` alone does not compile the engine; full startup still requires successful export. Interrupted downloads use temporary files and checksum verification. Exports serialize with a filesystem lock, validate staged files/PE architecture/import dependencies/archive hashes, and publish pointers atomically. Failed exports retain previous completed bundles and mark the expected export unavailable.

## Storage and backups

| Data | Location relative to source folder |
| --- | --- |
| Immutable engine directory | `.local/engine-artifacts/bundles/<id>/` |
| Engine download | `.local/engine-artifacts/<id>.zip` |
| Current/expected export pointers | `.local/engine-artifacts/current.json`, `expected.json` |
| Exported Windows setup | `.local/engine-artifacts/bundles/<id>/Install-Helper.cmd` |
| Library and folder settings | `.local/launcher/data/` |
| Launcher logs | `.local/launcher/logs/` |
| Web database, credential, helper logs | `.local/launcher-web/` |
| Per-user helper/configuration/backups | `%LOCALAPPDATA%\AnyPS5\DesktopHelper\` |
| Games and game-local saves/configuration | The complete Windows folders you selected |
| Disposable build cache/images | Docker-managed storage |

Do not edit immutable bundles. Back up the complete game folders, `.local/launcher`, `.local/launcher-web` and installed helper configuration after closing games and stopping helper/stack writes. Protect credentials; do not share them in source archives or reports. Restore to the same paths, or reinstall/reconfigure the helper for the new folder. A different PC needs its own local setup and pairing. `Start-AnyPS5.cmd -Uninstall` removes helper startup/executable while retaining data.

Do not delete `.local`, game folders or Docker volumes as routine troubleshooting, and do not globally prune Docker. Retry a failed build with `docker compose up -d --build --wait`; inspect the first failing URL/compiler stage. Check connectivity, certificate/proxy settings, free disk and Docker resources. Do not disable TLS or checksum checks. If a bundle is corrupt, preserve it for diagnosis and rebuild/export from source. A missing-folder or disconnected-helper message refers to Windows access, not the engine build.

Advanced isolated testing can override `ANYPS5_WEB_PORT`, `ANYPS5_WEB_STATE_DIR`, `ANYPS5_ENGINE_DIR` and `ANYPS5_CACHE_SCOPE`, with a separate Compose project name. Normal users need none of these settings. The generated installer defaults to port 8766; custom test ports use `Setup-WebHelper.ps1 -Port` explicitly.

## Validation

| Environment / check | Observed result |
| --- | --- |
| Windows | Windows 11 Home, 10.0.26300 |
| Docker Desktop / Engine / Compose | 4.99.0.245822 / 29.9.3 / 5.7.0, Linux x86-64 containers |
| Docker allocation | 8 CPUs, 30.14 GiB RAM; 5.38 GiB VM memory in use was sampled while two four-job cold builds ran. Per-build peak memory was not exposed by BuildKit and is not claimed. Lower allocations have not been validated. |
| Compiler and target | Pinned WinLibs GCC 15.2 under Wine 9; complete Windows x86-64 engine, launcher and helper; DWARF exception link check passed |
| Cold clone / source ZIP | Both completed all 4,299 steps, exported 413 patched PRXs, exited engine `0` and reached healthy web status with no helper connected. Compiler-stage times were 3,812.8 seconds each, running concurrently at four jobs each with separate initially empty compiler caches. Pinned tooling layers were cached; first tooling downloads add time. |
| Archive provenance | The ZIP had no `.git` or populated submodules. Its controlled source edit appeared in the fingerprint embedded in `relinker --version`; archive provenance recorded no Git revision. Both cold outputs passed native Windows synthetic execution with exit 42. |
| Cold export | 47.3 seconds per exporter in the observed concurrent runs |
| Cold working storage | 3,652,225,859 logical bytes / 4,439,064,576 allocated bytes in one compiler cache, excluding tooling/image layers and exported artifacts |
| Working storage | A populated-source incremental build used 2,286,549,354 bytes under its build cache, excluding toolchain/image layers and exported artifacts. Reserve additional Docker storage for dependency copies and caches. This is not a minimum disk requirement. |
| Runtime artifacts | 545 files / 391,118,372 payload bytes; 135,735,482-byte ZIP in the observed release. Final engine image about 696 MB and pinned tooling stage about 4.06 GB; Docker retains additional intermediate layers. |
| Incremental source build | 57.4 seconds inside the compiler stage on the observed run; this excludes image/context transfer and export |
| Unchanged background startup | `docker compose up -d` returned in 21.7 seconds with no compilation; initial web health verification can finish afterward |
| UI-only edit | A controlled CSS edit reused every engine filesystem layer; Docker regenerated its provenance attestation |
| Windows execution | New relinker `--version`/`--help`, synthetic ELF conversion, and the new-runtime imported function call returned 42 with a System32-only PATH |
| Real titles | Fresh outputs prepared from available DOOM PPSA99666 and Celeste PPSA59322 inputs; DOOM menu and Celeste title screen rendered after browser/helper launches; closure was observed in the browser |
| Automatic preparation | Bundled Windows interpreter; ELF conversion and explicit native launch returning 42; DOOM decrypted SELF and Sunshine conversion/import audits; Chromium cancellation, retry, invalid-input rejection, automatic registration, source deduplication, responsive controls and unchanged test-source hashes |
| Browser and helper | Pairing, paths containing spaces and quotes, scan/review/add, laptop and narrow layouts, download CRC/hash, generated installation, backend restart/reconnection and registration persistence passed |
| Standalone | Relocated native launcher opened the new DOOM output outside the checkout with this project's containers stopped and a System32-only PATH |
| Preservation | All 3,716 recorded existing collection/game file hashes were unchanged; original native build timestamps were retained |
| Recovery | Invalid downloads/runtime hashes, missing DLLs, stale pointers, interrupted publication and concurrent exports were exercised on fixtures; prior completed bundles were retained. A failed Windows helper startup restored and restarted the previous helper with its exact configuration. |

The cold tests used a prepared tracked-source snapshot and uninitialized submodules. The final source distribution is replayed from fresh folders after packaging changes using those same isolated caches; no host build outputs are used.

The rendering observations do not establish gameplay, controller input, audio or save/load compatibility. No disposable Windows VM has been used, so installation on a tool-free fresh PC remains unverified. The actual converter, native launcher and runtime smoke tests used a restricted host PATH. Local Git clone and ZIP tests run in isolated Compose projects with uninitialized submodules and separate compiler caches; no original `b/` artifacts or host toolchain are supplied to their build contexts.
