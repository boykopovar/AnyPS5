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
    std::fprintf(stderr, "libwebrtc AudioDeviceBuffer check failed at line %u\n", location.line());
    std::abort();
}

constexpr std::size_t kStorageSize = 256;

struct alignas(8) Storage {
    unsigned char bytes[kStorageSize];
};

WebRtcAudioDeviceBuffer* Fill(Storage& storage) {
    std::memset(storage.bytes, 0xA5, sizeof(storage.bytes));
    auto* buffer = reinterpret_cast<WebRtcAudioDeviceBuffer*>(storage.bytes);
    buffer->playing = false;
    buffer->recording = false;
    return buffer;
}

void RequireOnlyChanged(const Storage& storage, std::size_t offset, std::size_t size) {
    for (std::size_t index = 0; index < kStorageSize; ++index) {
        if (index >= offset && index < offset + size) continue;
        if (index == 56 || index == 57) continue;
        Require(storage.bytes[index] == 0xA5);
    }
}

void TestSampleRatesAndChannels() {
    Storage storage;
    WebRtcAudioDeviceBuffer* buffer = Fill(storage);
    Require(_ZN6webrtc17AudioDeviceBuffer22SetRecordingSampleRateEj_nid_postfix(buffer, 48000) == 0);
    std::uint32_t rate = 0;
    std::memcpy(&rate, storage.bytes + 32, sizeof(rate));
    Require(rate == 48000);
    RequireOnlyChanged(storage, 32, 4);

    buffer = Fill(storage);
    Require(_ZN6webrtc17AudioDeviceBuffer20SetPlayoutSampleRateEj_nid_postfix(buffer, 0xFFFFFFFFu) == 0);
    std::memcpy(&rate, storage.bytes + 36, sizeof(rate));
    Require(rate == 0xFFFFFFFFu);
    RequireOnlyChanged(storage, 36, 4);

    buffer = Fill(storage);
    Require(_ZN6webrtc17AudioDeviceBuffer20SetRecordingChannelsEm_nid_postfix(buffer, 0x100000002ull) == 0);
    std::uint64_t channels = 0;
    std::memcpy(&channels, storage.bytes + 40, sizeof(channels));
    Require(channels == 0x100000002ull);
    RequireOnlyChanged(storage, 40, 8);

    buffer = Fill(storage);
    Require(_ZN6webrtc17AudioDeviceBuffer18SetPlayoutChannelsEm_nid_postfix(buffer, 0) == 0);
    std::memcpy(&channels, storage.bytes + 48, sizeof(channels));
    Require(channels == 0);
    RequireOnlyChanged(storage, 48, 8);
}

void TestRegisterAudioCallback() {
    Storage storage;
    WebRtcAudioDeviceBuffer* buffer = Fill(storage);
    auto* callback = reinterpret_cast<WebRtcAudioTransport*>(storage.bytes + 128);
    Require(_ZN6webrtc17AudioDeviceBuffer21RegisterAudioCallbackEPNS_14AudioTransportE_nid_postfix(buffer, callback) == 0);
    Require(buffer->audioTransportCallback == callback);
    RequireOnlyChanged(storage, 24, 8);
    Require(_ZN6webrtc17AudioDeviceBuffer21RegisterAudioCallbackEPNS_14AudioTransportE_nid_postfix(buffer, nullptr) == 0);
    Require(buffer->audioTransportCallback == nullptr);

    buffer = Fill(storage);
    buffer->playing = true;
    Require(_ZN6webrtc17AudioDeviceBuffer21RegisterAudioCallbackEPNS_14AudioTransportE_nid_postfix(buffer, callback) == -1);
    RequireOnlyChanged(storage, 0, 0);

    buffer = Fill(storage);
    buffer->recording = true;
    Require(_ZN6webrtc17AudioDeviceBuffer21RegisterAudioCallbackEPNS_14AudioTransportE_nid_postfix(buffer, callback) == -1);
    RequireOnlyChanged(storage, 0, 0);
}

void FillFlags(Storage& storage, bool playing, bool recording) {
    std::memset(storage.bytes, 0xA5, sizeof(storage.bytes));
    auto* buffer = reinterpret_cast<WebRtcAudioDeviceBuffer*>(storage.bytes);
    buffer->playing = playing;
    buffer->recording = recording;
}

void RequireOnlyOffsetChanged(const Storage& before, const Storage& after, std::size_t offset) {
    for (std::size_t index = 0; index < kStorageSize; ++index) {
        if (index == offset) continue;
        Require(after.bytes[index] == before.bytes[index]);
    }
}

void TestStartStopPlayout() {
    Storage storage;
    FillFlags(storage, false, false);
    auto* buffer = reinterpret_cast<WebRtcAudioDeviceBuffer*>(storage.bytes);

    Storage before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer12StartPlayoutEv_nid_postfix(buffer) == 0);
    Require(buffer->playing);
    Require(!buffer->recording);
    RequireOnlyOffsetChanged(before, storage, 56);

    before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer12StartPlayoutEv_nid_postfix(buffer) == 0);
    Require(buffer->playing);
    RequireOnlyOffsetChanged(before, storage, 56);

    before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer11StopPlayoutEv_nid_postfix(buffer) == 0);
    Require(!buffer->playing);
    Require(!buffer->recording);
    RequireOnlyOffsetChanged(before, storage, 56);

    before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer11StopPlayoutEv_nid_postfix(buffer) == 0);
    Require(!buffer->playing);
    RequireOnlyOffsetChanged(before, storage, 56);

    FillFlags(storage, true, false);
    before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer12StartPlayoutEv_nid_postfix(buffer) == 0);
    Require(buffer->playing);
    RequireOnlyOffsetChanged(before, storage, 56);

    FillFlags(storage, false, true);
    before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer11StopPlayoutEv_nid_postfix(buffer) == 0);
    Require(!buffer->playing);
    Require(buffer->recording);
    RequireOnlyOffsetChanged(before, storage, 56);
}

void TestStartStopRecording() {
    Storage storage;
    FillFlags(storage, false, false);
    auto* buffer = reinterpret_cast<WebRtcAudioDeviceBuffer*>(storage.bytes);

    Storage before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer14StartRecordingEv_nid_postfix(buffer) == 0);
    Require(buffer->recording);
    Require(!buffer->playing);
    RequireOnlyOffsetChanged(before, storage, 57);

    before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer14StartRecordingEv_nid_postfix(buffer) == 0);
    Require(buffer->recording);
    RequireOnlyOffsetChanged(before, storage, 57);

    before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer13StopRecordingEv_nid_postfix(buffer) == 0);
    Require(!buffer->recording);
    Require(!buffer->playing);
    RequireOnlyOffsetChanged(before, storage, 57);

    before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer13StopRecordingEv_nid_postfix(buffer) == 0);
    Require(!buffer->recording);
    RequireOnlyOffsetChanged(before, storage, 57);

    FillFlags(storage, false, true);
    before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer14StartRecordingEv_nid_postfix(buffer) == 0);
    Require(buffer->recording);
    RequireOnlyOffsetChanged(before, storage, 57);

    FillFlags(storage, true, false);
    before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer13StopRecordingEv_nid_postfix(buffer) == 0);
    Require(!buffer->recording);
    Require(buffer->playing);
    RequireOnlyOffsetChanged(before, storage, 57);
}

void TestPlayoutAndRecordingAreIndependent() {
    Storage storage;
    FillFlags(storage, false, false);
    auto* buffer = reinterpret_cast<WebRtcAudioDeviceBuffer*>(storage.bytes);

    Require(_ZN6webrtc17AudioDeviceBuffer12StartPlayoutEv_nid_postfix(buffer) == 0);
    Require(_ZN6webrtc17AudioDeviceBuffer14StartRecordingEv_nid_postfix(buffer) == 0);
    Require(buffer->playing);
    Require(buffer->recording);

    Storage before = storage;
    Require(_ZN6webrtc17AudioDeviceBuffer12StartPlayoutEv_nid_postfix(buffer) == 0);
    RequireOnlyOffsetChanged(before, storage, 56);

    Require(_ZN6webrtc17AudioDeviceBuffer11StopPlayoutEv_nid_postfix(buffer) == 0);
    Require(!buffer->playing);
    Require(buffer->recording);

    Require(_ZN6webrtc17AudioDeviceBuffer13StopRecordingEv_nid_postfix(buffer) == 0);
    Require(!buffer->playing);
    Require(!buffer->recording);
}

template <typename Call>
void RequireInvalidArgument(Call call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

void TestNullBufferThrows() {
    RequireInvalidArgument([] { _ZN6webrtc17AudioDeviceBuffer22SetRecordingSampleRateEj_nid_postfix(nullptr, 16000); });
    RequireInvalidArgument([] { _ZN6webrtc17AudioDeviceBuffer20SetPlayoutSampleRateEj_nid_postfix(nullptr, 16000); });
    RequireInvalidArgument([] { _ZN6webrtc17AudioDeviceBuffer20SetRecordingChannelsEm_nid_postfix(nullptr, 1); });
    RequireInvalidArgument([] { _ZN6webrtc17AudioDeviceBuffer18SetPlayoutChannelsEm_nid_postfix(nullptr, 1); });
    RequireInvalidArgument([] { _ZN6webrtc17AudioDeviceBuffer21RegisterAudioCallbackEPNS_14AudioTransportE_nid_postfix(nullptr, nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc17AudioDeviceBuffer12StartPlayoutEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc17AudioDeviceBuffer11StopPlayoutEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc17AudioDeviceBuffer14StartRecordingEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc17AudioDeviceBuffer13StopRecordingEv_nid_postfix(nullptr); });
}

}

int main() {
    TestSampleRatesAndChannels();
    TestRegisterAudioCallback();
    TestStartStopPlayout();
    TestStartStopRecording();
    TestPlayoutAndRecordingAreIndependent();
    TestNullBufferThrows();
    return 0;
}
