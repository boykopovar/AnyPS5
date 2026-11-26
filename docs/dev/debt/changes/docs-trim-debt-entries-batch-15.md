# libSceAgcDriver

## change [sceAgcDriverSetSubmitValidationMode, sceAgcDriverGetSubmitValidationMode, sceAgcDriverSetSubmitValidationConfig, sceAgcDriverGetSubmitValidationConfig, sceAgcDriverSetValidationErrorOutputFrequency, sceAgcDriverGetShaderDebuggingStatus]

- [sceAgcDriverSetSubmitValidationMode, sceAgcDriverGetSubmitValidationMode, sceAgcDriverSetSubmitValidationConfig, sceAgcDriverGetSubmitValidationConfig, sceAgcDriverSetValidationErrorOutputFrequency, sceAgcDriverGetShaderDebuggingStatus](../../../core/libs/prx/libSceAgcDriver/Unimplemented.cpp) (libSceAgcDriver) - the five validation calls return `0x8A6C1000` without reading arguments or writing caller memory, and `sceAgcDriverGetShaderDebuggingStatus` returns 1, matching [OpenAGC](https://github.com/OpenAGC/OpenAGC/blob/3c7e8072f0d7684e930454794fd5c02056946d73/src/game_compat.c#L250-L291): in Sony's `libSceAgcDriver.sprx` (Dragon Quest VII Reimagined, `0x7bb0-0x7bf0`) each validation export is `mov eax, 0x8a6c1000; ret`, and debugging status is 1 on firmware 5.50 (`0x6740`). Parameter types follow OpenAGC and are ignored; later firmware and title usage are unverified

## change [PM4 submission rings]

- [PM4 submission rings](../../../core/libs/prx/libSceAgcDriver/Execution/src/Driver/Queues/SubmissionRing.cpp) (libSceAgcDriver) keep only the first 512 dwords per submission, omitting later packets. The [failure report](../../../core/libs/prx/libc/src/FailureReport.cpp) dump avoids CRT heap allocation by formatting static packet names into a 512-byte stack buffer per `WriteFile`. It runs on crashed process state at most once, yields ~100 times per ring lock before marking the queue busy (losing in-flight recordings), and reads rings unchecked: corrupted rings can fault inside the dump, hanging unhandled-exception reports in `Sleep(INFINITE)`. Linux has no crash report and prints rings only via `APS5_STALL_WATCHDOG_SECS`.

## change [Comp-to-single DCC keys]

- [Comp-to-single DCC keys](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DccMetadata.cpp) (libSceAgcDriver) - uniform key 0x10 (`GFX9_DCC_CLEAR_SINGLE` in Mesa `ac_descriptors.h`) sets every texel to the first element of its 256-byte block (where radv's `build_clear_dcc_comp_to_single_shader` writes the clear color). Sampled textures, storage images (marking keys uncompressed on first write), and CB DCC decompress passes expand it; fast-clear eliminate and mixed-key metadata passes throw, color targets use stored texels, and mixed keys or non-dividing formats read as stored. Uniformity checks one key byte per 256 surface bytes (see DCC key stores above), so pipe-aligned surfaces differing past that range still expand. Not verified on hardware

# libSceAppContent

## change [Additional content download and delete]

- [Additional content download and delete](../../../core/libs/prx/libSceAppContent/Export.cpp) (libSceAppContent) - unmodeled: `sceAppContentAddcontEnqueueDownload`, `sceAppContentAddcontDelete`, and `sceAppContentGetAddcontDownloadProgress` return `PARAMETER` for null pointers or padded labels (Enqueue from SDK `0x01500000`, Delete/GetProgress from `0x03500000`), and `NOT_FOUND` otherwise, matching [dlc_emu](https://github.com/drakmor/dlc_emu/blob/fcf69b16d46a9850578bce19dc2c1b4dc867cbb5/src/dlc_modules/dlc_content.cpp#L2328-L2341) for uninstalled labels. Layouts follow [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/AppContent/AppContent.cs#L45-L54) and [dlc_emu](https://github.com/drakmor/dlc_emu/blob/fcf69b16d46a9850578bce19dc2c1b4dc867cbb5/src/dlc_modules/dlc_content.h#L91-L92).

# libSceAudioOut

## change [Legacy vibration ports]

- [Legacy vibration ports](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp): PPSA99902's [input wrapper](https://github.com/Nazky/UnleashedRecomp-Prospero/blob/2ce067a1d51bb58670c7ee4a3aed893c504da805/UnleashedRecomp/hid/driver/sdl_hid.cpp#L316) opens type 10 with 256 stereo S16/F32 frames at 48000 Hz. Mono duplicates to both channels; wider layouts throw. Native output uses AudioOut2's 4-channel layout (6-channel on PulseAudio); without a controller sound card, the RMS motor transform runs on <=256-frame segments scheduled by queue frame count/rate, combining concurrent sources by per-motor max (drain/close remove only that port's segments) for non-DualSense pads. Unmeasured on hardware, not a waveform reproduction, single-controller routing only; AudioOut2 keeps its 80 ms peak hold.

# libSceAudiodec

## change [M4AAC extended parameters]

- [M4AAC extended parameters](../../../core/libs/prx/libSceAudiodec/Export.cpp): 28- and 32-byte variants add fields at offsets 24 (`0..1`, error `0x807F0305` `SCE_AUDIODEC_ERROR_M4AAC_INVALID_ENABLE_NONDELAY_OUTPUT`) and 28 (`0..2`, error `0x807F0306` `SCE_AUDIODEC_ERROR_M4AAC_INVALID_SURROUND_CHANNEL_INTERLEAVE_ORDER`, per fpPS4 `ps4_libsceaudiodec.pas`). Both non-delay 0 and 1 decode every AU immediately (shadPS4 holds 2 frames without `EnableNondelayOutput`; console delay for 0 is unmeasured). Interleave orders 1 and 2 leave mono/stereo unchanged (PPSA21607 uses order 2 for stereo) and throw on surround: mapping to shadPS4's `ExtlExtrLsRs`/`LsRsExtlExtr` and order 0 is unknown.

# libSceFont

## change [sceFontCharacterGetSyllableStringState]

- [sceFontCharacterGetSyllableStringState](../../../core/libs/prx/libSceFont/src/Text.cpp) (libSceFont) - signature `(textCharacter, int* syllableStringState)` follows [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/main/src/SharpProspero/Interop/Font/SceFont.Engine.cs) (`SceFontSyllableString`: 0 NonDetection, 1 Starting, 2 Following, 3 Depending, 4 Connecting). Measured on PS4 firmware 11.00 `libSceFont.sprx` (unmeasured on PS5): Latin, CJK, Hangul, Arabic, Hebrew, and Devanagari return state 0; Thai returns 1, 3, or 4; null char/result returns `SCE_FONT_ERROR_INVALID_PARAMETER` (null char writes 0 if result is non-null). Returns 0 outside Thai (U+0E00..U+0E7F) as in [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/d2413fc2ebd91d1b7234197c2dd5bd48e1cb6a51/src/libs/libFont.cpp#L1805-L1815), and throws on Thai; called by PPSA06699.

# libSceHmd2

## change [Hmd2BeforeInitializeError0004]

- [Hmd2BeforeInitializeError0004](../../../core/libs/prx/libSceHmd2/Export.cpp) (libSceHmd2) - before `sceHmd2Initialize`, 15 exports return `0x8a720004` without writing anything (tested with all-null and zeroed 16 KiB buffers in a firmware 13.60 payload): `kAMsjYHBbNQ` (SetVolume), `5m73zmLpsQ0` (ResetViewPosition), `EPeH4xuQ2XQ` (GetStatus), `TJ8lyRsjFY4` (ClearTrackerFts), `-XQVcLnyX68` (SetSlamCommand), `giQwQoHhYYA` (DfuStart), `zNc9IkgawMI`, `-tnG5UJPvAc` (GazeStartUserCalibration), `FPhFmG+jXuQ` (GazeStopUserCalibration), `CKlkvNngofQ`, `yL1CZaciFWE` (GazeSetUserCalibrationPoint), `qlzgyG3s0RY`, `kTIDrDLzMv0`, `9Gee0ef8a+U`, `ecZVjQdwmBI` (GazeSetEnabledEye). Game-process and post-init behaviour are unmeasured; throws after init succeeds

## change [Hmd2BeforeInitializeMinusOne]

- [Hmd2BeforeInitializeMinusOne](../../../core/libs/prx/libSceHmd2/Export.cpp) (libSceHmd2) - before `sceHmd2Initialize`, 12 exports return -1 without writing anything (tested with all-null and zeroed 16 KiB buffers in a firmware 13.60 payload): `EAkv8Mqt33E`/`Rv7bkmKvmz0` (RelocalizerStart/Stop), `1oPZizn0SNA`/`TwCpE-nNrlo` (TrackingStart/Stop), `jkqAeQfdtxw`/`+l0RQph81Z8` (LedDetectorStart/Stop), `qsmAtGw1Ncc`/`uKhwYoMsIsA` (ImageStart/Stop), `WNdaigr94dI`/`P6S5ZVWf-H0` (GenDataStart/Stop), `sun6NWE1sdg`/`--TFsJYXo04` (SlamTraceStart/Stop). Game-process and post-init behaviour are unmeasured; throws after init succeeds

# libSceHttp

## change [sceHttpAbortWaitRequest, sceHttpWaitRequest]

- [sceHttpAbortWaitRequest, sceHttpWaitRequest](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - abort takes only the epoll handle per [Orbital](https://github.com/AlexAltea/orbital/blob/master/tools/ida/db_types.json) and [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/main/src/SharpProspero/Interop/Net/Http.cs); behaviour/errors follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/network/http.cpp). On PS5 13.590.001 later waits/polls returned `SCE_HTTP_ERROR_ABORTED` (`0x80431080`; persistence and waking blocked waits unconfirmed). Waits woken by `sceHttpDestroyEpoll` or final `sceHttpTerm` return 0 events. Epoll calls and `sceHttpTerm` return `BEFORE_INIT` (`0x80431001`) without a live `sceHttpInit` context and `INVALID_ID` (`0x80431100`) for unknown ids; pointer handles risk stale-address reuse

# libSceHttpCache

## change [sceHttpCacheSystemInit and sceHttpCacheSystemClearAll]

- [sceHttpCacheSystemInit and sceHttpCacheSystemClearAll](../../../core/libs/prx/libSceHttpCache/Export.cpp) - wrappers at `0x2d570` and `0x2d680` in firmware 11.00 `libSceHttpCache.sprx` (SHA-256 `0d825d639b642a73b3ae7d7e0217f602104609d388cf8e0ae83ba50a8a2c2fd1`) validate a non-null path <= 0x400 bytes (`0x817a0012` on failure) and return 0 ignoring delegate status. Host copies via `CopyGuestString_nid_no_patch` (bad pointers/unterminated strings return `0x817a0012`); `SystemInit` returns 0 without storing, `SystemClearAll` throws via `NotImplemented_nid_no_patch`. Path conversion and disk layout are unmodeled; `SystemShutdown`, `SystemTerm`, and `SystemSendStatistics` return 0.

# libSceImeDialog

## change [sceImeDialogInit, sceImeDialogGetStatus, sceImeDialogGetResult, sceImeDialogAbort, sceImeDialogTerm]

- [sceImeDialogInit, sceImeDialogGetStatus, sceImeDialogGetResult, sceImeDialogAbort, sceImeDialogTerm](../../../core/libs/prx/libSceImeDialog/Export.cpp) (libSceImeDialog) - statuses (0-2), end statuses (0-2), and Init errors (`0x80bc0001`, `0x80bc0031`, `0x80bc0016`, `0x80bc0017`) follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/b2d7ec7d31f9ad9426db93afe079db8679b3e1e0/src/core/libraries/ime/ime_dialog.cpp) (only idle status 0 verified on PS5 11.60 payload). GetResult returns `0x80bc0002` for null (shadPS4: `0x80bc0031`) or unmapped pointers. Without an open dialog, Abort, Term, and valid-buffer GetResult return `0x80bc0107` writing nothing ([obSCEne](https://github.com/project-oops/obSCEne/blob/de5efc406319ade18df00654025494804405a7fb/reports/hardware/report-20261018-gt14-census.log#L530-L561), PS5 12.40). Abort on finished keeps `0x80bc0002`; Term closes running dialogs; accepted text writes once

# libSceNet

## change [sceNetGetNameToIndex and sceNetGetIfnameNumList]

- [sceNetGetNameToIndex and sceNetGetIfnameNumList](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - map ids via `sceNetGetIfName`'s 10-name table (0=loopback, 1=first up non-loopback with address, 2..9=matching host interface name). `sceNetGetNameToIndex` returns `EINVAL` on `-1` and `0x804101A4` (`ENODATA`) if absent; `sceNetGetIfnameNumList` checks init first (`0x804101c8`), rejects null/`capacity<=0`/`flags>1` with `EINVAL`, and lists ids 1..8 (flags 0) or 1, 0, 2..9 (flags 1). On PS5 11.60 `sceNetGetNameToIndex(0)` returned 2 before `sceNetInit` (unlike local `libSceNet.sprx` `ad19de2df01d46df`), and `1,4,5` / `1,0,4,5`; order without id 1 and other firmwares are unmeasured.

# libSceNgs2.native

## change [NGS2 ATRAC9 streaming]

- [NGS2 ATRAC9 streaming](../../../core/libs/prx/libSceNgs2.native/src/Atrac9.cpp) keeps duration/skip counts across compressed blocks. `APPEND` (2, `ONLY_DATA` in reference) ignores continuation sample/skip/repeat counts per [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/bb98f26433fa9e711c219aac1711fe8ecba5bf3a/src/libs/ngs2.cpp) (PPSA03747 sends 262044 bytes with 11310532 samples, then 16384-byte flags-3 blocks). Partial superframes buffer across blocks; completion callbacks allow refills, with the last waveform callback waiting for PCM drain. Buffering, callback timing, decoded-byte accounting, and starvation silence are unverified on hardware. Early-closed streams throw; fragmented repeats are unsupported; PCM retains existing behaviour.

# libSceNpCppWebApi

## change Vector<unsigned long>

- [sce::Np::CppWebApi::UserProfile::V1, ProfanityFilter::V2, CommunicationRestrictionStatus::V3](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - imported by an EOS module lacking `libSceNpCppWebApi.prx`. The 0x10-byte `ResponseHeaderBase` (count at 0x00, block at 0x08) matches PPSA21564/PPSA23732 (`0x14980`, `0x149a0`, `0x149b0`); `getHeaderValue` returns `0x80553512` on null block and throws otherwise. `WebApiFilterRequest` offset 0x00 is assumed. Offline, the five calls, `WebApiFilterRequestFactory::create`, and transaction `start`/`getResponseHeaders` return `SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN`, and response accessors throw. `Vector`/`Iterator` over `IntrusivePtr<BasicPresence>` throw on title-provided bodies; `Vector<unsigned long>` throws when non-empty
