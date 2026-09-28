# libcohtml — deploy note

Game `DT_NEEDED` expects exactly: `libcohtml.Prospero.prx`.

Copy next to the relinked eboot (same directory):

- `build/core/libs/libs/libcohtml.Prospero.prx` (patched build output, 50 NIDs) — required.
- `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` from `winlibs-gcc15/mingw64/bin` — required on Windows (see `docs/TechnicalDebt.md`).

Do NOT deploy:

- `libcohtml.prx` (build intermediate kept for the NID-patch pipeline, wrong `DT_NEEDED` name).
- Anything under `build/core/libs/libs/unpatched/` (NIDs not patched).
- `libc.prx` / `libkernel.prx` are not runtime imports of this module (no such DLL dependency); the game may need its other modules separately (out of scope).

Scope: raw-NID stubs only; real Cohtml rendering stays out of scope.
