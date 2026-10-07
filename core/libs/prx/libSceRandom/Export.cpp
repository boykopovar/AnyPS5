#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <random>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_RANDOM_ERROR_INVALID = static_cast<int>(0x817C0016);
constexpr std::size_t SCE_RANDOM_MAX_SIZE = 64;

}

extern "C" {

int APS5_VABI sceRandomGetRandomNumber(void* buf, size_t size) {
    if (!buf || size > SCE_RANDOM_MAX_SIZE)
        return SCE_RANDOM_ERROR_INVALID;
    static thread_local std::random_device device;
    auto* bytes = static_cast<unsigned char*>(buf);
    for (std::size_t offset = 0; offset < size; offset += sizeof(unsigned int)) {
        const unsigned int value = device();
        std::memcpy(bytes + offset, &value, std::min(sizeof(value), size - offset));
    }
    return 0;
}
}
