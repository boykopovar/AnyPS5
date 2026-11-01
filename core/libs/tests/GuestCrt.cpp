#include "prx/libc/include/Api.hpp"
#include "prx/libSceAmpr/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include <cfenv>
#include <climits>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <limits>

#define Require(condition) do { if (!(condition)) { std::fprintf(stderr, "guest_crt: line %d: %s\n", __LINE__, #condition); std::abort(); } } while (0)

static int g_handlerCalls = 0;
static void CountingHandler(const char* message, void* pointer, int error) {
    (void)message;
    (void)pointer;
    (void)error;
    ++g_handlerCalls;
}

static int g_libcConstraintCalls = 0;
static int g_libcConstraintError = 0;
static const char* g_libcConstraintMessage = nullptr;
static void APS5_VABI RecordLibcConstraint(const char* message, void* pointer, int error) {
    Require(pointer == nullptr);
    ++g_libcConstraintCalls;
    g_libcConstraintError = error;
    g_libcConstraintMessage = message;
}

static bool LibcViolation(const char* message) {
    const bool matched = g_libcConstraintCalls == 1 && g_libcConstraintError == EINVAL && g_libcConstraintMessage != nullptr &&
        std::strcmp(g_libcConstraintMessage, message) == 0;
    g_libcConstraintCalls = 0;
    g_libcConstraintMessage = nullptr;
    return matched;
}

int main() {
    Require(sceAmprFpclassifyd(std::numeric_limits<double>::quiet_NaN()) == 2);
    Require(sceAmprFpclassifyd(std::numeric_limits<double>::infinity()) == 1);
    Require(sceAmprFpclassifyd(0.0) == 0);
    Require(sceAmprFpclassifyd(std::numeric_limits<double>::denorm_min()) == -2);
    Require(sceAmprFpclassifyd(1.5) == -1);
    Require(__fpclassifyd_nid_postfix(0.0) == 0);

    Require(sceAmprFltrounds() == 1);
    Require(std::fesetround(FE_DOWNWARD) == 0);
    Require(sceAmprFltrounds() == 3);
    Require(std::fesetround(FE_TONEAREST) == 0);
    Require(sceAmprFltrounds() == 1);

    Require(sceAmprSinh(1.0, 2.0) == 2.0 * std::sinh(1.0));
    Require(sceAmprSinh(0.0, 5.0) == 0.0);

    SonyLconv* locale = sceAmprLocaleconv();
    Require(reinterpret_cast<std::uintptr_t>(&locale->decimal_point) - reinterpret_cast<std::uintptr_t>(locale) == 0x48);
    Require(std::strcmp(locale->decimal_point, ".") == 0);
    Require(std::memcmp(localeconv_nid_postfix()->chars, locale->chars, sizeof(locale->chars)) == 0);
    Require(std::strcmp(localeconv_nid_postfix()->decimal_point, ".") == 0);
    for (const char* text : locale->monetary) Require(text != nullptr && text[0] == '\0');
    for (char value : locale->chars) Require(value == CHAR_MAX);
    Require(std::strcmp(locale->false_text, "false") == 0 && std::strcmp(locale->true_text, "true") == 0);
    Require(locale->grouping[0] == '\0' && locale->thousands_sep[0] == '\0');

    Require(__udivti3_nid_postfix(100, 3) == 33);
    Require(SonyUdivti3(100, 3) == 33);
    Require(SonyUdivti3(0, 5) == 0);
    Require(SonyUdivti3((unsigned __int128)1 << 100, 3) == (((unsigned __int128)1 << 100) / 3));
    bool trapped = false;
    try {
        SonyUdivti3(1, 0);
    } catch (const std::exception&) {
        trapped = true;
    }
    Require(trapped);

    Require(__divti3_nid_postfix(100, 3) == 33);
    Require(__divti3_nid_postfix(-100, 3) == -33);
    Require(__divti3_nid_postfix(100, -3) == -33);
    Require(__divti3_nid_postfix(-100, -3) == 33);
    Require(SonyDivti3(0, 5) == 0);
    const __int128 large = (__int128)1 << 100;
    Require(SonyDivti3(large, 3) == large / 3);
    Require(SonyDivti3(-large, 3) == -(large / 3));
    Require(SonyDivti3(large, -3) == -(large / 3));
    Require(SonyDivti3(-large, -3) == large / 3);
    const __int128 minimum = (__int128)((unsigned __int128)1 << 127);
    const __int128 maximum = (__int128)(((unsigned __int128)1 << 127) - 1);
    Require(SonyDivti3(minimum, 1) == minimum);
    Require(SonyDivti3(minimum, -1) == minimum);
    Require(SonyDivti3(maximum, -1) == -maximum);
    trapped = false;
    try {
        SonyDivti3(1, 0);
    } catch (const std::exception&) {
        trapped = true;
    }
    Require(trapped);

    Require(__modti3_nid_postfix(100, 3) == 1);
    Require(__modti3_nid_postfix(-100, 3) == -1);
    Require(__modti3_nid_postfix(100, -3) == 1);
    Require(__modti3_nid_postfix(-100, -3) == -1);
    Require(SonyModti3(0, 5) == 0);
    Require(SonyModti3(3, 100) == 3);
    Require(SonyModti3(-3, 100) == -3);
    Require(SonyModti3(3, -100) == 3);
    Require(SonyModti3(-3, -100) == -3);
    Require(SonyModti3(large, 3) == large % 3);
    Require(SonyModti3(-large, 3) == -(large % 3));
    Require(SonyModti3(large, -3) == large % 3);
    Require(SonyModti3(-large, -3) == -(large % 3));
    Require(SonyModti3(minimum, 1) == 0);
    Require(SonyModti3(minimum, -1) == 0);
    Require(SonyModti3(maximum, 1) == 0);
    Require(SonyModti3(maximum, -1) == 0);
    trapped = false;
    try {
        SonyModti3(1, 0);
    } catch (const std::exception&) {
        trapped = true;
    }
    Require(trapped);

    const unsigned __int128 unsignedMaximum = ~(unsigned __int128)0;
    const unsigned __int128 largeDivisor = ((unsigned __int128)1 << 64) + 3;
    Require(__umodti3_nid_postfix(100, 3) == 1);
    Require(__umodti3_nid_postfix(0, 5) == 0);
    Require(__umodti3_nid_postfix(3, 100) == 3);
    Require(__umodti3_nid_postfix(unsignedMaximum, 1) == 0);
    Require(__umodti3_nid_postfix(unsignedMaximum, unsignedMaximum) == 0);
    Require(__umodti3_nid_postfix(largeDivisor, largeDivisor) == 0);
    Require(__umodti3_nid_postfix(unsignedMaximum, 10) == 5);
    Require(__umodti3_nid_postfix((unsigned __int128)1 << 100, 3) == 1);
    Require(__umodti3_nid_postfix(unsignedMaximum, (unsigned __int128)1 << 64) == 0xFFFFFFFFFFFFFFFFull);
    Require(__umodti3_nid_postfix(unsignedMaximum, largeDivisor) == 8);
    Require(__umodti3_nid_postfix((unsigned __int128)1 << 127, largeDivisor) == 0x8000000000000006ull);
    Require(__umodti3_nid_postfix(unsignedMaximum - 1, unsignedMaximum) == unsignedMaximum - 1);
    Require(__umodti3_nid_postfix(unsignedMaximum, (unsigned __int128)1 << 127) == ((unsigned __int128)1 << 127) - 1);
    trapped = false;
    try {
        __umodti3_nid_postfix(((unsigned __int128)0x1234 << 64) | 0x5678, 0);

    } catch (const std::runtime_error& error) {
        trapped = std::strcmp(error.what(), "__umodti3: integer divide by zero (0x00000000000012340000000000005678 % 0)") == 0;
    }
    Require(trapped);

    void* previous = sceAmprSetConstraintHandlerS(reinterpret_cast<void*>(&CountingHandler));
    Require(previous != nullptr);
    Require(previous != reinterpret_cast<void*>(&CountingHandler));
    const std::uint16_t hello[] = {'h', 'i', 0};
    std::uint16_t buffer[8] = {};
    Require(sceAmprWcscpyS(buffer, 8, hello) == 0);
    Require(buffer[0] == 'h' && buffer[1] == 'i' && buffer[2] == 0);
    const std::uint16_t longText[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 0};
    g_handlerCalls = 0;
    Require(sceAmprWcscpyS(buffer, 4, longText) == ERANGE);
    Require(g_handlerCalls == 1);
    Require(sceAmprWcscpyS(nullptr, 4, longText) == EINVAL);
    previous = sceAmprSetConstraintHandlerS(nullptr);
    Require(previous == reinterpret_cast<void*>(&CountingHandler));
    previous = sceAmprSetConstraintHandlerS(reinterpret_cast<void*>(&CountingHandler));
    Require(previous != nullptr && previous != reinterpret_cast<void*>(&CountingHandler));
    sceAmprIgnoreHandlerS("msg", nullptr, 0);

    char text[64];
    GuestTm known{};
    known.tm_year = 70;
    known.tm_mon = 0;
    known.tm_mday = 1;
    known.tm_wday = 4;
    Require(sceAmprAsctimeS(text, sizeof(text), &known) == 0);
    Require(std::strcmp(text, "Thu Jan  1 00:00:00 1970\n") == 0);
    void* const previousLibcHandler = set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(&RecordLibcConstraint));
    std::memset(text, 0, sizeof(text));
    Require(asctime_s_nid_postfix(text, sizeof(text), &known) == 0);
    Require(std::strcmp(text, "Thu Jan  1 00:00:00 1970\n") == 0);
    known.tm_wday = 7;
    text[0] = 'x';
    Require(sceAmprAsctimeS(text, sizeof(text), &known) == EINVAL && text[0] == '\0');
    known.tm_wday = 4;
    known.tm_year = 9000;
    text[0] = 'x';
    Require(sceAmprAsctimeS(text, sizeof(text), &known) == EINVAL && text[0] == '\0');
    for (const int year : {INT_MAX, INT_MAX - 1899, INT_MIN}) {
        known.tm_year = year;
        text[0] = 'x';
        Require(sceAmprAsctimeS(text, sizeof(text), &known) == EINVAL && text[0] == '\0');
        text[0] = 'x';
        Require(asctime_s_nid_postfix(text, sizeof(text), &known) == EINVAL && text[0] == '\0');
        Require(LibcViolation("asctime_s: bad argument"));
    }
    known.tm_year = 70;
    for (const std::size_t size : {std::size_t{0}, std::size_t{1} << 61, SIZE_MAX >> 1}) {
        text[0] = 'x';
        Require(asctime_s_nid_postfix(text, size, &known) == EINVAL && text[0] == 'x');
        Require(LibcViolation("asctime_s: bad destination argument"));
    }
    Require(asctime_s_nid_postfix(nullptr, sizeof(text), &known) == EINVAL);
    Require(LibcViolation("asctime_s: bad destination argument"));
    for (const std::size_t size : {std::size_t{1}, std::size_t{25}}) {
        text[0] = 'x';
        Require(asctime_s_nid_postfix(text, size, &known) == EINVAL && text[0] == '\0');
        Require(LibcViolation("asctime_s: bad argument"));
    }
    text[0] = 'x';
    Require(asctime_s_nid_postfix(text, 10, nullptr) == EINVAL && text[0] == '\0');
    Require(LibcViolation("asctime_s: bad argument"));
    for (const std::size_t size : {std::size_t{1} << 32, (std::size_t{1} << 61) - 1}) {
        std::memset(text, 'x', sizeof(text));
        Require(asctime_s_nid_postfix(text, size, &known) == 0);
        Require(std::strcmp(text, "Thu Jan  1 00:00:00 1970\n") == 0 && text[26] == 'x' && g_libcConstraintCalls == 0);
    }
    for (const int yday : {-1, 366}) {
        known.tm_yday = yday;
        text[0] = 'x';
        Require(asctime_s_nid_postfix(text, sizeof(text), &known) == EINVAL && text[0] == '\0');
        Require(LibcViolation("asctime_s: bad argument"));
    }
    known.tm_yday = 365;
    Require(asctime_s_nid_postfix(text, sizeof(text), &known) == 0 && g_libcConstraintCalls == 0);
    known.tm_yday = 0;
    known.tm_year = 57 - 1900;
    Require(asctime_s_nid_postfix(text, sizeof(text), &known) == 0 && std::strcmp(text, "Thu Jan  1 00:00:00 0057\n") == 0);
    known.tm_year = 70;
    known.tm_mday = 32;
    text[0] = 'x';
    Require(sceAmprAsctimeS(text, sizeof(text), &known) == EINVAL && text[0] == '\0');
    known.tm_mday = 1;
    Require(sceAmprAsctimeS(nullptr, sizeof(text), &known) == EINVAL);
    text[0] = 'x';
    Require(sceAmprAsctimeS(text, sizeof(text), nullptr) == EINVAL && text[0] == '\0');
    Require(sceAmprAsctimeS(text, 25, &known) == EINVAL);
    const std::int64_t epoch = 0;
    Require(sceAmprCtimeS(text, sizeof(text), &epoch) == 0);
    Require(std::strlen(text) == 25 && text[24] == '\n');
    char libcText[64];
    Require(ctime_s_nid_postfix(libcText, sizeof(libcText), &epoch) == 0);
    Require(std::strcmp(libcText, text) == 0);
    for (const std::size_t size : {std::size_t{0}, std::size_t{1} << 61}) {
        libcText[0] = 'x';
        Require(ctime_s_nid_postfix(libcText, size, &epoch) == EINVAL && libcText[0] == 'x');
        Require(LibcViolation("ctime_s: bad destination argument"));
    }
    Require(ctime_s_nid_postfix(nullptr, sizeof(libcText), &epoch) == EINVAL);
    Require(LibcViolation("ctime_s: bad destination argument"));
    libcText[0] = 'x';
    Require(ctime_s_nid_postfix(libcText, 25, &epoch) == EINVAL && libcText[0] == '\0');
    Require(LibcViolation("ctime_s: bad argument"));
    libcText[0] = 'x';
    Require(ctime_s_nid_postfix(libcText, 10, nullptr) == EINVAL && libcText[0] == '\0');
    Require(LibcViolation("ctime_s: bad argument"));
    std::memset(libcText, 'x', sizeof(libcText));
    Require(ctime_s_nid_postfix(libcText, std::size_t{1} << 32, &epoch) == 0 && std::strcmp(libcText, text) == 0 && libcText[26] == 'x');
    Require(asctime_s_nid_postfix(nullptr, sizeof(text), nullptr) == EINVAL);
    Require(LibcViolation("asctime_s: bad destination argument"));
    text[0] = 'x';
    Require(asctime_s_nid_postfix(text, 0, nullptr) == EINVAL && text[0] == 'x');
    Require(LibcViolation("asctime_s: bad destination argument"));
    const std::int64_t unconvertible = INT64_MAX;
    libcText[0] = 'x';
    Require(ctime_s_nid_postfix(libcText, 25, &unconvertible) == EINVAL && libcText[0] == '\0');
    Require(LibcViolation("ctime_s: bad argument"));
    struct FieldLimit { int GuestTm::*field; int low; int high; };
    for (const FieldLimit limit : {FieldLimit{&GuestTm::tm_sec, 0, 60}, FieldLimit{&GuestTm::tm_min, 0, 59}, FieldLimit{&GuestTm::tm_hour, 0, 23},
             FieldLimit{&GuestTm::tm_mday, 1, 31}, FieldLimit{&GuestTm::tm_mon, 0, 11}, FieldLimit{&GuestTm::tm_wday, 0, 6},
             FieldLimit{&GuestTm::tm_yday, 0, 365}, FieldLimit{&GuestTm::tm_year, -1900, 9999 - 1900}}) {
        for (const int value : {limit.low - 1, limit.low, limit.high, limit.high + 1}) {
            GuestTm fields = known;
            fields.*limit.field = value;
            text[0] = 'x';
            const bool valid = value >= limit.low && value <= limit.high;
            Require(asctime_s_nid_postfix(text, sizeof(text), &fields) == (valid ? 0 : EINVAL));
            Require(valid ? (g_libcConstraintCalls == 0 && std::strlen(text) == 25) : (text[0] == '\0' && LibcViolation("asctime_s: bad argument")));
        }
    }
    set_constraint_handler_s_nid_postfix(previousLibcHandler);
    Require(sceAmprCtimeS(text, 25, &epoch) == EINVAL);
    text[0] = 'x';
    Require(sceAmprCtimeS(text, sizeof(text), nullptr) == EINVAL && text[0] == '\0');

    char line[] = "a,b,,c";
    char* context = nullptr;
    Require(std::strcmp(sceAmprStrtokS(line, ",", &context), "a") == 0);
    Require(std::strcmp(sceAmprStrtokS(nullptr, ",", &context), "b") == 0);
    Require(std::strcmp(sceAmprStrtokS(nullptr, ",", &context), "c") == 0);
    Require(sceAmprStrtokS(nullptr, ",", &context) == nullptr);
    Require(sceAmprStrtokS(line, nullptr, &context) == nullptr);
    char again[] = "x y";
    char* again_context = nullptr;
    Require(std::strcmp(strtok_s_nid_postfix(again, " ", &again_context), "x") == 0);

    std::uint16_t wide[8] = {};
    Require(sceAmprMbstowcs(wide, "ABC", 8) == 3);
    Require(wide[0] == 65 && wide[1] == 66 && wide[2] == 67 && wide[3] == 0);
    Require(sceAmprMbstowcs(nullptr, "ABC", 8) == 3);
    Require(sceAmprMbstowcs(nullptr, "ABC", 0) == 3 && sceAmprMbstowcs(nullptr, "ABC", 1) == 3);
    Require(mbstowcs_nid_postfix(nullptr, "ABC", 0) == 3 && mbstowcs_nid_postfix(nullptr, "ABC", 1) == 3);
    Require(sceAmprMbstowcs(wide, "ABC", 2) == 2);
    Require(mbstowcs_nid_postfix(wide, "ABC", 8) == 3);

    const unsigned int firstRandom = _ZSt14_Random_devicev_nid_postfix();
    bool randomChanged = false;
    for (int draw = 0; draw < 8; ++draw) randomChanged |= _ZSt14_Random_devicev_nid_postfix() != firstRandom;
    Require(randomChanged);
}
