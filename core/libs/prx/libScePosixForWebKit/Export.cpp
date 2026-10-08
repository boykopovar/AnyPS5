#include "prx/libc/include/general/VabiMacros.hpp"
#include <cctype>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

extern "C" {

int APS5_VABI isatty_nid_postfix(int descriptor) {
#ifdef _WIN32
    return ::_isatty(descriptor);
#else
    return ::isatty(descriptor);
#endif
}

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

}
