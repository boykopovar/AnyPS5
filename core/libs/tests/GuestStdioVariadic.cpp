#include "prx/libc/include/FileStream.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdarg>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#ifndef _WIN32
#include <unistd.h>
#endif

extern "C" {
extern FileStream* __stdinp_nid_postfix;
int APS5_VABI fscanf_nid_postfix(FileStream*, const char*, ...);
int APS5_VABI sscanf_nid_postfix(const char*, const char*, ...);
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

#ifndef _WIN32
static std::FILE* OpenPipe(const char* text) {
    int descriptors[2];
    Require(::pipe(descriptors) == 0, "create scanner input pipe");
    const std::size_t length = std::strlen(text);
    Require(::write(descriptors[1], text, length) == static_cast<ssize_t>(length), "seed scanner input pipe");
    Require(::close(descriptors[1]) == 0, "finish scanner input pipe");
    auto* stream = ::fdopen(descriptors[0], "r");
    Require(stream != nullptr, "open scanner input pipe");
    return stream;
}
#endif

static int APS5_VABI ScanStream(FileStream* stream, const char* format, ...) {
    APS5_VA_BEGIN(format);
    VaList original;
    std::memcpy(&original, args, sizeof(original));
    const int result = vfscanf_nid_postfix(stream, format, args);
    Require(std::memcmp(&original, args, sizeof(original)) == 0, "vfscanf preserves the caller argument cursor");
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

    FileStream wideInput(std::tmpfile());
    Require(std::fputs("skip alpha 1 2 3 4 5 6 7!", wideInput.GetHandle()) >= 0, "seed wide variadic input");
    std::rewind(wideInput.GetHandle());
    char16_t wideWord[8]{};
    int values[7]{};
    int count = -1;
    std::int64_t longCount = -1;
    Require(ScanStream(&wideInput, "%*ls %5ls %d %d %d %d %d %d %d%ln%n", wideWord,
        &values[0], &values[1], &values[2], &values[3], &values[4], &values[5], &values[6], &longCount, &count) == 8,
        "vfscanf wide conversion and overflow pointer arguments");
    Require(std::u16string(wideWord) == u"alpha", "vfscanf writes guest 16-bit characters");
    for (int index = 0; index < 7; ++index) Require(values[index] == index + 1, "vfscanf mixed pointer values");
    Require(count == 24 && longCount == 24 && std::ftell(wideInput.GetHandle()) == 24,
        "vfscanf counts consumed input");
    Require(std::fgetc(wideInput.GetHandle()) == '!', "vfscanf preserves the next unread byte");
    Require(ScanStream(&wideInput, "%ls", wideWord) == EOF && std::feof(wideInput.GetHandle()), "vfscanf wide EOF");
    Require((wideInput.GuestState().flags & 0x20) != 0, "vfscanf synchronizes guest EOF");
    wideInput.Close();
    errno = 0;
    Require(ScanStream(&wideInput, "%ls", wideWord) == -1 && errno == 9, "vfscanf rejects a closed wide stream");

    FileStream mismatch(std::tmpfile());
    Require(std::fputs("1", mismatch.GetHandle()) >= 0, "seed wide matching failure");
    std::rewind(mismatch.GetHandle());
    wideWord[0] = u'!';
    Require(ScanStream(&mismatch, "%l[a-z]", wideWord) == 0 && wideWord[0] == u'!', "vfscanf wide matching failure preserves destination");
    Require(std::fgetc(mismatch.GetHandle()) == '1', "vfscanf wide matching failure preserves input");
    mismatch.Close();

    FileStream wideStandard(std::tmpfile());
    Require(std::fputs("abc XYdef QZ", wideStandard.GetHandle()) >= 0, "seed wide standard input");
    std::rewind(wideStandard.GetHandle());
    __stdinp_nid_postfix = &wideStandard;
    char16_t pair[3] = {u'!', u'!', u'!'};
    Require(ScanInput("%l[a-c] %2lc", wideWord, pair) == 2, "vscanf wide scanset and width");
    Require(std::u16string(wideWord) == u"abc" && pair[0] == u'X' && pair[1] == u'Y' && pair[2] == u'!',
        "vscanf writes two-byte elements without terminating lc");
    Require(scanf_nid_postfix("%S %C", wideWord, pair) == 2 && std::u16string(wideWord) == u"def" && pair[0] == u'Q',
        "scanf guest standard input wide aliases");
    Require(std::fgetc(wideStandard.GetHandle()) == 'Z', "scanf preserves next unread input");
    __stdinp_nid_postfix = standardInput;
    wideStandard.Close();

#ifndef _WIN32
    FileStream utf8(std::tmpfile());
    Require(std::fputs("žluť 7 été!", utf8.GetHandle()) >= 0, "seed UTF-8 variadic input");
    std::rewind(utf8.GetHandle());
    longCount = -1;
    Require(ScanStream(&utf8, "%4ls %d %2lc%ln", wideWord, &values[0], pair, &longCount) == 3,
        "vfscanf mixed UTF-8 and narrow conversions");
    Require(std::u16string(wideWord) == u"žluť" && values[0] == 7 && pair[0] == u'é' && pair[1] == u't' && pair[2] == u'!',
        "vfscanf decodes UTF-8 and counts width in characters");
    Require(longCount == 12 && std::ftell(utf8.GetHandle()) == 12, "vfscanf counts UTF-8 input bytes");
    Require(ScanStream(&utf8, "%lc", pair) == 1 && pair[0] == u'é', "vfscanf restores multibyte lookahead");
    Require(std::fgetc(utf8.GetHandle()) == '!', "vfscanf preserves input after a multibyte field");
    utf8.Close();

    FileStream utf8Set(std::tmpfile());
    Require(std::fputs("abé", utf8Set.GetHandle()) >= 0, "seed UTF-8 scanset input");
    std::rewind(utf8Set.GetHandle());
    Require(ScanStream(&utf8Set, "%l[a-z]", wideWord) == 1 && std::u16string(wideWord) == u"ab",
        "vfscanf scanset stops before a multibyte character");
    Require(ScanStream(&utf8Set, "%lc", pair) == 1 && pair[0] == u'é', "vfscanf scanset preserves multibyte input");
    utf8Set.Close();

    struct InvalidCase { const char* text; long consumed; };
    for (const auto& test : {InvalidCase{"\xff", 0}, InvalidCase{"\xc3", 1}, InvalidCase{"\xf0\x9f\x98\x80", 0}}) {
        FileStream invalid(std::tmpfile());
        Require(std::fputs(test.text, invalid.GetHandle()) >= 0, "seed unsupported wide input");
        std::rewind(invalid.GetHandle());
        wideWord[0] = u'!';
        bool rejected = false;
        try { ScanStream(&invalid, "%ls", wideWord); } catch (const std::runtime_error&) { rejected = true; }
        Require(rejected && wideWord[0] == u'!' && std::ftell(invalid.GetHandle()) == test.consumed,
            "vfscanf rejects unsupported encoding and consumes only its accepted prefix");
        invalid.Close();
    }

    for (int variant = 0; variant < 3; ++variant) {
        FileStream pipe(OpenPipe("abé7"));
        longCount = -1;
        __stdinp_nid_postfix = &pipe;
        const int result = variant == 0 ? ScanStream(&pipe, "%l[a-z]%ln", wideWord, &longCount) :
            variant == 1 ? fscanf_nid_postfix(&pipe, "%l[a-z]%ln", wideWord, &longCount) :
            ScanInput("%l[a-z]%ln", wideWord, &longCount);
        __stdinp_nid_postfix = standardInput;
        Require(result == 1 && std::u16string(wideWord) == u"ab" && longCount == 2,
            "nonseekable scanset counts only accepted bytes");
        char unread[2]{};
        Require(std::fread(unread, 1, 2, pipe.GetHandle()) == 2 && static_cast<unsigned char>(unread[0]) == 0xc3 &&
            static_cast<unsigned char>(unread[1]) == 0xa9, "ordinary fread sees all rejected multibyte input");
        Require(fscanf_nid_postfix(&pipe, "%d", &values[0]) == 1 && values[0] == 7,
            "a following narrow scan sees the original stream");
        Require(std::fgetc(pipe.GetHandle()) == EOF, "nonseekable stream reaches EOF after its input");
        pipe.Close();
    }
    {
        FileStream pipe(OpenPipe("é€Z"));
        count = -1;
        Require(ScanStream(&pipe, "%1lc%n", pair, &count) == 1 && pair[0] == u'é' && count == 2,
            "first pipe scan counts UTF-8 bytes");
        count = -1;
        Require(ScanStream(&pipe, "%1lc%n", pair, &count) == 1 && pair[0] == u'€' && count == 3,
            "second pipe scan retains the next multibyte character");
        Require(std::fgetc(pipe.GetHandle()) == 'Z', "ordinary fgetc follows repeated wide scans");
        pipe.Close();
    }
    {
        FileStream pipe(OpenPipe("é€Z"));
        longCount = -1;
        Require(ScanStream(&pipe, "%*2lc%ln", &longCount) == 0 && longCount == 5,
            "suppressed wide fields consume characters and count bytes");
        Require(std::fgetc(pipe.GetHandle()) == 'Z', "suppression leaves the next byte in the host stream");
        pipe.Close();
    }
    {
        FileStream pipe(OpenPipe("é7\n"));
        Require(ScanStream(&pipe, "%l[a-z]", wideWord) == 0, "nonseekable wide matching failure");
        char unread[8]{};
        Require(std::fgets(unread, sizeof(unread), pipe.GetHandle()) != nullptr && std::strcmp(unread, "é7\n") == 0,
            "ordinary fgets sees bytes rejected by the scanner");
        pipe.Close();
    }
    for (const char* text : {"\xc3X7", "\xe2\x82X7"}) {
        FileStream pipe(OpenPipe(text));
        int before = -1, after = -1;
        wideWord[0] = u'!';
        bool rejected = false;
        try { ScanStream(&pipe, "%n%ls%n", &before, wideWord, &after); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected && before == 0 && after == -1 && wideWord[0] == u'!',
            "an encoding failure preserves destination and later count conversions");
        Require(std::fgetc(pipe.GetHandle()) == 'X', "the first byte outside an invalid UTF-8 prefix remains unread");
        Require(fscanf_nid_postfix(&pipe, "%d", &values[0]) == 1 && values[0] == 7,
            "narrow scanning continues after an encoding failure");
        pipe.Close();
    }
    {
        FileStream pipe(OpenPipe("\xe2\x82"));
        bool rejected = false;
        try { ScanStream(&pipe, "%lc", pair); } catch (const std::runtime_error&) { rejected = true; }
        Require(rejected && std::feof(pipe.GetHandle()) && (pipe.GuestState().flags & 0x20) != 0,
            "truncated UTF-8 synchronizes guest EOF on the exception path");
        Require(std::fgetc(pipe.GetHandle()) == EOF, "truncated input has no discarded lookahead");
        pipe.Close();
    }
    {
        FileStream pipe(OpenPipe("\xf0\x9f\x98\x80Z"));
        bool rejected = false;
        try { ScanStream(&pipe, "%lc", pair); } catch (const std::runtime_error&) { rejected = true; }
        char unread[4]{};
        Require(rejected && std::fread(unread, 1, 4, pipe.GetHandle()) == 4 &&
            std::memcmp(unread, "\xf0\x9f\x98\x80", 4) == 0 && std::fgetc(pipe.GetHandle()) == 'Z',
            "an unsupported four-byte lead is rejected before consuming its sequence");
        pipe.Close();
    }

    struct FailureCase { const char* input; int result; int first; int second; std::int64_t count; };
    for (const auto& test : {
        FailureCase{"+", 0, -1, -1, -1},
        FailureCase{"   +", 0, -1, -1, -1},
        FailureCase{"x", 0, -1, -1, -1},
        FailureCase{"", EOF, -1, -1, -1},
        FailureCase{"   ", EOF, -1, -1, -1},
        FailureCase{"7 +", 1, 7, -1, 1},
        FailureCase{"7", 1, 7, -1, 1},
        FailureCase{"7 8", 2, 7, 8, 1},
        FailureCase{"7 8 ok", 3, 7, 8, 1}}) {
        for (int variant = 0; variant < 3; ++variant) {
            FileStream failure(std::tmpfile());
            Require(std::fputs(test.input, failure.GetHandle()) >= 0, "seed conversion failure case");
            std::rewind(failure.GetHandle());
            int initial = -1, firstValue = -1, secondValue = -1;
            std::int64_t afterFirst = -1;
            char16_t tail[8]{};
            const char* format = "%n%d%ln %d %ls";
            const int result = variant == 0 ? ScanStream(&failure, format, &initial, &firstValue, &afterFirst, &secondValue, tail) :
                variant == 1 ? fscanf_nid_postfix(&failure, format, &initial, &firstValue, &afterFirst, &secondValue, tail) :
                sscanf_nid_postfix(test.input, format, &initial, &firstValue, &afterFirst, &secondValue, tail);
            Require(result == test.result, "mixed scanner distinguishes matching failure and input failure");
            Require(initial == 0 && firstValue == test.first && secondValue == test.second && afterFirst == test.count,
                "mixed scanner preserves assignments and count conversion ordering");
            if (result == 3) Require(std::u16string(tail) == u"ok", "mixed scanner successful control");
            failure.Close();
        }
    }
    for (const auto& test : {FailureCase{"+", 0, -1, -1, -1}, FailureCase{"", EOF, -1, -1, -1},
        FailureCase{"7 +", 0, -1, -1, 1}, FailureCase{"7", 0, -1, -1, 1}, FailureCase{"7 8", 1, 8, -1, 1}}) {
        FileStream failure(std::tmpfile());
        Require(std::fputs(test.input, failure.GetHandle()) >= 0, "seed suppressed conversion failure");
        std::rewind(failure.GetHandle());
        int value = -1;
        std::int64_t afterFirst = -1;
        char16_t tail[8]{};
        Require(ScanStream(&failure, "%*d%ln %d %ls", &afterFirst, &value, tail) == test.result,
            "suppressed conversion does not turn matching failure into EOF");
        Require(value == test.first && afterFirst == test.count, "suppressed conversion count and pointer order");
        failure.Close();
    }
#endif

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
