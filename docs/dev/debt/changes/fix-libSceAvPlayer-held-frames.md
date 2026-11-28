# libSceAvPlayer

## add Functional

- [sceAvPlayerGetVideoData / sceAvPlayerGetVideoDataEx](../../../core/libs/prx/libSceAvPlayer/src/Source.cpp) (libSceAvPlayer) - how long a handed-out video frame stays valid on the console is unknown. The player keeps the last `num_output_video_framebuffers - 2` handed-out frames intact (at least the newest one) and, when the memory replacement provides 4 extra framebuffers after the decode-ahead group (12 in `SCE_AVPLAYER_AV_SYNC_MODE_NONE` when they fit, otherwise 4), at least the newest 5, and reuses free framebuffers oldest first. The count of 5 comes from PPSA12544's intro cinematic, which asks for 2 framebuffers and whose GPU work can still read a frame after later frames were handed out; it is not tied to the GPU having read the frame
