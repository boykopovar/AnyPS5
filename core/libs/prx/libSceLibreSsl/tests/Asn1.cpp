#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

static_assert(sizeof(Asn1String) == 24);
static_assert(offsetof(Asn1String, data) == 8);
static_assert(offsetof(Asn1String, flags) == 16);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

template<typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

void CheckConversion(int type, std::span<const unsigned char> input, std::span<const unsigned char> expected, int length = -2) {
    std::vector<unsigned char> storage(input.begin(), input.end());
    storage.push_back(0);
    Asn1String value{length == -2 ? static_cast<int>(input.size()) : length, type, storage.data(), 0x123456789abcdef};
    const Asn1String original = value;
    unsigned char sentinel = 0xab;
    unsigned char* result = &sentinel;
    Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &value) == static_cast<int>(expected.size()), "UTF-8 byte length");
    Require(result && result != &sentinel && result != storage.data(), "conversion owns a new buffer");
    Require(expected.empty() || std::memcmp(result, expected.data(), expected.size()) == 0, "converted UTF-8 bytes");
    Require(result[expected.size()] == 0, "conversion terminator");
    Require(value.length == original.length && value.type == original.type && value.data == original.data
        && value.flags == original.flags, "conversion preserves the input object");
    Require(std::equal(input.begin(), input.end(), storage.begin()), "conversion preserves the input bytes");
    free_nid_postfix(result);
}

constexpr std::uint64_t Asn1Error(int reason) {
    return (std::uint64_t{13} << 24) | (std::uint64_t{0xfff} << 12) | static_cast<std::uint64_t>(reason);
}

void RequireConversionError(unsigned char** output, int type, const unsigned char* data, int length, int reason, const char* message) {
    ERR_clear_error_nid_postfix();
    Asn1String value{length, type, const_cast<unsigned char*>(data), 0};
    const unsigned char* before = output ? *output : nullptr;
    Require(ASN1_STRING_to_UTF8_nid_postfix(output, &value) == -1, message);
    Require(!output || *output == before, "a failed conversion leaves the output alone");
    Require(ERR_get_error_nid_postfix() == Asn1Error(reason) && ERR_get_error_nid_postfix() == 0, message);
}

void MalformedStrings() {
    unsigned char sentinel = 0xab;
    unsigned char* result = &sentinel;
    constexpr unsigned char oddBmp[] = {0, 0x41, 0};
    RequireConversionError(&result, 30, oddBmp, 3, 129, "odd BMPString length");
    RequireConversionError(&result, 30, nullptr, 1, 129, "BMPString length is checked before the data");
    constexpr unsigned char bmpSurrogate[] = {0, 0x41, 0xd8, 0};
    RequireConversionError(&result, 30, bmpSurrogate, 4, 124, "BMPString surrogate");
    constexpr unsigned char shortUniversal[] = {0, 0, 0, 0x41, 0};
    RequireConversionError(&result, 28, shortUniversal, 5, 133, "UniversalString length not a multiple of four");
    RequireConversionError(&result, 28, nullptr, 2, 133, "UniversalString length is checked before the data");
    constexpr unsigned char universalBeyond[] = {0, 0x11, 0, 0};
    RequireConversionError(&result, 28, universalBeyond, 4, 124, "UniversalString above U+10FFFF");
    constexpr unsigned char universalSurrogate[] = {0, 0, 0xdc, 0};
    RequireConversionError(&result, 28, universalSurrogate, 4, 124, "UniversalString surrogate");
    constexpr unsigned char overlong[] = {0xc0, 0x80};
    constexpr unsigned char truncated[] = {'A', 0xe2, 0x82};
    constexpr unsigned char surrogate[] = {0xed, 0xa0, 0x80};
    constexpr unsigned char beyond[] = {0xf5, 0x80, 0x80, 0x80};
    constexpr unsigned char continuation[] = {0x80};
    for (const auto& [data, length] : {std::pair{overlong, 2}, std::pair{truncated, 3}, std::pair{surrogate, 3},
             std::pair{beyond, 4}, std::pair{continuation, 1}})
        RequireConversionError(&result, 12, data, length, 134, "malformed UTF8String");
    RequireConversionError(nullptr, 30, oddBmp, 3, 129, "a malformed string fails before the output is used");
}

void Accessors() {
    unsigned char data[] = {'a', 0, 'b'};
    Asn1String value{3, 12, data, std::numeric_limits<std::int64_t>::max()};
    Require(ASN1_STRING_data_nid_postfix(&value) == data, "data accessor preserves pointer identity");
    ASN1_STRING_data_nid_postfix(&value)[2] = 'c';
    Require(data[2] == 'c', "data accessor permits writes");
    Require(ASN1_STRING_length_nid_postfix(&value) == 3, "length includes embedded NUL");
    Require(ASN1_STRING_type_nid_postfix(&value) == 12, "type accessor");
    value = {-9, 0x7fffffff, nullptr, -1};
    Require(ASN1_STRING_data_nid_postfix(&value) == nullptr, "null data is preserved");
    Require(ASN1_STRING_length_nid_postfix(&value) == -9, "length is returned without normalization");
    Require(ASN1_STRING_type_nid_postfix(&value) == 0x7fffffff, "type is returned without normalization");
    RequireInvalidArgument([] { ASN1_STRING_data_nid_postfix(nullptr); }, "null data accessor input");
    RequireInvalidArgument([] { ASN1_STRING_length_nid_postfix(nullptr); }, "null length accessor input");
    RequireInvalidArgument([] { ASN1_STRING_type_nid_postfix(nullptr); }, "null type accessor input");
}

void Integers() {
    Require(ASN1_INTEGER_get_nid_postfix(nullptr) == 0, "null integer");
    unsigned char data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0};
    constexpr std::array<std::int64_t, 9> expected{
        0, 0x01, 0x0123, 0x012345, 0x01234567, 0x0123456789, 0x0123456789ab,
        0x0123456789abcd, 0x0123456789abcdef};
    for (int length = 0; length <= 8; ++length) {
        Asn1String value{length, 2, data, -1};
        Require(ASN1_INTEGER_get_nid_postfix(&value) == expected[length], "positive integer and 64-bit guest long");
        value.type = 0x102;
        Require(ASN1_INTEGER_get_nid_postfix(&value) == -expected[length], "negative integer magnitude");
    }
    Asn1String value{9, 2, nullptr, 0};
    Require(ASN1_INTEGER_get_nid_postfix(&value) == -1, "overlength is rejected before null data");
    value.length = 8;
    Require(ASN1_INTEGER_get_nid_postfix(&value) == 0, "integer with null data");
    for (int length : {std::numeric_limits<std::int32_t>::min(), -8, -1, 0}) {
        for (int type : {2, 0x102}) {
            for (auto* bytes : {data, static_cast<unsigned char*>(nullptr), reinterpret_cast<unsigned char*>(1)}) {
                value = {length, type, bytes, -1};
                const Asn1String original = value;
                Require(ASN1_INTEGER_get_nid_postfix(&value) == 0, "LibreSSL 2.5.4 nonpositive integer length");
                Require(value.length == original.length && value.type == original.type && value.data == original.data
                    && value.flags == original.flags, "nonpositive integer length preserves input");
            }
        }
    }
    value = {-1, 2, data, 0};
    for (int type : {-1, 0, 1, 3, 12, 0x100, 0x202, 0x302}) {
        value.type = type;
        Require(ASN1_INTEGER_get_nid_postfix(&value) == -1, "only integer types are accepted");
    }
    std::array<unsigned char, 8> maximum{0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    value = {8, 2, maximum.data(), 0};
    Require(ASN1_INTEGER_get_nid_postfix(&value) == std::numeric_limits<std::int64_t>::max(), "maximum signed integer");
    value.type = 0x102;
    Require(ASN1_INTEGER_get_nid_postfix(&value) == -std::numeric_limits<std::int64_t>::max(), "negative maximum magnitude");
    std::array<unsigned char, 8> minimum{0x80, 0, 0, 0, 0, 0, 0, 0};
    value = {8, 0x102, minimum.data(), -1};
    const Asn1String original = value;
    Require(ASN1_INTEGER_get_nid_postfix(&value) == std::numeric_limits<std::int64_t>::min(), "minimum signed integer");
    Require(value.length == original.length && value.type == original.type && value.data == original.data
        && value.flags == original.flags && minimum == std::array<unsigned char, 8>{0x80, 0, 0, 0, 0, 0, 0, 0}, "minimum integer preserves input");
    value.type = 2;
    RequireInvalidArgument([&] { ASN1_INTEGER_get_nid_postfix(&value); }, "positive magnitude above signed maximum");
    value.type = 0x102;
    minimum.back() = 1;
    RequireInvalidArgument([&] { ASN1_INTEGER_get_nid_postfix(&value); }, "negative magnitude below signed minimum");
    value.data = maximum.data();
    maximum[0] = 0x80;
    for (int type : {2, 0x102}) {
        value.type = type;
        RequireInvalidArgument([&] { ASN1_INTEGER_get_nid_postfix(&value); }, "source signed-overflow path throws");
    }
    std::array<unsigned char, 9> zero{};
    value = {9, 2, zero.data(), 0};
    Require(ASN1_INTEGER_get_nid_postfix(&value) == -1, "overlength leading zeros are not trimmed");
}

std::string Printed(const Asn1String& value, int expected = 1) {
    LibreSsl::Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio != nullptr, "a memory BIO");
    Require(ASN1_STRING_print_nid_postfix(bio, &value) == expected, "ASN1_STRING_print result");
    char* data = nullptr;
    const auto length = BIO_ctrl_nid_postfix(bio, 3, 0, &data);
    std::string text(data, static_cast<std::size_t>(length));
    BIO_free_nid_postfix(bio);
    return text;
}

void Print() {
    unsigned char mixed[] = {'h', 'i', '\n', '\r', '\t', 0, 0x7e, 0x7f, 0x80, 0xff, ' '};
    Asn1String value{sizeof(mixed), 4, mixed, 0};
    Require(Printed(value) == std::string("hi\n\r..~... "), "only printable ASCII, LF and CR are kept");
    std::string letters;
    for (int index = 0; index < 200; ++index) letters += static_cast<char>('a' + index % 26);
    value = {200, 22, reinterpret_cast<unsigned char*>(letters.data()), 0};
    Require(Printed(value) == letters, "output longer than the 80-byte chunk is complete");
    value = {0, 4, nullptr, 0};
    Require(Printed(value).empty(), "an empty string prints nothing");
    value.length = -1;
    Require(Printed(value).empty(), "a negative length prints nothing");
    Require(ASN1_STRING_print_nid_postfix(nullptr, nullptr) == 0, "a null string prints nothing");
    value = {2, 4, mixed, 0};
    Require(ASN1_STRING_print_nid_postfix(nullptr, &value) == 0, "a failed write returns 0");
    LibreSsl::Bio* readOnly = BIO_new_mem_buf_nid_postfix("x", 1);
    Require(readOnly && ASN1_STRING_print_nid_postfix(readOnly, &value) == 0, "a read-only BIO returns 0");
    BIO_free_nid_postfix(readOnly);
    ERR_clear_error_nid_postfix();
    value.data = nullptr;
    RequireInvalidArgument([&] { ASN1_STRING_print_nid_postfix(nullptr, &value); }, "null data with a positive length");
}

void Strings() {
    constexpr unsigned char latin[] = {0, 0x41, 0x7f, 0x80, 0xa3, 0xff};
    constexpr unsigned char latinUtf8[] = {0, 0x41, 0x7f, 0xc2, 0x80, 0xc2, 0xa3, 0xc3, 0xbf};
    constexpr unsigned char bmp[] = {0, 0, 0, 0x7f, 0, 0x80, 7, 0xff, 8, 0, 0x20, 0xac, 0xd7, 0xff, 0xe0, 0, 0xff, 0xff};
    constexpr unsigned char bmpUtf8[] = {0, 0x7f, 0xc2, 0x80, 0xdf, 0xbf, 0xe0, 0xa0, 0x80, 0xe2, 0x82, 0xac,
        0xed, 0x9f, 0xbf, 0xee, 0x80, 0x80, 0xef, 0xbf, 0xbf};
    constexpr unsigned char universal[] = {0, 0, 0, 0x41, 0, 1, 0, 0, 0, 1, 0xf6, 0, 0, 0x10, 0xff, 0xff};
    constexpr unsigned char universalUtf8[] = {0x41, 0xf0, 0x90, 0x80, 0x80, 0xf0, 0x9f, 0x98, 0x80, 0xf4, 0x8f, 0xbf, 0xbf};
    for (int type : {19, 20, 22, 23, 24, 26}) CheckConversion(type, latin, latinUtf8);
    CheckConversion(30, bmp, bmpUtf8);
    CheckConversion(28, universal, universalUtf8);
    CheckConversion(12, bmpUtf8, bmpUtf8);
    CheckConversion(12, universalUtf8, universalUtf8);
    for (int type : {12, 19, 20, 22, 23, 24, 26, 28, 30}) {
        CheckConversion(type, {}, {});
        Asn1String empty{0, type, nullptr, 0};
        unsigned char* result = nullptr;
        Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &empty) == 0 && result && *result == 0, "empty null-data string");
        free_nid_postfix(result);
    }
    constexpr unsigned char terminated[] = {'A', 0xc2, 0xa3, 0, 'X'};
    CheckConversion(12, terminated, std::span(terminated).first(3), -1);
    CheckConversion(12, terminated, std::span(terminated).first(3), -7);
    constexpr unsigned char ascii[] = {'A', 'B', 'C'};
    CheckConversion(19, ascii, ascii, -1);

    unsigned char sentinel = 0xab;
    unsigned char* result = &sentinel;
    Require(ASN1_STRING_to_UTF8_nid_postfix(&result, nullptr) == -1 && result == &sentinel, "null conversion input");
    Require(ASN1_STRING_to_UTF8_nid_postfix(nullptr, nullptr) == -1, "null input checked before output");
    for (int type = -1; type <= 32; ++type) {
        if (type == 12 || type == 19 || type == 20 || type == 22 || type == 23 || type == 24 || type == 26 || type == 28 || type == 30) continue;
        Asn1String value{0, type, nullptr, 0};
        Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &value) == -1 && result == &sentinel, "unsupported string tag");
        Require(ASN1_STRING_to_UTF8_nid_postfix(nullptr, &value) == -1, "tag checked before output");
    }
    Asn1String value{0, 12, nullptr, 0};
    RequireThrows([&] { ASN1_STRING_to_UTF8_nid_postfix(nullptr, &value); }, "null conversion output");
    value.length = 1;
    RequireThrows([&] { ASN1_STRING_to_UTF8_nid_postfix(&result, &value); }, "null nonempty string data");
    value.length = -1;
    RequireThrows([&] { ASN1_STRING_to_UTF8_nid_postfix(&result, &value); }, "null terminated-string data");
}

std::string PrintedTime(int type, const char* text, int expected) {
    Asn1String time{static_cast<int>(std::strlen(text)), type, reinterpret_cast<unsigned char*>(const_cast<char*>(text)), 0};
    LibreSsl::Bio* bio = BIO_new_nid_postfix(BIO_s_mem_nid_postfix());
    Require(bio != nullptr, "a memory BIO");
    Require(ASN1_TIME_print_nid_postfix(bio, &time) == expected, text);
    char* data = nullptr;
    const auto length = BIO_ctrl_nid_postfix(bio, 3, 0, &data);
    std::string output(data, static_cast<std::size_t>(length));
    BIO_free_nid_postfix(bio);
    return output;
}

void Times() {
    const std::pair<const char*, const char*> utc[] = {{"170102030405Z", "Jan  2 03:04:05 2017 GMT"}, {"4912312359Z", "Dec 31 23:59:00 2049 GMT"},
        {"500101000000", "Jan  1 00:00:00 1950"}, {"991231235959+0100", "Dec 31 23:59:59 1999"}, {"17010203040aZ", "Jan  2 03:04:00 2017 GMT"}};
    for (const auto& [input, output] : utc) Require(PrintedTime(23, input, 1) == output, input);
    const std::pair<const char*, const char*> generalized[] = {{"20170102030405Z", "Jan  2 03:04:05 2017 GMT"},
        {"20170102030405.123Z", "Jan  2 03:04:05.123 2017 GMT"}, {"20170102030405.Z", "Jan  2 03:04:05. 2017 GMT"},
        {"20170102030405.12a", "Jan  2 03:04:05.12 2017"}, {"201701020304", "Jan  2 03:04:00 2017"}, {"19991231235959.5", "Dec 31 23:59:59.5 1999"}};
    for (const auto& [input, output] : generalized) Require(PrintedTime(24, input, 1) == output, input);
    const std::pair<int, const char*> bad[] = {{23, "1713"}, {23, "171302030405Z"}, {23, "170002030405Z"}, {23, "17x102030405Z"},
        {24, "20170102030"}, {24, "20171302030405Z"}, {4, "170102030405Z"}, {22, "x"}};
    for (const auto& [type, input] : bad) Require(PrintedTime(type, input, 0) == "Bad time value", input);
    Asn1String time{13, 23, reinterpret_cast<unsigned char*>(const_cast<char*>("170102030405Z")), 0};
    Require(ASN1_TIME_print_nid_postfix(nullptr, &time) == 1, "a valid time returns 1 even when the write fails");
    time.type = 4;
    Require(ASN1_TIME_print_nid_postfix(nullptr, &time) == 0, "an invalid time returns 0 without a BIO");
    Asn1String empty{0, 23, nullptr, 0};
    Require(ASN1_TIME_print_nid_postfix(nullptr, &empty) == 0, "an empty time is bad");
    RequireInvalidArgument([] { ASN1_TIME_print_nid_postfix(nullptr, nullptr); }, "a null time");
    time.data = nullptr;
    RequireInvalidArgument([&] { ASN1_TIME_print_nid_postfix(nullptr, &time); }, "null time data with a positive length");
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, bytes * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr, "guarded page allocation");
        DWORD previous{};
        Require(VirtualProtect(data + bytes, bytes, PAGE_NOACCESS, &previous) != 0, "guard page protection");
#else
        bytes = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, bytes * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "guarded page mapping");
        data = static_cast<unsigned char*>(mapped);
        Require(mprotect(data + bytes, bytes, PROT_NONE) == 0, "guard page protection");
#endif
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes * 2);
#endif
    }

    template <typename T>
    T* Tail(const void* value, std::size_t size) {
        std::memcpy(data + bytes - size, value, size);
        return reinterpret_cast<T*>(data + bytes - size);
    }

    void ReadOnly() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0, "read-only page protection");
#else
        Require(mprotect(data, bytes, PROT_READ) == 0, "read-only page protection");
#endif
    }

private:
    unsigned char* data = nullptr;
    std::size_t bytes = 0;
};

template<typename TAction>
void Rejects(TAction action, const char* message) {
    errno = 71;
    bool rejected = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        rejected = errno == 71;
    }
    Require(rejected, message);
}

void GuestPointers() {
    auto* badString = reinterpret_cast<Asn1String*>(0x10);
    auto* badObject = reinterpret_cast<const LibreSslAsn1Object*>(0x10);
    auto* badData = reinterpret_cast<unsigned char*>(0x10);
    unsigned char* result = nullptr;
    Rejects([&] { ASN1_TIME_print_nid_postfix(nullptr, badString); }, "unmapped time");
    Rejects([&] { ASN1_INTEGER_get_nid_postfix(badString); }, "unmapped integer");
    Rejects([&] { ASN1_STRING_data_nid_postfix(badString); }, "unmapped string data accessor");
    Rejects([&] { ASN1_STRING_length_nid_postfix(badString); }, "unmapped string length accessor");
    Rejects([&] { ASN1_STRING_type_nid_postfix(badString); }, "unmapped string type accessor");
    Rejects([&] { ASN1_STRING_print_nid_postfix(nullptr, badString); }, "unmapped printed string");
    Rejects([&] { ASN1_STRING_to_UTF8_nid_postfix(&result, badString); }, "unmapped converted string");
    Rejects([&] { i2a_ASN1_OBJECT_nid_postfix(nullptr, badObject); }, "unmapped object");
    Asn1String time{13, 23, badData, 0};
    Rejects([&] { ASN1_TIME_print_nid_postfix(nullptr, &time); }, "unmapped time data");
    Asn1String integer{2, 2, badData, 0};
    Rejects([&] { ASN1_INTEGER_get_nid_postfix(&integer); }, "unmapped integer data");
    Asn1String printed{3, 4, badData, 0};
    Rejects([&] { ASN1_STRING_print_nid_postfix(nullptr, &printed); }, "unmapped printed data");
    Asn1String converted{3, 12, badData, 0};
    Rejects([&] { ASN1_STRING_to_UTF8_nid_postfix(&result, &converted); }, "unmapped converted data");
    converted.length = -1;
    Rejects([&] { ASN1_STRING_to_UTF8_nid_postfix(&result, &converted); }, "unmapped terminated data");
    Require(result == nullptr, "rejected conversions leave the output alone");

    {
        GuardedPage page;
        unsigned char latin[] = {'A', 0xff};
        Asn1String value{2, 20, page.Tail<unsigned char>(latin, sizeof(latin)), 0x55};
        Asn1String* stored = page.Tail<Asn1String>(&value, sizeof(value));
        Require(ASN1_STRING_length_nid_postfix(stored) == 2 && ASN1_STRING_type_nid_postfix(stored) == 20 &&
            ASN1_STRING_data_nid_postfix(stored) == value.data, "accessors read a string ending at a page boundary");
        Asn1String terminated{-1, 20, page.Tail<unsigned char>(latin, sizeof(latin)), 0};
        Rejects([&] { ASN1_STRING_to_UTF8_nid_postfix(&result, &terminated); }, "unterminated data running into an unmapped page");
        Asn1String longer{3, 20, terminated.data, 0};
        Rejects([&] { ASN1_STRING_to_UTF8_nid_postfix(&result, &longer); }, "data length running into an unmapped page");
        Rejects([&] { ASN1_STRING_print_nid_postfix(nullptr, &longer); }, "printed length running into an unmapped page");
        const unsigned char ending[] = {'A', 0xff, 0};
        terminated.data = page.Tail<unsigned char>(ending, sizeof(ending));
        Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &terminated) == 3 && result && std::memcmp(result, "A\xc3\xbf", 4) == 0,
            "terminated data ending at a page boundary converts");
        free_nid_postfix(result);
        result = nullptr;
    }

    {
        GuardedPage page;
        unsigned char* untouched = nullptr;
        unsigned char** slot = page.Tail<unsigned char*>(&untouched, sizeof(untouched));
        page.ReadOnly();
        unsigned char text[] = {'A'};
        Asn1String value{1, 12, text, 0};
        Rejects([&] { ASN1_STRING_to_UTF8_nid_postfix(slot, &value); }, "read-only output");
        Rejects([&] { ASN1_STRING_to_UTF8_nid_postfix(reinterpret_cast<unsigned char**>(0x10), &value); }, "unmapped output");
        Require(*slot == nullptr, "rejected output stays untouched");
    }
}

std::size_t lastAllocationSize = 0;
int allocationCount = 0;
bool failAllocation = false;
int freeCount = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    lastAllocationSize = size;
    ++allocationCount;
    return failAllocation ? nullptr : std::malloc(size);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    ++freeCount;
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected calloc"); }
void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Allocator() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    constexpr unsigned char input[] = {0xff};
    constexpr unsigned char expected[] = {0xc3, 0xbf};
    CheckConversion(20, input, expected);
    Require(allocationCount == 1 && lastAllocationSize == 3 && freeCount == 1, "conversion uses the application allocator");
    constexpr unsigned char universal[] = {0, 0, 0, 'A', 0, 0, 0x03, 0xa9, 0, 0, 0x20, 0xac, 0, 0x01, 0xf6, 0x00};
    constexpr unsigned char mixed[] = {'A', 0xce, 0xa9, 0xe2, 0x82, 0xac, 0xf0, 0x9f, 0x98, 0x80};
    CheckConversion(28, universal, mixed);
    Require(allocationCount == 2 && lastAllocationSize == sizeof(mixed) + 1 && freeCount == 2,
        "a conversion with every UTF-8 length makes one allocation of the exact size");
    unsigned char badUtf8[] = {'A', 0xc0, 0x80};
    Asn1String malformed{3, 12, badUtf8, 0};
    unsigned char* untouched = nullptr;
    ERR_clear_error_nid_postfix();
    Require(ASN1_STRING_to_UTF8_nid_postfix(&untouched, &malformed) == -1 && untouched == nullptr && allocationCount == 2,
        "a malformed string allocates nothing");
    ERR_clear_error_nid_postfix();
    failAllocation = true;
    unsigned char sentinel = 0xab;
    unsigned char* result = &sentinel;
    unsigned char latin[] = {0xff};
    Asn1String converted{1, 20, latin, 0};
    ERR_clear_error_nid_postfix();
    Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &converted) == -1 && result == &sentinel, "allocation failure of a converted string");
    Require(ERR_get_error_nid_postfix() == Asn1Error(65) && ERR_get_error_nid_postfix() == 0, "one malloc failure for a converted string");
    unsigned char utf8[] = {'A'};
    Asn1String copied{1, 12, utf8, 0};
    Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &copied) == -1 && result == &sentinel, "allocation failure of a UTF8String copy");
    Require(ERR_get_error_nid_postfix() == Asn1Error(65) && ERR_get_error_nid_postfix() == Asn1Error(65) && ERR_get_error_nid_postfix() == 0,
        "ASN1_STRING_set and ASN1_mbstring_ncopy both report the malloc failure of a UTF8String copy");
    failAllocation = false;
}

void FileAndSocketBio() {
    const auto tempRoot = std::filesystem::temp_directory_path() /
        ("anyps5-libressl-bio-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(tempRoot), "create temp directory for BIO tests");
    AddPathAlias_nid_no_patch("/bio-test", tempRoot.string().c_str());

    const char* borrowedPath = "/bio-test/borrowed.txt";
    FileStream* borrowedStream = fopen_nid_postfix(borrowedPath, "w+");
    Require(borrowedStream != nullptr, "open borrowed guest FileStream");
    LibreSsl::Bio* borrowedBio = BIO_new_fp_nid_postfix(borrowedStream, 0x10);
    Require(borrowedBio != nullptr && borrowedBio->shutdown == 0 && borrowedBio->initialized == 1 && borrowedBio->data == borrowedStream,
        "BIO_new_fp with 0x10 masks BioClose to 0");
    Require(BIO_ctrl_nid_postfix(borrowedBio, 8, 0, nullptr) == 0, "BIO_CTRL_GET_CLOSE returns 0 for 0x10");
    Require(BIO_write_nid_postfix(borrowedBio, "hi", 2) == 2, "write through borrowed file BIO");
    Require(BIO_free_nid_postfix(borrowedBio) == 1, "free borrowed file BIO");
    Require(fwrite_nid_postfix("!", 1, 1, borrowedStream) == 1, "borrowed FileStream remains writable after BIO_free");
    Require(fflush_nid_postfix(borrowedStream) == 0, "flush borrowed FileStream");
    rewind_nid_postfix(borrowedStream);
    char content[4]{};
    Require(fread_nid_postfix(content, 1, 3, borrowedStream) == 3 && std::memcmp(content, "hi!", 3) == 0,
        "borrowed FileStream remains readable after BIO_free");
    Require(fclose_nid_postfix(borrowedStream) == 0, "close borrowed FileStream");

    const char* ownedPath = "/bio-test/owned.txt";
    FileStream* ownedStream = fopen_nid_postfix(ownedPath, "w+");
    Require(ownedStream != nullptr, "open owned guest FileStream");
    LibreSsl::Bio* ownedBio = BIO_new_fp_nid_postfix(ownedStream, 0x11);
    Require(ownedBio != nullptr && ownedBio->shutdown == 1 && BIO_ctrl_nid_postfix(ownedBio, 8, 0, nullptr) == 1,
        "BIO_new_fp with 0x11 masks BioClose to 1");
    Require(BIO_write_nid_postfix(ownedBio, "ok", 2) == 2, "write through owned file BIO");
    Require(BIO_free_nid_postfix(ownedBio) == 1, "free owned file BIO closes stream");
    FileStream* verifyStream = fopen_nid_postfix(ownedPath, "r");
    Require(verifyStream != nullptr, "reopen file closed by BIO_free");
    char verified[4]{};
    Require(fread_nid_postfix(verified, 1, 2, verifyStream) == 2 && std::memcmp(verified, "ok", 2) == 0,
        "owned FileStream flushed and closed on BIO_free");
    Require(fclose_nid_postfix(verifyStream) == 0, "close verification FileStream");

    LibreSsl::Bio* fileBio = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    Require(fileBio != nullptr && fileBio->initialized == 0, "BIO_new(BIO_s_file()) starts uninitialized");
    char scratch[4]{};
    Require(fileBio->method->read(fileBio, scratch, sizeof(scratch)) == 0, "FileRead on uninitialized BIO returns 0");
    Require(fileBio->method->write(fileBio, "abc", 3) == 0, "FileWrite on uninitialized BIO returns 0");
    Require(fileBio->method->read(fileBio, scratch, -1) == -1, "FileRead keeps negative size returning -1");
    Require(fileBio->method->write(fileBio, "abc", -1) == -1, "FileWrite keeps negative size returning -1");
    fileBio->initialized = 1;
    Require(fileBio->method->read(fileBio, scratch, sizeof(scratch)) == 0, "FileRead with null FileStream returns 0");
    Require(BIO_write_nid_postfix(fileBio, "abc", 3) == 0, "BIO_write with null FileStream returns 0");
    fileBio->initialized = 0;
    Require(BIO_ctrl_nid_postfix(fileBio, 9, 0x12345678, nullptr) == 1, "BIO_CTRL_SET_CLOSE stores int on file BIO");
    Require(fileBio->shutdown == 0x12345678 && BIO_ctrl_nid_postfix(fileBio, 8, 0, nullptr) == 0x12345678,
        "BIO_CTRL_GET_CLOSE round-trips positive int on file BIO");
    Require(BIO_ctrl_nid_postfix(fileBio, 9, -5, nullptr) == 1 && fileBio->shutdown == -5 &&
        BIO_ctrl_nid_postfix(fileBio, 8, 0, nullptr) == -5, "BIO_CTRL_GET_CLOSE round-trips negative int on file BIO");
    Require(BIO_ctrl_nid_postfix(fileBio, 9, 0, nullptr) == 1 && BIO_ctrl_nid_postfix(fileBio, 8, 0, nullptr) == 0,
        "BIO_CTRL_SET_CLOSE resets file BIO shutdown to 0");
    Require(BIO_free_nid_postfix(fileBio) == 1, "free file BIO");

    LibreSsl::Bio* socketBio = BIO_new_socket_nid_postfix(17, 0x2a);
    Require(socketBio != nullptr && socketBio->shutdown == 0x2a && BIO_ctrl_nid_postfix(socketBio, 8, 0, nullptr) == 0x2a,
        "BIO_new_socket stores arbitrary int closeFlag");
    Require(BIO_ctrl_nid_postfix(socketBio, 9, 0x1357, nullptr) == 1 && socketBio->shutdown == 0x1357 &&
        BIO_ctrl_nid_postfix(socketBio, 8, 0, nullptr) == 0x1357, "BIO_ctrl round-trips positive int on socket BIO");
    Require(BIO_ctrl_nid_postfix(socketBio, 9, -3, nullptr) == 1 && socketBio->shutdown == -3 &&
        BIO_ctrl_nid_postfix(socketBio, 8, 0, nullptr) == -3, "BIO_ctrl round-trips negative int on socket BIO");
    Require(BIO_ctrl_nid_postfix(socketBio, 9, 0, nullptr) == 1 && BIO_ctrl_nid_postfix(socketBio, 8, 0, nullptr) == 0,
        "reset socket BIO close flag before free");
    Require(BIO_free_nid_postfix(socketBio) == 1, "free socket BIO");

    const char* writeOnlyPath = "/bio-test/writeonly.txt";
    FileStream* writeOnlyStream = fopen_nid_postfix(writeOnlyPath, "w");
    Require(writeOnlyStream != nullptr, "open write-only FileStream");
    LibreSsl::Bio* writeOnlyBio = BIO_new_fp_nid_postfix(writeOnlyStream, 0x11);
    Require(writeOnlyBio != nullptr, "wrap write-only FileStream in BIO");
    Require(BIO_write_nid_postfix(writeOnlyBio, nullptr, 4) == 0, "BIO_write with null buffer returns 0");
    Require(writeOnlyBio->method->read(writeOnlyBio, nullptr, 4) == 0, "FileRead with null buffer returns 0");
    Require(writeOnlyBio->method->write(writeOnlyBio, nullptr, 4) == 0, "FileWrite with null buffer returns 0");
    Require(writeOnlyBio->method->read(writeOnlyBio, scratch, -1) == -1, "initialized FileRead with negative size returns -1");
    Require(writeOnlyBio->method->write(writeOnlyBio, "abc", -1) == -1, "initialized FileWrite with negative size returns -1");
    ERR_clear_error_nid_postfix();
    errno = 0;
    char byte = 0;
    Require(writeOnlyBio->method->read(writeOnlyBio, &byte, 1) == -1, "FileRead on write-only FileStream returns -1");
    const int readErrno = errno;
    Require(readErrno != 0, "FileRead sets non-zero errno on write-only FileStream");
    const std::uint64_t sysError = ERR_get_error_nid_postfix();
    const std::uint64_t bioError = ERR_get_error_nid_postfix();
    Require(sysError == ((2ULL << 24) | (0xfffULL << 12) | (static_cast<std::uint64_t>(readErrno) & 0xfff)),
        "FileRead queues SYS error with function code 0xfff");
    Require(bioError == ((32ULL << 24) | (0xfffULL << 12) | 2),
        "FileRead queues BIO error with function code 0xfff");
    Require(ERR_get_error_nid_postfix() == 0, "FileRead queues only SYS and BIO errors");
    Require(BIO_free_nid_postfix(writeOnlyBio) == 1, "free write-only BIO");

    RemovePathAlias_nid_no_patch("/bio-test");
    std::error_code ignored;
    std::filesystem::remove_all(tempRoot, ignored);
}

} // namespace

int main(int argc, char**) {
    if (argc > 1) {
        Allocator();
        return 0;
    }
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Accessors();
    Print();
    Integers();
    Strings();
    MalformedStrings();
    Times();
    GuestPointers();
    FileAndSocketBio();
}
