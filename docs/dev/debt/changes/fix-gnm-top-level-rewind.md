# libSceGnmDriver

## add Functional

- [REWIND in a submitted draw command buffer](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - a REWIND packet in a draw command buffer given to `sceGnmSubmitCommandBuffers` or `sceGnmSubmitAndFlipCommandBuffers` throws, as one in a constant command buffer already did. The Gnm path merges the draw and constant streams into a copy and frees the copy once AGC has taken it, while AGC waits on a REWIND by reading its control word, and later the packets after it, at the address it was given. The title releases a REWIND by setting the control word in its own buffer, which the copy never sees. A REWIND in a buffer the draw command buffer calls is read from the title's memory, so the copy does not affect it.
