#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioDecoder check failed at line %u\n", location.line());
    std::abort();
}

unsigned char decoderStorage[64];
unsigned char bufferStorage[32];
std::int16_t samples[64];

WebRtcAudioDecoder* Decoder() {
    std::memset(decoderStorage, 0xC3, sizeof(decoderStorage));
    std::memset(bufferStorage, 0xA5, sizeof(bufferStorage));
    for (std::int16_t& sample : samples)
        sample = 0x1234;
    return reinterpret_cast<WebRtcAudioDecoder*>(decoderStorage);
}

bool Unchanged() {
    for (unsigned char byte : decoderStorage)
        if (byte != 0xC3) return false;
    for (unsigned char byte : bufferStorage)
        if (byte != 0xA5) return false;
    for (std::int16_t sample : samples)
        if (sample != 0x1234) return false;
    return true;
}

void TestPlc() {
    WebRtcAudioDecoder* decoder = Decoder();
    Require(!_ZNK6webrtc12AudioDecoder12HasDecodePlcEv_nid_postfix(decoder));
    Require(_ZN6webrtc12AudioDecoder9DecodePlcEmPs_nid_postfix(decoder, 0, samples) == 0);
    Require(_ZN6webrtc12AudioDecoder9DecodePlcEmPs_nid_postfix(decoder, 3, samples) == 0);
    Require(_ZN6webrtc12AudioDecoder9DecodePlcEmPs_nid_postfix(decoder, 1, nullptr) == 0);
    _ZN6webrtc12AudioDecoder11GeneratePlcEmPN3rtc7BufferTIsLb0EEE_nid_postfix(decoder, 480, reinterpret_cast<WebRtcInt16Buffer*>(bufferStorage));
    _ZN6webrtc12AudioDecoder11GeneratePlcEmPN3rtc7BufferTIsLb0EEE_nid_postfix(decoder, 480, nullptr);
    Require(Unchanged());
}

void TestErrorCode() {
    WebRtcAudioDecoder* decoder = Decoder();
    Require(_ZN6webrtc12AudioDecoder9ErrorCodeEv_nid_postfix(decoder) == 0);
    Require(Unchanged());
}

}

int main() {
    TestPlc();
    TestErrorCode();
    return 0;
}
