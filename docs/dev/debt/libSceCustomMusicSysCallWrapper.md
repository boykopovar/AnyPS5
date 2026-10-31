# libSceCustomMusicSysCallWrapper technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceCustomMusicSysCallWrapper/Export.cpp) - 101 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

[Version-pinned selected ELF export records](https://gist.github.com/kyon-eth/3fbb1a4a6f95a936b4cd58e9c3d8c50c/5e3af709224ed8235af11337309af0725e5685cb) list every NID below with its image label, decoded export module and library, raw qualified symbol, ELF function type, value and size for each inspected version-labelled image. The extract SHA-256 is `02ab27f12bcd0cc9fa03fa33f55fc201110f0d54a86bbbaa093d55d0a8cf262d`; its full-module sorted-function-NID digests and raw module-parameter words permit independent comparison. This is an analyst-produced metadata record, not independent authentication of the original images or proof of prototypes or behavior.

| Name | NID |
| --- | --- |
| `sceCustomMusicAudiodecClearContext` | `WsJoyOz47OE` |
| `sceCustomMusicAudiodecCpuDecode` | `mz62R6Qjmf0` |
| `sceCustomMusicAudiodecCreateDecoder` | `QCuN-9Dd1bg` |
| `sceCustomMusicAudiodecDecode` | `qqFUG6MJKcU` |
| `sceCustomMusicAudiodecDeleteDecoder` | `4rOCTBP87dQ` |
| `sceCustomMusicFileClose` | `E0KbSONVDcU` |
| `sceCustomMusicFileLseek` | `go-ecEub9P0` |
| `sceCustomMusicFileOpenRead` | `V4A7LcYAdgA` |
| `sceCustomMusicFileRead` | `xrrLysMOFr4` |
| `sceCustomMusicKernelGettimeofday` | `GlUEBpsSb2E` |
| `sceCustomMusicKernelNanosleep` | `SsMe-3HXEHE` |
| `sceCustomMusicNetAccept` | `DELu3JawLrk` |
| `sceCustomMusicNetBind` | `EOO+r1P-9Nc` |
| `sceCustomMusicNetConnect` | `wvD8sZ7XghA` |
| `sceCustomMusicNetCtlInit` | `jORbkQLFfRM` |
| `sceCustomMusicNetCtlTerm` | `ZqJXAMKrqus` |
| `sceCustomMusicNetEpollAbort` | `Injn5VOe4Gg` |
| `sceCustomMusicNetEpollControl` | `H466Vy6VBLk` |
| `sceCustomMusicNetEpollCreate` | `5TsLV1cUWTc` |
| `sceCustomMusicNetEpollDestroy` | `XcF5UiPx-pc` |
| `sceCustomMusicNetEpollWait` | `FQ3hKAacgR8` |
| `sceCustomMusicNetErrnoLoc` | `E62C8Og0N5E` |
| `sceCustomMusicNetInetPton` | `DRqtzRNDa7s` |
| `sceCustomMusicNetListen` | `m2UfRq+Z1NU` |
| `sceCustomMusicNetPoolCreate` | `D62ZUAXrLTM` |
| `sceCustomMusicNetPoolDestroy` | `4cT5gPSCILs` |
| `sceCustomMusicNetRecv` | `Plv2m81jN84` |
| `sceCustomMusicNetResolverAbort` | `nvJMgVPsXUo` |
| `sceCustomMusicNetResolverCreate` | `lWQRcz0JroQ` |
| `sceCustomMusicNetResolverDestroy` | `NwCCxIIlm6I` |
| `sceCustomMusicNetSend` | `3+ODHrZgJRc` |
| `sceCustomMusicNetSetsockopt` | `2rJROSjbRZw` |
| `sceCustomMusicNetSocketClose` | `uAM3M24ikfU` |
| `sceCustomMusicNetSocket` | `bl5G0iE1jE4` |
| `sceCustomMusicPthreadAttrDestroy` | `UKFhulDRkuo` |
| `sceCustomMusicPthreadAttrInit` | `rKSYoakH9zg` |
| `sceCustomMusicPthreadCondDestroy` | `364AWhMvihw` |
| `sceCustomMusicPthreadCondInit` | `u0ACyDAN+gk` |
| `sceCustomMusicPthreadCondWait` | `34KbODwYX1s` |
| `sceCustomMusicPthreadCreate` | `BPtJo-zdOJI` |
| `sceCustomMusicPthreadDetach` | `iZNSjGzrgBc` |
| `sceCustomMusicPthreadExit` | `0cnReUY0C94` |
| `sceCustomMusicPthreadJoin` | `EtoqDoZoLFQ` |
| `sceCustomMusicPthreadMutexDestroy` | `gFxwjwKPvzc` |
| `sceCustomMusicPthreadMutexInit` | `7cqf8yfv218` |
| `sceCustomMusicPthreadMutexLock` | `HCNWPzFo2ak` |
| `sceCustomMusicPthreadSetprio` | `L7x6JPdSMtQ` |
| `sceCustomMusicPthreadYield` | `4LwHgkQnNSY` |
| `Unknown` | `+LdekyVYugM` |
| `Unknown` | `-f8PcMdF8L0` |
| `Unknown` | `0WBrHbI33Ec` |
| `Unknown` | `0d4Aw8u3xzE` |
| `Unknown` | `1auNQHxva3I` |
| `Unknown` | `24ZHuc5b0RA` |
| `Unknown` | `2yE7PehS6iI` |
| `Unknown` | `3DyCYcJudI4` |
| `Unknown` | `5I2IqwB0Hes` |
| `Unknown` | `5p7ZpbyUKuo` |
| `Unknown` | `6PNhaGAw6Ho` |
| `Unknown` | `70NNIr-nfFo` |
| `Unknown` | `7uOVo0lL97g` |
| `Unknown` | `8t8UK2XkJ1Y` |
| `Unknown` | `CQv51NIK3i4` |
| `Unknown` | `DABJz+GrAWg` |
| `Unknown` | `EaC0JWOBvy4` |
| `Unknown` | `HNx-bHhat18` |
| `Unknown` | `J4+14VzlTHM` |
| `Unknown` | `JSZXadyt-nk` |
| `Unknown` | `Kxq7i4lQjGg` |
| `Unknown` | `L0MmfM0N-y8` |
| `Unknown` | `Mk6mbbOvaBM` |
| `Unknown` | `NbAJfB19zUk` |
| `Unknown` | `QEVVJH35Fis` |
| `Unknown` | `ROnMWfJ56t8` |
| `Unknown` | `SneVIJUdrLw` |
| `Unknown` | `Tr-kghWgcoI` |
| `Unknown` | `VDAd516qud8` |
| `Unknown` | `VTS+zlRk-wo` |
| `Unknown` | `Xkm5tRxA0UQ` |
| `Unknown` | `Y3l3L6A4OYE` |
| `Unknown` | `ZiO9aSV1ZKc` |
| `Unknown` | `bzddXxOnr30` |
| `Unknown` | `eTHTXNt0B2k` |
| `Unknown` | `fsQhNY+RJf8` |
| `Unknown` | `gxew9cxe+xk` |
| `Unknown` | `hd0MG4ZvfRA` |
| `Unknown` | `jETJjJLQtas` |
| `Unknown` | `jVD2jFJAQ28` |
| `Unknown` | `kso0cI0kq1U` |
| `Unknown` | `lgRL6Oblmyg` |
| `Unknown` | `mgSEL99WSwA` |
| `Unknown` | `n0aVoomYuOM` |
| `Unknown` | `n4jZ2HjzXUo` |
| `Unknown` | `oaaW7CuQLV0` |
| `Unknown` | `qqNVevi1LlY` |
| `Unknown` | `sWVPeXELNss` |
| `Unknown` | `sak+IpFmBkk` |
| `Unknown` | `y2Fu+vFwEXo` |
| `Unknown` | `yM-etuyf-2M` |
| `Unknown` | `zRDEHOFDvzQ` |
| `Unknown` | `zUgaEYLIysQ` |

### Unknown function info

- [ and sceCustomMusicSelect](../../../core/libs/prx/libSceCustomMusicSysCallWrapper/Export.cpp) (libSceCustomMusicSysCallWrapper) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceCustomMusicSysCallWrapper.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
