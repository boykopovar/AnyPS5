# libSceIcu

## add Functional

- [Locale availability, locale name and string hash exports](../../../core/libs/prx/libSceIcu/Availability.cpp) (libSceIcu) - they call ICU 78.1 through its C entry points and pass the guest pointers to it unchanged, except that a null locale given to `uloc_getName`, `uloc_addLikelySubtags` or `uloc_minimizeSubtags` becomes this library's default locale instead of the host's, and report whatever `UErrorCode` ICU writes into the guest's status. `uloc_addLikelySubtags` and `uloc_minimizeSubtags` read `langInfo` from the bundled ICU data package; `uloc_countAvailable`, `uloc_getAvailable`, `uloc_getName`, `ustr_hashUCharsN`, `ustr_hashCharsN` and `ustr_hashICharsN` are data-free, and the new test covers them. An unwritable guest pointer makes the wrapper return the function's failure value instead of faulting: the console faults there, which is not measured.
