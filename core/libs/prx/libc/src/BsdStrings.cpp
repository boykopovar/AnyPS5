#include "prx/libc/include/general/VabiMacros.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {

void APS5_VABI bcopy_nid_postfix(const void* source, void* destination, std::size_t count) {
    std::memmove(destination, source, count);
}

char* APS5_VABI index_nid_postfix(const char* text, int character) {
    return const_cast<char*>(std::strchr(text, character));
}

char* APS5_VABI rindex_nid_postfix(const char* text, int character) {
    return const_cast<char*>(std::strrchr(text, character));
}

void* APS5_VABI memrchr_nid_postfix(const void* memory, int character, std::size_t count) {
    const auto* bytes = static_cast<const unsigned char*>(memory);
    const auto target = static_cast<unsigned char>(character);
    while (count != 0) {
        --count;
        if (bytes[count] == target) return const_cast<unsigned char*>(bytes + count);
    }
    return nullptr;
}

std::size_t APS5_VABI wcscspn_nid_postfix(const char16_t* text, const char16_t* rejected) {
    std::size_t count = 0;
    for (; text[count] != 0; ++count) {
        for (const auto* current = rejected; *current != 0; ++current) {
            if (text[count] == *current) return count;
        }
    }
    return count;
}

int APS5_VABI flsl_nid_postfix(std::int64_t value) {
    return static_cast<int>(std::bit_width(static_cast<std::uint64_t>(value)));
}

}
