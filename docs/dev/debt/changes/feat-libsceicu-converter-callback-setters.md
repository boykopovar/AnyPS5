# libSceIcu

## add Functional

- [`ucnv_setFromUCallBack` and `ucnv_setToUCallBack`](../../../core/libs/prx/libSceIcu/ConverterCallbacks.cpp) forward to ICU 78.1 with host-side callback adapters. They translate the four ICU-provided callback functions, preserve guest callback/context pairs for replacements, and report null-converter errors through the status argument. The host converters used by these exports require no `icudt` data. Guest ABI validation is asserted in the callback test; runtime conversion with custom guest callbacks, invalid guest outputs, converter lifetime cleanup, and behavior on a PS5 console have not been measured.
