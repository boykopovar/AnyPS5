# libSceNKWebKit

## add Functional

- [WebCore::FormData](../../../core/libs/prx/libSceNKWebKit/src/FormData.cpp) (libSceNKWebKit) - the 72-byte layout in `FormDataAbi.hpp` (48-byte element) comes from the 11.x [FormData.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/FormData.h) and is not checked against the PS5 module. The `std::variant` index is 4 bytes, as libc++ ABI v1 lays it out; the low byte is read and `0xFF` (valueless) throws. Objects come from the guest `operator new` and `operator delete`, not FormData's ISO (libpas) heap; unverified. `lengthInBytes` throws for a file range to the end of the file and for blobs. `prepareForUpload`, `resolveBlobReferences` and `asSharedBuffer` still throw `NotImplemented_nid_no_patch`. Closes when: compared with a console module or the firmware's allocation size.

## remove `_ZN7WebCore8FormData10appendBlobERKN3WTF3URLE`

## remove `_ZN7WebCore8FormData10appendDataEPKvm`

## remove `_ZN7WebCore8FormData6createEPKvm`

## remove `_ZN7WebCore8FormData6createEv`

## remove `_ZN7WebCore8FormDataD1Ev`

## remove `_ZN7WebCore8FormDataD2Ev`

## remove `_ZNK7WebCore8FormData12isolatedCopyEv`

## remove `_ZNK7WebCore8FormData7flattenEv`

## remove `_ZNK7WebCore8FormData9asBlobURLEv`

## remove `_ZNK7WebCore8FormData13lengthInBytesEv`
