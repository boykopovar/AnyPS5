# libSceDiscId

## change [sceDiscIdGet, sceDiscIdGet2

- [sceDiscIdGet, sceDiscIdGet2](../../../core/libs/prx/libSceDiscId/Export.cpp) have no  device behind them: AnyPS5's kernel models no disc-id device, so the open fails and both return  after their argument checks. Null or unmapped destination pointers return  before attempting device access, preserving caller errno. What the device writes into the caller's structure, the structure's layout and the status word it reports are not known. The library side follows firmware 13.42  (sha256 ), read from disassembly to every return, not measured on a console. The status word  sends, which the library leaves unset, is zero here. Closes when: a  model exists and is measured against a console.
