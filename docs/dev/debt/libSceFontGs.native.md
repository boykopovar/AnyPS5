# libSceFontGs.native technical debt

### Unknown function info

- [sceFontGraphicsAgcSurfaceInit](../../../core/libs/prx/libSceFontGs.native/Export.cpp) (libSceFontGs.native) - still throws through `NotImplemented_nid_no_patch`. The libSceFontGsm.prx body (PPSA13197 unwrapped ELF, VA 0x39e0, size 0x324) validates surface format/attributes, writes a large AGC surface descriptor through a deep helper tree and returns 0x8a6cXXXX family errors; the full object layout it produces is not yet mapped, so no general implementation exists.
- [FontAgcSurface producer](../../../core/libs/prx/libSceFontGs.native/Export.cpp) (libSceFontGs.native) - the object consumed by `sceFontGraphicsAgcDrawupFillTextureImage`/`sceFontGraphicsAgcDrawupFillTexturePattern` (tag byte 0x58 at +0x0d, mode at +0x0f, busy flag at +0x1c, fill object at +0x28, initialized flag at +0x38) is produced by an export outside the six bridge functions (the FontGsm `sceFontGraphicsAgcSurfaceInit` path does not set +0x38), so its producer is not yet identified; the layout is firmware-verified from the libSceFontGsm.prx bridge bodies.
- [FontAgcSequence producer](../../../core/libs/prx/libSceFontGs.native/Export.cpp) (libSceFontGs.native) - the sequence object consumed by `sceFontGraphicsSequenceGetAgcCommandsForJump`/`...ForSubmit` (tag 0xf80 at +0x00, lock bit at +0x1b, commands descriptor at +0x28) is produced by `sceFontGraphicsStructureCanvasSequence` in libSceFont, which is itself still a stub; the descriptor layout (+0x28 first stream, +0x30 first size, +0x34 second count, +0x38 second stream) is firmware-verified.

### Silent stubs

- [sceFontSelectGraphicsAgc](../../../core/libs/prx/libSceFontGs.native/Export.cpp) (libSceFontGs.native) - returns `nullptr` for every index, matching the firmware 4.03 `libSceFontGs.native.sprx` body (`xor eax,eax; ret` at VA 0x250). The newer `libSceFontGsm.prx` sibling body returns a backend descriptor pointer when index == 0 and nullptr otherwise; that descriptor table is not available on this host path, so the 4.03 module behaviour is kept. PPSA13197 (Stellar Blade) binds this NID from its bundled `libSceFontGsm.prx`, so the host path only matters when a guest resolves it from `libSceFontGs.native` itself.

### Notes

- The five implemented [bridge exports](../../../core/libs/prx/libSceFontGs.native/Export.cpp) are transliterated from the `libSceFontGsm.prx` bodies shipped with PPSA13197 (unwrapped ELF exports `XKfh4naaANE`, `PwZEP8ZBK2Y`, `z-m061OoEqQ`, `qEGeDXvL3fc`, `6zxuBlsGF0I`, `CeSI7C4ihjo`): output initialization ordering, 0x80460002/0x8046009a/0x804600b8/0x80460001 error codes, the tag-before-out-pointer check order, the 16834.0f quad bound, and the +0x10 record-state write all match the disassembly. The drawup bridges forward to `sceFontGraphicsDrawupFillTextureImageObject`/`...PatternObject` in `core/libs/prx/libSceFont/src/Graphics.cpp` through the same export boundary firmware uses (PLT import of `CfkpBe2CqBQ`/`FL0unhGcFvI`).
