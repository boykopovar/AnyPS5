#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioEncoder::Encode check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

enum class Access { ReadOnly, None };

struct GuardedPage {
    std::size_t size = 0;
    unsigned char* base = nullptr;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        base = mapped == MAP_FAILED ? nullptr : static_cast<unsigned char*>(mapped);
#endif
        Require(base != nullptr);
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    template<typename T> T* As() const { return reinterpret_cast<T*>(base); }

    void Protect(Access access) const {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(base, size, access == Access::ReadOnly ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
        Require(mprotect(base, size, access == Access::ReadOnly ? PROT_READ : PROT_NONE) == 0);
#endif
    }
};

struct FakeEncoder {
    const void* const* vtable;
    int sampleRateHz;
    std::size_t channels;
    std::uint64_t written;
    std::uint64_t reported;
    std::string calls;
    void* info;
    std::uint32_t rtpTimestamp;
    const std::int16_t* audio;
    std::size_t samples;
    WebRtcBuffer* encoded;
};

int APS5_VABI SampleRateHz(const WebRtcAudioEncoder* encoder) {
    auto* fake = reinterpret_cast<FakeEncoder*>(const_cast<WebRtcAudioEncoder*>(encoder));
    fake->calls += 'r';
    return fake->sampleRateHz;
}

std::size_t APS5_VABI NumChannels(const WebRtcAudioEncoder* encoder) {
    auto* fake = reinterpret_cast<FakeEncoder*>(const_cast<WebRtcAudioEncoder*>(encoder));
    fake->calls += 'c';
    return fake->channels;
}

void* APS5_VABI EncodeImpl(void* info, WebRtcAudioEncoder* encoder, std::uint32_t rtpTimestamp, const std::int16_t* audio, std::size_t samples, WebRtcBuffer* encoded) {
    auto* fake = reinterpret_cast<FakeEncoder*>(encoder);
    fake->calls += 'e';
    fake->info = info;
    fake->rtpTimestamp = rtpTimestamp;
    fake->audio = audio;
    fake->samples = samples;
    fake->encoded = encoded;
    encoded->size += fake->written;
    std::memcpy(info, &fake->reported, sizeof(fake->reported));
    return info;
}

void* APS5_VABI EncodeImplWithoutInfo(void* info, WebRtcAudioEncoder* encoder, std::uint32_t, const std::int16_t*, std::size_t, WebRtcBuffer*) {
    reinterpret_cast<FakeEncoder*>(encoder)->calls += 'e';
    return info;
}

std::array<const void*, 30> Vtable() {
    std::array<const void*, 30> vtable {};
    vtable[2] = reinterpret_cast<const void*>(&SampleRateHz);
    vtable[3] = reinterpret_cast<const void*>(&NumChannels);
    vtable[29] = reinterpret_cast<const void*>(&EncodeImpl);
    return vtable;
}

WebRtcAudioEncoder* Guest(FakeEncoder& fake) {
    return reinterpret_cast<WebRtcAudioEncoder*>(&fake);
}

void* Encode(void* info, FakeEncoder& fake, std::uint32_t rtpTimestamp, const std::int16_t* audio, std::size_t samples, WebRtcBuffer* encoded) {
    return _ZN6webrtc12AudioEncoder6EncodeEjN3rtc9ArrayViewIKsLln4711EEEPNS1_7BufferTIhLb0EEE_nid_postfix(info, Guest(fake), rtpTimestamp, audio, samples, encoded);
}

void TestEncodeForwardsToEncodeImpl() {
    const auto vtable = Vtable();
    FakeEncoder fake {vtable.data(), 48000, 2, 37, 37};
    std::array<std::int16_t, 960> audio {};
    WebRtcBuffer encoded {5, 64, nullptr};
    alignas(8) unsigned char info[64] {};
    Require(Encode(info, fake, 0x89ABCDEFu, audio.data(), audio.size(), &encoded) == info);
    Require(fake.calls == "cre");
    Require(fake.info == info && fake.rtpTimestamp == 0x89ABCDEFu && fake.audio == audio.data() && fake.samples == 960 && fake.encoded == &encoded);
    Require(encoded.size == 42);
}

void TestEncodeAcceptsAnEmptyPacket() {
    const auto vtable = Vtable();
    FakeEncoder fake {vtable.data(), 16000, 1, 0, 0};
    std::array<std::int16_t, 160> audio {};
    WebRtcBuffer encoded {};
    alignas(8) unsigned char info[64] {};
    Require(Encode(info, fake, 1, audio.data(), audio.size(), &encoded) == info);
    Require(fake.calls == "cre" && encoded.size == 0);
}

void TestWrongSampleCountThrowsBeforeEncoding() {
    const auto vtable = Vtable();
    FakeEncoder fake {vtable.data(), 48000, 2, 0, 0};
    std::array<std::int16_t, 480> audio {};
    WebRtcBuffer encoded {};
    alignas(8) unsigned char info[64] {};
    RequireThrows<std::runtime_error>([&] { Encode(info, fake, 0, audio.data(), audio.size(), &encoded); });
    Require(fake.calls == "cr");
    RequireThrows<std::runtime_error>([&] { Encode(info, fake, 0, audio.data(), audio.size(), nullptr); });
}

void TestMismatchedEncodedSizeThrows() {
    const auto vtable = Vtable();
    FakeEncoder fake {vtable.data(), 8000, 1, 10, 9};
    std::array<std::int16_t, 80> audio {};
    WebRtcBuffer encoded {3, 0, nullptr};
    alignas(8) unsigned char info[64] {};
    RequireThrows<std::runtime_error>([&] { Encode(info, fake, 0, audio.data(), audio.size(), &encoded); });
    Require(fake.calls == "cre" && encoded.size == 13);
}

void TestNullArgumentsThrow() {
    auto vtable = Vtable();
    FakeEncoder fake {vtable.data(), 8000, 1, 0, 0};
    std::array<std::int16_t, 80> audio {};
    WebRtcBuffer encoded {};
    alignas(8) unsigned char info[64] {};
    RequireThrows<std::invalid_argument>([&] { Encode(nullptr, fake, 0, audio.data(), audio.size(), &encoded); });
    RequireThrows<std::invalid_argument>([&] {
        _ZN6webrtc12AudioEncoder6EncodeEjN3rtc9ArrayViewIKsLln4711EEEPNS1_7BufferTIhLb0EEE_nid_postfix(info, nullptr, 0, audio.data(), audio.size(), &encoded);
    });
    RequireThrows<std::invalid_argument>([&] { Encode(info, fake, 0, audio.data(), audio.size(), nullptr); });
    Require(fake.calls == "cr");
    vtable[29] = nullptr;
    RequireThrows<std::invalid_argument>([&] { Encode(info, fake, 0, audio.data(), audio.size(), &encoded); });
    FakeEncoder empty {nullptr, 8000, 1, 0, 0};
    RequireThrows<std::invalid_argument>([&] { Encode(info, empty, 0, audio.data(), audio.size(), &encoded); });
}

}

void TestInaccessibleGuestPointersThrow() {
    auto vtable = Vtable();
    FakeEncoder fake {vtable.data(), 8000, 1, 0, 0};
    std::array<std::int16_t, 80> audio {};
    WebRtcBuffer encoded {3, 16, nullptr};
    alignas(8) unsigned char info[64] {};
    GuardedPage none;
    none.Protect(Access::None);
    RequireThrows<std::invalid_argument>([&] {
        _ZN6webrtc12AudioEncoder6EncodeEjN3rtc9ArrayViewIKsLln4711EEEPNS1_7BufferTIhLb0EEE_nid_postfix(info, none.As<WebRtcAudioEncoder>(), 0, audio.data(), audio.size(), &encoded);
    });
    FakeEncoder hiddenVtable {none.As<const void*>(), 8000, 1, 0, 0};
    RequireThrows<std::invalid_argument>([&] { Encode(info, hiddenVtable, 0, audio.data(), audio.size(), &encoded); });
    Require(hiddenVtable.calls.empty());
    RequireThrows<std::invalid_argument>([&] { Encode(info, fake, 0, audio.data(), audio.size(), none.As<WebRtcBuffer>()); });
    Require(fake.calls == "cr");
    vtable[29] = reinterpret_cast<const void*>(&EncodeImplWithoutInfo);
    fake.calls.clear();
    RequireThrows<std::invalid_argument>([&] { Encode(none.As<void>(), fake, 0, audio.data(), audio.size(), &encoded); });
    Require(fake.calls == "cre" && encoded.size == 3);
}

int main() {
    TestEncodeForwardsToEncodeImpl();
    TestEncodeAcceptsAnEmptyPacket();
    TestWrongSampleCountThrowsBeforeEncoding();
    TestMismatchedEncodedSizeThrows();
    TestNullArgumentsThrow();
    TestInaccessibleGuestPointersThrow();
    return 0;
}
