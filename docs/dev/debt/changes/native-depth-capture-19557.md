# libSceAgcDriver

## add Functional

- [Native depth in frame capture](../../../core/libs/prx/libSceAgcDriver/Execution/src/NativeDepthCapture.cpp) (libSceAgcDriver) - GPU-only native depth with HTILE or multiple samples throws at capture; its metadata and sample history are not reconstructed. Capture and replay require companion owners with matching extent and format. Replay requires a fresh native depth cache and the same host depth format, with at most 1 GiB per depth plane and 4 GiB decoded in total; host-format conversion throws. On a device without unrestricted depth range, a float plane or clear outside [0, 1], including NaN, throws before restoration. The Minecraft Dungeons II shadow-map report (#15113) has not been rerun.
- [Shared native stencil rebinding](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - Lavapipe 26.2.6 copies depth as well as stencil when `CopyAspectFrom` requests only the stencil aspect of a D32S8 image. Rebinding a shared stencil plane between two surfaces with distinct depths therefore overwrites the destination depth. A synthetic no-capture control fails on main with this Mesa driver and passes on NVIDIA 615.78.08; console behavior was not measured.
- [Shared native stencil publication](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - ordinary rendering can leave the previous mirrorable stencil owner marked for guest publication, so a later flush may publish its stale replica after the current owner. Native capture and reconstruction reconcile the publication markers for their selected planes; rebinding outside those boundaries still needs the same ownership reconciliation.

## change [Host-only multisample memory in frame capture]

- [Host-only multisample memory in frame capture](../../../core/libs/prx/libSceAgcDriver/Execution/src/FrameRecorder.cpp) (libSceAgcDriver) - memory under multisampled color targets, their FMASK and native multisample depth whose samples are only on the host is zero in the snapshot and in deltas (`host_only_sample_bytes`, with a warning), so replay starts those targets without the contents earlier frames left in them.
