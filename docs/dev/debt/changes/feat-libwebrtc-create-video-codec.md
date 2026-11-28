# libwebrtc

## add Public source implementations

- [cricket::CreateVideoCodec(const SdpVideoFormat&) and cricket::Codec::~Codec](../../../core/libs/prx/libwebrtc/src/Codec.cpp) (libwebrtc) - unchecked on the console. From [codec.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/media/base/codec.cc) on a 264-byte layout derived from `codec.h` with the library's guest string, map and inlined-vector models; `std::optional<std::string>` is taken as the string followed by an engaged byte (48 bytes) and `std::vector<FeedbackParam>` as three pointers freed like the module's other vectors. The created codec points at a local vtable whose `ToCodecParameters` slot throws. An object that does not validate throws where upstream has undefined behaviour. Closes when: the layout and the vtable are compared with the console.
