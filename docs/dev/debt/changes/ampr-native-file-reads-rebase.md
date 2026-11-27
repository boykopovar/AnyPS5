# libSceAmpr

## add Unknown function info

- [sceAmprAprCommandBufferReadFile](../../../core/libs/prx/libSceAmpr/Export.cpp) (libSceAmpr) - inaccessible command-buffer fields or packet memory throw through `NotImplemented_nid_no_patch`; native memory-fault delivery and partial writes on faults are unverified. Recording and the four-argument `sceAmprMeasureCommandSizeReadFile` follow the supplied PPSA02530 01.300.000 `libSceAmpr.sprx` entries 0x2e6d0 and 0x2f750, checked locally against the library instructions, not on a console.

## change [AMPR command buffer offsets]

- [AMPR command buffer offsets](../../../core/libs/prx/libSceAmpr/Export.cpp) (libSceAmpr) - recording host-format commands with a current offset greater than the bound memory size throws before calculating remaining capacity or writing a command. This validates the emulator's own command-buffer encoding; the console's response to a corrupted opaque buffer object is unknown.

## change [AMPR command sizes]

- [AMPR command sizes](../../../core/libs/prx/libSceAmpr/Export.cpp) (libSceAmpr) - Commands other than `Nop` and `ReadFile` use host encodings that can exceed console sizes. On overflow, the buffer expands up to a 32 MiB cap on the guest heap, relocating `base` and `size`; below this cap a full buffer avoids `EBUSY`, placing commands outside the guest's allocation, and AMM submits resolve original pointers to expanded memory. Rebinding the same memory via `SetBuffer` frees prior expansions and resets that buffer's offset and commands to 0. Beyond the cap, appends return `EBUSY`. `Nop(n)` is n dwords and `NopWithData(n)` (n + 1) dwords with header `0x5452000F | payloadDwords << 8 | (type & 0xF) << 12`, and `ConstructNop` writes its word before payload and zeroes padding (FW 13.42, #10936); null `base` reports before count or range. `NopWithData` and `ConstructNop` stop at the record without trailing zeros. The at-start flag of `_04_00` writes is ignored; commands run in order.

## remove [sceAmprMeasureCommandSizeReadFile]

# libkernel

## change [AMPR object layout and compact gathers]

- [AMPR object layout and compact gathers](../../../core/libs/prx/libkernel/Apr/include/AprCommandBuffer.hpp) - the 24-byte object and offsets (type 0, cursor 4, count 8, capacity 12, pointer 16) follow [ampr_emu assertions](https://github.com/drakmor/ampr_emu/blob/cfa85df379f6eeeb165d7badf9b648e266fe77b7/include/ampr_emu_command_buffer_types.h#L138-L143) and its [C declaration](https://github.com/drakmor/ampr_emu/blob/cfa85df379f6eeeb165d7badf9b648e266fe77b7/include/ampr.h#L41-L47). AnyPS5 represents the 32-bit type word as low 16-bit type and high 16-bit recording flags. Compact gather encoding is AnyPS5 internal: opcode 29 and three 32-bit words encode the high offset byte, length minus one and low offset word. The 12-byte size reduces HFW's 1 MiB buffer pressure; it is not a claim about console opcode encoding. Existing 40-bit offsets and lengths 1 through 2^32 are retained; console behavior outside those limits is unverified.

## add Unknown function info

- [APR native read packets](../../../core/libs/prx/libkernel/Apr/src/Apr.cpp) (libkernel) - console handling of malformed packets and native kernel completion and fault semantics are unverified. The decoder accepts the 20/24-byte opcode 0x28 records emitted by the supplied PPSA02530 01.300.000 `libSceAmpr.sprx` encoder at 0x28d29; unsupported header bits, truncated packets and invalid destination ranges throw. Reads use the existing host file-transfer and queue paths, with their end-of-file and scheduling limitations below.
