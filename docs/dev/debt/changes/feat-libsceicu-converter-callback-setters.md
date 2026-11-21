# libSceIcu

## add Functional

- [`ucnv_setFromUCallBack` and `ucnv_setToUCallBack`](../../../core/libs/prx/libSceIcu/ConverterCallbacks.cpp) forward to ICU 78.1 with host-side callback adapters. They translate the four ICU-provided callback functions, preserve guest callback/context pairs for replacements, and report null-converter errors through the status argument. The host converters used by these exports require no `icudt` data. Resetting a direction to the default empties its registry entry but keeps the allocation, because ICU may still hold it as the context of the adapter installed in the other direction. Guest ABI validation is asserted in the callback test; runtime conversion with custom guest callbacks, invalid guest outputs, `ucnv_close` cleanup of the registry (that export still throws), and behavior on a PS5 console have not been measured.
