#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

std::array<unsigned char, 64> Filled(unsigned char value) {
    std::array<unsigned char, 64> bytes{};
    std::memset(bytes.data(), value, bytes.size());
    return bytes;
}

void Require(bool condition) {
    if (!condition) std::abort();
}

void RequireZeroedPrefix(const std::array<unsigned char, 64>& bytes, std::size_t zeroed) {
    for (std::size_t i = 0; i < bytes.size(); ++i) Require(bytes[i] == (i < zeroed ? 0 : 0x5A));
}

void RequireUnchanged(const std::array<unsigned char, 64>& bytes, unsigned char value) {
    for (const auto byte : bytes) Require(byte == value);
}

} // namespace

int main() {
    Require(_ZN3JSC11RegisterSet14stackRegistersEv() == 0x30);
    Require(_ZN3JSC11RegisterSet25reservedHardwareRegistersEv() == 0);
    Require(_ZN3JSC11RegisterSet21macroScratchRegistersEv() == 0x800);
    Require(_ZN3JSC11RegisterSet19calleeSaveRegistersEv() == 0xF028);
    Require(_ZN3JSC11RegisterSet12argumentGPRSEv() == 0x3C6);
    Require(_ZN3JSC11RegisterSet7allGPRsEv() == 0xFFFF);
    Require(_ZN3JSC11RegisterSet7allFPRsEv() == 0xFFFF0000u);

    struct Constructor {
        void (APS5_VABI *construct)(void*);
        std::size_t zeroed;
    };
    for (const Constructor constructor : {
             Constructor{_ZN10Deprecated11ScriptValueC1Ev, 8},
             Constructor{_ZN10Deprecated11ScriptValueC2Ev, 8},
             Constructor{_ZN9Inspector13AgentRegistryC1Ev, 16},
             Constructor{_ZN7bmalloc8FreeListC2Ev, 32},
         }) {
        auto object = Filled(0x5A);
        constructor.construct(object.data());
        RequireZeroedPrefix(object, constructor.zeroed);
    }

    struct LockedConstructor {
        void (APS5_VABI *construct)(void*, const void*);
        std::size_t zeroed;
    };
    for (const LockedConstructor constructor : {
             LockedConstructor{_ZN7bmalloc11AllIsoHeapsC1ERKSt11scoped_lockIJNS_5MutexEEE, 8},
             LockedConstructor{_ZN7bmalloc11AllIsoHeapsC2ERKSt11scoped_lockIJNS_5MutexEEE, 8},
             LockedConstructor{_ZN7bmalloc12IsoTLSLayoutC1ERKSt11scoped_lockIJNS_5MutexEEE, 16},
             LockedConstructor{_ZN7bmalloc12IsoTLSLayoutC2ERKSt11scoped_lockIJNS_5MutexEEE, 16},
         }) {
        auto object = Filled(0x5A);
        const auto lock = Filled(0xA5);
        constructor.construct(object.data(), lock.data());
        RequireZeroedPrefix(object, constructor.zeroed);
        RequireUnchanged(lock, 0xA5);
    }
}
