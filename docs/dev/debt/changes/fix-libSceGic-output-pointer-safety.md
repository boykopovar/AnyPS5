# libSceGic

## change [sceGicGetValue

- [sceGicGetValue](../../../core/libs/prx/libSceGic/Export.cpp) has no  device behind it: AnyPS5's kernel models no such device, so the open fails and the call returns  after its null check. Null output returns ; bad non-null pointers return  matching firmware. What the device writes into the caller's buffer, the buffer's size and the value word it reports are not known. The library side follows firmware 13.42  (sha256 ), read from disassembly to every return, not measured on a console. The request word the library leaves unset is zero here. Closes when: a  model exists and is measured against a console.
