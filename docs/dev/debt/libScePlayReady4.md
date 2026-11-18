# libScePlayReady4 technical debt

### Unknown function info

- [13.60 inventory additions](../../../core/libs/prx/libScePlayReady4/Export.cpp) - `scePlayReadyCdmiProcessLicenseAckResponse` (`CG-PDnXEyns`), `scePlayReadyCdmiGetSessionId2` (`POnQSapoh38`), `scePlayReadyCdmiGenerateLicenseAckRequest` (`dxbGiL38h+4`), `scePlayReadyCdmiGenerateRequest2` (`iZk5IYEltB8`), `scePlayReadyDomainJoinProcessResponse2` (`k5bAoX35xgg`) have unknown signatures and behavior. The parameterless `int` declarations only resolve imports and are not verified ABI prototypes; every call throws through `NotImplemented_nid_no_patch`. Name/NID bindings are from the [13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceplayready4-e431478f47.jsonl).

- [scePlayReadyApiPlatform, scePlayReadyCdmiClose, scePlayReadyCdmiCreateMediaKeySession, scePlayReadyCdmiCreateMediaKeySystemAccess, scePlayReadyCdmiCreateMediaKeys, scePlayReadyCdmiDecryptMultipleOpaque, scePlayReadyCdmiDecryptOpaque, scePlayReadyCdmiDecryptOpaque2, scePlayReadyCdmiDestroyMediaKeySession, scePlayReadyCdmiDestroyMediaKeySystemAccess, scePlayReadyCdmiDestroyMediaKeys, scePlayReadyCdmiFreeOpaqueDecryptedContent, scePlayReadyCdmiGenerateRequest, scePlayReadyCdmiGetConfiguration, scePlayReadyCdmiGetExpiration, scePlayReadyCdmiGetKeySystem, scePlayReadyCdmiGetMediaKeyCount, scePlayReadyCdmiGetMediaKeyStatus, scePlayReadyCdmiGetSessionAppContext, scePlayReadyCdmiGetSessionId, scePlayReadyCdmiIsLicenseAckRequired, scePlayReadyCdmiLoad, scePlayReadyCdmiRemove, scePlayReadyCdmiRetrieveOpaqueDecryptedContent, scePlayReadyCdmiSetServerCertificate, scePlayReadyCdmiSetSessionCallbacks, scePlayReadyCdmiUpdate, scePlayReadyLicenseCount, scePlayReadyProfileClear, scePlayReadyProfileEnter, scePlayReadyProfileLeave, scePlayReadyProfilePause, scePlayReadyProfileResume, scePlayReadyProfileStart and scePlayReadyProfileStop](../../../core/libs/prx/libScePlayReady4/Export.cpp) (libScePlayReady4) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePlayReady4.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

### Inventory exports declared elsewhere
 The PS5 3.20 export table for this module also contains 49 names already declared under libScePlayReady, libScePlayReady2 in this tree; the duplicate-export rule keeps each name in a single library, so they are omitted here. They resolve through the sibling declarations.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libScePlayReady4/Export.cpp) - 9 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Eight NIDs also appear in the later labelled `libScePlayReady4L` image; resolution between providers requires separate verification. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `Unknown` | `2OZeMccOeho` |
| `Unknown` | `3fo8fAvHn9A` |
| `Unknown` | `HNB-eWVwwng` |
| `Unknown` | `LL9r9r4cCdQ` |
| `Unknown` | `OqSN3eUfHMY` |
| `Unknown` | `XawoudE54JE` |
| `Unknown` | `asAQ9li6+mo` |
| `Unknown` | `o2H2P7xoLec` |
| `Unknown` | `sK0jAAnbbiI` |

### Older callable exports

- [Older callable exports](../../../core/libs/prx/libScePlayReady4/Export.cpp) - Thirty-eight additional older callable NIDs have unidentified names and are declared as throwing placeholders. Their prototypes and behavior are unverified. These NIDs do not appear in the later version-labelled module inventory; no cross-version identity or functional behavior is claimed. A separately shared initializer NID is excluded because it is already provided by another module and its structure differs across versions.
