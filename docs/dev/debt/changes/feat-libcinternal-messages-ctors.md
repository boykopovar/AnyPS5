# libSceLibcInternal

## remove [_ZNSt8messagesIcEC1EPKcm]

## remove [_ZNSt8messagesIcEC1ERKSt8_Locinfom]

## remove [_ZNSt8messagesIcEC2ERKSt8_Locinfom]

## remove [_ZNSt8messagesIcEC2Em]

## remove [_ZNSt8messagesIwEC1EPKcm]

## remove [_ZNSt8messagesIwEC1ERKSt8_Locinfom]

## remove [_ZNSt8messagesIwEC2Em]

## add Functional

- [messages<char> and messages<wchar_t> constructors](../../../core/libs/prx/libSceLibcInternal/Messages.cpp) (libSceLibcInternal): the console's `libSceLibcInternal.sprx` was not read for these 7 constructors and no title was seen calling them. They write only the vtable and the 64-bit reference count at 8 of a 0x10-byte object, because both `_Init` members are a single `ret` in firmware 13.42 and Microsoft's [xlocmes](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocmes#L38-L91) `messages` has no other members. `messages<char>` gets the exported vtable. `messages<wchar_t>` has no exported vtable here, so it gets one of this library without offset-to-top or type-info words, whose `do_open`, `do_get` and `do_close` behave as the exported `messages<wchar_t>` members. The constructors taking a locale name accept the names `_Locinfo(const char*)` accepts.
