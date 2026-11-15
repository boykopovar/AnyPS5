# libSceLibcInternal

## add Functional

- [messages<char>::open and get](../../../core/libs/prx/libSceLibcInternal/Messages.cpp) (libSceLibcInternal, NIDs `U2t+WvMQj8A` and `XxsPrrWJ52I`): the console's `libSceLibcInternal.sprx` was not read and no title was seen calling them. Unverified: `do_open` is read at vtable offset 0x20 and `do_get` at 0x28, their declaration order after the four facet slots in the Dinkumware `messages` of [microsoft/STL](https://github.com/microsoft/STL/blob/219514876ea86491de191ceaa88632616bbc0f19/stl/inc/xlocmes#L38-L94), whose members return their `do_` member, and `get` returns the 0x28-byte string through a hidden first argument. `messages<char>::close` and `do_get` and the `messages<wchar_t>` members still throw

## remove [_ZNKSt8messagesIcE3getEiiiRKSs]

## remove [_ZNKSt8messagesIcE4openERKSsRKSt6locale]
