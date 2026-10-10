#include "prx/libSceAgc/Misc/include/PacketInfo.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <tuple>

extern "C" int APS5_VABI sceAgcGetDataPacketPayloadRange(SceAgcMemoryRange* range, std::uint32_t* cmd, int type);
extern "C" int APS5_VABI sceAgcGetDataPacketPayloadAddress_0090(std::uint32_t** addr, std::uint32_t* cmd, int type);

namespace {

using Testing::Require;

template<typename TAction>
void ExpectFailure(TAction action) {
    const auto error = Testing::RequireThrows<std::runtime_error>(action, "expected an exception");
    Require(error.what()[0] != '\0', "empty exception message");
}

void VerifyRanges() {
    const std::array<std::tuple<std::uint32_t, int, std::ptrdiff_t, std::uint64_t>, 10> rows{{
        {0xc0047600u, 1, 2, 16}, {0xc0001000u, 1, 2, 0}, {0xffff7600u, 1, 2, 0xfffc}, {0xc0047600u, -1, 2, 16},
        {0xc0047600u, 7, 2, 16}, {0xc0021000u, 0, 1, 12}, {0xc0001000u, 0, 1, 4}, {0xfffe1000u, 0, 1, 0xfffc},
        {0xffff1000u, 0, -1, 0}, {0xffff7600u, 0, -1, 0}}};
    for (const auto& [header, type, offset, size] : rows) {
        std::array<std::uint32_t, 2> words{header, 0x12345678u};
        SceAgcMemoryRange range{reinterpret_cast<void*>(std::uintptr_t{0x1000}), 0xdeadu};
        Require(sceAgcGetDataPacketPayloadRange(&range, words.data(), type) == 0, "payload range failed");
        void* expected = offset < 0 ? nullptr : static_cast<void*>(words.data() + offset);
        Require(range.base == expected, "payload range base mismatch");
        Require(range.size == size, "payload range size mismatch");
        Require(words[0] == header && words[1] == 0x12345678u, "payload range query modified the packet");
        std::uint32_t* address = nullptr;
        Require(sceAgcGetDataPacketPayloadAddress_0090(&address, words.data(), type) == 0 && address == range.base,
              "payload range base differs from the payload address");
    }
}

void VerifyRejections() {
    std::array<std::uint32_t, 2> words{0xc0047600u, 0u};
    SceAgcMemoryRange range{};
    ExpectFailure([&] { sceAgcGetDataPacketPayloadRange(nullptr, words.data(), 1); });
    ExpectFailure([&] { sceAgcGetDataPacketPayloadRange(&range, nullptr, 1); });
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(words.data()) + 1);
    ExpectFailure([&] { sceAgcGetDataPacketPayloadRange(&range, misaligned, 0); });
    std::array<unsigned char, sizeof(SceAgcMemoryRange) + 8> storage{};
    auto* misalignedRange = reinterpret_cast<SceAgcMemoryRange*>(storage.data() + 4);
    if (reinterpret_cast<std::uintptr_t>(misalignedRange) % alignof(SceAgcMemoryRange) == 0) {
        misalignedRange = reinterpret_cast<SceAgcMemoryRange*>(storage.data() + 2);
    }
    ExpectFailure([&] { sceAgcGetDataPacketPayloadRange(misalignedRange, words.data(), 1); });
}

}

namespace {

const Testing::Case ranges{"GetPayloadRange_KnownHeaders_ReportsPayloadAddressAndSize", [] {
    VerifyRanges();
}};

const Testing::Case rejections{"GetPayloadRange_InvalidArguments_Throws", [] {
    VerifyRejections();
}};

} // namespace

int main(int argc, char** argv) {
    const int result = Testing::Run(argc, argv);
    LibcRunShutdown_nid_postfix();
    return result;
}
