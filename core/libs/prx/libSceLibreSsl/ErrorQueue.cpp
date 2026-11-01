#include "ErrorQueue.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdio>
#include <cstring>

namespace {
char errorBuffer[256];
}

extern "C" {
char* APS5_VABI ERR_error_string_nid_postfix(std::uint64_t error, char* buffer) {
    if (buffer == nullptr) buffer = errorBuffer;
    ERR_error_string_n_nid_postfix(error, buffer, sizeof(errorBuffer));
    return buffer;
}

void APS5_VABI ERR_error_string_n_nid_postfix(std::uint64_t error, char* buffer, std::size_t length) {
    if (buffer == nullptr && length != 0) NotImplemented_nid_no_patch(__func__);
    const int library = (error >> 24) & 255;
    const int function = (error >> 12) & 4095;
    const int reason = error & 4095;
    const int written = std::snprintf(buffer, length, "error:%08llX:lib(%d):func(%d):reason(%d)",
        static_cast<unsigned long long>(error), library, function, reason);
    if (written >= 0 && static_cast<std::size_t>(written) >= length && length > 4) {
        char* next = buffer;
        for (std::size_t index = 0; index < 4; ++index) {
            char* colon = std::strchr(next, ':');
            char* last = buffer + length - 5 + index;
            if (colon == nullptr || colon > last) {
                colon = last;
                *colon = ':';
            }
            next = colon + 1;
        }
    }
}

void APS5_VABI ERR_free_strings_nid_postfix() {
}

}
