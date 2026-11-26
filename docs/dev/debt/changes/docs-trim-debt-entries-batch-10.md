# libSceAgc

## change [sceAgcDcbDrawIndexMultiInstanced]

- [sceAgcDcbDrawIndexMultiInstanced](../../../core/libs/prx/libSceAgc/DcbDraw/src/DrawIndexed.cpp) (libSceAgc) - argument semantics are unknown. It emits the console's 15 dwords (firmware 13.40, retail title): tag `0xc6000006`, `DRAW_INDEX_MULTI_INST` (`0x3a`) with the first three args in order and the fourth twice (first copy >= 1), initiator `0x80` (`+0x20` when modifier bit 8 is set and bit 32 is clear, matching other draw encoders; console writes `0xa0` for `0x55555555`, prosper `agcbuild_DcbDrawIndexMultiInstanced_a`), then `0xc6000000`; rejects standard draw modifier bits without other checks. Current parameter names (index count, index address, object IDs, instance count) are unverified vs AMD's PFP layout (instance count, object ID address, max size, index base, index count, initiator). The driver refuses the packet.

## change [sceAgcDcbSetWorkloadsActive and sceAgcDcbSetWorkloadComplete]

- [sceAgcDcbSetWorkloadsActive and sceAgcDcbSetWorkloadComplete](../../../core/libs/prx/libSceAgc/DcbState/src/Workload.cpp) (libSceAgc) - writes [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/main/src/libs/agc.cpp) packets: 18-dword (active) or 12-dword (complete) `IT_NOP` with stream id, plus 64-bit workload mask (active) or workload id and clear mask (complete). Returns null for null command buffer, unregistered/out-of-range stream, workload id > 63, or (for active) duplicate id or count 0 or > 63. Layouts and checks come solely from KytyPS5; [prosper](https://github.com/mattias800/prosper/blob/main/prosper/tests/data/console_oracle/agcbuild.golden.tsv) only verifies null return for unregistered streams. `PPSA29866` calls `sceAgcDcbSetWorkloadsActive` while building command buffers (`sceAgcDcbSetWorkloadComplete` unobserved).

# libSceAgcDriver

## change [Resource registration]

- [Resource registration](../../../core/libs/prx/libSceAgcDriver/Resource/src/Registration.cpp) (libSceAgcDriver) - always fails with `RESOURCE_REGISTRATION_UNAVAILABLE`, so owner/resource queries, `sceAgcDriverSetResourceUserData`, `sceAgcDriverFindResourcesPublic`, `sceAgcDriverUnregisterOwnerAndResources`, `sceAgcDriverUnregisterResource`, and `sceAgcDriverUnregisterAllResourcesForOwner` return it without reading arguments (parameter lists from libSceGnmDriver, unverified). `sceAgcDriverGetResourceRegistrationMaxNameLength` also returns `RESOURCE_REGISTRATION_UNAVAILABLE` and writes nothing even for null output, matching [obSCEne](https://github.com/project-oops/obSCEne/blob/de5efc406319ade18df00654025494804405a7fb/reports/hardware/report-20261013-172400.log#L25935-L25948) (PS5 FW 12.40) and KytyPS5 (prosper writes `0xfc`, sharpemu `256`). `uint32_t*` matches `sce::Agc::ResourceRegistration::getMaxNameLength`.

# libSceAudioPropagation

## change [sceAudioPropagationResetAttributes](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) implements the 4.03 two-pass contract

- [sceAudioPropagationResetAttributes](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) implements the 4.03 two-pass contract (SHA-256 `59065A3AA06E44F8123CF2985B7FD89F26194662C433E9FE379748666DF0974B`: validator `0x8ab0`, fill `0x2c80`): validates per-id value sizes (16B for 0-3, 6, `0x10000`-`0x10005`; 24B for 4; 8B for 5, 7, `0x20000`) and live room handles (two for `0x10001`, one for `0x20000`, skipped if zero), then fills fixed patterns for ids 0-7 and `0x10001`-`0x10004`; `0x10000`, `0x10005`, and `0x20000` validate but fail in pass 2. Newer firmware accepts attribute 10 with declared size `0x6d0` while resetting through `0x6f0` (`0x20` past validated range); cross-version behavior is unresolved, so attribute 10 throws `NotImplemented` instead of returning the 4.03 refusal.

# libSceCustomMusicCore

## change sceCustomMusicCoreBgmClose, sceCustomMusicCoreBgmOpen, sceCustomMusicCoreBgmOpen2

- [sceCustomMusicCoreBgmClose, sceCustomMusicCoreBgmOpen, sceCustomMusicCoreBgmOpen2, sceCustomMusicCoreBgmOutput, sceCustomMusicCoreBgmSetAudioVolume, sceCustomMusicCoreBgmStart, sceCustomMusicCoreBgmStop, sceCustomMusicCoreGetBgmAuthorityStatus, sceCustomMusicCoreGetHdcpStatus, sceCustomMusicCoreGetSystemAudioVolume, sceCustomMusicCorePostEvent, sceCustomMusicCoreSendEvent, sceCustomMusicCoreSendMulticastEvent, sceCustomMusicCoreSendSpMessage, sceCustomMusicCoreSetSystemAudioVolume, sceCustomMusicCoreStartOperation, sceCustomMusicCoreStartOperation2 and sceCustomMusicCoreStopOperation](../../../core/libs/prx/libSceCustomMusicCore/Export.cpp) (libSceCustomMusicCore) - names/NIDs match the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCustomMusicCore.c) without credible prototypes. Parameterless declarations resolve imports only (no ABI claim; do not call before verifying signatures).

# libSceHmd2

## change [Hmd2BeforeInitializeParameterNull]

- [Hmd2BeforeInitializeParameterNull](../../../core/libs/prx/libSceHmd2/Export.cpp) (libSceHmd2) - pre-`sceHmd2Initialize`, the console returns `0x8a720008` without writing for all-null or zeroed 16 KiB buffer args across 16 exports: `MNtiJTDOPAU`, `RRy9WmqZfgU` (`sceHmd2InternalDecryptData`), `uobR4m9lgwc` (`sceHmd2InternalGetMirroringMode`), `-XPk5KWBYdo` (`sceHmd2InternalRelocalizerSetResultData`), `J44idh9izZM`, `Ps0wAvYaa1E`, `kLnMR8oQ1MY`, `kMz4rbBxvjo`, `DIU1sFRRaiE` (`sceHmd2GazeGetCalibrationData`), `ZQvU4vDzZGQ` (`sceHmd2GazeSetUserCalibrationData`), `TJpyGGP9YEY`, `Dih+Odk+RC0`, `oaqdSJw+HVY`, `m35gtHuanXU`, `CTM7QPLJF9Q` (`sceHmd2InternalGetFlipInfo`), `+rExHeC9lP0` (`sceHmd2GazeObjectSelectionInitialize`). Measured in a FW 13.60 payload (game pre-init assumed identical); args and post-init behavior are unmeasured, and the handler throws after `sceHmd2Initialize` succeeds.

# libSceLibcInternal

## change [getopt, getopt_long]

- [getopt, getopt_long](../../../core/libs/prx/libSceLibcInternal/Getopt.cpp) (libSceLibcInternal) follow FreeBSD 11 [getopt.c](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdlib/getopt.c) and [getopt_long.c](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdlib/getopt_long.c) with `GNU_COMPATIBLE`: `getopt` never permutes, `getopt_long` permutes unless `POSIXLY_CORRECT` is set or optstring starts with `+`, and `opterr`/`optind`/`optopt`/`optreset`/`optarg` start at 1/1/0/0/null. Warnings go to current guest `stderr` prefixed with `__progname` (FreeBSD `warnx` caches initial `stderr`). Null optstring, null argv/arg, negative `optind`, or `optind` moved inside pending permutation throw where FreeBSD faults. `getopt_long_only` is unexported. Console FreeBSD provenance is unverified.

# libSceNgs2.native

## change [sceNgs2RackGetVoiceHandle voice index]

- [sceNgs2RackGetVoiceHandle voice index](../../../core/libs/prx/libSceNgs2.native/src/Rack.cpp) (libSceNgs2.native) - out-of-range voice index returns `0x804A8305` leaving output handle untouched, after invalid-rack (`0x804A8202`) and null-output (`0x804A8010`) checks. Verified in `libSceNgs2.native.sprx` export `MwmHz8pAdAo` (FW 10.01 `0x11f10`, 12.70 `0x12130`, all 1.00–12.70 in [#5916](https://github.com/boykopovar/AnyPS5/pull/5916#issuecomment-6172832590)): out-of-range index uses the same error helper, writing output only on success. *Heretic + Hexen* (`PPSA26525`), *DOOM + DOOM II* (`PPSA21444`), and *STAR WARS: Dark Forces Remaster* (`PPSA19728`) create a 1-voice reverb rack, take voice 0, and request voices 1–3 at startup, logging the error.

# libSceNpSns

## change sceNpSnsFacebookAbortRequest, sceNpSnsIntAbortRequest, sceNpSnsIntCreateRequest

- [sceNpSnsFacebookAbortRequest, sceNpSnsIntAbortRequest, sceNpSnsIntCreateRequest, sceNpSnsIntDeleteRequest, sceNpSnsIntFbGetGameAccessToken, sceNpSnsIntFbGetGameAccessTokenAllowed, sceNpSnsIntFbGetSystemAccessToken, sceNpSnsIntFbGetTitleInfo, sceNpSnsIntLinkedStatus, sceNpSnsIntTwGetSystemAccessToken, sceNpSnsIntUnlink, sceNpSnsIntYtGetAccessToken, sceNpSnsIntYtRefreshMasterToken, sceNpSnsTwitchAbortRequest, sceNpSnsTwitchCreateRequest, sceNpSnsTwitchDeleteRequest, sceNpSnsTwitchGetAccessToken, sceNpSnsYouTubeAbortRequest, sceNpSnsYouTubeCreateRequest, sceNpSnsYouTubeDeleteRequest and sceNpSnsYouTubeGetAccessToken](../../../core/libs/prx/libSceNpSns/Export.cpp) (libSceNpSns) - names/NIDs match the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNpSns.c) without credible prototypes. Parameterless declarations resolve imports only (no ABI claim; do not call before verifying signatures).

# libSceUserService

## change [13.60 inventory exports]

- [13.60 inventory exports](../../../core/libs/prx/libSceUserService/Inventory1360Exports.cpp) - exports from [PS5 13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceuserservice-ec97f04de9.jsonl) for import resolution only. 8/12 entries are absent from the pinned symbol DB; each hashes to its NID, with 4 resolved (`AccessibilityKeyremapEnableAston`, `Hmd2CinematicAudioRotation`, `IsPersonalizedHrtfEnabled`, `UserWithHearingAssist`) and the rest using literal NID aliases (none in pinned 3.20 catalogue). Signatures, return types, and behavior are unverified; parameterless `int` stubs are placeholders, all throwing via `NotImplemented_nid_no_patch` with no runtime implementation.
