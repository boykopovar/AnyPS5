# libSceDiscId technical debt

### Functional

- [sceDiscIdGet, sceDiscIdGet2](../../../core/libs/prx/libSceDiscId/Export.cpp) have no `/dev/duid` device behind them: AnyPS5's kernel models no disc-id device, so the open fails and both return `0x816800FF` after their argument checks. What the device writes into the caller's structure, the structure's layout and the status word it reports are not known. The library side follows firmware 13.42 `libSceDiscId.sprx` (sha256 `13b4b951a445dcf6a1cced7c8e98dfc496cbad63c52df4cb4b19360587fc6ab2`), read from disassembly to every return, not measured on a console. The status word `sceDiscIdGet` sends, which the library leaves unset, is zero here. Closes when: a `/dev/duid` model exists and is measured against a console.
