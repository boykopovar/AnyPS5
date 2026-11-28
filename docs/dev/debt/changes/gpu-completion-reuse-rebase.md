# libSceAgcDriver

## add Functional

- [RELEASE_MEM GL2 write-back](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Packets/PacketMemory.cpp) throws for nonzero GL2_US/RANGE/DISCARD/SEQ, non-EOP event/index pairs except ACE CS_DONE, queued draw reads, and another thread's unfinished image publication. Those scopes, graphics EOS cache operations and cross-queue ownership are not modeled. Global write-back uses existing image/depth/shadow publication before completion. [AMD PAL](https://github.com/GPUOpen-Drivers/pal/blob/c5e800072a32f68b6ccc4422936d96167c6e0728/src/core/hw/gfxip/gfx9/gfx9CmdUtil.cpp#L3428) establishes the event/GCR fields; no PS5 comparison was run. GPU-ordered host imports, concurrent CPU-store races and title performance remain unverified.
