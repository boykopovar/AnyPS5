#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" int APS5_VABI vswprintf_nid_postfix(char16_t*, std::size_t, const char16_t*, VaList*);

static int APS5_VABI Format(char16_t* buffer, std::size_t size, const char16_t* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vswprintf_nid_postfix(buffer, size, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

static void Check(const char16_t* format, const char* input, const char16_t* expected) {
    char16_t buffer[32]{};
    const int result = Format(buffer, 32, format, input);
    const std::u16string wanted(expected);
    if (result != static_cast<int>(wanted.size()) || buffer != wanted) {
        std::fprintf(stderr, "wide precision: expected %zu units, got %d\n", wanted.size(), result);
        std::abort();
    }
}

int main() {
    Check(u"%.2s", "\xc3\xa9\xc3\xa8", u"\u00e9\u00e8");
    Check(u"%.1s", "\xc3\xa9\xc3\xa8", u"\u00e9");
    Check(u"%.0s", "\xc3\xa9", u"");
    Check(u"%.3s", "\xc3\xa9", u"\u00e9");
    Check(u"%s", "\xc3\xa9\xc3\xa8", u"\u00e9\u00e8");
    Check(u"%.2s", "abcd", u"ab");
    Check(u"%4.2s", "\xc3\xa9\xc3\xa8", u"  \u00e9\u00e8");
    Check(u"%-4.2s", "\xc3\xa9\xc3\xa8", u"\u00e9\u00e8  ");
    Check(u"%.2s", "\xf0\x9f\x98\x80x", u"\U0001f600");
    Check(u"%.1s", "\xf0\x9f\x98\x80x", u"");
    Check(u"%.3s", "\xf0\x9f\x98\x80x", u"\U0001f600x");
}
