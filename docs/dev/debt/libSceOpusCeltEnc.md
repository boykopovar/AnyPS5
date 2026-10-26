# libSceOpusCeltEnc technical debt

### Unknown function info

- [sceOpusCeltEncTerminate](../../../core/libs/prx/libSceOpusCeltEnc/Export.cpp): the context-ID pointer is confirmed. The null-input path does not define the return register, so an integer return contract is not established. The current integer return declaration is retained because changing it to `void` would require altering the protected stub body, which contains `return 0`.
