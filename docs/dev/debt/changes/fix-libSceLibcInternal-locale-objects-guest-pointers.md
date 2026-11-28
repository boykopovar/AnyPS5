# libSceLibcInternal

## add Unknown function info

- [LocaleObjects.cpp](../../../core/libs/prx/libSceLibcInternal/LocaleObjects.cpp): `_ZNSt6locale6globalERKS_`, `_ZNSt6locale7_LocimpC2Eb`, and `_Deletegloballocale` validate guest memory readability, writability, and callback executability and throw `std::invalid_argument` on unmapped, read-only, or non-executable guest pointers because PS5 hardware fault behavior is unverified.
