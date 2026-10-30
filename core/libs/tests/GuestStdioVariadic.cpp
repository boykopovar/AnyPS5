#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern FileStream* __stdinp_nid_postfix;

}

#ifdef _WIN32
#define APS5_VA_BEGIN(last) __builtin_sysv_va_list args; __builtin_sysv_va_start(args, last)
#define APS5_VA_END() __builtin_sysv_va_end(args)
#else
#define APS5_VA_BEGIN(last) std::va_list args; va_start(args, last)
#define APS5_VA_END() va_end(args)
#endif

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

static int APS5_VABI ScanStream(FileStream* stream, const char* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfscanf_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanInput(const char* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vscanf_nid_postfix(format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI PrintSecure(FileStream* stream, const char* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfprintf_s_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI PrintSecureOutput(const char* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vprintf_s_nid_postfix(format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanSecureStream(FileStream* stream, const char* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfscanf_s_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanSecureInput(const char* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vscanf_s_nid_postfix(format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI PrintWideSecure(FileStream* stream, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfwprintf_s_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI PrintWideSecureOutput(const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vwprintf_s_nid_postfix(format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static FileStream* SeededStream(const char* text) {
    auto* stream = new FileStream(std::tmpfile(), true);
    Require(std::fputs(text, stream->GetHandle()) >= 0, "seed a stream");
    std::rewind(stream->GetHandle());
    return stream;
}

static int NextByte(FileStream* stream) {
    return std::fgetc(stream->GetHandle());
}

static int APS5_VABI PrintWide(FileStream* stream, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfwprintf_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanWide(const char16_t* input, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vswscanf_nid_postfix(input, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanWideSized(const char16_t* input, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vswscanf_s_nid_postfix(input, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static long APS5_VABI SumThroughCopy(int count, ...) {
    APS5_VA_BEGIN(count);
    unsigned char before[24];
    std::memcpy(before, &args, sizeof(before));
#ifdef _WIN32
    __builtin_sysv_va_list copy;
#else
    std::va_list copy;
#endif
    _Vacopy_nid_postfix(reinterpret_cast<unsigned char*>(&copy), reinterpret_cast<const unsigned char*>(&args));

    Require(std::memcmp(&copy, &args, sizeof(before)) == 0, "_Vacopy copies all 24 bytes");
    long sum = 0;
    for (int index = 0; index < count; ++index) sum += __builtin_va_arg(copy, long) * (index + 1);
    Require(std::memcmp(&args, before, sizeof(before)) == 0, "_Vacopy leaves the source untouched");
    for (int index = 0; index < count; ++index) sum -= __builtin_va_arg(args, long);
    APS5_VA_END();
    return sum;
}

static void CheckVacopy() {
    long expected = 0;
    for (long index = 1; index <= 9; ++index) expected += index * 100 * index - index * 100;
    Require(SumThroughCopy(9, 100L, 200L, 300L, 400L, 500L, 600L, 700L, 800L, 900L) == expected, "_Vacopy copy reads every argument");
    unsigned char overlap[32];
    for (int index = 0; index < 32; ++index) overlap[index] = static_cast<unsigned char>(index);
    _Vacopy_nid_postfix(overlap + 8, overlap);

    const unsigned char shifted[32] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
    Require(std::memcmp(overlap, shifted, sizeof(overlap)) == 0, "_Vacopy copies the overflow area before the offsets");
}

static std::string Contents(FileStream& stream) {
    std::fflush(stream.GetHandle());
    std::rewind(stream.GetHandle());
    std::string text;
    for (int c; (c = std::fgetc(stream.GetHandle())) != EOF;) text.push_back(static_cast<char>(c));
    return text;
}

int main() {
    FileStream scanned(std::tmpfile());
    Require(std::fputs("42 answer 7", scanned.GetHandle()) >= 0, "seed the scanned stream");
    std::rewind(scanned.GetHandle());
    int number = 0;
    char word[16]{};
    Require(ScanStream(&scanned, "%d %15s", &number, word) == 2, "vfscanf assigns both conversions");
    Require(number == 42 && std::strcmp(word, "answer") == 0, "vfscanf values");
    Require(ScanStream(&scanned, "%d", &number) == 1 && number == 7, "vfscanf continues from the stream position");
    Require(ScanStream(&scanned, "%d", &number) == EOF && number == 7, "vfscanf at end of file");
    scanned.Close();

    FileStream input(std::tmpfile());
    Require(std::fputs("-5 12", input.GetHandle()) >= 0, "seed the standard input stream");
    std::rewind(input.GetHandle());
    FileStream* const standardInput = __stdinp_nid_postfix;
    __stdinp_nid_postfix = &input;
    int first = 0, second = 0;
    Require(ScanInput("%d", &first) == 1 && first == -5, "vscanf reads the guest standard input");
    Require(scanf_nid_postfix("%d", &second) == 1 && second == 12, "scanf reads the guest standard input");
    __stdinp_nid_postfix = standardInput;
    input.Close();

    FileStream written(std::tmpfile());
    Require(fwprintf_nid_postfix(&written, u"%d:%ls|", 7, u"été") == 6, "fwprintf counts wide characters");
    Require(PrintWide(&written, u"%5.1f", 2.25) == 5, "vfwprintf counts wide characters");
    Require(Contents(written) == "7:\xc3\xa9t\xc3\xa9|  2.3", "the stream holds the formatted text");
    written.Close();

    FileStream securePlain(std::tmpfile());
    FileStream secureChecked(std::tmpfile());
    FileStream secureList(std::tmpfile());
    Require(fprintf_nid_postfix(&securePlain, "%d|%s|%5.2f|%c|%ls", 42, "ab", 3.14159, 'z', u"é") == 16, "fprintf reference");
    Require(fprintf_s_nid_postfix(&secureChecked, "%d|%s|%5.2f|%c|%ls", 42, "ab", 3.14159, 'z', u"é") == 16, "fprintf_s count");
    Require(PrintSecure(&secureList, "%d|%s|%5.2f|%c|%ls", 42, "ab", 3.14159, 'z', u"é") == 16, "vfprintf_s count");
    const std::string expectedText = "42|ab| 3.14|z|\xc3\xa9";
    Require(Contents(securePlain) == expectedText, "fprintf reference bytes");
    Require(Contents(secureChecked) == expectedText, "fprintf_s writes the bytes fprintf writes");
    Require(Contents(secureList) == expectedText, "vfprintf_s writes the bytes fprintf writes");
    securePlain.Close();
    secureChecked.Close();
    secureList.Close();

    FileStream violations(std::tmpfile());
    signed char marker = -7;
    char sibling[16];
    Require(snprintf_s_nid_postfix(sibling, sizeof(sibling), "ab%hhn", &marker) == -1, "snprintf_s rejects %n");
    Require(fprintf_s_nid_postfix(&violations, "ab%hhn", &marker) == -1 && marker == -7, "fprintf_s rejects %n like snprintf_s");
    Require(PrintSecure(&violations, "ab%n", &number) == -1, "vfprintf_s rejects %n");
    Require(snprintf_s_nid_postfix(sibling, sizeof(sibling), "%s", static_cast<const char*>(nullptr)) == -1, "snprintf_s rejects a null %s");
    Require(fprintf_s_nid_postfix(&violations, "x%s", static_cast<const char*>(nullptr)) == -1, "fprintf_s rejects a null %s");
    Require(fprintf_s_nid_postfix(&violations, static_cast<const char*>(nullptr)) == -1, "fprintf_s rejects a null format");
    Require(PrintSecure(&violations, static_cast<const char*>(nullptr)) == -1, "vfprintf_s rejects a null format");
    Require(fprintf_s_nid_postfix(nullptr, "x") == -1, "fprintf_s rejects a null stream");
    Require(PrintSecure(nullptr, "x") == -1, "vfprintf_s rejects a null stream");
    Require(Contents(violations).empty(), "a rejected call writes nothing");
    Require(fprintf_s_nid_postfix(&violations, "%s", "") == 0, "fprintf_s of an empty string");
    violations.Close();
    errno = 0;
    Require(fprintf_s_nid_postfix(&violations, "x") == -1 && errno == 9, "fprintf_s on a closed stream sets EBADF");
    Require(PrintSecureOutput("vprintf_s: %d\n", 42) == 14, "vprintf_s counts the bytes written to standard output");
    Require(PrintSecureOutput("%n", &number) == -1 && PrintSecureOutput(static_cast<const char*>(nullptr)) == -1, "vprintf_s violations");

    FileStream wideChecked(std::tmpfile());
    FileStream wideList(std::tmpfile());
    int widePosition = -5;
    Require(fwprintf_s_nid_postfix(&wideChecked, u"%05d:%ls|", 42, u"été") == 10, "fwprintf_s counts wide characters");
    Require(PrintWideSecure(&wideList, u"%05d:%ls|", 42, u"été") == 10, "vfwprintf_s counts wide characters");
    Require(Contents(wideChecked) == "00042:\xc3\xa9t\xc3\xa9|" && Contents(wideList) == Contents(wideChecked), "wide secure output bytes");
    Require(fwprintf_s_nid_postfix(&wideChecked, u"a%n", &widePosition) == -1 && widePosition == -5, "fwprintf_s rejects %n");
    Require(PrintWideSecure(&wideChecked, u"a%ls", static_cast<const char16_t*>(nullptr)) == -1, "vfwprintf_s rejects a null %ls");
    Require(fwprintf_s_nid_postfix(&wideChecked, static_cast<const char16_t*>(nullptr)) == -1, "fwprintf_s rejects a null format");
    Require(fwprintf_s_nid_postfix(nullptr, u"x") == -1 && PrintWideSecure(nullptr, u"x") == -1, "wide secure forms reject a null stream");
    wideChecked.Close();
    wideList.Close();
    errno = 0;
    Require(fwprintf_s_nid_postfix(&wideList, u"x") == -1 && errno == 9, "fwprintf_s on a closed stream sets EBADF");
    Require(wprintf_s_nid_postfix(u"wprintf_s %d\n", 5) == 12, "wprintf_s counts wide characters");
    Require(PrintWideSecureOutput(u"vwprintf_s %d\n", 6) == 13, "vwprintf_s counts wide characters");
    Require(wprintf_s_nid_postfix(u"%n", &widePosition) == -1 && PrintWideSecureOutput(static_cast<const char16_t*>(nullptr)) == -1, "wide standard output violations");

    FileStream* sizedInput = SeededStream("17 hello Q 9 12abc key=val abcdef 4.5");
    char sizedWord[6]{};
    char sizedChar = 0;
    Require(fscanf_s_nid_postfix(sizedInput, "%d %s %c", &number, sizedWord, 6u, &sizedChar, 1u) == 3, "fscanf_s assigns the sized conversions");
    Require(number == 17 && std::strcmp(sizedWord, "hello") == 0 && sizedChar == 'Q', "fscanf_s values");
    Require(ScanSecureStream(sizedInput, "%d", &number) == 1 && number == 9, "vfscanf_s continues from the stream position");
    Require(fscanf_s_nid_postfix(sizedInput, "%d", &number) == 1 && number == 12 && NextByte(sizedInput) == 'a', "fscanf_s leaves the character that ended the number");
    char key[4]{};
    char value[8]{};
    Require(fscanf_s_nid_postfix(sizedInput, "bc %3[a-z]=%7s", key, 4u, value, 8u) == 2, "fscanf_s with a set and a string");
    Require(std::strcmp(key, "key") == 0 && std::strcmp(value, "val") == 0 && NextByte(sizedInput) == ' ', "fscanf_s set and string values");
    char tooSmall[4] = {'x', 'x', 'x', 'x'};
    char siblingSmall[4] = {'x', 'x', 'x', 'x'};
    Require(sscanf_s_nid_postfix("abcdef", "%s", siblingSmall, 4u) == 0 && siblingSmall[0] == '\0', "sscanf_s reference for an undersized buffer");
    Require(fscanf_s_nid_postfix(sizedInput, "%s", tooSmall, 4u) == 0 && tooSmall[0] == '\0', "fscanf_s rejects an undersized buffer like sscanf_s");
    sizedInput->Close();
    errno = 0;
    Require(fscanf_s_nid_postfix(sizedInput, "%d", &number) == EOF && errno == 9, "fscanf_s on a closed stream sets EBADF");
    delete sizedInput;

    FileStream* tailInput = SeededStream("x 3.5 7");
    int count = 0;
    Require(ScanSecureStream(tailInput, "%d", &number) == 0, "vfscanf_s matching failure");
    Require(fscanf_s_nid_postfix(tailInput, " x %*f%n %d", &count, &number) == 1 && count == 5 && number == 7, "fscanf_s suppression and %n");
    Require(fscanf_s_nid_postfix(tailInput, "%d", &number) == EOF, "fscanf_s at end of file");
    tailInput->Close();
    delete tailInput;

    FileStream* nullInput = SeededStream("5 6");
    Require(fscanf_s_nid_postfix(nullInput, "%d", static_cast<int*>(nullptr)) == EOF, "fscanf_s null destination");
    Require(fscanf_s_nid_postfix(nullInput, static_cast<const char*>(nullptr)) == EOF, "fscanf_s null format");
    Require(fscanf_s_nid_postfix(nullptr, "%d", &number) == EOF && ScanSecureStream(nullptr, "%d", &number) == EOF, "fscanf_s null stream");
    nullInput->Close();
    delete nullInput;

    FileStream* const savedInput = __stdinp_nid_postfix;
    FileStream* redirected = SeededStream("21 word 3 more");
    __stdinp_nid_postfix = redirected;
    char redirectedWord[8]{};
    int redirectedNumber = 0;
    Require(scanf_s_nid_postfix("%d %7s", &number, redirectedWord, 8u) == 2 && number == 21 && std::strcmp(redirectedWord, "word") == 0, "scanf_s reads the guest standard input");
    Require(ScanSecureInput("%d %4s", &redirectedNumber, redirectedWord, 5u) == 2 && redirectedNumber == 3 && std::strcmp(redirectedWord, "more") == 0, "vscanf_s reads the guest standard input");
    Require(scanf_s_nid_postfix("%d", &number) == EOF, "scanf_s at end of input");
    __stdinp_nid_postfix = savedInput;
    redirected->Close();
    delete redirected;

    std::int32_t a = 0, b = 0;
    Require(ScanWide(u"  31 -4", u"%d%d", &a, &b) == 2 && a == 31 && b == -4, "vswscanf");
    Require(ScanWide(u"", u"%d", &a) == EOF, "vswscanf on empty input");
    char16_t letters[4]{};
    Require(ScanWideSized(u"abcdef", u"%3ls", letters, 4u) == 1 && std::u16string(letters) == u"abc", "vswscanf_s %ls with a size");
    Require(ScanWideSized(u"abcdef", u"%ls", letters, 4u) == 0 && letters[0] == 0, "vswscanf_s rejects a too small buffer");
    CheckVacopy();
    return 0;
}
