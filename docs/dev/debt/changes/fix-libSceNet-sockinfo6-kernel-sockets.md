# libSceNet

## change [sceNetGetSockInfo6](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) is not measured on a console

- [sceNetGetSockInfo6](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) is not measured on a console; shadPS4 stubs it. It lists only IPv6 sockets and `flags` 0x1000 adds IPv4 sockets, reported with the IPv4 addresses, all-zero IPv6 addresses and no `F_IPV6` (0x400000), which every IPv6 socket sets ([SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/Net/Socket.cs#L251-L343), [flags](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/Net/Socket.cs#L698-L705), [docs](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/docs/net.md#L301-L304)). The default of leaving IPv4 out, an IPv4 socket's zero IPv6 addresses and accepting 0x40 are inferred from those names, not documented. An IPv4 socket without 0x1000 fails with `EAFNOSUPPORT`. A `socket()` socket is described and listed like `sceNetGetSockInfo`.

## change [sceNetGetSockInfo](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - a socket made with

- [sceNetGetSockInfo](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - a socket made with `socket()` is described from the libkernel table and listed, as on that console; its buffer sizes are the host's, and the other sceNet calls apart from `sceNetGetSockInfo6` still answer `EBADF` for it (not measured on a console).
