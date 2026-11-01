#include "prx/libSceImeBackend/include/Api.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) { if (!value) std::abort(); }

void CheckParamInit() {
    sceImeBackendParamInit(nullptr);
    const unsigned char fills[] = {0xaa, 0x00, 0xff};
    for (const unsigned char fill : fills) {
        unsigned char buffer[0x40];
        std::memset(buffer, fill, sizeof(buffer));
        sceImeBackendParamInit(buffer + 8);
        for (std::size_t i = 0; i < sizeof(buffer); ++i) {
            const bool inside = i >= 8 && i < 8 + 0x28;
            const unsigned char expected = !inside ? fill : i < 12 ? 0xff : 0;
            Require(buffer[i] == expected);
        }
    }
}

}

int main() {
    CheckParamInit();
    return 0;
}
