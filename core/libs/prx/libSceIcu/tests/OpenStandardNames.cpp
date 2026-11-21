// Derived from icu4c/source/data/mappings/convrtrs.txt@049e0d6a420629ac7db77256987d083a563287b5 (ICU 78.1):
// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/data/mappings/convrtrs.txt
//
// The upstream file's notice, verbatim:
//   Copyright (C) 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1995-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/ucnv.h"
#include "unicode/udata.h"
#include "unicode/uenum.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static_assert(std::is_same_v<decltype(&ucnv_openStandardNames_nid_postfix),
                             UEnumeration* (APS5_VABI*)(const char*, const char*, std::int32_t*)>);

namespace {

extern "C" std::uint16_t SetupOpenStandardNamesData(const void*, std::int32_t*);
extern "C" void SetOpenStandardNamesStatusForTest_nid_no_patch(std::int32_t);

alignas(16) constexpr unsigned char AliasData[] = {
    0x20, 0x00, 0xda, 0x27, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x43, 0x6d, 0x6e, 0x44,
    0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x69, 0x63, 0x75, 0x64, 0x74, 0x37, 0x38, 0x6c, 0x2f, 0x63, 0x6e, 0x76,
    0x61, 0x6c, 0x69, 0x61, 0x73, 0x2e, 0x69, 0x63, 0x75, 0x00, 0x69, 0x63, 0x75, 0x64, 0x74, 0x37,
    0x38, 0x6c, 0x2f, 0x7a, 0x7a, 0x2e, 0x69, 0x63, 0x75, 0x00, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
    0x20, 0x00, 0xda, 0x27, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x43, 0x76, 0x41, 0x6c,
    0x03, 0x00, 0x01, 0x00, 0x01, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0xad, 0xde,
    0x02, 0x00, 0x0a, 0x00, 0x0d, 0x00, 0x02, 0x00, 0x0a, 0x00, 0x0d, 0x00, 0x02, 0x00, 0x0a, 0x00,
    0x0d, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x41, 0x4c, 0x4c, 0x00, 0x4d, 0x49, 0x4d, 0x45,
    0x00, 0x00, 0x49, 0x41, 0x4e, 0x41, 0x00, 0x00, 0x00, 0x00, 0x55, 0x54, 0x46, 0x2d, 0x38, 0x00,
    0x75, 0x74, 0x66, 0x38, 0x00, 0x00, 0x00, 0x00, 0x61, 0x6c, 0x6c, 0x00, 0x6d, 0x69, 0x6d, 0x65,
    0x00, 0x00, 0x69, 0x61, 0x6e, 0x61, 0x00, 0x00, 0x00, 0x00, 0x75, 0x74, 0x66, 0x38, 0x00, 0x00,
    0x75, 0x74, 0x66, 0x38, 0x00, 0x00, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
    0x20, 0x00, 0xda, 0x27, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x43, 0x76, 0x41, 0x6c,
    0x03, 0x00, 0x01, 0x00, 0x01, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0xad, 0xde,
    0x02, 0x00, 0x0a, 0x00, 0x0d, 0x00, 0x02, 0x00, 0x0a, 0x00, 0x0d, 0x00, 0x02, 0x00, 0x0a, 0x00,
    0x0d, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x41, 0x4c, 0x4c, 0x00, 0x4d, 0x49, 0x4d, 0x45,
    0x00, 0x00, 0x49, 0x41, 0x4e, 0x41, 0x00, 0x00, 0x00, 0x00, 0x55, 0x54, 0x46, 0x2d, 0x38, 0x00,
    0x75, 0x74, 0x66, 0x38, 0x00, 0x00, 0x00, 0x00, 0x61, 0x6c, 0x6c, 0x00, 0x6d, 0x69, 0x6d, 0x65,
    0x00, 0x00, 0x69, 0x61, 0x6e, 0x61, 0x00, 0x00, 0x00, 0x00, 0x75, 0x74, 0x66, 0x38, 0x00, 0x00,
    0x75, 0x74, 0x66, 0x38, 0x00, 0x00, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
};

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "%s:%u\n", location.file_name(), location.line());
        std::abort();
    }
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(data != nullptr);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != MAP_FAILED);
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size);
#endif
    }

    std::int32_t* Status(std::int32_t value) {
        std::memcpy(data, &value, sizeof(value));
        return static_cast<std::int32_t*>(data);
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(data, size, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(data, size, PROT_READ) == 0);
#endif
    }

private:
    void* data = nullptr;
    std::size_t size = 0;
};

void Prefailed() {
    for (std::int32_t initial : {1, 4, 7, 15, 16, INT32_MAX}) {
        std::int32_t status = initial;
        Require(ucnv_openStandardNames_nid_postfix(reinterpret_cast<const char*>(1),
                                                   reinterpret_cast<const char*>(1), &status) == nullptr);
        Require(status == initial);
    }
}

void NullStatus() {
    try {
        ucnv_openStandardNames_nid_postfix("UTF-8", "MIME", nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    std::abort();
}

void ReadOnlyPrefailed() {
    ReadOnlyPage page;
    std::int32_t* status = page.Status(U_ILLEGAL_ARGUMENT_ERROR);
    page.Protect();
    Require(ucnv_openStandardNames_nid_postfix(reinterpret_cast<const char*>(1),
                                               reinterpret_cast<const char*>(1), status) == nullptr);
    Require(*status == U_ILLEGAL_ARGUMENT_ERROR);
}

void BundledData() {
    std::int32_t status = 0;
    bool threw = false;
    try {
        ucnv_openStandardNames_nid_postfix(reinterpret_cast<const char*>(1), reinterpret_cast<const char*>(1), &status);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(status == 0);
    status = 0;
    Require(ucnv_openStandardNames_nid_postfix(nullptr, nullptr, &status) == nullptr);
    Require(status == U_ILLEGAL_ARGUMENT_ERROR);
}

void ReducedData() {
    std::int32_t status = 0;
    Require(ucnv_openStandardNames_nid_postfix(reinterpret_cast<const char*>(1),
                                               reinterpret_cast<const char*>(1), &status) == nullptr);
    Require(status == U_FILE_ACCESS_ERROR);
    status = 0;
    Require(ucnv_openStandardNames_nid_postfix(nullptr, nullptr, &status) == nullptr);
    Require(status == U_FILE_ACCESS_ERROR);
}

std::string NextGuest(UEnumeration* enumeration) {
    std::int32_t status = 0;
    std::int32_t length = -1;
    const char* value = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(status == 0);
    return value ? std::string(value, static_cast<std::size_t>(length)) : std::string();
}

std::string NextHost(UEnumeration* enumeration) {
    UErrorCode status = U_ZERO_ERROR;
    std::int32_t length = -1;
    const char* value = uenum_next(enumeration, &length, &status);
    Require(U_SUCCESS(status));
    return value ? std::string(value, static_cast<std::size_t>(length)) : std::string();
}

void InstallAliasData() {
    UErrorCode dataStatus = U_ZERO_ERROR;
    udata_setCommonData(AliasData, &dataStatus);
    Require(U_SUCCESS(dataStatus));
    std::int32_t guestDataStatus = 0;
    const auto guestStandards = SetupOpenStandardNamesData(AliasData, &guestDataStatus);
    Require(guestDataStatus == 0);
    Require(guestStandards > 0);
}

void FullData() {
    std::int32_t guestStatus = 0;
    UEnumeration* guest = ucnv_openStandardNames_nid_postfix("UTF-8", "MIME", &guestStatus);
    Require(guest != nullptr);
    Require(guestStatus == 0);
    UErrorCode hostStatus = U_ZERO_ERROR;
    UEnumeration* host = ucnv_openStandardNames("UTF-8", "MIME", &hostStatus);
    Require(host != nullptr);
    Require(U_SUCCESS(hostStatus));
    std::int32_t countStatus = 0;
    Require(uenum_count_nid_postfix(guest, &countStatus) == uenum_count(host, &hostStatus));
    Require(countStatus == 0);
    for (;;) {
        const std::string guestValue = NextGuest(guest);
        const std::string hostValue = NextHost(host);
        Require(guestValue == hostValue);
        if (guestValue.empty()) break;
    }
    countStatus = 0;
    guest->reset(guest, &countStatus);
    uenum_reset(host, &hostStatus);
    Require(countStatus == 0);
    Require(NextGuest(guest) == NextHost(host));
    countStatus = 0;
    guest->reset(guest, &countStatus);
    uenum_reset(host, &hostStatus);
    std::int32_t guestLength = -1;
    std::int32_t hostLength = -1;
    const char16_t* guestUnicode = uenum_unext_nid_postfix(guest, &guestLength, &countStatus);
    const UChar* hostUnicode = uenum_unext(host, &hostLength, &hostStatus);
    Require(countStatus == 0);
    Require(U_SUCCESS(hostStatus));
    Require(guestLength == hostLength);
    Require(std::char_traits<char16_t>::compare(guestUnicode, hostUnicode, static_cast<std::size_t>(guestLength)) == 0);
    uenum_close_nid_postfix(guest);
    uenum_close(host);

    guestStatus = 0;
    Require(ucnv_openStandardNames_nid_postfix("not-a-converter", "MIME", &guestStatus) == nullptr);
    Require(guestStatus == 0);
    guestStatus = 0;
    Require(ucnv_openStandardNames_nid_postfix(nullptr, "MIME", &guestStatus) == nullptr);
    Require(guestStatus == U_ILLEGAL_ARGUMENT_ERROR);
    guestStatus = 0;
    Require(ucnv_openStandardNames_nid_postfix("UTF-8", nullptr, &guestStatus) == nullptr);
    Require(guestStatus == U_ILLEGAL_ARGUMENT_ERROR);

    guestStatus = 0;
    bool threw = false;
    try {
        ucnv_openStandardNames_nid_postfix(reinterpret_cast<const char*>(1), "MIME", &guestStatus);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(guestStatus == 0);

    ReadOnlyPage page;
    std::int32_t* readOnlyStatus = page.Status(0);
    page.Protect();
    guest = ucnv_openStandardNames_nid_postfix("UTF-8", "MIME", readOnlyStatus);
    Require(guest != nullptr);
    Require(*readOnlyStatus == 0);
    uenum_close_nid_postfix(guest);

    ReadOnlyPage warningPage;
    std::int32_t* warningStatus = warningPage.Status(0);
    warningPage.Protect();
    SetOpenStandardNamesStatusForTest_nid_no_patch(U_USING_FALLBACK_WARNING);
    threw = false;
    try {
        ucnv_openStandardNames_nid_postfix("UTF-8", "MIME", warningStatus);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(*warningStatus == 0);
}

}

int main(int argc, char** argv) {
    Prefailed();
    NullStatus();
    ReadOnlyPrefailed();
    if (argc == 1) {
        ReducedData();
        return 0;
    }
    if (std::strcmp(argv[1], "full") == 0) {
        InstallAliasData();
    } else {
        Require(std::strcmp(argv[1], "packaged") == 0);
        BundledData();
        Require(ucnv_countStandards() > 0);
    }
    FullData();
}
