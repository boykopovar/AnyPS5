# libwebrtc

## change [194 exports (59 without a known name, bound by NID)]

- [191 exports (59 without a known name, bound by NID)](../../../core/libs/prx/libwebrtc/Export.cpp) (libwebrtc) - every function export of `libwebrtc.sprx` from the firmware 13.60 export table that the next section does not implement, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export throws through `NotImplemented_nid_no_patch`

## add Public source implementations

- [webrtc::rnn_vad::RnnVad constructor, destructor and ComputeVadProbability](../../../core/libs/prx/libwebrtc/src/RnnVad.cpp) (libwebrtc) - layout, allocator and rounding are unchecked on the console. Adapted from M134 [rnn_vad](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/modules/audio_processing/agc2/rnn_vad) `rnn`, `rnn_fc`, `rnn_gru` and `vector_math`, identical at M127, and [rnnoise](https://chromium.googlesource.com/chromium/src/third_party/+/d212a71416ad7c6ceca04d6d27dfdd84fd00c04c/rnnoise/src) weights and activations, with their notices. The 536-byte object follows the headers; vectors are begin, end and capacity over host `new[]`, `FunctionView`s hold host functions. SSE2 and AVX2 dot products run portably in intrinsic order, AVX2 fused; other fused multiply-adds on the console would change last bits. Null, unreadable or unconstructed objects or inputs throw. Closes when: compared with the console.
