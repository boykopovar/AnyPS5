#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "prx/libSceLibreSsl/include/Evp.hpp"

namespace {

constexpr std::size_t ChaChaBlockBytes = 64;

struct ChaChaState {
    std::uint32_t input[16];
    unsigned char keystream[ChaChaBlockBytes];
    unsigned char unused;
};

std::uint32_t LoadLittle32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8 | static_cast<std::uint32_t>(bytes[2]) << 16 |
        static_cast<std::uint32_t>(bytes[3]) << 24;
}

void KeySetup(ChaChaState& state, const unsigned char* key) {
    state.input[0] = 0x61707865;
    state.input[1] = 0x3320646e;
    state.input[2] = 0x79622d32;
    state.input[3] = 0x6b206574;
    for (int index = 0; index < 8; ++index) state.input[4 + index] = LoadLittle32(key + 4 * index);
    state.unused = 0;
}

void IvSetup(ChaChaState& state, const unsigned char* iv) {
    for (int index = 0; index < 4; ++index) state.input[12 + index] = LoadLittle32(iv + 4 * index);
    state.unused = 0;
}

}
