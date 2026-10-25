# libSceVideoRecordingP technical debt

### Unknown function info

- [sceVideoRecordingGetInfo, sceVideoRecordingSetInfo](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) support native validation and errors before a recording context exists. Open and initialization remain protected unimplemented exports; live recording queries, control and volume changes are unavailable.

- [_sceVideoRecordingQueryParam_nid_postfix](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp): the supplied module reads a 64-bit size and writes a 32-bit mode at offset 12 for sizes at least 16, plus a 32-bit option at offset 28 for sizes at least 64. Mode/option meanings remain unresolved; the protected implementation is unchanged.
- [sceVideoRecordingCloseByHandle, sceVideoRecordingGetInfoByHandle](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) - native handles are context pointers. Recording state and per-handle queries remain unimplemented.
- [sceVideoRecordingCopyBGRAtoNV12](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) - the destination, source, 32-bit width and height arguments are confirmed. Pixel conversion remains unimplemented.
- [sceVideoRecordingGetSupportInfo, sceVideoRecordingGetVideoOutMode](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) - GetSupportInfo writes an 18-byte payload whose fields remain unresolved. GetVideoOutMode takes a four-byte output confirmed by the native recording assembly and Avcap2 IPC descriptor; the mode values remain unresolved. Both exports remain unimplemented.
- [sceVideoRecordingInitialize](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp): the native initializer consumes no caller arguments; the former optional parameter is not read by the Avcap2 initializer or recording setup. Its unused parameter slot is retained because the protected body references it.
- [sceVideoRecordingOpen, sceVideoRecordingOpen2](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) - the 32-byte and 40-byte parameter variants, four argument registers and 32-bit heap size are recovered. Mode, output mode, option and format semantics remain incomplete. QueryHddSize2 returns a signed 64-bit size or error; recording and sizing exports remain unimplemented.

- [Handle and support queries](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) - native recording handles are context pointers, and GetSupportInfo writes 18 bytes. Support payload field semantics remain incomplete. RestModeProcessStart, RestModeProcessGetStatus and SendCommand consume no caller arguments in the supplied native module; their existing unused parameter slots remain because the protected bodies reference those parameter names.

### Functional

- [sceVideoRecordingGetStatus](../../../core/libs/prx/libSceVideoRecordingP/Export.cpp) (libSceVideoRecordingP) always returns 0, `None` ("The recorder is not open") in [SharpProspero's `VideoRecordingStatus`](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/Video/VideoRecording.cs#L18-L29), also listed in [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/15abf68c9b52c4b04d094f2837719fdb871021f3/src/core/libraries/video_recording/video_recording.h#L32-L36). It is true here because `sceVideoRecordingOpen` throws, so no recorder is ever open. Unverified: whether the console returns 0 or an error before `sceVideoRecordingInit`/`sceVideoRecordingInitialize`, and when it reports `Running` (1) or `Paused` (2)
