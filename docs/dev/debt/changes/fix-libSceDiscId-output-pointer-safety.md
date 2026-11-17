# libSceDiscId

## change [sceDiscIdGet, sceDiscIdGet2]

- [sceDiscIdGet, sceDiscIdGet2](../../../core/libs/prx/libSceDiscId/Export.cpp) have no `/dev/duid` device behind them: AnyPS5's kernel models no disc-id device, so the open fails and both return `0x816800FF` after their argument checks. Non-null destinations are probed upfront for 1 writable byte under `GuestArena::HostWrite` with `GuestWritable_nid_no_patch`, returning `0x81680016` (preserving `errno`) as an unmeasured host-safety fallback because `DiscId`/`DiscId2` layouts are unknown. What the device writes, struct layouts and status word are unknown; library logic follows 13.42 `libSceDiscId.sprx` (sha256 `13b4b951a445dcf6a1cced7c8e98dfc496cbad63c52df4cb4b19360587fc6ab2`) disassembly, with zero sent as `sceDiscIdGet`'s status word. Closes when: a `/dev/duid` model exists and is measured against a console. Labels: console, tested.
