#include "prx/libSceVrHand/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrInvalidArg = static_cast<int>(0x8a870006);
constexpr std::uint8_t kFill = 0xaa;

std::array<std::uint8_t, 0x40> ValidParam() {
    std::array<std::uint8_t, 0x40> param{};
    param[0] = 0x20;
    return param;
}

bool Untouched(const std::array<std::uint8_t, 0x28>& out) {
    for (const auto value : out) if (value != kFill) return false;
    return true;
}

int Query(const std::array<std::uint8_t, 0x40>& param, std::array<std::uint8_t, 0x28>& out) {
    out.fill(kFill);
    return sceVrHandQueryMemory(param.data(), out.data());
}

}

int main() {
    std::array<std::uint8_t, 0x28> out{};
    auto param = ValidParam();

    out.fill(kFill);
    Require(sceVrHandQueryMemory(nullptr, out.data()) == kErrInvalidArg && Untouched(out));
    Require(sceVrHandQueryMemory(param.data(), nullptr) == kErrInvalidArg);

    for (const std::uint32_t size : {0u, 0x1fu, 0x21u, 0x40u, 0x120u, 0x10020u, 0x1000020u}) {
        param = ValidParam();
        std::memcpy(param.data(), &size, sizeof(size));
        Require(Query(param, out) == kErrInvalidArg && Untouched(out));
    }
    for (std::size_t index = 4; index < 0x20; ++index) {
        param = ValidParam();
        param[index] = 1;
        Require(Query(param, out) == kErrInvalidArg && Untouched(out));
    }

    param = ValidParam();
    std::memset(param.data() + 0x20, 0xcc, 0x20);
    Require(Query(param, out) == 0);
    std::uint32_t at0 = 0;
    std::uint32_t at4 = 0;
    std::memcpy(&at0, out.data(), sizeof(at0));
    std::memcpy(&at4, out.data() + 4, sizeof(at4));
    Require(at0 == 0x3c00000);
    Require(at4 == 0x10000);
    for (std::size_t index = 8; index < 0x1c; ++index) Require(out[index] == 0);
    for (std::size_t index = 0x1c; index < out.size(); ++index) Require(out[index] == kFill);
    return 0;
}
