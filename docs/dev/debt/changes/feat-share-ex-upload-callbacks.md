# libSceShare

## remove | `sceShareFeatureExPermit` | `KVfMsyLNp5w` |

## remove | `sceShareFeatureExProhibit` | `x+EVsi+Qhrk` |

## remove | `sceShareRegisterUploadCallback` | `K0I-WwCewVU` |

## remove | `sceShareUnregisterUploadCallback` | `4WSYopUYUPI` |

## change [callable exports]

- Two [callable exports](../../../core/libs/prx/libSceShare/Export.cpp) are throwing import-resolution declarations. Their names, signatures and behavior are unverified. The module identity is `libSceShare`; the library identities are `libSceShareForAcf` and `libSceShareForSf`. The entries occur in two version-labelled images; these labels are not authenticated.

## add Silent stubs

- [sceShareFeatureExPermit, sceShareFeatureExProhibit, sceShareRegisterUploadCallback and sceShareUnregisterUploadCallback](../../../core/libs/prx/libSceShare/Export.cpp) - in firmware 13.42's `libSceShare.native.sprx` ([reading](https://github.com/boykopovar/AnyPS5/issues/21847#issuecomment-6939388841), disassembly only, not run on a console) the `Ex` forms take the plain forms' one `uint32_t` and run their request routine with bit 35 added to the command; here they return the plain forms' success. `sceShareRegisterUploadCallback(callback, userData)` returns `0x81960002` for a null or already registered callback, whatever `userData`, `sceShareUnregisterUploadCallback(callback)` for a null or unknown one, and both otherwise 0. The console's library-open checks (`0x8196000C`, `0x81960001`) depend on state whose setter wasn't read and aren't modelled, as for the plain forms; upload callbacks are never called here.
