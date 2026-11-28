# libSceAjm.native

## change [sceAjmDecAt9ParseConfigData]

- [sceAjmDecAt9ParseConfigData](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - for configuration words with sync 0xFE, except the vibration profiles (channel index 6 or 7 with sample rate index 1, 4 or 7, which keep their behavior, console unmeasured), it also returns SCE_AJM_ERROR_INVALID_PARAMETER for a sample rate index of 8 or more, a channel index of 6 or more, a superframe index of 1 or 3 and any of the three low bits set, as far as 41 words measured on the console show (one run of each word, a second run identical); the field layout is assumed and untested for other combinations. Non-null unwritable output or unreadable input pointers return SCE_AJM_ERROR_INVALID_PARAMETER with caller errno preserved via GuestCopyIn_nid_no_patch and GuestCopyOut_nid_no_patch as host-safety fallbacks. Console results for invalid guest pointers are unmeasured.
