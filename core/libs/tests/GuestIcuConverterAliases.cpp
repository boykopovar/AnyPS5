#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "ucnv_io.h"
#include "udataswp.h"
#include "unicode/ucnv.h"
#include "unicode/udata.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
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
    std::fprintf(stderr, "ICU converter aliases: %s\n", message);
    std::abort();
}

template<class TCall>
void RequireRuntimeError(TCall call, const char* message) {
    errno = 71;
    try {
        call();
    } catch (const std::runtime_error&) {
        Require(errno == 71, "guest pointer failure preserves errno");
        return;
    }
    Require(false, message);
}

template<class TCall>
void RequireInvalidArgument(TCall call, const char* message) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

std::string CheckedString(const char* source) {
    std::string result;
    for (;;) {
        char value = 0;
        Require(GuestCopyIn_nid_no_patch(&value, source + result.size(), sizeof(value)),
                "borrowed alias pointer is guest-readable");
        if (!value) return result;
        result.push_back(value);
    }
}

void CheckParity(const char* alias, std::int32_t initial) {
    UErrorCode nativeStatus = static_cast<UErrorCode>(initial);
    const auto count = ucnv_countAliases(alias, &nativeStatus);
    std::vector<const char*> expected(count);
    if (U_SUCCESS(nativeStatus) && count != 0) ucnv_getAliases(alias, expected.data(), &nativeStatus);

    std::vector<const char*> actual(static_cast<std::size_t>(count) + 2,
                                    reinterpret_cast<const char*>(std::uintptr_t{0x1234}));
    std::int32_t guestStatus = initial;
    ucnv_getAliases_nid_postfix(alias, actual.data() + 1, &guestStatus);
    Require(guestStatus == static_cast<std::int32_t>(nativeStatus), "status matches pinned ICU");
    Require(actual.front() == reinterpret_cast<const char*>(std::uintptr_t{0x1234}) &&
            actual.back() == reinterpret_cast<const char*>(std::uintptr_t{0x1234}), "output canaries remain intact");
    for (std::size_t index = 0; index < count; ++index) {
        Require(CheckedString(actual[index + 1]) == expected[index], "alias value and order match pinned ICU");
        Require(CheckedString(expected[index]) == expected[index], "native alias storage is guest-readable");
    }
}

void Parity() {
    static_assert(std::is_same_v<decltype(&ucnv_getAliases_nid_postfix),
                  void (APS5_VABI*)(const char*, const char**, std::int32_t*)>);
    const std::string shortLimit(UCNV_MAX_CONVERTER_NAME_LENGTH - 1, 'a');
    const std::string exactLimit(UCNV_MAX_CONVERTER_NAME_LENGTH, 'a');
    const std::string pastLimit(UCNV_MAX_CONVERTER_NAME_LENGTH + 1, 'a');
    for (std::int32_t status : {0, -1, -128, 1, 15}) {
        CheckParity(nullptr, status);
        CheckParity("", status);
        CheckParity("UTF-8", status);
        CheckParity("u*T@f08", status);
        CheckParity("not-a-converter", status);
        CheckParity(shortLimit.c_str(), status);
        CheckParity(exactLimit.c_str(), status);
        CheckParity(pastLimit.c_str(), status);
    }
}

void GuestPointers() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(pages != nullptr, "allocate guest pages");
#else
    const long configured = sysconf(_SC_PAGESIZE);
    const std::size_t pageSize = static_cast<std::size_t>(configured > 0 ? configured : 4096);
    void* mapping = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "allocate guest pages");
    auto* pages = static_cast<char*>(mapping);
#endif
    const std::int32_t success = 0;
    const std::int32_t failure = 1;
    std::memcpy(pages, &success, sizeof(success));
    std::memcpy(pages + 16, &failure, sizeof(failure));
    std::memcpy(pages + pageSize - 6, "UTF-8", 6);
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(pages, pageSize, PAGE_READONLY, &previous) != 0, "protect input page");
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "protect guard page");
#else
    Require(mprotect(pages, pageSize, PROT_READ) == 0, "protect input page");
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0, "protect guard page");
#endif
    auto* readOnlySuccess = reinterpret_cast<std::int32_t*>(pages);
    auto* readOnlyFailure = reinterpret_cast<std::int32_t*>(pages + 16);
    auto* invalidStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    const auto* invalidAlias = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    auto** invalidOutput = reinterpret_cast<const char**>(std::uintptr_t{0x10});

    RequireInvalidArgument([&] { ucnv_getAliases_nid_postfix(invalidAlias, invalidOutput, nullptr); },
                           "null status throws before other pointers are read");
    RequireRuntimeError([&] { ucnv_getAliases_nid_postfix("UTF-8", nullptr, invalidStatus); },
                        "unreadable status throws");
    ucnv_getAliases_nid_postfix(invalidAlias, invalidOutput, readOnlyFailure);
    Require(*readOnlyFailure == failure, "prefailed read-only status returns before other pointers are read");
    RequireRuntimeError([&] { ucnv_getAliases_nid_postfix("UTF-8", nullptr, readOnlySuccess); },
                        "successful read-only status is rejected before ICU work");

    UErrorCode probeStatus = U_ZERO_ERROR;
    const auto count = ucnv_countAliases("UTF-8", &probeStatus);
    std::int32_t boundaryStatus = 0;
    std::vector<const char*> aliases(static_cast<std::size_t>(count) + 2,
                                     reinterpret_cast<const char*>(std::uintptr_t{0x5678}));
    ucnv_getAliases_nid_postfix(pages + pageSize - 6, aliases.data() + 1, &boundaryStatus);
    Require(boundaryStatus == static_cast<std::int32_t>(probeStatus), "page-boundary alias status matches native");
    if (U_SUCCESS(probeStatus) && count != 0) {
        RequireRuntimeError([&] { ucnv_getAliases_nid_postfix(invalidAlias, aliases.data() + 1, &boundaryStatus); },
                            "unreadable alias throws after alias data is available");
        boundaryStatus = 0;
        RequireRuntimeError([&] { ucnv_getAliases_nid_postfix("UTF-8", invalidOutput, &boundaryStatus); },
                            "unwritable alias array throws without a partial write");
    } else {
        boundaryStatus = 0;
        ucnv_getAliases_nid_postfix(invalidAlias, invalidOutput, &boundaryStatus);
        Require(boundaryStatus == static_cast<std::int32_t>(probeStatus),
                "alias-data failure precedes input and output access");
    }

#ifdef _WIN32
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, pageSize * 3);
#endif
}


void NameQueries() {
    static_assert(std::is_same_v<decltype(&ucnv_countAvailable_nid_postfix), std::int32_t (APS5_VABI*)()>);
    static_assert(std::is_same_v<decltype(&ucnv_getAvailableName_nid_postfix),
                  const char* (APS5_VABI*)(std::int32_t)>);
    static_assert(std::is_same_v<decltype(&ucnv_countStandards_nid_postfix), std::uint16_t (APS5_VABI*)()>);
    static_assert(std::is_same_v<decltype(&ucnv_countAliases_nid_postfix),
                  std::uint16_t (APS5_VABI*)(const char*, std::int32_t*)>);
    static_assert(std::is_same_v<decltype(&ucnv_getAlias_nid_postfix),
                  const char* (APS5_VABI*)(const char*, std::uint16_t, std::int32_t*)>);
    static_assert(std::is_same_v<decltype(&ucnv_getStandardName_nid_postfix),
                  const char* (APS5_VABI*)(const char*, const char*, std::int32_t*)>);

    const auto available = ucnv_countAvailable();
    Require(ucnv_countAvailable_nid_postfix() == available, "available count matches pinned ICU");
    for (std::int32_t index : {-1, 0, 1, available - 1, available, available + 1})
        Require(ucnv_getAvailableName_nid_postfix(index) == ucnv_getAvailableName(index),
                "available name matches pinned ICU");
    Require(ucnv_countStandards_nid_postfix() == ucnv_countStandards(), "standard count matches pinned ICU");

    const std::string pastLimit(UCNV_MAX_CONVERTER_NAME_LENGTH + 1, 'a');
    const char* names[] = {nullptr, "", "UTF-8", "u*T@f08", "ISO-8859-1", "not-a-converter", pastLimit.c_str()};
    const char* standards[] = {nullptr, "", "MIME", "IANA", "WINDOWS", "not-a-standard"};
    for (std::int32_t initial : {0, -1, -128, 1, 15}) {
        for (const char* name : names) {
            UErrorCode nativeStatus = static_cast<UErrorCode>(initial);
            const auto count = ucnv_countAliases(name, &nativeStatus);
            std::int32_t guestStatus = initial;
            Require(ucnv_countAliases_nid_postfix(name, &guestStatus) == count &&
                    guestStatus == static_cast<std::int32_t>(nativeStatus), "alias count matches pinned ICU");
            for (std::uint16_t index : {std::uint16_t{0}, std::uint16_t{1}, count, std::uint16_t{0xffff}}) {
                nativeStatus = static_cast<UErrorCode>(initial);
                const char* expected = ucnv_getAlias(name, index, &nativeStatus);
                guestStatus = initial;
                const char* actual = ucnv_getAlias_nid_postfix(name, index, &guestStatus);
                Require(actual == expected && guestStatus == static_cast<std::int32_t>(nativeStatus),
                        "alias matches pinned ICU");
                if (actual) Require(CheckedString(actual) == actual, "alias storage is guest-readable");
            }
            for (const char* standard : standards) {
                nativeStatus = static_cast<UErrorCode>(initial);
                const char* expected = ucnv_getStandardName(name, standard, &nativeStatus);
                guestStatus = initial;
                const char* actual = ucnv_getStandardName_nid_postfix(name, standard, &guestStatus);
                Require(actual == expected && guestStatus == static_cast<std::int32_t>(nativeStatus),
                        "standard name matches pinned ICU");
                if (actual) Require(CheckedString(actual) == actual, "standard name storage is guest-readable");
            }
        }
    }

    const auto* invalid = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    auto* invalidStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    RequireInvalidArgument([&] { ucnv_countAliases_nid_postfix(invalid, nullptr); }, "count: null status throws");
    RequireInvalidArgument([&] { ucnv_getAlias_nid_postfix(invalid, 0, nullptr); }, "alias: null status throws");
    RequireInvalidArgument([&] { ucnv_getStandardName_nid_postfix(invalid, invalid, nullptr); },
                           "standard: null status throws");
    RequireRuntimeError([&] { ucnv_countAliases_nid_postfix("UTF-8", invalidStatus); }, "count: bad status throws");
    std::int32_t failed = 1;
    Require(ucnv_countAliases_nid_postfix(invalid, &failed) == 0 && failed == 1, "count: prefailed status returns");
    Require(!ucnv_getAlias_nid_postfix(invalid, 0, &failed) && failed == 1, "alias: prefailed status returns");
    Require(!ucnv_getStandardName_nid_postfix(invalid, invalid, &failed) && failed == 1,
            "standard: prefailed status returns");
    std::int32_t ok = 0;
    RequireRuntimeError([&] { ucnv_countAliases_nid_postfix(invalid, &ok); }, "count: unreadable alias throws");
    ok = 0;
    RequireRuntimeError([&] { ucnv_getAlias_nid_postfix(invalid, 0, &ok); }, "alias: unreadable alias throws");
    ok = 0;
    RequireRuntimeError([&] { ucnv_getStandardName_nid_postfix("UTF-8", invalid, &ok); },
                        "standard: unreadable standard throws");
}

void AliasTableQueries() {
    static_assert(std::is_same_v<decltype(&ucnv_io_countKnownConverters_nid_postfix),
                  std::uint16_t (APS5_VABI*)(std::int32_t*)>);
    static_assert(std::is_same_v<decltype(&ucnv_io_getConverterName_nid_postfix),
                  const char* (APS5_VABI*)(const char*, std::int8_t*, std::int32_t*)>);
    const std::string longName(UCNV_MAX_CONVERTER_NAME_LENGTH + 1, 'a');
    const char* aliases[] = {nullptr, "", "UTF-8", "x-u*T@f08", "ISO-2022-JP", "no-such-converter", longName.c_str()};
    for (std::int32_t initial : {0, -128, 15}) {
        auto nativeStatus = static_cast<UErrorCode>(initial);
        const auto expectedCount = ucnv_io_countKnownConverters(&nativeStatus);
        auto status = initial;
        Require(ucnv_io_countKnownConverters_nid_postfix(&status) == expectedCount && status == nativeStatus,
                "known converter count and status match bundled ICU");
        for (const char* alias : aliases) {
            for (const bool withOption : {false, true}) {
                nativeStatus = static_cast<UErrorCode>(initial);
                UBool expectedOption = 73;
                auto* nativeOutput = withOption ? &expectedOption : nullptr;
                const char* expected = ucnv_io_getConverterName(alias, nativeOutput, &nativeStatus);
                status = initial;
                std::int8_t option = 73;
                const char* actual = ucnv_io_getConverterName_nid_postfix(alias, withOption ? &option : nullptr, &status);
                Require(status == nativeStatus && option == expectedOption, "lookup status and option match bundled ICU");
                Require((actual == nullptr) == (expected == nullptr), "lookup result presence matches bundled ICU");
                if (actual) Require(CheckedString(actual) == expected, "canonical name matches bundled ICU");
            }
        }
    }
    const auto* unreadableAlias = reinterpret_cast<const char*>(std::uintptr_t{1});
    auto* unwritableOption = reinterpret_cast<std::int8_t*>(std::uintptr_t{1});
    std::int32_t failed = U_BUFFER_OVERFLOW_ERROR;
    Require(!ucnv_io_getConverterName_nid_postfix(unreadableAlias, unwritableOption, &failed) &&
            failed == U_BUFFER_OVERFLOW_ERROR, "prefailed lookup returns before reading alias or writing option");
}

std::vector<char> LoadAliasTable(const char* path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open(), "open the bundled alias data fixture");
    const std::vector<char> input((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Require(!input.empty() && input.size() <= INT32_MAX, "read the bundled alias data fixture");
    const auto size = static_cast<std::int32_t>(input.size());
    UErrorCode status = U_ZERO_ERROR;
    auto* swapper = udata_openSwapperForInputData(input.data(), size, U_IS_BIG_ENDIAN, U_CHARSET_FAMILY, &status);
    Require(swapper && U_SUCCESS(status), "open a swapper for the alias fixture");
    std::vector<char> output(input.size());
    const auto written = ucnv_swapAliases(swapper, input.data(), size, output.data(), &status);
    udata_closeSwapper(swapper);
    Require(written > 0 && written <= size && U_SUCCESS(status), "convert the alias fixture to native byte order");
    output.resize(static_cast<std::size_t>(written));
    return output;
}

std::vector<char> AliasPackage(const std::vector<char>& aliases) {
    struct DataHeader {
        std::uint16_t headerSize;
        std::uint8_t magic1;
        std::uint8_t magic2;
        UDataInfo info;
    };
    static_assert(sizeof(DataHeader) == 24);
    constexpr std::uint32_t headerSize = 32;
    constexpr DataHeader header{
        headerSize, 0xda, 0x27,
        {sizeof(UDataInfo), 0, U_IS_BIG_ENDIAN, U_CHARSET_FAMILY, U_SIZEOF_UCHAR, 0,
         {'C', 'm', 'n', 'D'}, {1, 0, 0, 0}, {0, 0, 0, 0}},
    };
    constexpr char name[] = U_ICUDATA_NAME "/cnvalias.icu";
    constexpr char trailingName[] = U_ICUDATA_NAME "/zz-test-end.icu";
    constexpr std::uint32_t nameOffset = 5 * sizeof(std::uint32_t);
    constexpr std::uint32_t trailingNameOffset = nameOffset + sizeof(name);
    constexpr std::uint32_t dataOffset = (trailingNameOffset + sizeof(trailingName) + 15u) & ~15u;
    const auto trailingDataOffset = (dataOffset + static_cast<std::uint32_t>(aliases.size()) + 15u) & ~15u;
    const std::uint32_t toc[] = {2, nameOffset, dataOffset, trailingNameOffset, trailingDataOffset};
    std::vector<char> package(headerSize + trailingDataOffset + aliases.size());
    std::memcpy(package.data(), &header, sizeof(header));
    auto* contents = package.data() + headerSize;
    std::memcpy(contents, toc, sizeof(toc));
    std::memcpy(contents + nameOffset, name, sizeof(name));
    std::memcpy(contents + trailingNameOffset, trailingName, sizeof(trailingName));
    std::memcpy(contents + dataOffset, aliases.data(), aliases.size());
    std::memcpy(contents + trailingDataOffset, aliases.data(), aliases.size());
    return package;
}

void AliasTableData(const char* path) {
    static const auto package = AliasPackage(LoadAliasTable(path));
    UErrorCode nativeStatus = U_ZERO_ERROR;
    udata_setCommonData(package.data(), &nativeStatus);
    std::int32_t status = U_ZERO_ERROR;
    udata_setCommonData_nid_postfix(package.data(), &status);
    Require(U_SUCCESS(nativeStatus) && status <= 0, "install the alias table on both sides");

    AliasTableQueries();
    status = U_ZERO_ERROR;
    Require(ucnv_io_countKnownConverters_nid_postfix(&status) > 0 && status == U_ZERO_ERROR,
            "the installed table has known converters");
    std::int8_t option = 73;
    const char* name = ucnv_io_getConverterName_nid_postfix("UTF-8", &option, &status);
    Require(name && CheckedString(name) == "UTF-8" && option == 0 && status == U_ZERO_ERROR,
            "a UTF-8 lookup writes a false option flag");
    name = ucnv_io_getConverterName_nid_postfix("ISO-2022-JP", &option, &status);
    Require(name && CheckedString(name).find(',') != std::string::npos && option == 1 && status <= 0,
            "an option-bearing converter writes a true option flag");
    auto* unwritableOption = reinterpret_cast<std::int8_t*>(std::uintptr_t{1});
    status = U_ZERO_ERROR;
    RequireRuntimeError([&] { ucnv_io_getConverterName_nid_postfix("UTF-8", unwritableOption, &status); },
                        "successful lookup rejects an unwritable option");
    Require(!ucnv_io_getConverterName_nid_postfix("no-such-converter", unwritableOption, &status),
            "a missing lookup leaves the option pointer untouched");
}

}

int main(int argc, char** argv) {
    if (argc > 1) {
        AliasTableData(argv[1]);
        return 0;
    }
    Parity();
    GuestPointers();
    NameQueries();
    AliasTableQueries();
}
