#include "prx/libSceImeBackend/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

void CheckParamInit() {
    sceImeBackendParamInit(nullptr);
    for (const unsigned char fill : {static_cast<unsigned char>(0xaa), static_cast<unsigned char>(0x00), static_cast<unsigned char>(0xff)}) {
        std::array<unsigned char, 0x40> buffer{};
        buffer.fill(fill);
        sceImeBackendParamInit(buffer.data() + 8);
        for (std::size_t i = 0; i < buffer.size(); ++i) {
            const bool inside = i >= 8 && i < 8 + 0x28;
            const unsigned char expected = !inside ? fill : i < 12 ? 0xff : 0;
            Require(buffer[i] == expected);
        }
        for (std::size_t i = 8; i < 8 + 0x28; ++i) buffer[i] = 0x5Au;
        sceImeBackendParamInit(buffer.data() + 8);
        for (std::size_t i = 0; i < buffer.size(); ++i) {
            const bool inside = i >= 8 && i < 8 + 0x28;
            const unsigned char expected = !inside ? fill : i < 12 ? 0xff : 0;
            Require(buffer[i] == expected);
        }
    }
}

void CheckForTestFunction() {
    for (int call = 0; call < 3; ++call) {
        Require(sceImeBackendForTestFunction() == static_cast<int>(0x80bc00ffu));
    }
}

}

int main() {
    CheckParamInit();
    CheckForTestFunction();
    return 0;
}
