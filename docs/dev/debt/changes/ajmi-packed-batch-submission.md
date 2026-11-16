# libSceAjmi

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - the parameterless declarations remain throwing import-resolution placeholders, not verified ABI contracts. Packed-batch submission is covered below. Neither version label independently authenticates retail firmware provenance. Data exports require separate layout and ownership analysis. Candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

## add Packed batch submission

- [sceAjmiBatchStartBuffer](../../../core/libs/prx/libSceAjmi/Export.cpp) inherits the native signature and [known gaps](libSceAjm.native.md). The firmware 13.42 symbol-table report in [#15495](https://github.com/boykopovar/AnyPS5/pull/15495) places it at the same address as `sceAjmBatchStartBuffer` in `libSceAjmi.sprx` (SHA-256 `ff0dc985c1f7a6b25a766904e3a535b55e44d0d4b8872329d2bd6c019491df0b`). Its use is recorded in the [Opus CELT module investigation](https://github.com/boykopovar/AnyPS5/pull/10563#issuecomment-6412138282), not a console measurement. Jobs execute synchronously; priority is not validated. Codec 23 requires stream-only flags. Unsupported chunks and codec operations retain native throws; inaccessible batch and result memory is not generally validated. Named and NID-patched tests exercise PCM jobs, malformed lengths, null arguments, shared batch handles and unsupported chunks. No console or title execution or additional codec support is established.
