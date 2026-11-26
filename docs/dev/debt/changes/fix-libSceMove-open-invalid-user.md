# libSceMove

## change [sceMoveOpen and sceMoveClose]

- [sceMoveOpen and sceMoveClose](../../../core/libs/prx/libSceMove/Export.cpp): the host generates increasing handles from 0x30B0100, validates type 0 for SDK versions above 0x03ffffff with 0x80EE0006 while accepting arbitrary types for earlier SDK versions, returns 0x80EE0007 only for a duplicate type and index, and throws for users other than -1 and the initial user. A firmware 10.71 PS4 ([measurements](https://github.com/boykopovar/AnyPS5/discussions/7941#discussioncomment-19045601)) returned handles 0x03140800 and 0x03150801 for indexes 0 and 1, which come from the device ioctl; two values do not show what the bytes encode. It returned 0x80EE0007 for type 1 without naming the index or the open handles. User -1 returns its 0x80EE0006 after the initialization check; its order against the eight-slot limit (0x80EE0005) is not measured. Closing an unknown handle returns 0x80EE0004.
