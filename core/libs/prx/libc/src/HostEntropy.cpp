#ifdef _WIN32
#define _CRT_RAND_S
#endif
#include "prx/libc/include/HostEntropy.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <system_error>
#ifdef __APPLE__
#include <sys/random.h>
#elif !defined(_WIN32)
#include <unistd.h>
#endif

void HostEntropy_nid_no_patch(void* buffer, std::size_t size) {
    if (size == 0)
        return;
    if (buffer == nullptr)
        throw std::invalid_argument("HostEntropy: null buffer");
    auto* bytes = static_cast<unsigned char*>(buffer);
    std::size_t remaining = size;
#ifdef _WIN32
    while (remaining > 0) {
        unsigned int value = 0;
        if (const errno_t error = rand_s(&value); error != 0)
            throw std::system_error(error, std::generic_category(), "HostEntropy: rand_s failed");
        const std::size_t chunk = std::min(sizeof(value), remaining);
        std::memcpy(bytes, &value, chunk);
        bytes += chunk;
        remaining -= chunk;
    }
#else
    constexpr std::size_t MaxChunk = 256;
    while (remaining > 0) {
        const std::size_t chunk = std::min(MaxChunk, remaining);
        if (getentropy(bytes, chunk) != 0)
            throw std::system_error(errno, std::generic_category(), "HostEntropy: getentropy failed");
        bytes += chunk;
        remaining -= chunk;
    }
#endif
}
