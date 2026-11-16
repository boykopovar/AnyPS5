# libwebrtc

## change FileWrapper::Close, Flush, Write and its move constructor

- [FileWrapper::Close, Flush, Write and its move constructor](../../../core/libs/prx/libwebrtc/src/FileWrapper.cpp) - unverified on PS5; module revision sits between [M127](https://webrtc.googlesource.com/src/+/e0b28a6a81a989c1f5c89e30fcd247870047390d/rtc_base/system/file_wrapper.cc) and [M134](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/rtc_base/system/file_wrapper.cc) where these agree. Null, inaccessible, or unwritable wrappers (`GuestPtr` and `GuestWritable_nid_no_patch`), invalid move objects, non-open `Flush`/`Write`, and empty/inaccessible nonempty `Write` payloads throw (preserving `errno`); `Write` stages through 4096-byte host buffers, and file errors follow guest libc.

## change webrtc::WavReader::Reset, ReadSamples for int16_t and Close

- [webrtc::WavReader::Reset, ReadSamples for int16_t and Close](../../../core/libs/prx/libwebrtc/src/WavReader.cpp) (libwebrtc) - unverified on PS5; assumes 64-byte layout (vtable, rate 8, channels 16, format 24, total 32, unread 40, file 48, start 56) from [wav_file.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/common_audio/wav_file.cc), [audio_util.h](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/common_audio/include/audio_util.h) and [file_wrapper.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/rtc_base/system/file_wrapper.cc) (identical at M127). Readers use `GuestPtr`, `ReadSamples` probes destination writability before `fread` and stages 4096-sample chunks; failed checks or missing streams throw, and reads use guest libc. Closes when: compared with the console.
