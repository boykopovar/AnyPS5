#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

extern "C" {

extern FileStream* __stdinp_nid_postfix;
}

namespace {

constexpr std::size_t failed = static_cast<std::size_t>(-1);
constexpr std::size_t tooLarge = std::size_t{1} << 61;

int constraintCalls = 0;
int constraintError = 0;
std::string constraintMessage;

void APS5_VABI RecordConstraint(const char* message, void* pointer, int error) {
    ++constraintCalls;
    constraintMessage = message;
    constraintError = error;
    if (pointer != nullptr) {
        std::fprintf(stderr, "constraint handler received a non-null pointer\n");
        std::abort();
    }
}

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void RequireViolation(int before, const char* message, int error) {
    if (constraintCalls != before + 1 || constraintMessage != message || constraintError != error) {
        std::fprintf(stderr, "expected one \"%s\" violation, got %d calls (last \"%s\")\n", message, constraintCalls - before, constraintMessage.c_str());
        std::abort();
    }
}

int APS5_VABI CompareInts(const void* left, const void* right, void* context) {
    ++*static_cast<int*>(context);
    const int a = *static_cast<const int*>(left);
    const int b = *static_cast<const int*>(right);
    return (a > b) - (a < b);
}

std::basic_string<std::uint16_t> Wide(const char* text) {
    std::basic_string<std::uint16_t> wide;
    while (*text) wide.push_back(static_cast<unsigned char>(*text++));
    return wide;
}

void CheckBsearch() {
    const int values[] = {1, 3, 3, 3, 5, 7, 9};
    int calls = 0;
    const int key = 3;
    Require(bsearch_s_nid_postfix(&key, values, 7, sizeof(int), CompareInts, &calls) == &values[3] && calls == 1,
        "bsearch_s did not probe the middle element first");
    const int missing = 4;
    calls = 0;
    Require(bsearch_s_nid_postfix(&missing, values, 7, sizeof(int), CompareInts, &calls) == nullptr && calls == 3,
        "bsearch_s found a missing key");
    const int last = 9;
    Require(bsearch_s_nid_postfix(&last, values, 7, sizeof(int), CompareInts, &calls) == &values[6], "bsearch_s missed the last element");
    int before = constraintCalls;
    Require(bsearch_s_nid_postfix(nullptr, nullptr, 0, sizeof(int), nullptr, nullptr) == nullptr && constraintCalls == before,
        "bsearch_s reported an empty array");
    Require(bsearch_s_nid_postfix(nullptr, values, 7, sizeof(int), CompareInts, &calls) == nullptr, "bsearch_s accepted a null key");
    RequireViolation(before, "bsearch_s: bad argument", 22);
    before = constraintCalls;
    Require(bsearch_s_nid_postfix(&key, values, 0, tooLarge, CompareInts, &calls) == nullptr, "bsearch_s accepted an oversized element");
    RequireViolation(before, "bsearch_s: bad argument", 22);
    struct ZeroStrideCase { std::size_t count; int lessCalls; int greaterCalls; };
    const ZeroStrideCase zeroStrideCases[] = {{0, 0, 0}, {1, 1, 1}, {2, 2, 1}, {3, 2, 2}, {4, 3, 2}, {7, 3, 3}, {8, 4, 3}, {15, 4, 4}, {16, 5, 4}, {1023, 10, 10}, {1024, 11, 10}, {tooLarge - 1, 61, 61}};
    for (const auto& entry : zeroStrideCases) for (const int zeroKey : {0, 1, 2}) {
        before = constraintCalls;
        calls = 0;
        *__error_nid_postfix() = 97;
        void* result = bsearch_s_nid_postfix(&zeroKey, values, entry.count, 0, CompareInts, &calls);
        const void* expected = entry.count != 0 && zeroKey == 1 ? values : nullptr;
        const int expectedCalls = zeroKey == 0 ? entry.lessCalls : zeroKey == 2 ? entry.greaterCalls : entry.count != 0;
        Require(result == expected && calls == expectedCalls, "bsearch_s zero stride result or comparison count");
        Require(constraintCalls == before && *__error_nid_postfix() == 97, "bsearch_s zero stride invoked a constraint or changed errno");
    }
    before = constraintCalls;
    calls = 0;
    Require(bsearch_s_nid_postfix(&key, values, tooLarge, 0, CompareInts, &calls) == nullptr && calls == 0, "bsearch_s searched an oversized count");
    RequireViolation(before, "bsearch_s: bad argument", 22);
    calls = 0;
    Require(bsearch_s_nid_postfix(&values[0], values, 1, tooLarge - 1, CompareInts, &calls) == values && calls == 1, "bsearch_s rejected an RSIZE_MAX element size");
    before = constraintCalls;
    Require(bsearch_s_nid_postfix(nullptr, values, 7, 0, CompareInts, &calls) == nullptr, "bsearch_s zero stride accepted a null key");
    RequireViolation(before, "bsearch_s: bad argument", 22);
}

void CheckWcstok() {
    auto text = Wide(",,ab;c,,;");
    const auto delimiters = Wide(",;");
    std::size_t remaining = text.size() + 1;
    std::uint16_t* context = nullptr;
    std::uint16_t* token = wcstok_s_nid_postfix(text.data(), &remaining, delimiters.c_str(), &context);
    Require(token == text.data() + 2 && token[0] == 'a' && token[1] == 'b' && token[2] == 0 && remaining == 5 && context == text.data() + 5,
        "wcstok_s did not return the first token");
    token = wcstok_s_nid_postfix(nullptr, &remaining, delimiters.c_str(), &context);
    Require(token == text.data() + 5 && token[1] == 0 && remaining == 3 && context == text.data() + 7, "wcstok_s did not return the second token");
    token = wcstok_s_nid_postfix(nullptr, &remaining, delimiters.c_str(), &context);
    Require(token == nullptr && remaining == 1 && context == text.data() + 9, "wcstok_s did not stop at the end of the string");

    int before = constraintCalls;
    auto unterminated = Wide("abc");
    remaining = 2;
    Require(wcstok_s_nid_postfix(unterminated.data(), &remaining, delimiters.c_str(), &context) == nullptr, "wcstok_s read past s1max");
    RequireViolation(before, "wcstok_s: missing delimiter", 22);
    before = constraintCalls;
    auto separators = Wide(",,,x");
    remaining = 3;
    Require(wcstok_s_nid_postfix(separators.data(), &remaining, delimiters.c_str(), &context) == nullptr, "wcstok_s skipped past s1max");
    RequireViolation(before, "wcstok_s: missing token", 22);
    before = constraintCalls;
    remaining = 0;
    Require(wcstok_s_nid_postfix(separators.data(), &remaining, delimiters.c_str(), &context) == nullptr, "wcstok_s accepted a zero s1max");
    RequireViolation(before, "wcstok_s: missing token", 22);
    before = constraintCalls;
    context = nullptr;
    remaining = 4;
    Require(wcstok_s_nid_postfix(nullptr, &remaining, delimiters.c_str(), &context) == nullptr, "wcstok_s accepted a null context string");
    RequireViolation(before, "wcstok_s: bad argument", 22);
}

void CheckWideToMultibyte() {
    int status = 7;
    char bytes[4] = {'x', 'x', 'x', 'x'};
    Require(wctomb_s_nid_postfix(&status, bytes, sizeof(bytes), 'A') == 0 && status == 1 && bytes[0] == 'A', "wctomb_s did not convert");
    Require(wctomb_s_nid_postfix(&status, bytes, sizeof(bytes), 0x263a) == 0 && status == 3 && std::memcmp(bytes, "\xE2\x98\xBA", 3) == 0,
        "wctomb_s did not convert U+263A to UTF-8");
    std::memset(bytes, 'x', sizeof(bytes));
    status = 7;
    int smallBefore = constraintCalls;
    Require(wctomb_s_nid_postfix(&status, bytes, 2, 0x263a) == 34 && status == 7 && std::memcmp(bytes, "xxxx", 4) == 0,
        "wctomb_s wrote past a two-byte buffer");
    RequireViolation(smallBefore, "wctomb_s: buffer too small", 34);
    Require(wctomb_s_nid_postfix(&status, nullptr, 0, 'A') == 0 && status == 0, "wctomb_s reported state dependence");
    status = 7;
    int before = constraintCalls;
    Require(wctomb_s_nid_postfix(&status, bytes, 0, 'A') == 34 && status == 7, "wctomb_s wrote into a zero-size buffer");
    RequireViolation(before, "wctomb_s: buffer too small", 34);
    before = constraintCalls;
    Require(wctomb_s_nid_postfix(nullptr, bytes, sizeof(bytes), 'A') == 22, "wctomb_s accepted a null status");
    RequireViolation(before, "wctomb_s: bad argument", 22);

    unsigned char state[128]{};
    std::size_t result = 7;
    Require(wcrtomb_s_nid_postfix(&result, bytes, sizeof(bytes), 'B', state) == 0 && result == 1 && bytes[0] == 'B', "wcrtomb_s did not convert");
    Require(wcrtomb_s_nid_postfix(&result, nullptr, 0, 'B', state) == 0 && result == 1, "wcrtomb_s did not measure the reset sequence");
    Require(wcrtomb_s_nid_postfix(&result, bytes, sizeof(bytes), 0x263a, state) == 0 && result == 3 && std::memcmp(bytes, "\xE2\x98\xBA", 3) == 0,
        "wcrtomb_s did not convert U+263A to UTF-8");
    std::memset(bytes, 'x', sizeof(bytes));
    before = constraintCalls;
    Require(wcrtomb_s_nid_postfix(&result, bytes, 2, 0x263a, state) == 34 && result == failed && bytes[0] == 0 && std::memcmp(bytes + 1, "xxx", 3) == 0,
        "wcrtomb_s wrote past a two-byte buffer");
    RequireViolation(before, "wcrtomb_s: buffer too small", 34);
    before = constraintCalls;
    bytes[0] = 'x';
    Require(wcrtomb_s_nid_postfix(&result, bytes, sizeof(bytes), 'B', nullptr) == 22 && result == failed && bytes[0] == 0, "wcrtomb_s accepted a null state");
    RequireViolation(before, "wcrtomb_s: bad argument", 22);
    before = constraintCalls;
    bytes[0] = 'x';
    Require(wcrtomb_s_nid_postfix(&result, bytes, 0, 'B', state) == 22 && result == failed && bytes[0] == 'x', "wcrtomb_s accepted a zero-size buffer");
    RequireViolation(before, "wcrtomb_s: bad argument", 22);
}

void CheckMultibyteToWide() {
    std::uint16_t wide[8];
    std::size_t result = 0;
    Require(mbstowcs_s_nid_postfix(&result, nullptr, 0, "hello", 0) == 0 && result == 5, "mbstowcs_s did not count the characters");
    Require(mbstowcs_s_nid_postfix(&result, wide, 8, "hello", 8) == 0 && result == 5 && wide[0] == 'h' && wide[4] == 'o' && wide[5] == 0,
        "mbstowcs_s did not convert the string");
    Require(mbstowcs_s_nid_postfix(&result, wide, 8, "hello", 3) == 0 && result == 3 && wide[2] == 'l' && wide[3] == 0,
        "mbstowcs_s did not stop at len");
    wide[0] = 'x';
    Require(mbstowcs_s_nid_postfix(&result, wide, 8, "hello", 0) == 0 && result == 0 && wide[0] == 0, "mbstowcs_s did not handle a zero len");
    int before = constraintCalls;
    wide[0] = 'x';
    Require(mbstowcs_s_nid_postfix(&result, wide, 5, "hello", 8) == 34 && result == failed && wide[0] == 0, "mbstowcs_s overflowed the buffer");
    RequireViolation(before, "mbstowcs_s: buffer overflow", 34);
    before = constraintCalls;
    wide[0] = 'x';
    Require(mbstowcs_s_nid_postfix(&result, wide, 8, nullptr, 8) == 22 && result == failed && wide[0] == 0, "mbstowcs_s accepted a null source");
    RequireViolation(before, "mbstowcs_s: bad argument", 22);

    unsigned char state[128]{};
    const char* source = "world";
    Require(mbsrtowcs_s_nid_postfix(&result, nullptr, 0, &source, 0, state) == 0 && result == 5 && std::strcmp(source, "world") == 0,
        "mbsrtowcs_s did not count the characters");
    Require(mbsrtowcs_s_nid_postfix(&result, wide, 8, &source, 8, state) == 0 && result == 5 && wide[4] == 'd' && wide[5] == 0 && source == nullptr,
        "mbsrtowcs_s did not convert the string");
    const char* text = "world";
    source = text;
    Require(mbsrtowcs_s_nid_postfix(&result, wide, 8, &source, 2, state) == 0 && result == 2 && wide[2] == 0 && source == text + 2,
        "mbsrtowcs_s did not stop at len");
    before = constraintCalls;
    source = text;
    Require(mbsrtowcs_s_nid_postfix(&result, wide, 3, &source, 8, state) == 34 && result == failed && wide[0] == 0 && source == text + 3,
        "mbsrtowcs_s overflowed the buffer");
    RequireViolation(before, "mbsrtowcs_s: buffer too small", 34);
    before = constraintCalls;
    source = nullptr;
    Require(mbsrtowcs_s_nid_postfix(&result, wide, 8, &source, 8, state) == 22 && result == failed, "mbsrtowcs_s accepted a null source string");
    RequireViolation(before, "mbsrtowcs_s: bad argument", 22);
    before = constraintCalls;
    source = text;
    Require(mbsrtowcs_s_nid_postfix(&result, wide, tooLarge, &source, 8, state) == 22 && result == failed, "mbsrtowcs_s accepted an oversized buffer");
    RequireViolation(before, "mbsrtowcs_s: bad argument", 22);
}

void CheckGets(const char* path) {
    {
        std::ofstream input(path, std::ios::binary | std::ios::trunc);
        input << "short\nexactly8\nrest";
        Require(input.good(), "could not write the gets_s input");
    }
    FileStream* const saved = __stdinp_nid_postfix;
    FileStream* const stream = fopen_nid_postfix(path, "r");
    Require(stream != nullptr, "could not open the gets_s input");
    __stdinp_nid_postfix = stream;
    char buffer[8];
    Require(gets_s_nid_postfix(buffer, sizeof(buffer)) == buffer && std::strcmp(buffer, "short") == 0, "gets_s did not read a line");
    int before = constraintCalls;
    Require(gets_s_nid_postfix(buffer, sizeof(buffer)) == nullptr && buffer[0] == 0, "gets_s accepted a line longer than the buffer");
    RequireViolation(before, "gets_s: short buffer", 34);
    Require(gets_s_nid_postfix(buffer, sizeof(buffer)) == buffer && std::strcmp(buffer, "rest") == 0, "gets_s did not discard the rest of a long line");
    buffer[0] = 'x';
    Require(gets_s_nid_postfix(buffer, sizeof(buffer)) == nullptr && buffer[0] == 0, "gets_s did not report end of file");
    before = constraintCalls;
    Require(gets_s_nid_postfix(buffer, 0) == nullptr, "gets_s accepted a zero size");
    RequireViolation(before, "gets_s: zero buffer size", 22);
    before = constraintCalls;
    Require(gets_s_nid_postfix(nullptr, sizeof(buffer)) == nullptr, "gets_s accepted a null buffer");
    RequireViolation(before, "gets_s: bad argument", 22);
    __stdinp_nid_postfix = saved;
    fclose_nid_postfix(stream);

    std::remove(path);
}

}  // namespace

int main(int argc, char** argv) {
    Require(argc > 1, "usage: guest_bounds_checking_tests <scratch file>");
    set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(RecordConstraint));

    CheckBsearch();
    CheckWcstok();
    CheckWideToMultibyte();
    CheckMultibyteToWide();
    CheckGets(argv[1]);
    return 0;
}
