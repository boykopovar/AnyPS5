# libSceTextToSpeech technical debt

### Unknown function info

- [sceTextToSpeechGetStatusImpl and sceTextToSpeechInitImpl](../../../core/libs/prx/libSceTextToSpeech/Export.cpp) (libSceTextToSpeech) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceTextToSpeech.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified

- [sceTextToSpeechCancelImpl, sceTextToSpeechCloseImpl, sceTextToSpeechOpenImpl, sceTextToSpeechPauseImpl, sceTextToSpeechResumeImpl, sceTextToSpeechSpeakContinuousImpl, sceTextToSpeechSpeakImpl, sceTextToSpeechTermImpl and NID `6ZLFpZLuqEk`](../../../core/libs/prx/libSceTextToSpeech/Export.cpp) (libSceTextToSpeech) - `sceTextToSpeechInitImpl` throws, so the library is never initialized; these answer as firmware 13.42 does then, claiming only the 32-bit first argument and result. From disassembly of `libSceTextToSpeech.sprx` (SHA-256 `76c4b8869383ea1b92a4e13c9239bee045a63ba2ab7ff452c464d22c3d5f7cee`), not run on a console: a first argument above 2 (unsigned), or not 1 or 2 for Pause, Resume and `6ZLFpZLuqEk`, or not above 0 (signed) for SpeakContinuous, returns `0x814900FF`; otherwise the state word at `0x18048` is read, and while it is 0 they return `0x81490020`. It starts zero and only `sceTextToSpeechInitImpl` (`0x319f`) sets it from 0.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceTextToSpeech/Export.cpp) - 1 entry observed in the 3.20 export inventory and present in the later 13.60-labelled inventory is an import-resolution declaration only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. It throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `L-pHC7qtEug` |
