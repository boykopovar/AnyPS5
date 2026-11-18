#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

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

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

void TestParseResultMove() {
    int frame = 0;
    WebRtcAudioDecoderParseResult source {0x12345678u, -3, &frame};
    WebRtcAudioDecoderParseResult moved;
    std::memset(&moved, 0xA5, sizeof(moved));
    _ZN6webrtc12AudioDecoder11ParseResultC1EOS1__nid_postfix(&moved, &source);
    Require(moved.timestamp == 0x12345678u && moved.priority == -3 && moved.frame == &frame);
    Require(source.frame == nullptr && source.timestamp == 0x12345678u && source.priority == -3);

    WebRtcAudioDecoderParseResult empty {7, 1, nullptr};
    _ZN6webrtc12AudioDecoder11ParseResultC1EOS1__nid_postfix(&moved, &empty);
    Require(moved.timestamp == 7 && moved.priority == 1 && moved.frame == nullptr);

    WebRtcAudioDecoderParseResult self {9, 2, &frame};
    _ZN6webrtc12AudioDecoder11ParseResultC1EOS1__nid_postfix(&self, &self);
    Require(self.timestamp == 9 && self.priority == 2 && self.frame == &frame);
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc12AudioDecoder11ParseResultC1EOS1__nid_postfix(nullptr, &empty); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc12AudioDecoder11ParseResultC1EOS1__nid_postfix(&moved, nullptr); });
}

void TestParseResultDestructor() {
    WebRtcAudioDecoderParseResult empty {1, 2, nullptr};
    _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(&empty);
    Require(empty.timestamp == 1 && empty.priority == 2 && empty.frame == nullptr);
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(nullptr); });
}

struct FakeFrame {
    const void* const* vtable;
    int deleted = 0;
};

int g_firstDeleted = 0;
int g_secondDeleted = 0;
const void* g_firstVtable[4];
const void* g_secondVtable[4];

void APS5_VABI DeleteFirst(void* frame) {
    ++g_firstDeleted;
    ++static_cast<FakeFrame*>(frame)->deleted;
}

void APS5_VABI DeleteSecond(void* frame) {
    ++g_secondDeleted;
    ++static_cast<FakeFrame*>(frame)->deleted;
}

void TestParseResultDestructorDeletesFrame() {
    g_firstVtable[1] = reinterpret_cast<const void*>(DeleteFirst);
    g_secondVtable[1] = reinterpret_cast<const void*>(DeleteSecond);
    FakeFrame first{g_firstVtable};
    FakeFrame second{g_secondVtable};
    WebRtcAudioDecoderParseResult owner {5, 1, &first};
    _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(&owner);
    Require(g_firstDeleted == 1 && g_secondDeleted == 0 && first.deleted == 1 && second.deleted == 0);
    WebRtcAudioDecoderParseResult other {6, 2, &second};
    _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(&other);
    Require(g_firstDeleted == 1 && g_secondDeleted == 1 && second.deleted == 1);
    WebRtcAudioDecoderParseResult moved;
    WebRtcAudioDecoderParseResult source {7, 3, &first};
    _ZN6webrtc12AudioDecoder11ParseResultC1EOS1__nid_postfix(&moved, &source);
    _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(&source);
    Require(g_firstDeleted == 1);
    _ZN6webrtc12AudioDecoder11ParseResultD1Ev_nid_postfix(&moved);
    Require(g_firstDeleted == 2 && first.deleted == 2);
}

int main() {
    TestPlc();
    TestErrorCode();
    TestParseResultMove();
    TestParseResultDestructor();
    TestParseResultDestructorDeletesFrame();
    return 0;
}
