#ifdef _WIN32
#define _CRT_RAND_S
#endif
#include "prx/libc/include/HostEntropy.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <system_error>
#ifdef __APPLE__
#include <sys/random.h>
#elif !defined(_WIN32)
#include <unistd.h>
#endif

void HostEntropy_nid_no_patch(void* buffer, std::size_t size) {
    auto* bytes = static_cast<unsigned char*>(buffer);
#ifdef _WIN32
    for (std::size_t offset = 0; offset < size; offset += sizeof(unsigned int)) {
        unsigned int value = 0;
        if (const errno_t error = rand_s(&value); error != 0)
            throw std::system_error(error, std::generic_category(), "HostEntropy: rand_s failed");
        std::memcpy(bytes + offset, &value, std::min(sizeof(value), size - offset));
    }
#else
    constexpr std::size_t MaxChunk = 256;
    for (std::size_t offset = 0; offset < size; offset += MaxChunk) {
        if (getentropy(bytes + offset, std::min(MaxChunk, size - offset)) != 0)
            throw std::system_error(errno, std::generic_category(), "HostEntropy: getentropy failed");
    }
#endif
}
