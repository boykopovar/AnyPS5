#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <utility>

extern "C" std::uint32_t APS5_VABI sceAgcGetPacketSize(std::uint32_t* packet);

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

void testPacketSize() {
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 7> sizes{{
        {0xc0047600u, 6}, {0xc0001000u, 2}, {0xfffe1000u, 0x4000}, {0xffff7600u, 0x4001},
        {0xffff1000u, 1}, {0xffff1001u, 1}, {0xffff10fcu, 1}}};
    for (const auto& [header, size] : sizes) {
        std::array<std::uint32_t, 2> words{header, 0xffff1000u};
        check(sceAgcGetPacketSize(words.data()) == size, "packet size mismatch");
        check(words[0] == header && words[1] == 0xffff1000u, "packet size query modified the packet");
    }
}

void testRejections() {
    for (const auto header : {0x80000000u, 0x3fff1000u, 0x40001000u, 0x00047600u}) {
        std::uint32_t word = header;
        expectFailure([&] { sceAgcGetPacketSize(&word); });
    }
    expectFailure([] { sceAgcGetPacketSize(nullptr); });
    std::array<std::uint32_t, 2> words{0x00100000u, 0x000000c0u};
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(words.data()) + 1);
    expectFailure([&] { sceAgcGetPacketSize(misaligned); });
}

}

int main() {
    try {
        testPacketSize();
        testRejections();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC packet info tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
