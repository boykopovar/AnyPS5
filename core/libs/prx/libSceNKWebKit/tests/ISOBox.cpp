#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using NKWebKit::WebCoreAbi::ISOBox;
using NKWebKit::WebCoreAbi::ISOFullBox;
using NKWebKit::WebCoreAbi::ISOWebVTTCue;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ISOBox check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template <typename T>
void Store(unsigned char* object, std::size_t offset, T value) {
    std::memcpy(object + offset, &value, sizeof(value));
}

const void* Address(const unsigned char* object, std::size_t offset) {
    return object + offset;
}

void TestBox() {
    alignas(16) unsigned char object[128];
    std::memset(object, 0xA5, sizeof(object));
    Store<std::uint64_t>(object, 8, 0x0123456789ABCDEFull);
    Store<std::uint32_t>(object, 16, 0x6D6F6F76u);
    Store<std::uint64_t>(object, 24, 0x1122334455667788ull);
    Store<std::uint32_t>(object, 32, 16u);
    Store<std::uint32_t>(object, 36, 12u);
    const auto* box = reinterpret_cast<const ISOBox*>(object);

    Require(_ZNK7WebCore6ISOBox4sizeEv(box) == 0x0123456789ABCDEFull);
    const auto& type = _ZNK7WebCore6ISOBox7boxTypeEv(box);
    Require(static_cast<const void*>(&type) == Address(object, 16));
    Require(type.value == 0x6D6F6F76u);
    const auto& extended = _ZNK7WebCore6ISOBox12extendedTypeEv(box);
    Require(static_cast<const void*>(&extended) == Address(object, 24));
    Require(reinterpret_cast<std::uintptr_t>(extended.buffer) == 0x1122334455667788ull);
    Require(extended.capacity == 16u);
    Require(extended.size == 12u);

    Store<std::uint64_t>(object, 8, 8u);
    Require(_ZNK7WebCore6ISOBox4sizeEv(box) == 8u);
}

void TestFullBox() {
    alignas(16) unsigned char object[128];
    std::memset(object, 0xA5, sizeof(object));
    Store<std::uint8_t>(object, 40, 1u);
    Store<std::uint32_t>(object, 44, 0x00ABCDEFu);
    const auto* box = reinterpret_cast<const ISOFullBox*>(object);

    Require(_ZNK7WebCore10ISOFullBox7versionEv(box) == 1u);
    Require(_ZNK7WebCore10ISOFullBox5flagsEv(box) == 0x00ABCDEFu);

    Store<std::uint8_t>(object, 40, 0xFFu);
    Store<std::uint32_t>(object, 44, 0u);
    Require(_ZNK7WebCore10ISOFullBox7versionEv(box) == 0xFFu);
    Require(_ZNK7WebCore10ISOFullBox5flagsEv(box) == 0u);
}

void TestCue() {
    alignas(16) unsigned char object[128];
    std::memset(object, 0xA5, sizeof(object));
    Store<std::int64_t>(object, 40, 900000);
    Store<std::uint32_t>(object, 48, 90000u);
    Store<std::uint8_t>(object, 52, 1u);
    Store<std::int64_t>(object, 56, 45000);
    Store<std::uint32_t>(object, 64, 1000u);
    Store<std::uint8_t>(object, 68, 1u);
    for (std::size_t offset = 72; offset <= 104; offset += 8) {
        Store<std::uintptr_t>(object, offset, 0x1000 + offset);
    }
    const auto* cue = reinterpret_cast<const ISOWebVTTCue*>(object);

    const auto& presentation = _ZNK7WebCore12ISOWebVTTCue16presentationTimeEv(cue);
    Require(static_cast<const void*>(&presentation) == Address(object, 40));
    Require(presentation.m_timeValue == 900000);
    Require(presentation.m_timeScale == 90000u);
    Require(presentation.m_timeFlags == 1u);
    const auto& duration = _ZNK7WebCore12ISOWebVTTCue8durationEv(cue);
    Require(static_cast<const void*>(&duration) == Address(object, 56));
    Require(duration.m_timeValue == 45000);
    Require(duration.m_timeScale == 1000u);
    Require(duration.m_timeFlags == 1u);

    const auto& sourceID = _ZNK7WebCore12ISOWebVTTCue8sourceIDEv(cue);
    Require(static_cast<const void*>(&sourceID) == Address(object, 72));
    Require(reinterpret_cast<std::uintptr_t>(sourceID.impl) == 0x1000 + 72);
    const auto& id = _ZNK7WebCore12ISOWebVTTCue2idEv(cue);
    Require(static_cast<const void*>(&id) == Address(object, 80));
    Require(reinterpret_cast<std::uintptr_t>(id.impl) == 0x1000 + 80);
    const auto& originalStartTime = _ZNK7WebCore12ISOWebVTTCue17originalStartTimeEv(cue);
    Require(static_cast<const void*>(&originalStartTime) == Address(object, 88));
    Require(reinterpret_cast<std::uintptr_t>(originalStartTime.impl) == 0x1000 + 88);
    const auto& settings = _ZNK7WebCore12ISOWebVTTCue8settingsEv(cue);
    Require(static_cast<const void*>(&settings) == Address(object, 96));
    Require(reinterpret_cast<std::uintptr_t>(settings.impl) == 0x1000 + 96);
    const auto& cueText = _ZNK7WebCore12ISOWebVTTCue7cueTextEv(cue);
    Require(static_cast<const void*>(&cueText) == Address(object, 104));
    Require(reinterpret_cast<std::uintptr_t>(cueText.impl) == 0x1000 + 104);

    const auto* box = reinterpret_cast<const ISOBox*>(object);
    Store<std::uint64_t>(object, 8, 0x58u);
    Store<std::uint32_t>(object, 16, _ZN7WebCore12ISOWebVTTCue11boxTypeNameEv().value);
    Require(_ZNK7WebCore6ISOBox4sizeEv(box) == 0x58u);
    Require(_ZNK7WebCore6ISOBox7boxTypeEv(box).value == 0x76747463u);
}

void TestConstants() {
    Require(_ZN7WebCore6ISOBox14minimumBoxSizeEv() == 8u);
    const auto name = _ZN7WebCore12ISOWebVTTCue11boxTypeNameEv();
    Require(name.value == 0x76747463u);
    const unsigned char bigEndian[4] = {
        static_cast<unsigned char>(name.value >> 24),
        static_cast<unsigned char>(name.value >> 16),
        static_cast<unsigned char>(name.value >> 8),
        static_cast<unsigned char>(name.value),
    };
    Require(std::memcmp(bigEndian, "vttc", 4) == 0);
}

}

int main() {
    TestBox();
    TestFullBox();
    TestCue();
    TestConstants();
    return 0;
}
