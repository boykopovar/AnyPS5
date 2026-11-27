# libSceAgcDriver

## change [PS4 command buffers that an INDIRECT_BUFFER calls]

- [PS4 command buffers that an INDIRECT_BUFFER calls](../../../core/libs/prx/libSceAgcDriver/Execution/src/Pm4.cpp) (libSceAgcDriver, libSceGnmDriver) - GNM lowers only the top-level packets of a PS4 title's submission; in a buffer they call or chain to, AGC lowers as GNM does `EVENT_WRITE_EOP`, `EVENT_WRITE_EOS`, `SURFACE_SYNC`, the `DMA_DATA` tag read and a seven-dword `RELEASE_MEM` with dword 1 bits beyond the event type and index or with an interrupt (context 0), and does not reread that buffer when the title rewrites it later. Predicated ones still throw, as do the CE/DE counter packets and a flip reservation (a 64-dword marker NOP, or the markers `0x68750777` and `0x68750778`). In a PS4 title the check accepts a typed user-data marker NOP, header bit 1 on a compute-queue packet other than a NOP, a seven-dword `RELEASE_MEM` and `ACQUIRE_MEM` bit 16 (`TC_VOL_ACTION_ENA`, the L2 action for volatile lines), which AGC ignores. None of these forms is measured on a PS4.

# libSceGnmDriver

## change [Command buffers a compute ring calls]

- [Command buffers a compute ring calls](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - a buffer reached by a predicated, chained or conditional INDIRECT_BUFFER is left to AGC, which lowers in it the GCN packets its check refuses as written, and a call three levels below the ring or a REWIND in a called buffer throws. A plain call is copied when `sceGnmDingDong` is called and lowered in its place, so packets the title rewrites afterwards are not seen, where the AGC executor rereads a called buffer behind a wait. The two levels are the hardware's IB1 and IB2, not measured on a console. The seven-dword RELEASE_MEM follows [shadPS4's `PM4CmdReleaseMem`](https://github.com/shadps4-emu/shadPS4/blob/a808e7626e3c7de8d0f0fda6a91f620b42f6c762/src/video_core/amdgpu/pm4_cmds.h#L907-L935) and the packets CUSA01709 01.02 puts in the buffers its rings call (header 0xc0054902).
