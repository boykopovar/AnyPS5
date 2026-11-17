#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PAL::SessionID check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

using EnableGenerationProtection = void (APS5_VABI *)();
using GenerateSessionID = PAL::SessionID (APS5_VABI *)();
static_assert(std::is_same_v<decltype(&_ZN3PAL9SessionID26enableGenerationProtectionEv), EnableGenerationProtection>);
static_assert(std::is_same_v<decltype(&_ZN3PAL9SessionID26generateEphemeralSessionIDEv), GenerateSessionID>);
static_assert(std::is_same_v<decltype(&_ZN3PAL9SessionID27generatePersistentSessionIDEv), GenerateSessionID>);

void TestCounters() {
    Require(_ZN3PAL9SessionID27generatePersistentSessionIDEv().m_identifier == 2);
    Require(_ZN3PAL9SessionID27generatePersistentSessionIDEv().m_identifier == 3);
    Require(_ZN3PAL9SessionID26generateEphemeralSessionIDEv().m_identifier == 0x8000000000000002);
    Require(_ZN3PAL9SessionID26generateEphemeralSessionIDEv().m_identifier == 0x8000000000000003);
    Require(_ZN3PAL9SessionID27generatePersistentSessionIDEv().m_identifier == 4);
    Require(_ZN3PAL9SessionID26generateEphemeralSessionIDEv().m_identifier == 0x8000000000000004);
    std::printf("PAL::SessionID generators: first IDs and independent counters passed\n");
}

void TestGenerationProtection() {
    _ZN3PAL9SessionID26enableGenerationProtectionEv();
    RequireThrows([] { _ZN3PAL9SessionID27generatePersistentSessionIDEv(); });
    RequireThrows([] { _ZN3PAL9SessionID26generateEphemeralSessionIDEv(); });
    _ZN3PAL9SessionID26enableGenerationProtectionEv();
    RequireThrows([] { _ZN3PAL9SessionID27generatePersistentSessionIDEv(); });
    RequireThrows([] { _ZN3PAL9SessionID26generateEphemeralSessionIDEv(); });
    std::printf("PAL::SessionID generation protection: both generators throw after enabling\n");
}

}

int main() {
    TestCounters();
    TestGenerationProtection();
}
