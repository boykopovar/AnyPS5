#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/ucnv.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU converter helpers: %s\n", message);
    std::abort();
}

template<class TCall>
void RequiresInvalidArgument(TCall call, const char* message = "null normalization argument must throw") {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

void DefaultConverterLifecycle() {
    u_flushDefaultConverter_nid_postfix();
    u_releaseDefaultConverter_nid_postfix(nullptr);
    RequiresInvalidArgument([] { u_getDefaultConverter_nid_postfix(nullptr); }, "null acquire status must throw");
    std::uint64_t foreign = 0;
    RequiresInvalidArgument([&] { u_releaseDefaultConverter_nid_postfix(reinterpret_cast<UConverter*>(&foreign)); },
                            "release must reject a pointer that acquire did not issue");

    std::int32_t status = 0;
    auto* first = u_getDefaultConverter_nid_postfix(&status);
    Require(first && status <= 0, "acquire returns a live converter");
    u_releaseDefaultConverter_nid_postfix(first);
    RequiresInvalidArgument([&] { u_releaseDefaultConverter_nid_postfix(first); }, "already returned converter must be rejected");

    for (const std::int32_t incoming : {std::int32_t{-129}, INT32_MIN}) {
        status = incoming;
        auto* below = u_getDefaultConverter_nid_postfix(&status);
        Require(below && status == incoming, "a status below the warning range is not a failure");
        u_releaseDefaultConverter_nid_postfix(below);
    }

    for (const std::int32_t incoming : {1, 7, 27, 0x10000}) {
        status = incoming;
        auto* cached = u_getDefaultConverter_nid_postfix(&status);
        Require(cached == first && status == incoming, "an incoming failure still takes the cached converter");
        u_releaseDefaultConverter_nid_postfix(cached);
    }
    status = 0;
    auto* reused = u_getDefaultConverter_nid_postfix(&status);
    Require(reused == first && status <= 0, "get, release, get reuses the cached converter");
    for (const std::int32_t incoming : {1, 7}) {
        status = incoming;
        Require(!u_getDefaultConverter_nid_postfix(&status) && status == incoming,
                "an incoming failure opens no converter while the cache is empty");
    }
    status = 0;
    auto* second = u_getDefaultConverter_nid_postfix(&status);
    Require(second && second != reused && status <= 0, "checked-out converters are distinct");
    u_releaseDefaultConverter_nid_postfix(second);
    u_flushDefaultConverter_nid_postfix();
    u_releaseDefaultConverter_nid_postfix(reused);
    u_flushDefaultConverter_nid_postfix();
    status = 1;
    Require(!u_getDefaultConverter_nid_postfix(&status) && status == 1, "an incoming failure opens no converter after a flush");
}

UConverter* AcquireDefaultConverter() {
    std::int32_t status = U_ZERO_ERROR;
    auto* converter = u_getDefaultConverter_nid_postfix(&status);
    Require(converter && status <= 0, "acquire the configured default converter");
    return converter;
}

void DefaultConverterNames() {
    const char* initialName = ucnv_getDefaultName_nid_postfix();
    Require(initialName && *initialName, "initial default converter name");
    const std::string platformName = initialName;
    auto* initial = AcquireDefaultConverter();
    const auto platformType = ucnv_getType(initial);
    u_releaseDefaultConverter_nid_postfix(initial);

    if constexpr (U_CHARSET_IS_UTF8) {
        ucnv_setDefaultName_nid_postfix("UTF-16LE");
        Require(std::strcmp(ucnv_getDefaultName_nid_postfix(), "UTF-8") == 0, "fixed UTF-8 default");
        auto* fixed = AcquireDefaultConverter();
        Require(ucnv_getType(fixed) == UCNV_UTF8, "fixed UTF-8 converter");
        u_releaseDefaultConverter_nid_postfix(fixed);
        return;
    }

    ucnv_setDefaultName_nid_postfix("UTF-8");
    Require(std::strcmp(ucnv_getDefaultName_nid_postfix(), "UTF-8") == 0, "set UTF-8");
    auto* previous = AcquireDefaultConverter();
    Require(ucnv_getType(previous) == UCNV_UTF8, "acquire UTF-8");

    const bool useLittleEndian = platformType != UCNV_UTF16_LittleEndian;
    const char* canonicalName = useLittleEndian ? "UTF-16LE" : "UTF-16BE";
    const std::string extendedName = std::string(useLittleEndian ? "utf-16le" : "utf-16be") + ",ignored=" + std::string(850, 'x');
    const auto expectedType = useLittleEndian ? UCNV_UTF16_LittleEndian : UCNV_UTF16_BigEndian;
    ucnv_setDefaultName_nid_postfix(extendedName.c_str());
    Require(std::strcmp(ucnv_getDefaultName_nid_postfix(), canonicalName) == 0, "canonicalize the configured name");
    u_releaseDefaultConverter_nid_postfix(previous);
    auto* current = AcquireDefaultConverter();
    Require(ucnv_getType(current) == expectedType, "old checkout cannot repopulate the cache");
    u_releaseDefaultConverter_nid_postfix(current);

    ucnv_setDefaultName_nid_postfix("anyps5-no-such-converter");
    Require(std::strcmp(ucnv_getDefaultName_nid_postfix(), canonicalName) == 0, "unknown converter leaves the default name intact");
    current = AcquireDefaultConverter();
    Require(ucnv_getType(current) == expectedType, "unknown converter leaves the encoding intact");
    u_releaseDefaultConverter_nid_postfix(current);

    ucnv_setDefaultName_nid_postfix(nullptr);
    auto* reset = AcquireDefaultConverter();
    Require(ucnv_getType(reset) == platformType, "reset discards the cached converter");
    Require(platformName == ucnv_getDefaultName_nid_postfix(), "reset restores the platform default name");
    u_releaseDefaultConverter_nid_postfix(reset);
    u_flushDefaultConverter_nid_postfix();
}

char Ebcdic(unsigned char value) {
    if (value >= 'a' && value <= 'i') return static_cast<char>(0x81 + value - 'a');
    if (value >= 'j' && value <= 'r') return static_cast<char>(0x91 + value - 'j');
    if (value >= 's' && value <= 'z') return static_cast<char>(0xa2 + value - 's');
    if (value >= 'A' && value <= 'Z') return static_cast<char>(static_cast<unsigned char>(Ebcdic(value + 32)) + 0x40);
    if (value >= '0' && value <= '9') return static_cast<char>(0xf0 + value - '0');
    return value ? '\x40' : 0;
}

std::string ToEbcdic(std::string_view text) {
    std::string result;
    for (unsigned char value : text) result += Ebcdic(value);
    return result;
}

void CheckStrip(const std::string& source, const std::string& expected, bool ebcdic) {
    const auto call = ebcdic ? ucnv_io_stripEBCDICForCompare_nid_postfix : ucnv_io_stripASCIIForCompare_nid_postfix;
    std::string output(source.size() + 4, '@');
    const auto saved = source;
    Require(call(output.data() + 1, source.c_str()) == output.data() + 1, "strip returns destination");
    Require(std::string_view(output.data() + 1, expected.size()) == expected, "strip bytes match expected normalization");
    Require(output[expected.size() + 1] == 0, "strip terminates output");
    Require(output.front() == '@' && std::all_of(output.begin() + expected.size() + 2, output.end(), [](char value) { return value == '@'; }), "strip preserves both canaries");
    Require(source == saved, "strip leaves separate input unchanged");
    std::string inPlace = source;
    inPlace.append(3, '@');
    inPlace[source.size()] = 0;
    Require(call(inPlace.data(), inPlace.data()) == inPlace.data(), "strip supports identical source and destination");
    Require(std::string_view(inPlace.c_str()) == expected, "in-place strip bytes match");
    Require(inPlace[source.size() + 1] == '@' && inPlace[source.size() + 2] == '@', "in-place strip preserves tail");
}

void NameMatching() {
    struct Case { const char* input; const char* expected; };
    constexpr Case cases[] = {
        {"", ""}, {" -_./@!\t\r\n", ""}, {"UTF-8", "utf8"}, {"u*T@f08", "utf8"},
        {"ibm-001200", "ibm1200"}, {"0", "0"}, {"000", "0"}, {"00090", "90"},
        {"10002", "10002"}, {"10-02", "102"}, {"00-01", "01"}, {"0a01", "0a1"},
        {"A0_01Z", "a01z"}, {"AbCdEfGhIjKlMnOpQrStUvWxYz", "abcdefghijklmnopqrstuvwxyz"},
        {"09.090.0.001", "99001"}, {"utf-8,version=02", "utf8version2"},
    };
    for (const auto& test : cases) {
        CheckStrip(test.input, test.expected, false);
        CheckStrip(ToEbcdic(test.input), ToEbcdic(test.expected), true);
    }
    Require(ucnv_compareNames_nid_postfix("UTF-8", "u*T@f08") == 0, "converter aliases compare equal");
    Require(ucnv_compareNames_nid_postfix("u*T@f08", "UTF-8") == 0, "alias equality is symmetric");
    Require(ucnv_compareNames_nid_postfix("00-01", "01") == '0' - '1', "ignored separators reset numeric runs before zero stripping");
    for (const auto& left : cases) for (const auto& right : cases) {
        const auto a = reinterpret_cast<const unsigned char*>(left.expected);
        const auto b = reinterpret_cast<const unsigned char*>(right.expected);
        std::size_t index = 0;
        while (a[index] && a[index] == b[index]) ++index;
        const int expected = static_cast<int>(a[index]) - static_cast<int>(b[index]);
        Require(ucnv_compareNames_nid_postfix(left.input, right.input) == expected, "comparison returns first differing normalized-byte difference");
    }
    for (unsigned value = 1; value <= 255; ++value) {
        const std::string source(1, static_cast<char>(value));
        std::string ascii;
        if ((value >= '0' && value <= '9') || (value >= 'a' && value <= 'z')) ascii = source;
        if (value >= 'A' && value <= 'Z') ascii += static_cast<char>(value + 32);
        CheckStrip(source, ascii, false);
        std::string ebcdic;
        for (char c : std::string_view("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"))
            if (static_cast<unsigned char>(Ebcdic(c)) == value) ebcdic.assign(1, Ebcdic(c >= 'A' && c <= 'Z' ? c + 32 : c));
        CheckStrip(source, ebcdic, true);
    }
    const std::string longName = "000" + std::string(4096, 'Q') + "-0001200";
    CheckStrip(longName, "0" + std::string(4096, 'q') + "1200", false);
    CheckStrip(ToEbcdic(longName), ToEbcdic("0" + std::string(4096, 'q') + "1200"), true);
    Require(ucnv_compareNames_nid_postfix("utf8", "utf16") == '8' - '1', "names are compared lexically, not numerically");
    char output[4] = {'@', '@', '@', 0};
    RequiresInvalidArgument([&] { ucnv_compareNames_nid_postfix(nullptr, ""); });
    RequiresInvalidArgument([&] { ucnv_compareNames_nid_postfix("", nullptr); });
    for (auto call : {ucnv_io_stripASCIIForCompare_nid_postfix, ucnv_io_stripEBCDICForCompare_nid_postfix}) {
        RequiresInvalidArgument([&] { call(nullptr, ""); });
        RequiresInvalidArgument([&] { call(output, nullptr); });
        Require(output[0] == '@' && output[1] == '@' && output[2] == '@', "invalid strip leaves output unchanged");
    }
}

void CheckSignature(const char* source, std::int32_t length, const char* expected, std::int32_t expectedLength) {
    for (std::int32_t initial : {0, -1, -128}) {
        std::int32_t status = initial;
        std::array<std::int32_t, 3> output{91, 92, 93};
        const char* result = ucnv_detectUnicodeSignature_nid_postfix(source, length, &output[1], &status);
        Require((!expected && !result) || (expected && result && std::strcmp(expected, result) == 0), "detected encoding matches");
        Require(output[1] == expectedLength && output[0] == 91 && output[2] == 93, "signature length and canaries match");
        Require(status == initial, "successful detection preserves existing success or warning status");
        result = ucnv_detectUnicodeSignature_nid_postfix(source, length, nullptr, &status);
        Require((!expected && !result) || (expected && result && std::strcmp(expected, result) == 0), "optional length output is supported");
    }
}

void Signatures() {
    struct Case { std::array<unsigned char, 6> bytes; const char* name; int length; };
    constexpr Case cases[] = {
        {{0xfe, 0xff}, "UTF-16BE", 2}, {{0xff, 0xfe}, "UTF-16LE", 2},
        {{0xef, 0xbb, 0xbf}, "UTF-8", 3}, {{0xff, 0xfe, 0, 0}, "UTF-32LE", 4},
        {{0, 0, 0xfe, 0xff}, "UTF-32BE", 4}, {{0x0e, 0xfe, 0xff}, "SCSU", 3},
        {{0xfb, 0xee, 0x28}, "BOCU-1", 3}, {{0x2b, 0x2f, 0x76, 0x38, 0x2d}, "UTF-7", 5},
        {{0x2b, 0x2f, 0x76, 0x38}, "UTF-7", 4}, {{0x2b, 0x2f, 0x76, 0x39}, "UTF-7", 4},
        {{0x2b, 0x2f, 0x76, 0x2b}, "UTF-7", 4}, {{0x2b, 0x2f, 0x76, 0x2f}, "UTF-7", 4},
        {{0xdd, 0x73, 0x66, 0x73}, "UTF-EBCDIC", 4},
    };
    for (const auto& test : cases) {
        const char* bytes = reinterpret_cast<const char*>(test.bytes.data());
        for (int length = 0; length <= test.length; ++length) {
            const char* expected = nullptr;
            int expectedLength = 0;
            if (length == test.length) { expected = test.name; expectedLength = test.length; }
            else if (test.bytes[0] == 0xff && length >= 2) { expected = "UTF-16LE"; expectedLength = 2; }
            else if (test.length == 5 && length == 4) { expected = "UTF-7"; expectedLength = 4; }
            CheckSignature(bytes, length, expected, expectedLength);
        }
        std::array<char, 12> padded;
        padded.fill('@');
        std::copy_n(bytes, test.length, padded.begin());
        CheckSignature(padded.data(), static_cast<std::int32_t>(padded.size()), test.name, test.length);
        padded[test.length] = 0;
        if (test.bytes[0] == 0) CheckSignature(padded.data(), -1, nullptr, 0);
        else if (test.length == 4 && test.bytes[0] == 0xff) CheckSignature(padded.data(), -1, "UTF-16LE", 2);
        else CheckSignature(padded.data(), -1, test.name, test.length);
    }
    CheckSignature("+/v7-", 5, nullptr, 0);
    CheckSignature("ordinary ASCII", -1, nullptr, 0);
    CheckSignature("\xef\xbb\xbf", 0, nullptr, 0);
    CheckSignature("", 0, nullptr, 0);
    std::int32_t length = 123;
    for (std::int32_t initial : {1, 15, INT32_MAX}) {
        std::int32_t status = initial;
        Require(!ucnv_detectUnicodeSignature_nid_postfix(nullptr, -2, &length, &status), "preexisting failure is returned immediately");
        Require(status == initial && length == 123, "preexisting failure preserves all outputs");
    }
    Require(!ucnv_detectUnicodeSignature_nid_postfix(nullptr, -2, &length, nullptr) && length == 123, "null status suppresses input access and output mutation");
    for (std::int32_t invalid : {-2, INT32_MIN}) {
        std::int32_t status = -1;
        Require(!ucnv_detectUnicodeSignature_nid_postfix("x", invalid, &length, &status), "invalid length is rejected");
        Require(status == 1 && length == 123, "invalid length changes only status");
    }
    for (std::int32_t size : {-1, 0, 1}) {
        std::int32_t status = 0;
        Require(!ucnv_detectUnicodeSignature_nid_postfix(nullptr, size, &length, &status), "null source is rejected even when empty");
        Require(status == 1 && length == 123, "null source changes only status");
    }
}

template<class TCall>
void RequiresRuntimeErrorKeepingErrno(TCall call, const char* message) {
    errno = 71;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71, message);
}

void SignatureGuestPointers() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(pages != nullptr, "allocate guest pointer pages");
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
    void* mapping = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "allocate guest pointer pages");
    auto* pages = static_cast<char*>(mapping);
#endif
    const std::int32_t zero = 0;
    const std::int32_t failing = 1;
    std::memcpy(pages, &zero, sizeof(zero));
    std::memcpy(pages + 16, &failing, sizeof(failing));
    std::memcpy(pages + pageSize - 3, "\xef\xbb", 3);
#ifdef _WIN32
    DWORD oldProtect = 0;
    Require(VirtualProtect(pages, pageSize, PAGE_READONLY, &oldProtect) != 0, "protect status page");
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &oldProtect) != 0, "protect guard page");
#else
    Require(mprotect(pages, pageSize, PROT_READ) == 0, "protect status page");
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0, "protect guard page");
#endif
    auto* readOnlyZero = reinterpret_cast<std::int32_t*>(pages);
    auto* readOnlyFailing = reinterpret_cast<std::int32_t*>(pages + 16);
    auto* unmappedStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    const auto* unmappedSource = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    const char* bom = "\xef\xbb\xbf";

    RequiresRuntimeErrorKeepingErrno([&] { ucnv_detectUnicodeSignature_nid_postfix(bom, 3, nullptr, unmappedStatus); },
                                     "unreadable status throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_detectUnicodeSignature_nid_postfix(nullptr, 0, nullptr, readOnlyZero); },
                                     "unwritable status throws and keeps errno");
    Require(!ucnv_detectUnicodeSignature_nid_postfix(nullptr, -2, nullptr, readOnlyFailing), "read-only failing status returns immediately");
    std::int32_t status = 0;
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_detectUnicodeSignature_nid_postfix(unmappedSource, 4, nullptr, &status); },
                                     "unreadable source with a length throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_detectUnicodeSignature_nid_postfix(unmappedSource, -1, nullptr, &status); },
                                     "unreadable terminated source throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_detectUnicodeSignature_nid_postfix(bom, 3, readOnlyZero, &status); },
                                     "unwritable signature length throws and keeps errno");
    Require(status == 0, "guest pointer failures leave the status unchanged");
    std::int32_t length = 123;
    Require(!ucnv_detectUnicodeSignature_nid_postfix(pages + pageSize - 3, -1, &length, &status), "terminated source stops before the guard page");
    Require(length == 0 && status == 0, "short terminated source reports no signature");

#ifdef _WIN32
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, pageSize * 2);
#endif
}

void NameMatchingGuestPointers() {
    struct GuardedPages {
        std::size_t pageSize = 4096;
        char* data = nullptr;

        GuardedPages() {
#ifdef _WIN32
            auto* mapped = VirtualAlloc(nullptr, pageSize * 3, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            Require(mapped != nullptr, "allocate name matching pages");
            data = static_cast<char*>(mapped);
            DWORD oldProtect = 0;
            Require(VirtualProtect(data + pageSize * 2, pageSize, PAGE_NOACCESS, &oldProtect) != 0,
                    "protect name matching guard page");
#else
            const long hostPage = sysconf(_SC_PAGESIZE);
            if (hostPage > 0) pageSize = static_cast<std::size_t>(hostPage);
            void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(mapped != MAP_FAILED, "allocate name matching pages");
            data = static_cast<char*>(mapped);
            Require(mprotect(data + pageSize * 2, pageSize, PROT_NONE) == 0,
                    "protect name matching guard page");
#endif
        }

        ~GuardedPages() {
#ifdef _WIN32
            if (data) VirtualFree(data, 0, MEM_RELEASE);
#else
            if (data) munmap(data, pageSize * 3);
#endif
        }

        void ProtectReadOnly(std::size_t offset, std::size_t bytes) {
#ifdef _WIN32
            DWORD oldProtect = 0;
            Require(VirtualProtect(data + offset, bytes, PAGE_READONLY, &oldProtect) != 0, "protect read-only pages");
#else
            Require(mprotect(data + offset, bytes, PROT_READ) == 0, "protect read-only pages");
#endif
        }
    };

    const auto* unmapped = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    auto* unmappedOut = reinterpret_cast<char*>(std::uintptr_t{0x10});

    RequiresRuntimeErrorKeepingErrno([&] { ucnv_compareNames_nid_postfix(unmapped, "utf8"); },
                                     "compareNames rejects unreadable left");
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_compareNames_nid_postfix("utf8", unmapped); },
                                     "compareNames rejects unreadable right");

    std::array<char, 32> scratch{};
    scratch.fill('@');
    for (auto call : {ucnv_io_stripASCIIForCompare_nid_postfix, ucnv_io_stripEBCDICForCompare_nid_postfix}) {
        RequiresRuntimeErrorKeepingErrno([&] { call(scratch.data(), unmapped); },
                                         "strip rejects unreadable name");
        Require(std::all_of(scratch.begin(), scratch.end(), [](char c) { return c == '@'; }),
                "unreadable name leaves destination unchanged");
        RequiresRuntimeErrorKeepingErrno([&] { call(unmappedOut, "utf8"); },
                                         "strip rejects unmapped destination");
    }

    GuardedPages pages;
    char* const tail = pages.data + pages.pageSize * 2;
    std::memset(pages.data, '@', pages.pageSize * 2);

    std::memcpy(tail - 5, "utf8", 5);
    Require(ucnv_compareNames_nid_postfix(tail - 5, "u*T@f08") == 0,
            "compareNames stops at NUL before guard page");
    Require(ucnv_compareNames_nid_postfix("u*T@f08", tail - 5) == 0,
            "compareNames right operand stops at NUL before guard page");

    std::memcpy(tail - 2, "0", 2);
    Require(ucnv_compareNames_nid_postfix(tail - 2, "000") == 0,
            "compareNames lookahead after leading zero sees NUL before guard page");

    std::memcpy(tail - 3, "utf", 3);
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_compareNames_nid_postfix(tail - 3, "utf8"); },
                                     "compareNames rejects left crossing into guard page");
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_compareNames_nid_postfix("utf8", tail - 3); },
                                     "compareNames rejects right crossing into guard page");

    tail[-1] = '0';
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_compareNames_nid_postfix(tail - 1, "0"); },
                                     "compareNames zero lookahead rejects crossing into guard page");

    const std::string crossPage(2000, 'A');
    std::memcpy(pages.data + pages.pageSize - 100, crossPage.c_str(), crossPage.size() + 1);
    const std::string expectedLower(2000, 'a');
    Require(ucnv_compareNames_nid_postfix(pages.data + pages.pageSize - 100, expectedLower.c_str()) == 0,
            "compareNames handles valid names crossing a page boundary");
    Require(ucnv_io_stripASCIIForCompare_nid_postfix(pages.data, pages.data + pages.pageSize - 100) == pages.data,
            "strip supports names crossing a writable page boundary");
    Require(std::string_view(pages.data) == expectedLower, "cross-page stripped content matches");
    Require(ucnv_io_stripASCIIForCompare_nid_postfix(pages.data + pages.pageSize - 100,
                                                     pages.data + pages.pageSize - 100) ==
                pages.data + pages.pageSize - 100,
            "in-place strip supports names crossing a writable page boundary");
    Require(std::string_view(pages.data + pages.pageSize - 100) == expectedLower,
            "in-place cross-page stripped content matches");

    std::memset(pages.data, '@', pages.pageSize * 2);
    std::memcpy(tail - 5, "UTF8", 5);
    Require(ucnv_io_stripASCIIForCompare_nid_postfix(scratch.data(), tail - 5) == scratch.data() &&
                std::strcmp(scratch.data(), "utf8") == 0,
            "ASCII strip reads NUL-terminated name before guard page");
    const std::string ebcdicUtf8 = ToEbcdic("UTF8");
    std::memcpy(tail - 5, ebcdicUtf8.c_str(), ebcdicUtf8.size() + 1);
    Require(ucnv_io_stripEBCDICForCompare_nid_postfix(scratch.data(), tail - 5) == scratch.data() &&
                std::strcmp(scratch.data(), ToEbcdic("utf8").c_str()) == 0,
            "EBCDIC strip reads NUL-terminated name before guard page");

    std::memcpy(tail - 3, "UTF", 3);
    scratch.fill('@');
    for (auto call : {ucnv_io_stripASCIIForCompare_nid_postfix, ucnv_io_stripEBCDICForCompare_nid_postfix}) {
        RequiresRuntimeErrorKeepingErrno([&] { call(scratch.data(), tail - 3); },
                                         "strip rejects unterminated name crossing into guard page");
        Require(std::all_of(scratch.begin(), scratch.end(), [](char c) { return c == '@'; }),
                "unterminated guard-crossing name leaves destination unchanged");
    }

    tail[-1] = '0';
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_io_stripASCIIForCompare_nid_postfix(scratch.data(), tail - 1); },
                                     "ASCII strip zero lookahead rejects crossing into guard page");
    tail[-1] = Ebcdic('0');
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_io_stripEBCDICForCompare_nid_postfix(scratch.data(), tail - 1); },
                                     "EBCDIC strip zero lookahead rejects crossing into guard page");

    std::memset(tail - 16, '@', 16);
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_io_stripASCIIForCompare_nid_postfix(tail - 2, "UTF-8"); },
                                     "ASCII strip rejects destination crossing into guard page");
    Require(tail[-2] == '@' && tail[-1] == '@',
            "rejected ASCII guard-crossing write makes no partial destination write");
    RequiresRuntimeErrorKeepingErrno(
        [&] { ucnv_io_stripEBCDICForCompare_nid_postfix(tail - 2, ToEbcdic("UTF-8").c_str()); },
        "EBCDIC strip rejects destination crossing into guard page");
    Require(tail[-2] == '@' && tail[-1] == '@',
            "rejected EBCDIC guard-crossing write makes no partial destination write");

    Require(ucnv_io_stripASCIIForCompare_nid_postfix(tail - 5, "UTF-8") == tail - 5 &&
                std::strcmp(tail - 5, "utf8") == 0 && tail[-6] == '@',
            "ASCII strip writes exact fit before guard page");
    Require(ucnv_io_stripEBCDICForCompare_nid_postfix(tail - 5, ToEbcdic("UTF-8").c_str()) == tail - 5 &&
                std::strcmp(tail - 5, ToEbcdic("utf8").c_str()) == 0 && tail[-6] == '@',
            "EBCDIC strip writes exact fit before guard page");

    std::memcpy(tail - 6, "UTF-8", 6);
    Require(ucnv_io_stripASCIIForCompare_nid_postfix(tail - 6, tail - 6) == tail - 6 &&
                std::strcmp(tail - 6, "utf8") == 0,
            "in-place strip succeeds near guard page");

    std::memcpy(pages.data + 32, "UTF-8", 6);
    std::memcpy(pages.data + 64, "kept", 5);
    const std::string ebcdicInput = ToEbcdic("UTF-8");
    std::memcpy(pages.data + 96, ebcdicInput.c_str(), ebcdicInput.size() + 1);
    pages.ProtectReadOnly(0, pages.pageSize);
    Require(ucnv_compareNames_nid_postfix(pages.data + 32, "u*T@f08") == 0,
            "compareNames reads from read-only page");
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_io_stripASCIIForCompare_nid_postfix(pages.data + 64, "abc"); },
                                     "ASCII strip rejects read-only destination");
    RequiresRuntimeErrorKeepingErrno([&] { ucnv_io_stripASCIIForCompare_nid_postfix(pages.data + 64, ""); },
                                     "ASCII strip rejects read-only destination even for empty output");
    RequiresRuntimeErrorKeepingErrno(
        [&] { ucnv_io_stripEBCDICForCompare_nid_postfix(pages.data + 64, ToEbcdic("abc").c_str()); },
        "EBCDIC strip rejects read-only destination");
    RequiresRuntimeErrorKeepingErrno(
        [&] { ucnv_io_stripASCIIForCompare_nid_postfix(pages.data + 32, pages.data + 32); },
        "strip rejects read-only in-place buffer");
    Require(std::strcmp(pages.data + 64, "kept") == 0 && std::strcmp(pages.data + 32, "UTF-8") == 0,
            "read-only buffers stay unchanged");
    Require(ucnv_io_stripEBCDICForCompare_nid_postfix(scratch.data(), pages.data + 96) == scratch.data() &&
                std::strcmp(scratch.data(), ToEbcdic("utf8").c_str()) == 0,
            "EBCDIC strip reads from read-only page");
}

}

int main() {
    DefaultConverterLifecycle();
    DefaultConverterNames();
    NameMatching();
    NameMatchingGuestPointers();
    Signatures();
    SignatureGuestPointers();
}
