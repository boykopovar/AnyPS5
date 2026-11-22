# libSceAgcDriver

## add Opt-in draw paths

- [Bindless image heap](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureService.cpp) (`APS5_BINDLESS_IMAGES=1`) - opt-in. Storage, nested, packed and BY tables, fetch shaders and mesh pipelines keep per-draw arrays; their T#s and S#s behind a run-time address are refused. Exact priming can pin past the budget, which only evicts idle entries afterwards. Refused as fault records (64 keep a reason): multisampled images, depth compare on color, unmapped surfaces, T#s without a runtime mode, and table S#s with unnormalized coordinates, `min`/`max` reduction, a border, `ANISO_OVERRIDE` or, without the extension, non-seamless cube filtering. A key outside the snapshot reads the live descriptor; a dispatch storing into its own table is not refused. `APS5_BINDLESS_LAZY=1` reads null until the frame after a first miss. Unverified on a console. Closes when: the excluded classes are served or shown unnecessary and the heap is the default.
