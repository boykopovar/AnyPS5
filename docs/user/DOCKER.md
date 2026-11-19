# AnyPS5 with Docker

Build the Windows x86-64 engine, native launcher, desktop helper and local web app from this fork:

```powershell
docker compose up --build
```

Run this in the folder containing `compose.yaml`. A clone and a source ZIP both work. No host compiler, Git, CMake, Ninja, Python or Node installation is required. Docker retrieves the tested upstream source and dependencies; existing host `b/` output is not used.

## Requirements

Use Windows x86-64, Docker Desktop in Linux-container mode, Docker Compose v2.17 or newer and internet access for engine builds. The builder uses pinned WinLibs GCC 15.2, CMake 3.31.6, Ninja 1.12.1 and Python 3.13.3 under Wine 9 inside Linux. These tools preserve the project's required System V exception ABI. The exported engine and helper execute on Windows.

The default build parallelism is four jobs. Set `$env:ANYPS5_BUILD_JOBS = '2'` for a smaller Docker allocation, or increase it if your allocation supports more compiler processes. The first build needs substantial time and free Docker disk space. Cached builds still check the configured upstream ref.

## First run

1. Extract the source ZIP or clone this fork. Open PowerShell in the source folder.
2. Run `docker compose up --build`. The engine compiles, verifies and exports its Windows bundle, then exits with code **0**. An exited engine service is normal. A short `setup` service creates the web app's state and pairing credential, then also exits. The web service starts without waiting for the engine export and shows its progress; if the export fails, the page shows the reason.
3. Open [http://127.0.0.1:8766](http://127.0.0.1:8766). The public **About** tab explains capabilities, limitations and the build's exact upstream revision.
4. Download the engine/helper ZIP from the setup panel, extract it completely and run **Install-Helper.cmd**. Choose the source folder containing `compose.yaml` if asked. Alternatively, run `Install-Helper.cmd` directly from the source folder after export.
5. The installer opens the app in your default browser, already signed in. To sign in again, or in another browser window, run **Pair-Browser.cmd**. It opens a one-time sign-in link that expires in two minutes; if the browser cannot be opened, it shows an eight-digit code to enter instead. The permanent credential remains in local storage and is not shown in the browser.
6. Paste a Windows Explorer folder path into **Add folder**, scan and review the results. Link existing prepared folders or choose **Prepare & add** for supported extracted inputs. The game runs only when you choose **Play**.

After initial setup, **Start-AnyPS5.cmd** builds/starts the stack, installs the matching per-user helper and opens pairing. Close games and finish or cancel preparation before updating the helper. The installer retains a previous executable/configuration and restores them if startup fails.

## Start, stop and cleanup

```powershell
docker compose up --build
docker compose up -d --build --wait
docker compose ps -a
docker compose logs --tail=100
docker compose down
```

`-d` is an option for **up**, not **down**. `docker compose down -d` is invalid. Use `docker compose down --remove-orphans` to also remove obsolete services belonging to this Compose project.

Default `down` removes the stack's containers and network. It retains images, compiler/download caches, exported engine bundles, pairing state, library entries, settings, saves and game folders. Ctrl+C stops an attached `up`; `down` removes its containers afterward. No global Docker prune is needed.

When Compose replaces the engine service, the running web service picks up the newly published bundle on its own. `/healthz` reports only that the web server is up; `/ready` also checks the engine bundle.

Windows helper, game and preparation processes have a separate lifecycle. The helper stays installed and reconnects after the backend returns. Cancel preparation and close games normally before shutdown if you want those Windows processes to stop. Docker cannot stop or restart an interactive Windows desktop process by itself.

To remove the helper's sign-in startup and executable, run:

```powershell
.\Start-AnyPS5.cmd -Uninstall
```

Configuration, cached bundles, tokens, logs, games and saves are retained. Reinstall the helper before using desktop actions again. Shutdown and uninstall are not data-erasure commands.

## Upstream source updates

Every engine build resolves the configured ref from `https://github.com/boykopovar/AnyPS5.git` through BuildKit's Git source operation. The default is the exact upstream revision this fork was last tested with, `base_revision` in `tools/engine/upstream.lock`, so every clone of the same fork commit builds the same upstream source. The build records the exact resolved upstream SHA, fork overlay base, built-source fingerprint and dependency revisions in the engine manifest. The web app's About view displays the upstream SHA and engine identity.

The builder compares supplied fork files with the base in `tools/engine/upstream.lock`, retains upstream changes to unmodified files and merges the fork's changes into updated files. New launcher/build files are retained. A conflict, missing source, unsupported dependency change or failed compilation stops the build; the host checkout and prior completed exports are not replaced. There is no silent fallback to stale upstream source.

Third-party source follows the resolved upstream gitlinks, including recursive modules. `tools/engine/dependencies.lock` retains tested toolchain downloads and baseline submodule metadata; baseline submodule revisions do not override newer upstream gitlinks. Explicit fork dependency overrides and deletions belong in `upstream.lock`. When rebasing the fork, maintainers must advance its base to the upstream revision used for that rebase.

To build newer or different upstream source, set the ref. Use `main` for the latest upstream, or a full commit SHA for an exact revision:

```powershell
$env:ANYPS5_UPSTREAM_REF = 'main'
docker compose up -d --build --wait
Remove-Item Env:ANYPS5_UPSTREAM_REF
```

Plain `up` can reuse existing images; use `--build` to rebuild. A web-only build does not rebuild the engine. New upstream commits can introduce regressions or fail to merge with the fork, and latest source does not guarantee title compatibility. Keep the default for a shared release and include its SHA with the source ZIP and engine ZIP. Byte-for-byte reproducibility is not claimed.

After a successful engine update, install its helper using the generated installer or **Start-AnyPS5.cmd**. Existing prepared games keep their own runtime libraries; updating the engine does not automatically upgrade them or migrate saves into a new copy.

## Preparation and compatibility

A complete Windows game folder contains its launch executable, matching `libs`, `app0` resources and required launch settings. Existing prepared folders are linked in place. Availability checks for linked folders establish file presence, not gameplay compatibility.

Automatic preparation validates the engine, unwraps decrypted SELF inputs where needed, converts main/companion executables and declared PRX dependencies, and audits imports before copying remaining game assets. It publishes a separate output and registers it only after success. It never launches the game automatically. Progress, cancellation and **Open log on Windows** remain available during preparation.

The default output folder is `%LOCALAPPDATA%\AnyPS5\PreparedGames`. To choose another drive, create a folder there and save its Windows path under **Settings > Preparation storage**. This affects new copies only. The app shows available space; failed space checks report required, available and missing GB. Preparation needs a full resource copy, matching runtime and conversion workspace.

Current limitations:

- Encrypted SELF inputs, package extraction/decryption, games, firmware and keys are not supplied or handled automatically.
- Missing exports/libraries, unsupported console syscalls and incompatible executable/module layouts stop preparation. Disabling syscall validation is not an automatic preparation strategy.
- Dynamic external modules without static dependency declarations may require manual preparation. Executables sharing module directories or needing incompatible shared-module outputs are unsupported by the automatic workflow.
- Main executables are limited to 2 GiB; each game is limited to 250,000 resource files. Linked resources and UNC paths are unsupported. Use local drive paths; map network storage to a drive first.
- Prepared/available means files and applicable loader checks passed. Rendering, controllers, audio, save/load and gameplay require title-specific testing on the actual CPU/GPU/drivers. Exit code 0 is not proof that a game rendered or was playable.
- The web app does not edit per-game environment/launch settings or remove individual library entries; use the native launcher. It does not automatically update existing game copies or migrate saves.
- A helper restart loses live game monitoring and requires closure confirmation before another launch. The desktop folder picker can pause heartbeats while its modal dialog is open.

AnyPS5 relinks executables and reimplements system libraries; it is not a general PS5 emulator. See the upstream [game bring-up discussions](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) for reports, which apply to their tested build and hardware.

## Data locations

| Data | Location |
| --- | --- |
| Web/library collection | `.local/launcher` |
| Pairing and backend state | `.local/launcher-web` |
| Engine exports and downloads | `.local/engine-artifacts` |
| Engine bundle | `.local/engine-artifacts/bundles/<id>` |
| Windows download | `.local/engine-artifacts/<id>.zip` |
| Installed helper and immutable cache | `%LOCALAPPDATA%\AnyPS5\DesktopHelper` |
| Default prepared outputs | `%LOCALAPPDATA%\AnyPS5\PreparedGames` |
| Custom prepared outputs | Folder saved in Settings |

Back up collection/state and complete game folders, including their saves, settings and matching libraries. Game data and credentials are excluded from distributable engine/source archives. Keep runtime output outside `b/` and source release packages.

## Maintainer checks

The fork's **Sync upstream into fork** GitHub Actions workflow checks `boykopovar/AnyPS5` daily at 09:23 UTC and can be started manually. It prepares a merge into `sync/upstream`, refreshes the overlay base and recursive dependency metadata, runs integration tests, then opens a pull request against this fork's `main`. It does not replace `main` or merge the pull request automatically. An existing open sync pull request is retained for review, including a manual candidate whose title starts with `chore(sync): merge upstream `. Merge conflicts or failed tests stop publication and appear as a failed workflow run.

Enable Actions for the fork and **Settings > Actions > General > Workflow permissions > Allow GitHub Actions to create and approve pull requests**. Workflow-created pull requests can require approval before their other Actions checks run. Review native build and gameplay results separately; the scheduled checks do not establish Windows compilation or title compatibility. Merging a sync pull request advances the default upstream revision in `upstream.lock`, `compose.yaml` and `tools/engine/Dockerfile` together.

For a manual source sync, work on a new branch, merge upstream without discarding fork changes, update recursive submodules, run `python tools/engine/sources.py --write`, and advance `base_revision` in `tools/engine/upstream.lock` and the `ANYPS5_UPSTREAM_REF` defaults in `compose.yaml` and `tools/engine/Dockerfile` to that exact upstream SHA. Run the checks below and open a pull request. Do not force-reset the fork to upstream or replace installed game runtimes during a source sync.

Run the engine/source integration, worker and web backend tests with a development Python installation:

```powershell
python tools/engine/tests.py
python tools/launcher/TestsPreparation.py
python tools/launcher/TestsWeb.py
```

Then verify `down`, a fresh `up --build`, public setup/About, browser pairing, supported preparation, unsupported-input handling, native launch and persistent user data. Use separate test state/ports and remove those test containers and disposable fixtures afterward. Do not prune unrelated projects or delete registered game data.
