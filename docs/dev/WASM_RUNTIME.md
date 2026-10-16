# WebAssembly runtime

Runs a relinked title in a browser with WebGPU.

## Parts

- [core/wasm-aot](../../core/wasm-aot): `anyps5-wasm-aot` (Rust). Translates the relinker's Linux output (`app.elf` and the guest modules it loads) to one `game.wasm`.
- [core/wasm-runtime](../../core/wasm-runtime): Emscripten build of the system libraries that runs `game.wasm`.
  - Graphics: WebGPU on a dedicated GPU thread. Shaders come from the recompiler's [WGSL backend](../../core/shader/recompiler/WgslBackend).
  - Input: keyboard and standard gamepads through `ScePad`.
  - Audio: `SceAudioOut` ports mixed in an AudioWorklet.
  - Saves: `/_sd` in IndexedDB, synced after each unmount and save commit.
- [core/wasm-runtime/web](../../core/wasm-runtime/web): loader page and a local server that sends the COOP/COEP headers `SharedArrayBuffer` needs.

## Requirements

- Emscripten 6.0.12 (`emcmake` in `PATH`), Rust with Cargo, Python 3.
- Node.js 26 for the tests (memory64).
- A browser with WebGPU: Chrome on macOS or Windows. On Linux, start Chrome with `--enable-unsafe-webgpu --enable-features=Vulkan`.

## Build

1. Relink the title for Linux as described in [USAGE](../user/USAGE.md). `<out>` is the relinker's `linux` output directory, `<game>` an absolute path to an empty directory.
2. Create and activate a Python virtual environment:

   ```sh
   python3 -m venv .venv
   . .venv/bin/activate
   ```

3. Translate, build the runtime and package the game data:

   ```sh
   cargo run --release --manifest-path core/wasm-aot/Cargo.toml --bin anyps5-wasm-aot -- <out>/app.elf -o <game>/game.wasm
   emcmake cmake -S . -B build-wasm-runtime -DANYPS5_BUILD_WASM_RUNTIME=ON -DCMAKE_BUILD_TYPE=Release
   cmake --build build-wasm-runtime --target anyps5-runtime-web
   cd <out> && python <emsdk>/upstream/emscripten/tools/file_packager.py <game>/game.data --preload app0@/app0 --js-output=<game>/game.data.js --no-node
   ```

## Run

1. From the repository root, with the virtual environment active:

   ```sh
   python core/wasm-runtime/web/serve.py --runtime build-wasm-runtime/core/wasm-runtime --game <game> --port 8080
   ```

2. Open `http://127.0.0.1:8080/`. The server listens on `127.0.0.1` only; from another machine forward the port first: `ssh -N -L 8080:127.0.0.1:8080 <host>`.
3. Click the page once to start audio.

- `?env=NAME=VALUE` sets an environment variable for the runtime; repeat it for more.
- Saves stay in the browser profile; the same browser and origin continue them.

| Key | Pad |
|---|---|
| Enter, Space | Cross |
| C | Circle |
| I | Triangle |
| Escape | Options |
| Arrows | D-pad |
| W A S D | Left stick |
| T F G H | Right stick |
| Q / E, Alt | L1 / R1 |
| Shift / Ctrl | L3 / R3 |
| Backspace / Tab | Touchpad left / right |

## Tests

```sh
emcmake cmake -S . -B build-wasm-runtime -DANYPS5_BUILD_WASM_RUNTIME=ON -DCMAKE_BUILD_TYPE=Release -DANYPS5_NODE=<node 26>
cmake --build build-wasm-runtime
ctest --test-dir build-wasm-runtime -R wasm_
cargo test --release --manifest-path core/wasm-aot/Cargo.toml
python -m unittest discover -s tools/tests -p test_wasm_nid_thunks.py
```
