#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"
#include <array>
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

static int APS5_VABI ScanStream(FileStream* stream, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfwscanf_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanStreamSized(FileStream* stream, const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfwscanf_s_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanInput(const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vwscanf_nid_postfix(format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanInputSized(const char16_t* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vwscanf_s_nid_postfix(format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanNarrow(FileStream* stream, const char* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfscanf_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanNarrowSized(FileStream* stream, const char* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vfscanf_s_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static int APS5_VABI ScanNarrowInput(const char* format, ...) {
    APS5_VA_BEGIN(format);
    const int result = vscanf_nid_postfix(format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

static FileStream* Open(const char* text) {
    auto* stream = new FileStream(TemporaryFile::Open());
    Require(std::fputs(text, stream->GetHandle()) >= 0, "seed the stream");
    std::rewind(stream->GetHandle());
    return stream;
}

static void Close(FileStream* stream) {
    stream->Close();
    delete stream;
}

static void CheckNarrowFormatWideFields() {
    std::int32_t value = 0;
    std::int64_t count = -1;
    char16_t word[8]{};
    char16_t letter = u'!';
    char narrow[8]{};

    FileStream* stream = Open("  zlu ku 7");
    Require(fscanf_nid_postfix(stream, "%ls %lc", word, &letter) == 2 && std::u16string(word) == u"zlu" && letter == u'k', "fscanf %ls and %lc");
    Require(ScanNarrow(stream, "%S%ln %d", word, &count, &value) == 2 && std::u16string(word) == u"u" && count == 1 && value == 7, "vfscanf %S with %ln counting bytes");
    Require(fgetc_nid_postfix(stream) == EOF, "the whole input is consumed");
    Close(stream);

    stream = Open("12 ab cd");
    Require(fscanf_nid_postfix(stream, "%d %ls %7s", &value, word, narrow) == 3 && value == 12 && std::u16string(word) == u"ab" && std::strcmp(narrow, "cd") == 0, "wide and narrow fields share the stream");
    Close(stream);

    stream = Open("ab\xc3\xa9t rest");
    Require(fscanf_nid_postfix(stream, "%l[a-c]", word) == 1 && std::u16string(word) == u"ab", "a scanset stops at a byte outside it");
    *__error_nid_postfix() = 0;
    Require(fscanf_nid_postfix(stream, "%l[^ ] %7s", word, narrow) == 0 && *__error_nid_postfix() == 86, "a byte after a UTF-8 lead byte fails the field");
    Require(fgetc_nid_postfix(stream) == 0xc3 && fgetc_nid_postfix(stream) == 0xa9, "the whole invalid multibyte character stays in the stream");
    Close(stream);

    char16_t three[5] = {u'!', u'!', u'!', u'!', u'!'};
    stream = Open("xyzw");
    Require(fscanf_nid_postfix(stream, "%3lc", three) == 1 && three[0] == u'x' && three[1] == u'y' && three[2] == u'z' && three[3] == u'!', "%3lc stores three units without a terminator");
    Require(fgetc_nid_postfix(stream) == 'w', "the character after %3lc stays in the stream");
    Close(stream);

    stream = Open("xyz");
    Require(fscanf_nid_postfix(stream, "%l[a-c]", word) == 0, "a scanset matching nothing");
    Require(fgetc_nid_postfix(stream) == 'x', "the rejected character stays in the stream");
    Close(stream);

    stream = Open("   ");
    Require(fscanf_nid_postfix(stream, "%ls", word) == EOF, "only white space before %ls");
    Close(stream);

    stream = Open("hello world");
    char16_t fits[6]{};
    char16_t small[4] = {u'!', u'!', u'!', u'!'};
    Require(fscanf_s_nid_postfix(stream, "%ls", fits, 6u) == 1 && std::u16string(fits) == u"hello", "fscanf_s %ls that fits");
    Require(ScanNarrowSized(stream, "%ls", small, 4u) == 0 && small[0] == 0, "vfscanf_s rejects a too small buffer");
    Close(stream);

    FileStream* const standardInput = __stdinp_nid_postfix;
    stream = Open("ab c");
    __stdinp_nid_postfix = stream;
    Require(scanf_nid_postfix("%ls", word) == 1 && std::u16string(word) == u"ab", "scanf %ls reads the guest standard input");
    Require(ScanNarrowInput(" %lc", &letter) == 1 && letter == u'c', "vscanf %lc reads the guest standard input");
    __stdinp_nid_postfix = standardInput;
    Close(stream);
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

static void CheckNarrowWideFailurePosition() {
    struct Case {
        const char* input;
        const char* format;
        int result;
        long position;
        int next;
        int error;
        int count;
        std::array<char16_t, 8> output;
        const char* remaining;
    };
    const Case cases[]{
        {"\xc3\xa9", "%ls%n", 0, 0, 195, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\xa9"},
        {"\x61\xc3\xa9", "%ls%n", 0, 1, 195, 86, -77, {0x61, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\xa9"},
        {"\x61\x62\x80", "%ls%n", 0, 2, 128, 86, -77, {0x61, 0x62, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\x80"},
        {"\xc3\x20\x61", "%ls%n", -1, 1, 32, 97, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\x20\x61"},
        {"\xc3", "%ls%n", -1, 1, -1, 97, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, ""},
        {"\x80", "%ls%n", -1, 0, 128, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\x80"},
        {"\x61\x80", "%ls%n", 0, 1, 128, 86, -77, {0x61, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\x80"},
        {"\xc3\xa9", "%1ls%n", -1, 1, 169, 97, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xa9"},
        {"\x61\xc3\xa9", "%2ls%n", -1, 2, 169, 97, -77, {0x61, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xa9"},
        {"\x61\xc3\xa9", "%3ls%n", 0, 1, 195, 86, -77, {0x61, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\xa9"},
        {"\xc3\xa9", "%2lc%n", 0, 0, 195, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\xa9"},
        {"\x61\xc3\xa9", "%l[^ ]%n", 0, 1, 195, 86, -77, {0x61, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\xa9"},
        {"\xc3\xa9", "%S%n", 0, 0, 195, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\xa9"},
        {"\x61\xc3\xa9", "%lS%n", 0, 1, 195, 86, -77, {0x61, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\xa9"},
        {"\xff\x7a", "%ls%n", -1, 1, 122, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\x7a"},
        {"\x61\xff\x7a", "%ls%n", 0, 2, 122, 86, -77, {0x61, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\x7a"},
        {"\xc3\xff\x7a", "%ls%n", 0, 1, 195, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\x7a"},
        {"\xc3\x80\x7a", "%ls%n", 0, 0, 195, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\x80\x7a"},
        {"\xc3\xc3\x7a", "%ls%n", 0, 0, 195, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\xc3\x7a"},
        {"\xc3\xff\x7a", "%2lc%n", 0, 1, 195, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\x7a"},
        {"\xc3\xff\x7a", "%3ls%n", 0, 1, 195, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\x7a"},
        {"\xc3\xff\x7a", "%l[^ ]%n", 0, 1, 195, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\x7a"},
        {"\xff\x7a", "%S%n", -1, 1, 122, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\x7a"},
        {"\xc3\xff\x7a", "%lS%n", 0, 1, 195, 86, -77, {0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5, 0xa5a5}, "\xc3\x7a"},
    };
    for (const auto& item : cases) {
        for (const bool secure : {false, true}) {
            for (const bool variadic : {false, true}) {
                auto* stream = Open(item.input);
                std::array<char16_t, 8> output;
                output.fill(0xa5a5);
                int count = -77;
                *__error_nid_postfix() = 97;
                const int result = secure ? (variadic ? ScanNarrowSized(stream, item.format, output.data(), 8u, &count) : fscanf_s_nid_postfix(stream, item.format, output.data(), 8u, &count)) : (variadic ? ScanNarrow(stream, item.format, output.data(), &count) : fscanf_nid_postfix(stream, item.format, output.data(), &count));
                Require(result == item.result && count == item.count && output == item.output, "wide field failure keeps measured result, count and stored units");
                Require(*__error_nid_postfix() == item.error, "wide field failure keeps measured errno");
                Require(ftell_nid_postfix(stream) == item.position, "wide field failure keeps the measured stream position");
                const int next = fgetc_nid_postfix(stream);
                Require(next == item.next, "wide field failure leaves the measured next input byte");
                std::string remaining;
                if (next != EOF) remaining += static_cast<char>(next);
                for (int byte; (byte = fgetc_nid_postfix(stream)) != EOF;) remaining += static_cast<char>(byte);
                Require(remaining == item.remaining, "wide field failure keeps the measured remaining input");
                Close(stream);
            }
        }
    }
}

int main() {
    CheckNarrowWideFailurePosition();
    CheckConsumption();
    CheckWideText();
    CheckStandardInput();
    CheckNarrowFormatWideFields();
    return 0;
}
