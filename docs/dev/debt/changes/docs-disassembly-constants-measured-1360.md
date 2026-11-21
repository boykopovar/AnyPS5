# libSceCdlgUtilServer

## change [`aFrwE9W9faU` and `W4zcizD4i3I`]

- [`aFrwE9W9faU` and `W4zcizD4i3I`](../../../core/libs/prx/libSceCdlgUtilServer/Export.cpp) (libSceCdlgUtilServer) - names and signatures are unknown. Read from disassembly of firmware 13.42; a payload process on a PS5 Pro (firmware 13.60) resolved `aFrwE9W9faU` by its NID text and returned 0, confirming that reading, while `W4zcizD4i3I` was not measured: `aFrwE9W9faU` returns 0, as its body (`xor %eax,%eax; ret` at `0xf40`) does, and its no-parameter `int` declaration claims only the 32-bit return; `W4zcizD4i3I` returns the byte at offset 0x10 of the object its first argument points to, zero-extended (`movzbl 0x10(%rdi),%eax; ret` at `0x10e0`), and nothing else is read. Meaning is unknown; a null or unreadable object throws.

# libSceFreeTypeOt

## change [sceFreeTypeOtLibrarySetLcdFilter, sceFreeTypeOtLibrarySetLcdFilterWeights]

- [sceFreeTypeOtLibrarySetLcdFilter, sceFreeTypeOtLibrarySetLcdFilterWeights](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) (libSceFreeTypeOt) - the console prototypes are unverified; the no-parameter `int` declarations claim only the 32-bit return. Their NIDs `VW4GD-TaIUc` and `GiuNQjoj4fs` are those of `FT_Library_SetLcdFilter` and `FT_Library_SetLcdFilterWeights`. In firmware 13.42's `libSceFreeTypeOt.sprx` (SHA-256 `5c5dd90618603c07987c3df49ad5dbf194017473a5e89369b328162d87a6b164`), read from disassembly and confirmed in a payload process on a PS5 Pro (firmware 13.60), both are `mov $0x7,%eax; ret` (`0x115a0`, `0x11590`): they return 7 without reading an argument or writing memory, and return 7 here. The console module exports them under their NIDs only - the `sceFreeTypeOtLibrarySetLcdFilter*` names resolve nowhere in its name table, on firmware 13.60 as well.

# libSceFsInternalForVsh

## change [`0XlyVxRNeIw`, `W8CmA+wZ9+w` and `lxyJjALNmp4`]

- [`0XlyVxRNeIw`, `W8CmA+wZ9+w` and `lxyJjALNmp4`](../../../core/libs/prx/libSceFsInternalForVsh/Export.cpp) (libSceFsInternalForVsh) - names and signatures are unknown; each returns 0, as its firmware 13.42 body (`xor %eax,%eax; ret` at `0x1ddd0`, `0x1dde0` and `0x35c80`) does. The no-parameter `int` declarations claim only the 32-bit return value. A payload process on a PS5 Pro (firmware 13.60) resolved all three by their NID text and returned 0 from each, so the readings hold on current firmware.

# libSceGLSlimVSH

## change [sceSlimglClientStartIPC and sceSlimglIPCSetup]

- [sceSlimglClientStartIPC and sceSlimglIPCSetup](../../../core/libs/prx/libSceGLSlimVSH/Export.cpp) (`QvXbiLqpn6c`, `F4pQ5tsDo74`) - their prototypes are unverified and they keep the no-parameter declarations. In firmware 13.42 both bodies are `xor %eax,%eax; ret`: they return 0 without reading an argument or setting up any IPC, and so do these: a payload process on a PS5 Pro (firmware 13.60) returned 0 from both, loading the module with `dlopen` alone.

# libSceHmd

## change [libSceHmd VR2D queries]

- [libSceHmd VR2D queries](../../../core/libs/prx/libSceHmd/Export.cpp) (libSceHmd) - `sceHmdInternalIsGameVr2d`, `sceHmdInternalIsMiniAppVr2d`, `sceHmdInternalIsCommonDlgVr2d` and `sceHmdInternalIsCommonDlgMiniAppVr2d` return false: nothing runs in VR2D without a headset, and the console answers 0 to the same four queries in libSceHmd2 (`Hmd2BeforeInitializeZero` in [libSceHmd2](libSceHmd2.md)). A payload process on a PS5 Pro (firmware 13.60) answered 0 for all four libSceHmd queries as well, before any `sceHmdInit` and without a headset attached.

# libScePsm

## change [Unknown NIDs `0PIOM2UIUpQ`, `W8PzsJgkCGU` and `PN1KQl3wnoU`]

- [Unknown NIDs `0PIOM2UIUpQ`, `W8PzsJgkCGU` and `PN1KQl3wnoU`](../../../core/libs/prx/libScePsm/Export.cpp) (libScePsm) - names, parameters and the meaning of the results are unknown; the no-parameter `int` declarations claim only the 32-bit return. In firmware 13.42's `libScePsm.sprx` (SHA-256 `513bb2659efc14094c85272e9e2c8ed2678c17ecf652ddf1902f7200b156756d`), read from disassembly and not run on a console, `0PIOM2UIUpQ` (`0x230e40`) and `W8PzsJgkCGU` (`0x230e50`) are `xor %eax,%eax; ret` and `PN1KQl3wnoU` (`0x23b320`) is `mov $0x80b30006,%eax; ret`; none reads an argument or writes memory. A payload process on a PS5 Pro (firmware 13.60) resolved all three by their NID text and answered 0, 0 and `0x80B30006`, so the readings hold on current firmware.

# libSceSystemLogger2

## change [sceSystemLogger2Terminate]

- [sceSystemLogger2Terminate](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) (libSceSystemLogger2) - returns 0 and leaves the initialized flag alone: the fw 13.40 export is a bare `xor %eax,%eax; ret`, and a payload process on a PS5 Pro (firmware 13.60) returned 0 from it as well. Only `sceSystemLogger2TerminateWithResourceRelease` stops the logger.

# libfontconfig

## change [FcConfigGetCurrent and FcInitBringUptoDate]

- [FcConfigGetCurrent and FcInitBringUptoDate](../../../core/libs/prx/libfontconfig/Export.cpp) (libfontconfig, NIDs `2M54M-SKV-k` and `Ck9k-L7rz-I`) - return what firmware 13.42's `system_ex/common_ex/lib/libfontconfig.sprx` returns, read from disassembly and confirmed on a console: `FcConfigGetCurrent` is `xor %eax,%eax; ret` (no current configuration, NULL) and `FcInitBringUptoDate` is `mov $0x1,%eax; ret` (FcTrue). Neither reads arguments or state in that module, and a payload process on a PS5 Pro (firmware 13.60) answered NULL and 1 respectively.

# libwebrtc

## change [Immediate returns and `Vf6lJz4-bEU`]

- [Immediate returns and `Vf6lJz4-bEU`](../../../core/libs/prx/libwebrtc/Export.cpp) (libwebrtc) - `Vf6lJz4-bEU` (`libwebrtcUnknown37`) has no known name or signature; it returns 0 as its firmware 13.42 body (`xor %eax,%eax; ret`) does. In firmware 13.42 the `D1` destructors of `webrtc::PacketOptions` and `webrtc::rtcp::Pli` and `AudioEncoder::OnReceivedUplinkRecoverablePacketLossFraction` only return, and `VideoFrameBuffer::GetI420` returns null, as in [video_frame_buffer.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video/video_frame_buffer.cc). [audio_encoder.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/audio_codecs/audio_encoder.cc) marks `OnReceivedUplinkRecoverablePacketLossFraction` as `RTC_DCHECK_NOTREACHED()`, which a release build leaves out. Read from disassembly; a payload process on a PS5 Pro (firmware 13.60) resolved `Vf6lJz4-bEU` by its NID text and returned 0, so that reading holds on current firmware, while the member-function bodies were not probed.
