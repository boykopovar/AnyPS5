# libSceLibcInternal

## remove _ZNSt8messagesIcE7_GetcatEPPKNSt6locale5facetEPKS1_

## remove _ZNSt8messagesIwE7_GetcatEPPKNSt6locale5facetEPKS1_

## add Functional

- [messages<char>::_Getcat and messages<wchar_t>::_Getcat](../../../core/libs/prx/libSceLibcInternal/Messages.cpp) (libSceLibcInternal, NIDs `96Ev+CE1luE`, `XghI4vmw8mU`): for an empty facet slot they build a `messages` facet for the locale's name with no references, through the same path as the `(const char*, size_t)` constructor, and they always return 6, `_X_MESSAGES` in [microsoft/STL `xlocinfo`](https://github.com/microsoft/STL/blob/main/stl/inc/xlocinfo), as its `xlocmes` does (`num_get` returns 4 and `time_get` 5 here, the same numbering). An empty slot with a null locale throws. The console's bodies for these NIDs were not read.
