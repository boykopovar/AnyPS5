#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "unicode/udata.h"
#include "unicode/uenum.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "unicode/utypes.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
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

static_assert(std::is_same_v<decltype(&uloc_openAvailableByType_nid_postfix),
                             UEnumeration* (APS5_VABI*)(std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ures_openAvailableLocales_nid_postfix),
                             UEnumeration* (APS5_VABI*)(const char*, std::int32_t*)>);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU available locales: %s\n", message);
    std::abort();
}

template<class TCall>
void RejectsInvalid(TCall call, const char* message) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t alignment, std::size_t size) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* result = nullptr;
    return posix_memalign(&result, alignment, size) == 0 ? result : nullptr;
#endif
}
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t alignment) {
#ifdef _WIN32
    return _aligned_realloc(pointer, size, alignment);
#else
    void* result = Align(alignment, size);
    std::free(pointer);
    return result;
#endif
}
int APS5_VABI PosixAlign(void** pointer, std::size_t alignment, std::size_t size) {
#ifdef _WIN32
    *pointer = _aligned_malloc(size, alignment);
    return *pointer ? 0 : 12;
#else
    return posix_memalign(pointer, alignment, size);
#endif
}

struct CommonDataHeader {
    std::uint16_t headerSize;
    std::uint8_t magic1;
    std::uint8_t magic2;
    UDataInfo info;
};

struct CommonDataPackage {
    CommonDataHeader header;
    std::uint32_t count;
    std::uint32_t reserved;
    const char* entryName;
    const void* entryData;
};

static_assert(sizeof(CommonDataHeader) == 24);
static_assert(offsetof(CommonDataPackage, count) == 24);
static_assert(offsetof(CommonDataPackage, entryName) == 32);
static_assert(offsetof(CommonDataPackage, entryData) == 40);

std::vector<std::uint32_t> BuildResIndexBundle() {
    constexpr std::uint32_t IndexCount = 6;
    std::vector<std::uint8_t> payload((1 + IndexCount) * sizeof(std::uint32_t), 0);
    const auto addKey = [&](std::string_view key) -> std::uint16_t {
        const auto offset = static_cast<std::uint16_t>(payload.size());
        payload.insert(payload.end(), key.begin(), key.end());
        payload.push_back(0);
        return offset;
    };
    const std::uint16_t offAlias = addKey("AliasLocales");
    const std::uint16_t offInstalled = addKey("InstalledLocales");
    const std::uint16_t offEn = addKey("en");
    const std::uint16_t offFr = addKey("fr_FR");
    const std::uint16_t offIw = addKey("iw");
    const std::uint16_t offNo = addKey("no_NO_NY");
    while (payload.size() % 4 != 0) payload.push_back(0);
    const auto keysTop = static_cast<std::uint32_t>(payload.size() / 4);

    const auto append16 = [&](std::uint16_t value) {
        const std::size_t pos = payload.size();
        payload.resize(pos + sizeof(value));
        std::memcpy(payload.data() + pos, &value, sizeof(value));
    };
    const auto append32 = [&](std::uint32_t value) {
        const std::size_t pos = payload.size();
        payload.resize(pos + sizeof(value));
        std::memcpy(payload.data() + pos, &value, sizeof(value));
    };
    const auto appendTable = [&](const std::vector<std::pair<std::uint16_t, std::uint32_t>>& items) -> std::uint32_t {
        Require(payload.size() % 4 == 0, "table alignment");
        const auto wordOffset = static_cast<std::uint32_t>(payload.size() / 4);
        append16(static_cast<std::uint16_t>(items.size()));
        for (const auto& item : items) append16(item.first);
        if ((items.size() & 1u) == 0) append16(0);
        for (const auto& item : items) append32(item.second);
        return (std::uint32_t{2} << 28) | wordOffset;
    };

    const std::uint32_t installedTable = appendTable({{offEn, 0}, {offFr, 0}});
    const std::uint32_t aliasTable = appendTable({{offIw, 0}, {offNo, 0}});
    const std::uint32_t rootTable = appendTable({{offAlias, aliasTable}, {offInstalled, installedTable}});
    const auto bundleTop = static_cast<std::uint32_t>(payload.size() / 4);

    const std::array<std::uint32_t, 1 + IndexCount> headerWords{
        rootTable,
        IndexCount,
        keysTop,
        bundleTop,
        bundleTop,
        2,
        1,
    };
    std::memcpy(payload.data(), headerWords.data(), sizeof(headerWords));

    CommonDataHeader resHeader{};
    resHeader.headerSize = sizeof(CommonDataHeader);
    resHeader.magic1 = 0xda;
    resHeader.magic2 = 0x27;
    resHeader.info.size = sizeof(UDataInfo);
    resHeader.info.isBigEndian = U_IS_BIG_ENDIAN;
    resHeader.info.charsetFamily = U_CHARSET_FAMILY;
    resHeader.info.sizeofUChar = U_SIZEOF_UCHAR;
    resHeader.info.dataFormat[0] = 'R';
    resHeader.info.dataFormat[1] = 'e';
    resHeader.info.dataFormat[2] = 's';
    resHeader.info.dataFormat[3] = 'B';
    resHeader.info.formatVersion[0] = 1;
    resHeader.info.formatVersion[1] = 2;

    std::vector<std::uint32_t> bundle((sizeof(resHeader) + payload.size()) / sizeof(std::uint32_t), 0);
    std::memcpy(bundle.data(), &resHeader, sizeof(resHeader));
    std::memcpy(reinterpret_cast<std::uint8_t*>(bundle.data()) + sizeof(resHeader), payload.data(), payload.size());
    return bundle;
}

void CompareEnumerations(UEnumeration* host, UEnumeration* guest, UErrorCode& hostStatus, std::int32_t& guestStatus) {
    const auto hostCount = uenum_count(host, &hostStatus);
    const auto guestCount = uenum_count_nid_postfix(guest, &guestStatus);
    Require(hostCount == guestCount && static_cast<std::int32_t>(hostStatus) == guestStatus, "count matches host ICU");
    for (;;) {
        std::int32_t hostLength = -1;
        std::int32_t guestLength = -1;
        const char* hostName = uenum_next(host, &hostLength, &hostStatus);
        const char* guestName = uenum_next_nid_postfix(guest, &guestLength, &guestStatus);
        Require((hostName == nullptr) == (guestName == nullptr), "iteration ends with host ICU");
        Require(hostLength == guestLength && static_cast<std::int32_t>(hostStatus) == guestStatus, "iteration metadata matches host ICU");
        if (!hostName) break;
        Require(std::strcmp(hostName, guestName) == 0, "locale name matches host ICU");
    }
}

void CompareResourceLocales(const char* path, bool found) {
    UErrorCode hostStatus = U_USING_FALLBACK_WARNING;
    std::int32_t status = hostStatus;
    UEnumeration* host = ures_openAvailableLocales(path, &hostStatus);
    UEnumeration* guest = ures_openAvailableLocales_nid_postfix(path, &status);
    Require((host != nullptr) == found && (guest != nullptr) == found && status == hostStatus,
            "resource package open matches bundled ICU");
    if (guest) {
        Require(uenum_count_nid_postfix(guest, &status) == 2, "resource package enumerates installed locales");
        CompareEnumerations(host, guest, hostStatus, status);
        guest->reset(guest, &status);
        std::int32_t length = -1;
        const char16_t* first = uenum_unext_nid_postfix(guest, &length, &status);
        Require(first && length == 2 && std::u16string_view(first, static_cast<std::size_t>(length)) == u"en",
                "resource package enumeration can reset and read UTF-16");
    }
    uenum_close(host);
    uenum_close_nid_postfix(guest);
}

void CheckErrorsAndParity() {
    for (std::int32_t type : {0, 1, 2}) {
        UErrorCode hostStatus = U_ZERO_ERROR;
        UEnumeration* host = uloc_openAvailableByType(static_cast<ULocAvailableType>(type), &hostStatus);
        std::int32_t guestStatus = 0;
        UEnumeration* guest = uloc_openAvailableByType_nid_postfix(type, &guestStatus);
        Require((host == nullptr) == (guest == nullptr), "availability matches reduced host data");
        Require(static_cast<std::int32_t>(hostStatus) == guestStatus, "open status matches reduced host data");
        if (host) CompareEnumerations(host, guest, hostStatus, guestStatus);
        uenum_close(host);
        uenum_close_nid_postfix(guest);
    }

    for (std::int32_t badType : {std::numeric_limits<std::int32_t>::min(), -1, 3, 4, std::numeric_limits<std::int32_t>::max()}) {
        std::int32_t status = 0;
        Require(!uloc_openAvailableByType_nid_postfix(badType, &status) && status == U_ILLEGAL_ARGUMENT_ERROR,
                "invalid type sets U_ILLEGAL_ARGUMENT_ERROR");
        status = -128;
        Require(!uloc_openAvailableByType_nid_postfix(badType, &status) && status == U_ILLEGAL_ARGUMENT_ERROR,
                "invalid type overwrites warning with U_ILLEGAL_ARGUMENT_ERROR");
    }

    for (std::int32_t initial : {1, 15, std::numeric_limits<std::int32_t>::max()}) {
        for (std::int32_t type : {-1, 0, 3}) {
            std::int32_t status = initial;
            Require(!uloc_openAvailableByType_nid_postfix(type, &status) && status == initial, "prefailed status is preserved");
        }
    }

    RejectsInvalid([] { uloc_openAvailableByType_nid_postfix(0, nullptr); }, "null status is rejected");
    const auto* unreadablePath = reinterpret_cast<const char*>(std::uintptr_t{1});
    std::int32_t prefailed = U_BUFFER_OVERFLOW_ERROR;
    Require(!ures_openAvailableLocales_nid_postfix(unreadablePath, &prefailed) && prefailed == U_BUFFER_OVERFLOW_ERROR,
            "resource locale enumeration preserves a pre-existing failure without reading the path");
    RejectsInvalid([] { ures_openAvailableLocales_nid_postfix(nullptr, nullptr); },
                   "resource locale enumeration rejects null status");
    CompareResourceLocales("anyps5_missing_locale_test_package", false);

#ifdef _WIN32
    const std::size_t pageSize = 4096;
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(pages != nullptr, "allocate status pages");
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
    void* mapping = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "allocate status pages");
    auto* pages = static_cast<char*>(mapping);
#endif
    const std::int32_t success = 0;
    const std::int32_t failure = 1;
    std::memcpy(pages, &success, sizeof(success));
    std::memcpy(pages + 16, &failure, sizeof(failure));
#ifdef _WIN32
    DWORD oldProtection = 0;
    Require(VirtualProtect(pages, pageSize, PAGE_READONLY, &oldProtection) != 0, "protect read-only status page");
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &oldProtection) != 0, "protect unreadable status page");
#else
    Require(mprotect(pages, pageSize, PROT_READ) == 0, "protect read-only status page");
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0, "protect unreadable status page");
#endif
    auto* readOnlySuccess = reinterpret_cast<std::int32_t*>(pages);
    auto* readOnlyFailure = reinterpret_cast<std::int32_t*>(pages + 16);
    auto* unreadable = reinterpret_cast<std::int32_t*>(pages + pageSize);

    Require(!uloc_openAvailableByType_nid_postfix(-1, readOnlyFailure), "read-only prefailed status is not written");
    errno = 71;
    RejectsInvalid([&] { uloc_openAvailableByType_nid_postfix(0, unreadable); }, "unreadable status is rejected");
    Require(errno == 71, "unreadable status keeps errno");
    errno = 71;
    RejectsInvalid([&] { uloc_openAvailableByType_nid_postfix(-1, readOnlySuccess); }, "read-only changed status is rejected");
    Require(errno == 71, "read-only changed status keeps errno");

#ifdef _WIN32
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, pageSize * 2);
#endif
}

void CheckEnumeration(std::int32_t type, const std::vector<std::string_view>& expected) {
    std::int32_t status = -128;
    UEnumeration* enumeration = uloc_openAvailableByType_nid_postfix(type, &status);
    Require(enumeration != nullptr && status == -128, "open succeeds with res_index data and preserves warning");
    Require(!enumeration->baseContext && enumeration->context && enumeration->close && enumeration->count &&
            enumeration->uNext && enumeration->next && enumeration->reset,
            "adapter exposes full guest enumeration vtable");
    Require(uenum_count_nid_postfix(enumeration, &status) == static_cast<std::int32_t>(expected.size()) && status == -128,
            "count matches res_index entries");

    for (const std::string_view item : expected) {
        std::int32_t length = -1;
        const char* value = uenum_next_nid_postfix(enumeration, &length, &status);
        Require(value != nullptr && std::string_view(value, static_cast<std::size_t>(length)) == item && status == -128,
                "next returns expected locale ID");
    }
    std::int32_t endLength = -1;
    Require(!uenum_next_nid_postfix(enumeration, &endLength, &status) && endLength == 0 && status == -128,
            "next reports exhaustion");

    enumeration->reset(enumeration, &status);
    Require(status == -128, "reset preserves warning");
    for (const std::string_view item : expected) {
        std::int32_t length = -1;
        const char16_t* wide = uenum_unext_nid_postfix(enumeration, &length, &status);
        Require(wide != nullptr && length == static_cast<std::int32_t>(item.size()) && status == -128,
                "unext returns expected UTF-16 length");
        for (std::size_t index = 0; index < item.size(); ++index) {
            Require(wide[index] == static_cast<char16_t>(item[index]), "unext characters match ASCII locale ID");
        }
        Require(wide[item.size()] == 0, "unext output is NUL-terminated");
    }

    enumeration->reset(enumeration, &status);
    const char* firstWithNullLength = uenum_next_nid_postfix(enumeration, nullptr, &status);
    Require(firstWithNullLength != nullptr && std::string_view(firstWithNullLength) == expected.front(),
            "next accepts null resultLength");
    uenum_close_nid_postfix(enumeration);

    UErrorCode hostStatus = U_ZERO_ERROR;
    UEnumeration* host = uloc_openAvailableByType(static_cast<ULocAvailableType>(type), &hostStatus);
    std::int32_t guestStatus = 0;
    UEnumeration* guest = uloc_openAvailableByType_nid_postfix(type, &guestStatus);
    Require(host != nullptr && guest != nullptr && hostStatus == U_ZERO_ERROR && guestStatus == 0,
            "host and guest open succeed");
    CompareEnumerations(host, guest, hostStatus, guestStatus);
    uenum_close(host);
    uenum_close_nid_postfix(guest);
}

void CheckWithSyntheticData() {
    static const std::vector<std::uint32_t> resIndex = BuildResIndexBundle();
    static const CommonDataPackage package{
        {
            sizeof(CommonDataHeader),
            0xda,
            0x27,
            {sizeof(UDataInfo), 0, U_IS_BIG_ENDIAN, U_CHARSET_FAMILY, U_SIZEOF_UCHAR, 0, {'T', 'o', 'C', 'P'}, {1, 0, 0, 0}, {0, 0, 0, 0}},
        },
        1,
        0,
        U_ICUDATA_NAME "/res_index.res",
        resIndex.data(),
    };

    std::int32_t status = 0;
    udata_setCommonData_nid_postfix(&package, &status);
    Require(status == 0, "udata_setCommonData accepts synthetic res_index package");
    UErrorCode hostStatus = U_ZERO_ERROR;
    udata_setCommonData(&package, &hostStatus);
    Require(hostStatus == U_ZERO_ERROR, "host udata_setCommonData accepts synthetic res_index package");

    Require(uloc_countAvailable_nid_postfix() == 2, "uloc_countAvailable sees InstalledLocales");
    Require(std::string_view(uloc_getAvailable_nid_postfix(0)) == "en", "first available locale is en");
    Require(std::string_view(uloc_getAvailable_nid_postfix(1)) == "fr_FR", "second available locale is fr_FR");

    CheckEnumeration(ULOC_AVAILABLE_DEFAULT, {"en", "fr_FR"});
    CheckEnumeration(ULOC_AVAILABLE_ONLY_LEGACY_ALIASES, {"iw", "no_NO_NY"});
    CheckEnumeration(ULOC_AVAILABLE_WITH_LEGACY_ALIASES, {"en", "fr_FR", "iw", "no_NO_NY"});
    CompareResourceLocales(nullptr, true);

    UResourceBundle* bundle = ures_openDirect(nullptr, "res_index", &hostStatus);
    Require(bundle != nullptr && U_SUCCESS(hostStatus), "open a native resource bundle with the synthetic index");
    const char* expectedLocale = ures_getLocale(bundle, &hostStatus);
    status = U_ZERO_ERROR;
    Require(expectedLocale != nullptr && ures_getLocale_nid_postfix(bundle, &status) == expectedLocale &&
            ures_getLocaleInternal_nid_postfix(bundle, &status) == expectedLocale && status == hostStatus,
            "both guest getters return the bundled ICU actual locale pointer");
    ures_close(bundle);

    std::int32_t invalidStatus = 0;
    Require(!uloc_openAvailableByType_nid_postfix(ULOC_AVAILABLE_COUNT, &invalidStatus) &&
            invalidStatus == U_ILLEGAL_ARGUMENT_ERROR,
            "invalid type still fails with res_index loaded");
}

}

int main(int argc, char** argv) {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());

    const std::string_view scenario = argc > 1 ? argv[1] : "";
    if (scenario == "errors") {
        CheckErrorsAndParity();
    } else if (scenario == "data") {
        CheckWithSyntheticData();
    } else {
        std::fprintf(stderr, "unknown available locales scenario\n");
        return 1;
    }
    return 0;
}
