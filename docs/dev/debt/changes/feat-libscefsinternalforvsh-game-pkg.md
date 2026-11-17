# libSceFsInternalForVsh

## add Functional

- [sceFsMountGamePkg and sceFsUmountGamePkg](../../../core/libs/prx/libSceFsInternalForVsh/Export.cpp) (libSceFsInternalForVsh) - throw where firmware 13.60 reaches the kernel. The mount reads the image's 0x5a0-byte header, copies it to the DDR3 cache or enables the GDDR5 cache through `/dev/pfsctldev` or the image file, attaches it through `/dev/lvdctl` and calls `nmount` with `fstype` `pfs` and the keys; the unmount calls `statfs`, `unmount` and `sceFsLvdDetach` and can free the DDR3 slot. The host has no lvd or PFS layer. The checks before that return `0x80020016` as the module does: a null option, mount path or image, or a DDR3 size at 0x18 or GDDR5 flag at 0x20 that does not fit the image (a `game` outer image needs only DDR3, a `game` inner image only GDDR5, any other neither). Read from disassembly, not measured on a console. Closes when: package images can be mounted on the host.
