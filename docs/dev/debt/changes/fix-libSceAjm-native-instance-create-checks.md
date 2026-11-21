# libSceAjm.native

## add Unknown function info

- [sceAjmInstanceCreate argument checks](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - not measured on a console: read from firmware 13.42's `libSceAjm.native.sprx` ([disassembly](https://github.com/boykopovar/AnyPS5/issues/21856#issuecomment-6939955028)). A null `instance` returns 0x80930005; then `(flags & 0x380) >= 0x180`, `(flags & 0x2400) == 0x2000` or a bit of `0xf0000000ffffc000` returns 0x8093000C; then the context is checked. The firmware leaves the context to its `/dev/ajm` request, whose answers are not modelled beyond 0x80930002 for an ID that was never handed out. Closes when: measured on a PS5
