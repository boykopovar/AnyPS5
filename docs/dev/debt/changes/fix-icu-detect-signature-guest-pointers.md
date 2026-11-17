# libSceIcu

## add Functional

- [ucnv_detectUnicodeSignature](../../../core/libs/prx/libSceIcu/ConverterHelpers.cpp) reads the guest `UErrorCode* errorCode` and the signature prefix and writes the status and `signatureLength` via `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting unreadable or unwritable pointers via `NotImplemented_nid_no_patch(__func__)`; a known-length source is read in one copy of at most five bytes.
