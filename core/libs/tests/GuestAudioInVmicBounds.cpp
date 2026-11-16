#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

constexpr int ErrorInvalidHandle = static_cast<int>(0x80260101);
constexpr int ErrorInvalidSize = static_cast<int>(0x80260102);
constexpr int ErrorInvalidFreq = static_cast<int>(0x80260103);
constexpr int ErrorInvalidPointer = static_cast<int>(0x80260105);
constexpr int ErrorInvalidParam = static_cast<int>(0x80260106);
constexpr int ErrorInvalidState = static_cast<int>(0x80260109);

constexpr int UserId = 0x10000000;
constexpr std::uint32_t HandleBase = 0x30000000;

void Require(bool value) {
    if (!value) std::abort();
}

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestVmicParameterBoundsAndPrecedence() {
    for (const std::uint32_t invalidLen : {0u, 1u, 64u, 96u, 127u, 129u, 255u, 257u, 512u, 1024u, 0xFFFFFFFFu}) {
        Require(sceAudioInVmicCreate(UserId, invalidLen, 0, 0) == ErrorInvalidSize);
        Require(sceAudioInVmicCreate(UserId, invalidLen, 8000, 2) == ErrorInvalidSize);
        Require(sceAudioInVmicCreate(UserId, invalidLen, 16000, 0) == ErrorInvalidSize);
        Require(sceAudioInVmicCreate(UserId, invalidLen, 48000, 2) == ErrorInvalidSize);
        Require(sceAudioInVmicCreate(0, invalidLen, 96000, 99) == ErrorInvalidSize);
    }

    for (const std::uint32_t validLen : {128u, 256u}) {
        for (const std::uint32_t invalidFreq : {0u, 1u, 8000u, 11025u, 22050u, 32000u, 44100u, 48001u, 96000u, 192000u, 0xFFFFFFFFu}) {
            Require(sceAudioInVmicCreate(UserId, validLen, invalidFreq, 0) == ErrorInvalidFreq);
            Require(sceAudioInVmicCreate(UserId, validLen, invalidFreq, 2) == ErrorInvalidFreq);
            Require(sceAudioInVmicCreate(0, validLen, invalidFreq, 99) == ErrorInvalidFreq);
        }

        for (const std::uint32_t validFreq : {16000u, 48000u}) {
            for (const int user : {UserId, 0, -1, 0x10000001}) {
                for (const std::uint32_t param : {0u, 1u, 2u, 0x10u, 99u}) {
                    Require(Throws([&] { return sceAudioInVmicCreate(user, validLen, validFreq, param); }));
                }
            }
        }
    }
}

void TestVmicDebtStubs() {
    Require(Throws([] { sceAudioInVmicDestroy(); }));
    Require(Throws([] { sceAudioInVmicWrite(); }));
    Require(Throws([] { sceAudioInVmicGetStatus(); }));
    Require(Throws([] { sceAudioInVmicSetMute(); }));
}

void TestPortLifecycleUnchanged() {
    const int handle = sceAudioInOpen(UserId, 0, 0, 256, 48000, 2);
    Require(handle >= static_cast<int>(HandleBase));

    Require(sceAudioInInput(handle, nullptr) == 0);
    Require(sceAudioInGetSilentState(handle) == 0);
    Require(sceAudioInGetRerouteCount(handle) == 0);

    TestVmicDebtStubs();

    Require(sceAudioInInput(handle, nullptr) == 0);
    Require(sceAudioInGetSilentState(handle) == 0);
    Require(sceAudioInGetRerouteCount(handle) == 0);

    Require(sceAudioInClose(handle) == 0);
    Require(sceAudioInClose(handle) == ErrorInvalidState);
    Require(sceAudioInInput(handle, nullptr) == ErrorInvalidState);
    Require(sceAudioInGetSilentState(handle) == ErrorInvalidState);
    Require(sceAudioInGetRerouteCount(handle) == ErrorInvalidState);
}

} // namespace

int main() {
    Require(sceAudioInInit() == 0);

    TestVmicParameterBoundsAndPrecedence();
    TestVmicDebtStubs();
    TestPortLifecycleUnchanged();

    return 0;
}
