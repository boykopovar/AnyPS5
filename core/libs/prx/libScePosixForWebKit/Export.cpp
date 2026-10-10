#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <random>

extern "C" {

char* APS5_VABI strcasestr_nid_postfix(const char* text, const char* needle) {
    if (*needle == '\0') return const_cast<char*>(text);
    for (; *text != '\0'; ++text) {
        const char* candidate = text;
        const char* match = needle;
        while (*candidate != '\0' && *match != '\0' &&
               std::tolower(static_cast<unsigned char>(*candidate)) ==
               std::tolower(static_cast<unsigned char>(*match))) {
            ++candidate;
            ++match;
        }
        if (*match == '\0') return const_cast<char*>(text);
    }
    return nullptr;
}

void APS5_VABI arc4random_buf_nid_postfix(void* buffer, std::size_t size) {
    static thread_local std::random_device device;
    auto* bytes = static_cast<unsigned char*>(buffer);
    for (std::size_t offset = 0; offset < size; offset += sizeof(unsigned int)) {
        const unsigned int value = device();
        std::memcpy(bytes + offset, &value, std::min(sizeof(value), size - offset));
    }
}

}
