# libSceFreeType

## add Unknown function info

- [BuildInfo.cpp](../../../core/libs/prx/libSceFreeType/src/BuildInfo.cpp): `FT_Library_Version` and `FT_Get_TrueType_Engine_Type` validate guest library readability via integer offset arithmetic and `GuestArenaPrepareHostRead` and write version outputs via `GuestCopyOut` while preserving `errno`, throwing `std::invalid_argument` on unwritable outputs because PS5 hardware fault behavior is unverified.
