# libSceGic technical debt

### Functional

- [sceGicGetValue](../../../core/libs/prx/libSceGic/Export.cpp) has no `/dev/gic` device behind it: AnyPS5's kernel models no such device, so the open fails and the call returns `0x816800FF` after its null check. What the device writes into the caller's buffer, the buffer's size and the value word it reports are not known. The library side follows firmware 13.42 `libSceGic.sprx` (sha256 `b5291c2781775c7877d0b8392729dde2a57b21e76f99ee94884388cea5e35ce1`), read from disassembly to every return, not measured on a console. The request word the library leaves unset is zero here. Closes when: a `/dev/gic` model exists and is measured against a console.
