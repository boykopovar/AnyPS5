# libSceFios2

## change Overlays and resolution

- Overlays and resolution: [probed exports](../../../core/libs/prx/libSceFios2/src/Control.cpp) (libSceFios2) - `sceFiosOverlayResolveSync(flag, in, out, max)` follows PS4 10.71 measurements. It copies `in` to `out`, rejects flags other than 0 and 1 with 0x80820020 and walks overlays of order 0 to 0x7f, lowest order first. It matches whole path components after dropping empty and `.` components and a trailing slash. Opaque overlays always redirect; translucent and writable ones only when the source exists, writable with flag 1 always; newer ones when the source exists and the original is missing, has modification date 0 or is older (unsigned, so equal dates keep the original, as PS4 `libSceFios2.sprx` compares them). Not measured: `..` components (kept as they are) and overlays with equal orders (kept in insertion order). The console raises SIGILL when the call comes before `sceFiosInitialize`; the host resolves anyway.
