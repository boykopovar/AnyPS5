#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" {
int* APS5_VABI __error_nid_postfix();
extern FileStream* __stdinp_nid_postfix;
int APS5_VABI vfwscanf_nid_postfix(FileStream*, const char16_t*, void*);
int APS5_VABI vfwscanf_s_nid_postfix(FileStream*, const char16_t*, void*);
int APS5_VABI vwscanf_nid_postfix(const char16_t*, void*);
int APS5_VABI vwscanf_s_nid_postfix(const char16_t*, void*);
int APS5_VABI fwscanf_nid_postfix(FileStream*, const char16_t*, ...);
int APS5_VABI fwscanf_s_nid_postfix(FileStream*, const char16_t*, ...);
int APS5_VABI wscanf_nid_postfix(const char16_t*, ...);
int APS5_VABI wscanf_s_nid_postfix(const char16_t*, ...);
std::int32_t APS5_VABI fgetwc_nid_postfix(FileStream*);
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

static int APS5_VABI ScanStream(FileStream* stream, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfwscanf_nid_postfix(stream, format, args);
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanStreamSized(FileStream* stream, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfwscanf_s_nid_postfix(stream, format, args);
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanInput(const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vwscanf_nid_postfix(format, args);
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanInputSized(const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vwscanf_s_nid_postfix(format, args);
    APS5_VA_END();
    return result;
}

static FileStream* Open(const char* text) {
    auto* stream = new FileStream(std::tmpfile());
    Require(std::fputs(text, stream->GetHandle()) >= 0, "seed the stream");
    std::rewind(stream->GetHandle());
    return stream;
}

static void Close(FileStream* stream) {
    stream->Close();
    delete stream;
}

static void CheckConsumption() {
    std::int32_t value = -1;
    std::int64_t count = -1;
    char16_t word[8]{};

    FileStream* stream = Open("  42 rest");
    Require(ScanStream(stream, u"%d", &value) == 1 && value == 42, "an integer after leading space");
    Require(fgetwc_nid_postfix(stream) == u' ', "the character after the field stays in the stream");
    Require(fwscanf_nid_postfix(stream, u"%ls", word) == 1 && std::u16string(word) == u"rest", "a following fwscanf continues");
    Require(fwscanf_nid_postfix(stream, u"%d", &value) == EOF, "end of file before any conversion");
    Close(stream);

    stream = Open("0xg");
    std::uint32_t hex = 7;
    Require(ScanStream(stream, u"%x", &hex) == 0 && hex == 7, "a hexadecimal prefix without digits is a matching failure");
    Require(fgetwc_nid_postfix(stream) == u'g', "the prefix is consumed and the next character is pushed back");
    Close(stream);

    stream = Open("12abc");
    Require(ScanStream(stream, u"%d%ln", &value, &count) == 1 && value == 12 && count == 2, "%ln counts the consumed characters");
    Require(fgetwc_nid_postfix(stream) == u'a', "the field stops before the first letter");
    Close(stream);

    stream = Open("ab");
    Require(ScanStream(stream, u"ac") == 0, "a literal mismatch");
    Require(fgetwc_nid_postfix(stream) == u'b', "the mismatching character is pushed back");
    Close(stream);

    stream = Open("");
    Require(ScanStream(stream, u"x") == 0, "a literal at end of input returns zero");
    Close(stream);

    stream = Open("");
    Require(ScanStream(stream, u"%%") == -1, "a percent literal at end of input is an input failure");
    Close(stream);

    stream = Open("x");
    Require(ScanStream(stream, u"%d", &value) == 0, "a letter is not an integer");
    Require(fgetwc_nid_postfix(stream) == u'x', "the rejected letter is pushed back");
    Close(stream);

    stream = Open("-");
    Require(ScanStream(stream, u"%d", &value) == 0, "a lone sign is a matching failure");
    Require(fgetwc_nid_postfix(stream) == -1, "the sign is consumed");
    Close(stream);

    stream = Open("   ");
    Require(ScanStream(stream, u"%d", &value) == EOF, "only white space");
    Close(stream);

    stream = Open("2.5x");
    double real = 0;
    Require(ScanStream(stream, u"%lf", &real) == 1 && real == 2.5, "a float");
    Require(fgetwc_nid_postfix(stream) == u'x', "the character after the float");
    Close(stream);

    stream = Open("1e+");
    Require(ScanStream(stream, u"%lf", &real) == 1 && real == 1.0, "an incomplete exponent converts its mantissa");
    Close(stream);
}

static void CheckWideText() {
    std::int32_t value = 0;
    char16_t letters[8]{};
    FileStream* stream = Open("5 ab\xc3\xa9t\xc3\xa9");
    Require(ScanStream(stream, u"%d %l[abc]", &value, letters) == 2 && value == 5 && std::u16string(letters) == u"ab", "a scanset stops at a non-ASCII character");
    Require(fgetwc_nid_postfix(stream) == 0xe9, "a non-ASCII lookahead goes back as UTF-8");
    Require(fwscanf_nid_postfix(stream, u"%ls", letters) == 1 && std::u16string(letters) == u"té", "the rest decodes");
    Close(stream);

    stream = Open("hello world");
    char16_t small[4]{};
    Require(ScanStreamSized(stream, u"%3ls", small, 4u) == 1 && std::u16string(small) == u"hel", "vfwscanf_s %ls with a size");
    Require(fwscanf_s_nid_postfix(stream, u"%ls", small, 4u) == 1 && std::u16string(small) == u"lo", "fwscanf_s %ls that fits");
    Require(fwscanf_s_nid_postfix(stream, u"%ls", small, 4u) == 0 && small[0] == 0, "fwscanf_s rejects a too small buffer");
    Close(stream);

    Require(fwscanf_nid_postfix(nullptr, u"%d", &value) == EOF, "null stream");
    Require(vfwscanf_s_nid_postfix(nullptr, u"%d", nullptr) == EOF, "null stream with sizes");
}

static void CheckStandardInput() {
    FileStream* const standardInput = __stdinp_nid_postfix;
    FileStream* stream = Open("-5 12 ab cd");
    __stdinp_nid_postfix = stream;
    std::int32_t first = 0, second = 0;
    char16_t word[4]{};
    Require(ScanInput(u"%d", &first) == 1 && first == -5, "vwscanf reads the guest standard input");
    Require(wscanf_nid_postfix(u"%d", &second) == 1 && second == 12, "wscanf reads the guest standard input");
    Require(ScanInputSized(u"%ls", word, 4u) == 1 && std::u16string(word) == u"ab", "vwscanf_s");
    Require(wscanf_s_nid_postfix(u"%ls", word, 4u) == 1 && std::u16string(word) == u"cd", "wscanf_s");
    __stdinp_nid_postfix = standardInput;
    Close(stream);
}

static void CheckShortCharacterFields() {
    const char* inputs[]{"", "a", "ab", "abc", "abcd"};
    for (const char* input : inputs) {
        const std::size_t length = std::string(input).size();
        const std::size_t consumed = length < 3 ? length : 3;
        for (int mode = 0; mode < 4; ++mode) {
            FileStream* stream = Open(input);
            char16_t output[]{0xa55a, 0xa55a, 0xa55a, 0xa55a, 0xa55a};
            int count = -1;
            const int result = mode == 0 ? fwscanf_nid_postfix(stream, u"%3lc%n", output + 1, &count)
                             : mode == 1 ? ScanStream(stream, u"%3lc%n", output + 1, &count)
                             : mode == 2 ? fwscanf_s_nid_postfix(stream, u"%3lc%n", output + 1, 3u, &count)
                                         : ScanStreamSized(stream, u"%3lc%n", output + 1, 3u, &count);
            Require(result == (length == 0 ? EOF : 1), "short stream character assignment count");
            Require(count == (length == 0 ? -1 : static_cast<int>(consumed)), "short stream character consumed count");
            for (std::size_t index = 0; index < 5; ++index) {
                const char16_t expected = index > 0 && index <= consumed ? static_cast<char16_t>(input[index - 1]) : 0xa55a;
                Require(output[index] == expected, "short stream character exact write extent without terminator");
            }
            Require(fgetwc_nid_postfix(stream) == (length > 3 ? 'd' : -1), "short stream character lookahead");
            Close(stream);
        }
    }
    for (bool sized : {false, true}) {
        FileStream* stream = Open("ab");
        int count = -1;
        const int result = sized ? ScanStreamSized(stream, u"%*3lc%n", &count) : ScanStream(stream, u"%*3lc%n", &count);
        Require(result == 0 && count == 2, "suppressed short stream field continues to count");
        Require(fgetwc_nid_postfix(stream) == -1, "suppressed short stream field consumes input");
        Close(stream);
    }
}

static void CheckZeroCapacityCharacterFields() {
    for (bool indirect : {false, true}) {
        FileStream* stream = Open("a");
        char16_t output[]{0xa55a, 0xa55a, 0xa55a};
        int count = -1;
        *__error_nid_postfix() = 0;
        const int result = indirect ? ScanStreamSized(stream, u"%3lc%n", output + 1, 0u, &count)
                                    : fwscanf_s_nid_postfix(stream, u"%3lc%n", output + 1, 0u, &count);
        const int error = *__error_nid_postfix();
        Require(result == 0, "zero-capacity stream character field returns no assignment");
        Require(count == -1, "zero-capacity stream character field does not reach percent n");
        Require(error == 0, "zero-capacity stream character field preserves errno");
        for (const char16_t unit : output) Require(unit == 0xa55a, "zero-capacity stream character field leaves output untouched");
        Close(stream);
    }
}

static void CheckSecureCharacterCapacities() {
    const char* inputs[]{"", "a", "ab", "abc", "abcd"};
    for (const char* input : inputs) {
        const std::size_t length = std::string(input).size();
        for (unsigned int capacity : {1u, 2u, 3u, 4u}) {
            const std::size_t limit = capacity < 3 ? capacity : 3;
            const std::size_t consumed = length < limit ? length : limit;
            for (bool indirect : {false, true}) {
                FileStream* stream = Open(input);
                char16_t output[]{0xa55a, 0xa55a, 0xa55a, 0xa55a, 0xa55a, 0xa55a};
                int count = -1;
                *__error_nid_postfix() = 0;
                const int result = indirect ? ScanStreamSized(stream, u"%3lc%n", output + 1, capacity, &count)
                                            : fwscanf_s_nid_postfix(stream, u"%3lc%n", output + 1, capacity, &count);
                Require(result == (length == 0 ? EOF : 1), "positive secure stream character capacity assignment count");
                Require(count == (length == 0 ? -1 : static_cast<int>(consumed)), "positive secure stream character capacity consumed count");
                Require(*__error_nid_postfix() == 0, "positive secure stream character capacity preserves errno");
                for (std::size_t index = 0; index < 6; ++index) {
                    const char16_t expectedUnit = index > 0 && index <= consumed ? static_cast<char16_t>(input[index - 1]) : 0xa55a;
                    Require(output[index] == expectedUnit, "positive secure stream character capacity exact writes without terminator");
                }
                Require(fgetwc_nid_postfix(stream) == (length > consumed ? input[consumed] : -1),
                        "positive secure stream character capacity leaves lookahead");
                Close(stream);
            }
        }
    }
    for (bool indirect : {false, true}) {
        FileStream* stream = Open("7 abcZ");
        char16_t output[]{0xa55a, 0xa55a, 0xa55a, 0xa55a};
        int value = -1, count = -1;
        const int result = indirect ? ScanStreamSized(stream, u"%d %3lc%n", &value, output + 1, 2u, &count)
                                    : fwscanf_s_nid_postfix(stream, u"%d %3lc%n", &value, output + 1, 2u, &count);
        Require(result == 2 && value == 7 && count == 4, "positive secure stream capacity after another assignment");
        Require(output[0] == 0xa55a && output[1] == u'a' && output[2] == u'b' && output[3] == 0xa55a,
                "positive secure stream capacity after another assignment preserves bounds");
        Require(fgetwc_nid_postfix(stream) == u'c', "positive secure stream capacity after another assignment leaves lookahead");
        Close(stream);
    }
}

int main() {
    CheckZeroCapacityCharacterFields();
    CheckSecureCharacterCapacities();
    CheckShortCharacterFields();
    CheckConsumption();
    CheckWideText();
    CheckStandardInput();
    return 0;
}
