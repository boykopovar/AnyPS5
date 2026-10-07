#include "prx/libc/include/general/VabiMacros.hpp"
#include <cctype>

extern "C" {

char* APS5_VABI strcasestr_nid_postfix(const char* text, const char* needle) {
    if (*needle == '\0')
        return const_cast<char*>(text);
    for (; *text != '\0'; ++text) {
        const char* candidate = text;
        const char* match = needle;
        while (*candidate != '\0' && *match != '\0' &&
               std::tolower(static_cast<unsigned char>(*candidate)) ==
                   std::tolower(static_cast<unsigned char>(*match))) {
            ++candidate;
            ++match;
        }
        if (*match == '\0')
            return const_cast<char*>(text);
    }
    return nullptr;
}
}
