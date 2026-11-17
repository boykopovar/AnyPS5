# libSceFsInternalForVsh

## remove `sceFsDevpfsMountForVSH`

## remove `sceFsDevpfsUmountForVSH`

## remove `sceFsDevpfsWriteChunk`

## add Functional

- [sceFsDevpfsCreateForVSH, sceFsDevpfsMountForVSH, sceFsDevpfsUmountForVSH and sceFsDevpfsWriteChunk](../../../core/libs/prx/libSceFsInternalForVsh/Export.cpp) (libSceFsInternalForVsh) - throw, because firmware 13.60 reaches the kernel before it returns anything. `CreateForVSH` opens `/dev/devpfs.ctl`, creates the image and formats it with ioctl `0x8030b001` and a 0x6c8-byte parameter block that holds the 16 bytes of its second argument at 0x1a0. `MountForVSH` calls `nmount` with `fstype` `devpfs`, `from` set to `devpfs.img` in the image's directory, and the budget id `*(char**)opt`. `UmountForVSH` unmounts the path, then remounts `/mnt/bfs_user`. `WriteChunk` issues ioctl `0xc028b075` on the descriptor it is given and returns the driver's status. The host has no devpfs layer, and what the `WriteChunk` arguments and the seed mean is unknown. Read from disassembly, not measured on a console. Closes when: a devpfs image can be created and mounted on the host.
