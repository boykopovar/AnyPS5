#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>

extern "C" std::uint32_t APS5_VABI sceAgcGetPacketSize(std::uint32_t* packet);

namespace {

using Testing::Require;

template<typename TAction>
void ExpectFailure(TAction action) {
    const auto error = Testing::RequireThrows<std::runtime_error>(action, "expected an exception");
    Require(error.what()[0] != '\0', "empty exception message");
}

void VerifyPacketSize() {
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 7> sizes{{
        {0xc0047600u, 6}, {0xc0001000u, 2}, {0xfffe1000u, 0x4000}, {0xffff7600u, 0x4001},
        {0xffff1000u, 1}, {0xffff1001u, 1}, {0xffff10fcu, 1}}};
    for (const auto& [header, size] : sizes) {
        std::array<std::uint32_t, 2> words{header, 0xffff1000u};
        Require(sceAgcGetPacketSize(words.data()) == size, "packet size mismatch");
        Require(words[0] == header && words[1] == 0xffff1000u, "packet size query modified the packet");
    }
}

void VerifyRejections() {
    for (const auto header : {0x80000000u, 0x3fff1000u, 0x40001000u, 0x00047600u}) {
        std::uint32_t word = header;
        ExpectFailure([&] { sceAgcGetPacketSize(&word); });
    }
    ExpectFailure([] { sceAgcGetPacketSize(nullptr); });
    std::array<std::uint32_t, 2> words{0x00100000u, 0x000000c0u};
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(words.data()) + 1);
    ExpectFailure([&] { sceAgcGetPacketSize(misaligned); });
}

}

namespace {

const Testing::Case packetSize{"GetPacketSize_KnownHeaders_ReportsDwordCount", [] {
    VerifyPacketSize();
}};

const Testing::Case rejections{"GetPacketSize_InvalidOrMisalignedPacket_Throws", [] {
    VerifyRejections();
}};

} // namespace

int main(int argc, char** argv) {
    const int result = Testing::Run(argc, argv);
    LibcRunShutdown_nid_postfix();
    return result;
}
