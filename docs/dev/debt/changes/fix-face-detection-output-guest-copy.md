# libSceFace

## change sceFaceDetectionGetDefaultParam

- [`sceFaceDetectionGetDefaultParam`](../../../core/libs/prx/libSceFace/Export.cpp) `(void *param)` - verified against the GTAVC module body (entry `0x1f80`, size `0x42`): null output returns `0x808b0002`; otherwise a 0x38-byte defaults block is written verbatim: `{u32 56, f32 1.0, f32 0.841 (0x3f574bc7), u32 13200}` at `+0x00`, `0` at `+0x10`, `{u32 0, 0, 2, 2}` at `+0x14`, `{u32 0, 0, f32 0.5, u32 1}` at `+0x24`, `0` at `+0x34`. Field semantics are not published; the bytes are copied exactly as the binary writes them. A non-null unwritable guest output now returns the known null-parameter error to protect the host; the console's behavior for such a pointer is unverified.
