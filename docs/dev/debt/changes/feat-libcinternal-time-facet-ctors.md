# libSceLibcInternal

## remove e5jQyuEE+9U

## remove y0hzUSFrkng

## remove J2xO4cttypo

## remove bU3S1OS1sc0

## remove p-SW25yE-Q8

## remove 8H3yBUytv-0

## remove QdPT7uDTlo0

## remove 6htjEH2Gi-w

## remove WQQlL0n2SpU

## add Functional

- [time_get<wchar_t> construction and time_put<char> lifecycle](../../../core/libs/prx/libSceLibcInternal/TimeGetFields.cpp) are unverified on a console and use C-locale data. The wide time_get constructors initialize the existing 0x30-byte layout; _Getcat returns category 5 and allocates a facet only when requested. The narrow time_put initializer copies the existing TimeInfo layout, and deleting destructors release the guest allocation. Adapted from Microsoft STL [xloctime](https://github.com/microsoft/STL/blob/770da5aa457af11f0a796600e1aa6f7f6fce14a6/stl/inc/xloctime#L207-L235) and [time_put](https://github.com/microsoft/STL/blob/770da5aa457af11f0a796600e1aa6f7f6fce14a6/stl/inc/xloctime#L859-L887). New wide time_put constructors remain throwing because _Tnames beyond +0x10 is unproved; its existing C1(refs) is unchanged.
