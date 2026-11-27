# libSceIcu

## add Functional

- [Locale availability, locale name and string hash exports](../../../core/libs/prx/libSceIcu/Availability.cpp) (libSceIcu) - the locale wrappers return a failure value for unwritable output buffers or status pointers. Console fault behaviour remains unmeasured. `uloc_getName`, `uloc_addLikelySubtags` and `uloc_minimizeSubtags` use the guest default locale for a null locale argument and report ICU's status through the guest pointer. The likely-subtag calls read the bundled `langInfo` resource. `uloc_countAvailable` and `uloc_getAvailable` read the installed locales from `res_index`. `uloc_getName`, `ustr_hashUCharsN`, `ustr_hashCharsN` and `ustr_hashICharsN` require no resource data.
