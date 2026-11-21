#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioDecoder Decode check failed at line %u\n", location.line());
    std::abort();
}

struct FakeDecoder {
    const void* vtable;
    int duration;
    std::size_t channels;
    int result;
    int path;
    std::array<int, 3> calls{};
    std::size_t callCount = 0;
    const std::uint8_t* encoded = nullptr;
    std::size_t encodedLen = 0;
    int sampleRate = 0;
    std::int16_t* decoded = nullptr;
    std::int32_t* speechType = nullptr;
};

int RecordDuration(const WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int path) {
    auto& fake = *reinterpret_cast<FakeDecoder*>(const_cast<WebRtcAudioDecoder*>(decoder));
    Require(fake.path == path);
    fake.calls.at(fake.callCount++) = 1;
    fake.encoded = encoded;
    fake.encodedLen = encodedLen;
    return fake.duration;
}

int APS5_VABI Duration(const WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen) {
    return RecordDuration(decoder, encoded, encodedLen, 0);
}

int APS5_VABI RedundantDuration(const WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen) {
    return RecordDuration(decoder, encoded, encodedLen, 1);
}

std::size_t APS5_VABI Channels(const WebRtcAudioDecoder* decoder) {
    auto& fake = *reinterpret_cast<FakeDecoder*>(const_cast<WebRtcAudioDecoder*>(decoder));
    fake.calls.at(fake.callCount++) = 2;
    return fake.channels;
}

int RecordDecode(WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRate, std::int16_t* decoded, std::int32_t* speechType, int path) {
    auto& fake = *reinterpret_cast<FakeDecoder*>(decoder);
    Require(fake.path == path);
    Require(fake.encoded == encoded && fake.encodedLen == encodedLen);
    fake.calls.at(fake.callCount++) = 3;
    fake.sampleRate = sampleRate;
    fake.decoded = decoded;
    fake.speechType = speechType;
    decoded[0] = 0x1234;
    decoded[1] = -1234;
    *speechType = path + 1;
    return fake.result;
}

int APS5_VABI Decode(WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRate, std::int16_t* decoded, std::int32_t* speechType) {
    return RecordDecode(decoder, encoded, encodedLen, sampleRate, decoded, speechType, 0);
}

int APS5_VABI RedundantDecode(WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRate, std::int16_t* decoded, std::int32_t* speechType) {
    return RecordDecode(decoder, encoded, encodedLen, sampleRate, decoded, speechType, 1);
}

using Vtable = std::array<void (APS5_VABI *)(), 15>;

template <typename TCallback>
void SetCallback(Vtable& vtable, std::size_t slot, TCallback callback) {
    static_assert(sizeof(callback) == sizeof(vtable[slot]));
    std::memcpy(&vtable[slot], &callback, sizeof(callback));
}

using Export = int (APS5_VABI *)(WebRtcAudioDecoder*, const std::uint8_t*, std::size_t, int, std::size_t, std::int16_t*, std::int32_t*);

void TestCase(Export function, int path, int duration, std::size_t channels, std::size_t capacity, int result, bool dispatch) {
    Vtable vtable{};
    SetCallback(vtable, 8, Duration);
    SetCallback(vtable, 9, RedundantDuration);
    if (duration >= 0) SetCallback(vtable, 12, Channels);
    SetCallback(vtable, 13, Decode);
    SetCallback(vtable, 14, RedundantDecode);
    FakeDecoder fake{vtable.data(), duration, channels, result, path};
    const std::uint8_t encoded[]{0x91, 0x37, 0xFA};
    std::int16_t decoded[]{-1, -1, -1, -1};
    std::int32_t speechType = 99;
    int actual = function(reinterpret_cast<WebRtcAudioDecoder*>(&fake), encoded, sizeof(encoded), 48000, capacity, decoded, &speechType);
    Require(actual == (dispatch ? result : -1));
    Require(fake.encoded == encoded && fake.encodedLen == sizeof(encoded));
    Require(fake.calls[0] == 1);
    Require(fake.callCount == (duration < 0 ? 2u : dispatch ? 3u : 2u));
    if (duration >= 0) Require(fake.calls[1] == 2);
    if (dispatch) {
        Require(fake.calls[fake.callCount - 1] == 3);
        Require(fake.sampleRate == 48000 && fake.decoded == decoded && fake.speechType == &speechType);
        Require(decoded[0] == 0x1234 && decoded[1] == -1234 && speechType == path + 1);
    } else {
        Require(decoded[0] == -1 && decoded[1] == -1 && speechType == 99);
        Require(fake.decoded == nullptr && fake.speechType == nullptr);
    }
    Require(decoded[2] == -1 && decoded[3] == -1);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    int path = std::strcmp(argv[1], "redundant") == 0 ? 1 : 0;
    Export function = path ? _ZN6webrtc12AudioDecoder15DecodeRedundantEPKhmimPsPNS0_10SpeechTypeE_nid_postfix : _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix;
    TestCase(function, path, 3, 1, 6, 17, true);
    TestCase(function, path, 3, 1, 5, 17, false);
    TestCase(function, path, 3, 2, 12, 23, true);
    TestCase(function, path, 3, 2, 11, 23, false);
    TestCase(function, path, 3, 24, 143, 23, false);
    TestCase(function, path, -1, 2, 0, -7, true);
    TestCase(function, path, -2, 2, 0, 0, true);
    TestCase(function, path, 0, 2, 0, -9, true);
    TestCase(function, path, 3, 0, 0, 29, true);
    TestCase(function, path, 1, std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max() - 2, 23, false);
    try {
        function(nullptr, nullptr, 0, 48000, 0, nullptr, nullptr);
        Require(false);
    } catch (const std::invalid_argument&) {
    }
    FakeDecoder empty{};
    try {
        function(reinterpret_cast<WebRtcAudioDecoder*>(&empty), nullptr, 0, 48000, 0, nullptr, nullptr);
        Require(false);
    } catch (const std::invalid_argument&) {
    }
    Vtable nullCallbacks{};
    empty.vtable = nullCallbacks.data();
    try {
        function(reinterpret_cast<WebRtcAudioDecoder*>(&empty), nullptr, 0, 48000, 0, nullptr, nullptr);
        Require(false);
    } catch (const std::invalid_argument&) {
    }
    return 0;
}
