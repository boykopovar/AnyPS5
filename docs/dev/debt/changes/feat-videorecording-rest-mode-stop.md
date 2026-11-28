# libSceVideoRecording.native

## remove | `sceVideoRecordingRestModeProcessStop` | `F8pia9FXpPs` |

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceVideoRecording.native/Export.cpp) - 26 entries observed in a 3.20-labelled image and a later 13.60-labelled image are import-resolution declarations only. Neither image label authenticates a retail firmware version. Parameter lists, return types and behavior are unverified apart from the checks under Functional; the remaining parameterless `int` declarations are placeholders, not verified ABI contracts. Past those checks every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

## add Functional

- [sceVideoRecordingRestModeProcessStop](../../../core/libs/prx/libSceVideoRecording.native/Export.cpp) (libSceVideoRecording.native) - returns `0x80a80002`, the answer firmware 13.42 gives when the built-in recorder has no client ([disassembly](https://github.com/boykopovar/AnyPS5/issues/21884#issuecomment-6951018831), not measured on a console): the export at `0xceb0` (NID `F8pia9FXpPs`, which is the hash of this name) takes no arguments and has the shape of `RestModeProcessStart` and `RestModeProcessGetStatus`, with service method `0x4040001f`. No client is ever created here, as recorder initialization needs the recording service; with one, the call's service answer is unread.
