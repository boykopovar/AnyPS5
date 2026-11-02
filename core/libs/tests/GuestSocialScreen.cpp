#include "prx/libSceSocialScreen/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrInvalidArg = static_cast<int>(0x81440003);

}

int main() {
    Require(sceSocialScreenInitializeSeparateModeParameter(nullptr) == kErrInvalidArg);
    std::array<std::uint8_t, 0x40> parameter{};
    parameter.fill(0xaa);
    Require(sceSocialScreenInitializeSeparateModeParameter(parameter.data()) == 0);
    Require(parameter[0] == 1 && parameter[1] == 0 && parameter[2] == 0 && parameter[3] == 0);
    for (std::size_t i = 4; i < 0x20; ++i) Require(parameter[i] == 0);
    for (std::size_t i = 0x20; i < parameter.size(); ++i) Require(parameter[i] == 0xaa);
    return 0;
}
