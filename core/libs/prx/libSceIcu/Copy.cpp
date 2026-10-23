#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {

char16_t* APS5_VABI u_strcpy_nid_postfix(char16_t* destination, const char16_t* source) {
    std::size_t index = 0;
    do {
        destination[index] = source[index];
    } while (source[index++] != 0);
    return destination;
}

char16_t* APS5_VABI u_strncpy_nid_postfix(char16_t* destination, const char16_t* source, std::int32_t count) {
    for (std::int32_t index = 0; index < count; ++index) {
        destination[index] = source[index];
        if (source[index] == 0) break;
    }
    return destination;
}

char16_t* APS5_VABI u_strcat_nid_postfix(char16_t* destination, const char16_t* source) {
    char16_t* end = destination;
    while (*end != 0) ++end;
    u_strcpy_nid_postfix(end, source);
    return destination;
}

char16_t* APS5_VABI u_strncat_nid_postfix(char16_t* destination, const char16_t* source, std::int32_t count) {
    if (count <= 0) return destination;
    char16_t* end = destination;
    while (*end != 0) ++end;
    std::int32_t index = 0;
    while (index < count && source[index] != 0) {
        end[index] = source[index];
        ++index;
    }
    end[index] = 0;
    return destination;
}

char16_t* APS5_VABI u_memcpy_nid_postfix(char16_t* destination, const char16_t* source, std::int32_t count) {
    if (count > 0) std::memcpy(destination, source, static_cast<std::size_t>(count) * sizeof(char16_t));
    return destination;
}

}
