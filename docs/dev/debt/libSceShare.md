# libSceShare technical debt

### Silent stubs

- [sceShareFeaturePermit, sceShareFeatureProhibit, sceShareInitialize, sceShareRegisterContentEventCallback, sceShareSetCaptureSource, sceShareSetContentParam, sceShareSetScreenshotOverlayImage, sceShareTerminate and sceShareUnregisterContentEventCallback](../../../core/libs/prx/libSceShare/Export.cpp) return success without interacting with share or capture services.

### Unknown function info

- [sceShareGetCurrentStatus](../../../core/libs/prx/libSceShare/Export.cpp) (libSceShare) - validation (feature flag non-zero, status non-null) and the all-zero 16-byte status taken from the [KytyPS5](https://github.com/KytyPS5/KytyPS5) reimplementation, not confirmed on a PS5 title; the meaning of the recording status values is unknown
- [sceShareGetRunningStatus](../../../core/libs/prx/libSceShare/Export.cpp) - the native wrapper writes only the first 32-bit word, populated from a kernel event flag. The host reports zero because capture and broadcast services are absent; native initialization checks and event-flag failures are not modeled.
- [sceShareSetContentParamForApplicationTitle](../../../core/libs/prx/libSceShare/Export.cpp) - the recovered string length limit is 1 through 5119 bytes. The title is validated but not sent to the capture service; native initialization and service failures are not modeled.

### Functional

- [sceShareCaptureVideoClipExtended, sceShareCaptureScreenshotExtended](../../../core/libs/prx/libSceShare/Export.cpp) - the 40-byte video and 24-byte screenshot parameters and request-ID write precedence are recovered. Video values at offsets 8 and 16 and screenshot mode semantics remain incomplete. Valid requests still return NOT_SUPPORTED without capture; screenshot mode 2 platform checks, throttling, initialization and the backend text limit are not modeled.

### Additional callable exports

- Six [callable exports](../../../core/libs/prx/libSceShare/Export.cpp) are throwing import-resolution declarations. Names for two entries, signatures and behavior are unverified. The module identity is `libSceShare`; the library identities are `libSceShareForAcf` and `libSceShareForSf`. The entries occur in two version-labelled images; these labels are not authenticated.

| Name | NID |
| --- | --- |
| `sceShareFeatureExPermit` | `KVfMsyLNp5w` |
| `sceShareFeatureExProhibit` | `x+EVsi+Qhrk` |
| `sceShareRegisterUploadCallback` | `K0I-WwCewVU` |
| `sceShareUnregisterUploadCallback` | `4WSYopUYUPI` |
| `Unknown` | `YPIiGz4uKOU` |
| `Unknown` | `txixe0fiPKM` |
