# libSceSystemService

## add Unknown function info

- [sceSystemServiceLaunchWebBrowser URI](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - the URI is copied in with `CopyGuestString_nid_no_patch` capped at 6016 bytes, which keeps the 6015-character limit and its `0x80a10007` result; an unreadable URI returns `0x80a10003` like a null one, because the console's behaviour on unmapped memory is unmeasured.
