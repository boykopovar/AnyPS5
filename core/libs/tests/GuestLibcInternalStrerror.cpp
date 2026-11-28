#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal strerror: %s\n", message);
    std::abort();
}

void CheckFixed() {
    for (const int error : {0, 1, 2, 22, 34, 94, 107, 108, 152, 1040, 1050, 1051, 1062, 1074}) {
        char buffer[32];
        std::memset(buffer, 'x', sizeof(buffer));
        const char* text = _Strerror_nid_postfix(error, buffer);
        Require(text != buffer, "a fixed message must not be written into the buffer");
        Require(std::strcmp(text, strerror_nid_postfix(error)) == 0, "a fixed message must match strerror");
        Require(text == _Strerror_nid_postfix(error, nullptr), "a fixed message must be the same constant text");
        for (const char byte : buffer) Require(byte == 'x', "a fixed message must leave the buffer untouched");
    }
}

void CheckNumbered() {
    for (const int error : {-1, 95, 96, 106, 109, 1000, 0x7fffffff}) {
        char buffer[32];
        std::memset(buffer, 'x', sizeof(buffer));
        char expected[22];
        std::snprintf(expected, sizeof(expected), "error_code=0x%08x", static_cast<unsigned>(error));
        Require(_Strerror_nid_postfix(error, buffer) == buffer, "a numbered message must be written into the buffer");
        Require(std::strcmp(buffer, expected) == 0, "a numbered message must use the strerror format");
        Require(std::strcmp(buffer, strerror_nid_postfix(error)) == 0, "a numbered message must match strerror");
        for (std::size_t i = sizeof(expected); i < sizeof(buffer); ++i) Require(buffer[i] == 'x', "only 22 bytes may be written");
        const char* own = _Strerror_nid_postfix(error, nullptr);
        Require(std::strcmp(own, expected) == 0, "a null buffer must use the thread's own buffer");
    }
}

}

int main() {
    CheckFixed();
    CheckNumbered();
    return 0;
}
