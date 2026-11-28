# relinker

## add Functional

- An executable's own `R_X86_64_DTPMOD64` (symbol 0, addend 0) is written as the address of the module's PE `_tls_index` (`imageBase + tlsIndex`) in Windows output, matching what the [guest module writer](../../../core/relinker/elfpatcher/src/windows/WindowsGuestModuleWriter.cpp) writes for a module's own TLS block; that is the module word [__tls_get_addr](../../../core/libs/prx/libkernel/Module/src/Tls.cpp) reads. The value comes from that path, not a measurement. A slot inside the `PT_TLS` template or crossing its boundary still throws: [WindowsTlsBuilder](../../../core/relinker/elfpatcher/src/windows/WindowsTlsBuilder.cpp) copies the template into `.gtls` at conversion time, before the slot could be written. macOS still rejects DTPMOD64, as [MacOsImage](../../../core/relinker/elfpatcher/src/macos/MacOsImage.cpp) exposes no descriptor address for executables. PPSA24263's `telescope23.prx` failed on this relocation (#11591); the title was not retested.
