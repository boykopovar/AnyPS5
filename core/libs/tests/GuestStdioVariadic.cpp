#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern FileStream* __stdinp_nid_postfix;
int APS5_VABI vfscanf_nid_postfix(FileStream*, const char*, void*);
int APS5_VABI vscanf_nid_postfix(const char*, void*);
int APS5_VABI scanf_nid_postfix(const char*, ...);
int APS5_VABI vfwprintf_nid_postfix(FileStream*, const char16_t*, void*);
int APS5_VABI fwprintf_nid_postfix(FileStream*, const char16_t*, ...);
int APS5_VABI vswscanf_nid_postfix(const char16_t*, const char16_t*, void*);
int APS5_VABI vswscanf_s_nid_postfix(const char16_t*, const char16_t*, void*);
void APS5_VABI _Vacopy_nid_postfix(void*, const void*);
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
    const int result = vfscanf_nid_postfix(stream, format, args);
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanInput(const char* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vscanf_nid_postfix(format, args);
    APS5_VA_END();
    return result;
}

static int APS5_VABI PrintWide(FileStream* stream, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfwprintf_nid_postfix(stream, format, args);
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanWide(const char16_t* input, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vswscanf_nid_postfix(input, format, args);
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanWideSized(const char16_t* input, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vswscanf_s_nid_postfix(input, format, args);
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
    _Vacopy_nid_postfix(&copy, &args);
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
    Require(Contents(written) == "7:\xc3\xa9t\xc3\xa9|  2.2", "the stream holds the formatted text");
    written.Close();

    std::int32_t a = 0, b = 0;
    Require(ScanWide(u"  31 -4", u"%d%d", &a, &b) == 2 && a == 31 && b == -4, "vswscanf");
    Require(ScanWide(u"", u"%d", &a) == EOF, "vswscanf on empty input");
    char16_t letters[4]{};
    Require(ScanWideSized(u"abcdef", u"%3ls", letters, 4u) == 1 && std::u16string(letters) == u"abc", "vswscanf_s %ls with a size");
    Require(ScanWideSized(u"abcdef", u"%ls", letters, 4u) == 0 && letters[0] == 0, "vswscanf_s rejects a too small buffer");
    CheckVacopy();
    return 0;
}
