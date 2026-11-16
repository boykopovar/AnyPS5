# libwebrtc

## add webrtc::AudioBuffer constructor

- [webrtc::AudioBuffer constructor](../../../core/libs/prx/libwebrtc/src/AudioBuffer.cpp) (libwebrtc) - console layout and allocator choice are unverified. Adapted from M134 `AudioBuffer`, `AudioBuffer::AudioBuffer` validates guest object writability with `GuestWritable_nid_no_patch` and copies the constructed state via `GuestPtr` (releasing allocated channel buffers, resamplers, and splitting filters on copy failure), while `CopyFrom` and `CopyTo` keep direct access on hot per-frame paths. Closes when: compared with the console.

## change rtc::VideoSinkWants constructors and destructor

- [rtc::VideoSinkWants constructors and destructor](../../../core/libs/prx/libwebrtc/src/VideoSinkWants.cpp) (libwebrtc) - console layout and allocator choice are unverified. Defaulted in M134 [video_source_interface.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video/video_source_interface.cc) and its header; M127 agrees except absl::optional becomes std::optional (64 bytes, vector at 24). Constructor, copy constructor, and destructor validate guest objects with `GuestWritable_nid_no_patch`, probe readable resolution elements in chunks before allocating, and copy via `GuestPtr`. Null, inaccessible, or malformed inputs throw without leaking. Closes when: compared with the console.

## change webrtc::PushSincResampler, webrtc::PushResampler<int16_t> and webrtc::voe::RemixAndResample

- [webrtc::PushSincResampler, webrtc::PushResampler<int16_t> and webrtc::voe::RemixAndResample](../../../core/libs/prx/libwebrtc/src/Resampler.cpp) (libwebrtc) - unverified on the console: 64- and 104-byte layouts with host pointees and scalar convolution. `PushSincResampler` ctor/dtor and `PushResampler<int16_t>::~PushResampler` validate writability with `GuestWritable_nid_no_patch` and copy structs via `GuestPtr`, while `Resample` and `RemixAndResample` keep direct access on hot per-frame DSP paths. Invalid counts or inaccessible targets throw. Closes when: compared with the console.

## change WebRtcOpus encoder functions

- [WebRtcOpus encoder functions](../../../core/libs/prx/libwebrtc/src/Opus.cpp) (libwebrtc) - 16 `WebRtcOpus_*` exports from [opus_interface.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/modules/audio_coding/codecs/opus/opus_interface.cc) backed by bundled libopus 1.6.1. `WebRtcOpus_EncoderCreate` and `WebRtcOpus_EncoderFree` validate writability with `GuestWritable_nid_no_patch`, copy the handle via `GuestPtr`, and verify live handle ownership before freeing (returning -1 on unwritable or unowned targets), while `WebRtcOpus_Encode` and per-frame setters keep direct access. Frames over 60 ms throw (`WEBRTC_OPUS_SUPPORT_120MS_PTIME` unknown). Closes when: compared with the console.

## change webrtc::ANAStats::ANAStats() and the AudioEncoder GetANAStats, GetDtx, SetTargetBitrate, DisableAudioNetworkAdaptor, OnReceivedRtt, OnReceivedOverhead and SetReceiverFrameLengthRange defaults

- [webrtc::ANAStats::ANAStats() and the AudioEncoder GetANAStats, GetDtx, SetTargetBitrate, DisableAudioNetworkAdaptor, OnReceivedRtt, OnReceivedOverhead and SetReceiverFrameLengthRange defaults](../../../core/libs/prx/libwebrtc/src/AudioEncoder.cpp) (libwebrtc) - adapted from [audio_encoder.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/audio_codecs/audio_encoder.cc) on the 56-byte `ANAStats` layout of seven libc++ optionals. Both `ANAStats::ANAStats()` and `GetANAStats()` validate writability with `GuestWritable_nid_no_patch`, read via `GuestPtr` to preserve untouched optional bytes, clear engaged/nullState fields, and write back via `GuestPtr`. Closes when: compared with the console.

## change webrtc::VideoEncoder::EncoderInfo constructor and destructor

- [webrtc::VideoEncoder::EncoderInfo constructor and destructor](../../../core/libs/prx/libwebrtc/src/VideoEncoderInfo.cpp) (libwebrtc) - nothing is checked on the console. Adapted from [video_encoder.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video_codecs/video_encoder.cc) on the 272-byte M132/M134 layout (`GuestStringStorage` for heap storage). Constructor and destructor validate writability with `GuestWritable_nid_no_patch`, copy via `GuestPtr`, and validate string/vector/inlined-vector invariants before freeing or writing back. Closes when: compared with the console.

## change webrtc::EchoCanceller3Config copy constructor and operator=

- [webrtc::EchoCanceller3Config copy constructor and operator=](../../../core/libs/prx/libwebrtc/src/EchoCanceller3Config.cpp) (libwebrtc) - copies all 608 bytes via `GuestPtr` (`EchoCanceller3Config()` reads via `GuestPtr` after `GuestWritable_nid_no_patch` to preserve padding bytes, writes default initializers, and copies back via `GuestPtr`; self-assignment probes writability). Firmware 13.42 leaves padding offsets unwritten. Adapted from [echo_canceller3_config.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/audio/echo_canceller3_config.cc). Null or inaccessible pointers throw. Closes when: compared with the console.

## change webrtc::EchoCanceller3Factory constructor

- [webrtc::EchoCanceller3Factory constructor](../../../core/libs/prx/libwebrtc/src/EchoCanceller3Factory.cpp) (libwebrtc) - nothing is checked on the console. Adapted from [echo_canceller3_factory.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/audio/echo_canceller3_factory.cc): validates and copies source `config` and destination `factory` via `GuestPtr`, writing the M133 vtable and 608-byte config atomically; `Create` overloads throw. Null or inaccessible guest pointers throw. Closes when: revision and console layout are compared.

## change webrtc::VideoDecoder::GetDecoderInfo

- [webrtc::VideoDecoder::GetDecoderInfo](../../../core/libs/prx/libwebrtc/src/VideoDecoder.cpp) (libwebrtc) - unchecked on console; guest string follows `libc` rules (`GuestStringStorage`). Adapted from [video_decoder.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video_codecs/video_decoder.cc): validates `info` writability, reads decoder vtable/slot 8 and the returned C-string via checked `GuestCopyIn_nid_no_patch`, and copies the 48-byte `DecoderInfo` out via `GuestPtr` (cleaning heap storage if copy-out fails). Closes when: compared with the console.

## change webrtc::VideoFrame(buffer, timestamp_rtp, render_time_ms, rotation) constructor and destructor

- [webrtc::VideoFrame(buffer, timestamp_rtp, render_time_ms, rotation) constructor and destructor](../../../core/libs/prx/libwebrtc/src/VideoFrame.cpp) (libwebrtc) - unchecked on console. Constructor validates writability and copies via `GuestPtr` (preserving `id_`), reads the buffer reference safely, invokes AddRef (slot 0), and writes back via `GuestPtr`; destructor reads via `GuestPtr`, throws via `NotImplemented_nid_no_patch` on `packet_infos_`, or invokes Release (slot 1) with checked vtable reads. Closes when: compared with the console.

## change webrtc::RtpExtension destructor

- [webrtc::RtpExtension destructor](../../../core/libs/prx/libwebrtc/src/RtpExtension.cpp) (libwebrtc) - unchecked on console. Adapted from [rtp_parameters.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/rtp_parameters.cc): validates extension writability via `GuestWritable_nid_no_patch` and copies via `GuestPtr`; reads large-allocation (>= 0x1000) header back-pointers via `GuestPtr` before freeing via guest `operator delete`. Null or inaccessible pointers throw. Closes when: compared with the console.

## change webrtc::I420BufferInterface::ToI420 and GetI420 and webrtc::VideoFrameBuffer::GetMappedFrameBuffer

- [webrtc::I420BufferInterface::ToI420 and GetI420 and webrtc::VideoFrameBuffer::GetMappedFrameBuffer](../../../core/libs/prx/libwebrtc/src/VideoFrameBuffer.cpp) (libwebrtc) - unchecked on console. Adapted from [video_frame_buffer.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video/video_frame_buffer.cc): `ToI420` and `GetMappedFrameBuffer` probe `result` writability, read buffer vtable/slots via `GuestPtr`/`GuestCopyIn_nid_no_patch`, and copy out via `GuestPtr`. Non-native types throw `std::runtime_error`; bad pointers throw `std::invalid_argument`. Closes when: compared with the console.

## change webrtc::ColorSpace copy constructor and operator=

- [webrtc::ColorSpace copy constructor and operator=](../../../core/libs/prx/libwebrtc/src/ColorSpace.cpp) (libwebrtc) - unchecked on console. Copies bytes 0..64 via `GuestBytes` through a host staging buffer (leaving tail padding 65..67 untouched), or probes `GuestWritable_nid_no_patch` on self-assignment. Follows [color_space.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video/color_space.cc). Null or inaccessible sources/destinations throw. Closes when: compared with the console.

## change webrtc::VideoEncoder legacy InitEncode

- [webrtc::VideoEncoder legacy InitEncode](../../../core/libs/prx/libwebrtc/src/VideoEncoderInitEncode.cpp) (libwebrtc) - console revision, `Settings` layout, and vtable dispatch remain unverified. Adapted from [video_encoder.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video_codecs/video_encoder.cc): reads guest vtable pointer and slot 4 callback via `GuestPtr`/`GuestCopyIn_nid_no_patch` while preserving `errno`, then invokes guest InitEncode. Closes when: compared with the console.

## change webrtc::CodecSpecificInfo constructor and destructor

- [webrtc::CodecSpecificInfo constructor and destructor](../../../core/libs/prx/libwebrtc/src/CodecSpecificInfo.cpp) (libwebrtc) - unchecked on console (2064-byte M132/M134 layout). Both constructor and destructor validate writability with `GuestWritable_nid_no_patch` and copy via `GuestPtr` (preserving untouched payload in constructor), copying `templateStructure` template descriptors via `GuestPtr` before validating and freeing child allocations. Closes when: compared with the console.

## change webrtc::EncodedImage constructor and destructor and EncodedImageBuffer::Create(const uint8_t*, size_t)

- [webrtc::EncodedImage constructor and destructor and EncodedImageBuffer::Create(const uint8_t*, size_t)](../../../core/libs/prx/libwebrtc/src/EncodedImage.cpp) (libwebrtc) - unchecked on console. `EncodedImageBuffer::Create`, `EncodedImage::EncodedImage()`, and `EncodedImage::~EncodedImage()` probe writability/readability in chunks (`GuestWritable_nid_no_patch`, `GuestBytes`, `GuestPtr`, `GuestCopyIn_nid_no_patch`) before allocating or freeing nodes/packet-infos, while small buffer accessors (`AddRefBuffer`, `BufferData`, `BufferSize`, `BufferHasOneRef`) keep direct access. Closes when: compared with the console.
