# libSceNpCommon

## add Unknown function info

- [NpTitleId and NpCommId text](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) (libSceNpCommon) - `NpTitleId::Parse`, `NpTitleId::ConstructFromTitleId`, `NpCommId::Parse` and `NpCommId::Equals` copy the text with `CopyGuestString_nid_no_patch` capped at 13 bytes, which is enough for every length they accept; unreadable text returns `SCE_NP_ERROR_INVALID_ARGUMENT` (`Equals`: false), because the console's behaviour on unmapped memory is unmeasured. The length-taking overloads still read the text in place.
