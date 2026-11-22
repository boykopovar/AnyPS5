#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "cookie change observer check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

}

int main() {
    alignas(16) unsigned char session[64];
    std::memset(session, 0x3c, sizeof(session));
    alignas(8) unsigned char function[8];
    std::memset(function, 0x6e, sizeof(function));

    _ZN7WebCore27startObservingCookieChangesERKNS_21NetworkStorageSessionEON3WTF8FunctionIFvvEEE(session, function);
    for (unsigned char byte : function) Require(byte == 0x6e);
    _ZN7WebCore26stopObservingCookieChangesERKNS_21NetworkStorageSessionE(session);
    _ZN7WebCore26stopObservingCookieChangesERKNS_21NetworkStorageSessionE(session);
    _ZN7WebCore27startObservingCookieChangesERKNS_21NetworkStorageSessionEON3WTF8FunctionIFvvEEE(session, function);
    _ZN7WebCore26stopObservingCookieChangesERKNS_21NetworkStorageSessionE(session);
    for (unsigned char byte : session) Require(byte == 0x3c);
    for (unsigned char byte : function) Require(byte == 0x6e);
    return 0;
}
