#include "prx/libSceIcu/CaseMapping.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libSceIcu/JavaUtf8.hpp"
#include "prx/libSceIcu/Utf32.hpp"
#include "prx/libSceIcu/UText.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "unicode/ustring.h"
#undef utext_setup
#undef utext_openUChars
#undef utext_close
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU guest text check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void Reject(TAction action) {
    errno = 73;
    try {
        action();
    } catch (const std::invalid_argument&) {
        Require(errno == 73);
        return;
    }
    Require(false);
}

class Pages {
public:
    Pages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        data = static_cast<std::byte*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(data + pageSize, pageSize, PAGE_NOACCESS, &previous));
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        data = static_cast<std::byte*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(data != MAP_FAILED);
        Require(mprotect(data + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~Pages() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, pageSize * 2);
#endif
    }

    template<typename TValue>
    TValue* End(std::size_t count = 0) {
        return reinterpret_cast<TValue*>(data + pageSize) - count;
    }

    void ReadOnly(bool enabled) {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(data, pageSize, enabled ? PAGE_READONLY : PAGE_READWRITE, &previous));
#else
        Require(mprotect(data, pageSize, enabled ? PROT_READ : PROT_READ | PROT_WRITE) == 0);
#endif
    }

private:
    std::byte* data = nullptr;
    std::size_t pageSize = 0;
};

void ConversionBuffers() {
    Pages input;
    Pages output;
    Pages statusPage;
    auto* status = statusPage.End<std::int32_t>(1);
    *status = 0;
    statusPage.ReadOnly(true);
    const auto maximum = std::numeric_limits<std::int32_t>::max();
    auto* utf32 = input.End<std::int32_t>(2);
    utf32[0] = 0x1f642;
    utf32[1] = 0;
    auto* utf16 = output.End<char16_t>(3);
    std::int32_t length = -1;
    errno = 73;
    Require(u_strFromUTF32_nid_postfix(utf16, maximum, &length, utf32, -1, status) == utf16);
    Require(errno == 73 && length == 2 && utf16[0] == 0xd83d && utf16[1] == 0xde42 && utf16[2] == 0);

    auto* narrow = output.End<char>(4);
    auto* units = input.End<char16_t>(2);
    units[0] = 0x20ac;
    units[1] = 0;
    errno = 73;
    Require(u_strToJavaModifiedUTF8_nid_postfix(narrow, maximum, &length, units, -1, status) == narrow);
    Require(errno == 73 && length == 3 && std::memcmp(narrow, "\xe2\x82\xac", 4) == 0);

    auto* bytes = input.End<char>(4);
    std::memcpy(bytes, "\xe2\x82\xac", 4);
    utf16 = output.End<char16_t>(2);
    std::int32_t substitutions = -1;
    Require(u_strFromJavaModifiedUTF8WithSub_nid_postfix(utf16, maximum, &length, bytes, -1, -1, &substitutions, status) == utf16);
    Require(length == 1 && substitutions == 0 && utf16[0] == 0x20ac && utf16[1] == 0);

    units = input.End<char16_t>(2);
    units[0] = 0xd83d;
    units[1] = 0xde42;
    auto* points = output.End<std::int32_t>(2);
    Require(u_strToUTF32WithSub_nid_postfix(points, maximum, &length, units, 2, -1, &substitutions, status) == points);
    Require(length == 1 && substitutions == 0 && points[0] == 0x1f642 && points[1] == 0);

    std::array<char16_t, 4> buffer{};
    std::int32_t error = 0;
    Reject([&] { u_strFromUTF32_nid_postfix(buffer.data(), 4, &length, input.End<std::int32_t>(), 1, &error); });
    Reject([&] { u_strToUTF32_nid_postfix(points, 2, &length, input.End<char16_t>(), 1, &error); });
    Reject([&] { u_strFromJavaModifiedUTF8WithSub_nid_postfix(buffer.data(), 4, &length, input.End<char>(), 1, -1, nullptr, &error); });
    Reject([&] { u_strToJavaModifiedUTF8_nid_postfix(narrow, 4, &length, input.End<char16_t>(), 1, &error); });
    Reject([&] { u_strFromUTF32_nid_postfix(buffer.data(), 4, &length, utf32, 0, statusPage.End<std::int32_t>()); });
    Reject([&] { u_strToJavaModifiedUTF8_nid_postfix(narrow, 4, output.End<std::int32_t>(), u"A", 1, &error); });
    Reject([&] { u_strFromJavaModifiedUTF8WithSub_nid_postfix(buffer.data(), 4, &length, "A", 1, -1, output.End<std::int32_t>(), &error); });

    output.ReadOnly(true);
    Reject([&] { u_strFromUTF32_nid_postfix(utf16, 2, &length, nullptr, 0, &error); });
    Reject([&] { u_strToUTF32_nid_postfix(points, 2, &length, u"A", 1, &error); });
    Reject([&] { u_strToJavaModifiedUTF8_nid_postfix(narrow, 4, &length, u"A", 1, &error); });
    Reject([&] { u_strFromJavaModifiedUTF8WithSub_nid_postfix(utf16, 2, &length, "A", 1, -1, nullptr, &error); });

    statusPage.ReadOnly(false);
    *status = 15;
    statusPage.ReadOnly(true);
    auto* absent32 = input.End<std::int32_t>();
    auto* absent16 = input.End<char16_t>();
    auto* absent8 = input.End<char>();
    Require(!u_strFromUTF32WithSub_nid_postfix(absent16, maximum, absent32, absent32, maximum, 0, absent32, status));
    Require(!u_strToUTF32WithSub_nid_postfix(absent32, maximum, absent32, absent16, maximum, 0, absent32, status));
    Require(!u_strFromJavaModifiedUTF8WithSub_nid_postfix(absent16, maximum, absent32, absent8, maximum, 0, absent32, status));
    Require(!u_strToJavaModifiedUTF8_nid_postfix(absent8, maximum, absent32, absent16, maximum, status));
    Require(*status == 15);

    output.ReadOnly(false);
    auto* invalid = input.End<std::int32_t>(1);
    *invalid = 0xd800;
    error = 0;
    length = -77;
    Require(!u_strFromUTF32WithSub_nid_postfix(buffer.data(), 4, &length, invalid, 2, -1, &substitutions, &error));
    Require(error == 10 && length == -77 && substitutions == 0);
    auto* invalidByte = input.End<unsigned char>(1);
    *invalidByte = 0xff;
    error = 0;
    Require(!u_strFromJavaModifiedUTF8WithSub_nid_postfix(buffer.data(), 4, &length,
        reinterpret_cast<char*>(invalidByte), 2, -1, &substitutions, &error));
    Require(error == 10 && length == -77 && substitutions == 0);
}

void CaseBuffers() {
    Pages input;
    Pages output;
    Pages statusPage;
    auto* status = statusPage.End<std::int32_t>(1);
    *status = 0;
    statusPage.ReadOnly(true);
    auto* text = input.End<char16_t>(2);
    text[0] = u'\u00df';
    text[1] = 0;
    input.ReadOnly(true);
    auto* result = output.End<char16_t>(3);
    errno = 73;
    Require(u_strToUpper_nid_postfix(result, std::numeric_limits<std::int32_t>::max(), text, -1, "", status) == 2);
    Require(errno == 73 && result[0] == u'S' && result[1] == u'S' && result[2] == 0);
    Require(u_strToLower_nid_postfix(result, 3, u"\u0130", 1, "", status) == 2);
    Require(result[0] == u'i' && result[1] == 0x0307 && result[2] == 0);
    Require(u_strFoldCase_nid_postfix(result, 3, text, -1, 0, status) == 2);
    Require(result[0] == u's' && result[1] == u's' && result[2] == 0);

    std::int32_t error = 0;
    Reject([&] { u_strToUpper_nid_postfix(result, 3, input.End<char16_t>(), 1, "", &error); });
    Reject([&] { u_strToLower_nid_postfix(result, 3, text, -1, input.End<char>(), &error); });
    Reject([&] { u_strFoldCase_nid_postfix(result, 3, text, -1, 0, input.End<std::int32_t>()); });
    output.ReadOnly(true);
    Reject([&] { u_strFoldCase_nid_postfix(result, 3, text, -1, 0, &error); });
    error = 0;
    Require(u_strToUpper_nid_postfix(text, 1, text, 1, "", &error) == 2);
    Require(error == 15 && text[0] == u'\u00df' && text[1] == 0);

    error = 15;
    auto* absent = input.End<char16_t>();
    Require(u_strToUpper_nid_postfix(absent, 3, absent, -1, input.End<char>(), &error) == 0);
    Require(u_strToLower_nid_postfix(absent, 3, absent, -1, input.End<char>(), &error) == 0);
    Require(u_strFoldCase_nid_postfix(absent, 3, absent, -1, 0, &error) == 0);
    Require(error == 15);

    Require(u_strcasecmp_nid_postfix(text, u"SS", 0) == 0);
    Require(u_strncasecmp_nid_postfix(text, u"SS", std::numeric_limits<std::int32_t>::max(), 0) == 0);
    Require(u_memcasecmp_nid_postfix(input.End<char16_t>(), input.End<char16_t>(), 0, 0) == 0);
    Require(u_strCaseCompare_nid_postfix(text, -1, u"SS", 2, 0, status) == 0);
    Require(u_strCaseCompare_nid_postfix(absent, 2, absent, 2, 0, nullptr) == 0);
    Reject([&] { u_strCaseCompare_nid_postfix(absent, 1, text, 1, 0, status); });
    Reject([&] { u_strcasecmp_nid_postfix(absent, text, 0); });
    Reject([&] { u_strncasecmp_nid_postfix(text, absent, 1, 0); });
    Reject([&] { u_memcasecmp_nid_postfix(absent, text, 1, 0); });

    std::int32_t first = -77;
    std::int32_t second = -88;
    u_caseInsensitivePrefixMatch_nid_postfix(u"Fust", -1, u"Fu\u00dfball", -1, 0, &first, &second, status);
    Require(first == 2 && second == 2);
    first = -77;
    Reject([&] { u_caseInsensitivePrefixMatch_nid_postfix(u"abc", 3, u"ABC", 3, 0, &first, output.End<std::int32_t>(1), status); });
    Require(first == -77);
    u_caseInsensitivePrefixMatch_nid_postfix(u"abc", 3, u"ABC", 3, 0, nullptr, output.End<std::int32_t>(), status);
}

void CaseDifferential() {
    const std::array<std::u16string, 12> inputs{{u"", u"abc", u"ABC", u"Stra\u00dfe", u"\u0130I\u0131i",
        u"\u039f\u03a3", u"\ufb03", u"a\u0301b", u"\U00010400\U00010428", u"unchanged run", u"\ud7ff\ue000",
        std::u16string{u'a', 0, u'B', 0xd800, 0xdc00, 0xdfff}}};
    for (const auto& input : inputs) {
        for (const auto options : {0u, 1u, 0x4000u}) {
            for (std::int32_t capacity = 0; capacity <= 16; ++capacity) {
                for (const auto initial : {0, -124, -127}) {
                    for (const auto offset : {0, 1, 20}) {
                        std::array<char16_t, 64> expected;
                        expected.fill(0x5a5a);
                        std::copy(input.begin(), input.end(), expected.begin());
                        expected[input.size()] = 0;
                        auto actual = expected;
                        auto nativeStatus = static_cast<UErrorCode>(initial);
                        std::int32_t guestStatus = initial;
                        const auto length = static_cast<std::int32_t>(input.size());
                        const auto nativeLength = u_strFoldCase(expected.data() + offset, capacity, expected.data(), length, options, &nativeStatus);
                        errno = 73;
                        const auto guestLength = u_strFoldCase_nid_postfix(actual.data() + offset, capacity, actual.data(), length, options, &guestStatus);
                        Require(errno == 73);
                        Require(nativeLength == guestLength && nativeStatus == guestStatus && expected == actual);
                    }
                }
            }
        }
    }
}

void ConversionDifferential() {
    const std::array<std::int32_t, 12> values{0, 0x41, 0xd7ff, 0xd800, 0xdfff, 0xe000, 0xffff, 0x10000, 0x10ffff, 0x110000, -1, 0x1f642};
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::array<std::int32_t, 4> input{0x41, values[index], values[(index + 5) % values.size()], 0x42};
        for (const auto substitution : {-1, 0xfffd, 0x1f642}) {
            for (std::int32_t capacity = 0; capacity <= 9; ++capacity) {
                std::array<char16_t, 12> expected;
                expected.fill(0x5a5a);
                auto actual = expected;
                UErrorCode nativeStatus = U_ZERO_ERROR;
                std::int32_t guestStatus = 0;
                std::int32_t nativeLength = -77, guestLength = -77;
                std::int32_t nativeCount = -88, guestCount = -88;
                const auto* nativeResult = u_strFromUTF32WithSub(expected.data(), capacity, &nativeLength, input.data(), 4, substitution, &nativeCount, &nativeStatus);
                const auto* guestResult = u_strFromUTF32WithSub_nid_postfix(actual.data(), capacity, &guestLength, input.data(), 4, substitution, &guestCount, &guestStatus);
                Require((nativeResult == nullptr) == (guestResult == nullptr));
                Require(nativeStatus == guestStatus && nativeLength == guestLength && nativeCount == guestCount && expected == actual);
            }
        }
    }
}

void WatchedOutput() {
    constexpr std::size_t bytes = 0x4000;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0);
    void* first = nullptr;
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&first, bytes, 3, 0, physical, 0) == 0);
    Require(sceKernelMapDirectMemory(&second, bytes, 3, 0, physical, 0) == 0);
    auto* output = static_cast<char16_t*>(first);
    {
        const GuestArena::HostWrite access(first, bytes);
        Require(access.Open());
        std::fill_n(output, bytes / sizeof(char16_t), 0x5a5a);
    }
#ifdef _WIN32
    const bool watched = GuestArena::GuestArenaWriteWatched_nid_postfix();
    std::array<void*, 16> pages{};
    std::size_t count = pages.size();
    if (watched) Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(first), bytes, pages.data(), &count, true));
#endif
    std::int32_t status = 0;
    Require(u_strToUpper_nid_postfix(output, 16, u"Stra\u00dfe", -1, "", &status) == 7);
    Require(status == 0 && std::memcmp(second, u"STRASSE", 8 * sizeof(char16_t)) == 0);
#ifdef _WIN32
    if (watched) {
        count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(first), bytes, pages.data(), &count, true));
        Require(count == 1 && pages[0] == first);
    }
#endif
    const std::int32_t source[] = {0x1f642};
    std::int32_t length = 0;
    Require(u_strFromUTF32_nid_postfix(output, 16, &length, source, 1, &status) == output);
    Require(status == 0 && length == 2 && static_cast<char16_t*>(second)[0] == 0xd83d && static_cast<char16_t*>(second)[1] == 0xde42);
#ifdef _WIN32
    if (watched) {
        count = pages.size();
        Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(first), bytes, pages.data(), &count, true));
        Require(count == 1 && pages[0] == first);
    }
#endif
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(second), bytes) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(first), bytes) == 0);
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0);
}

void* APS5_VABI ProviderAllocate_nid_no_patch(const void*, std::size_t bytes) {
    return std::malloc(bytes);
}

void* APS5_VABI ProviderReallocate_nid_no_patch(const void*, void* pointer, std::size_t bytes) {
    return std::realloc(pointer, bytes);
}

void APS5_VABI ProviderFree_nid_no_patch(const void*, void* pointer) {
    std::free(pointer);
}

void ProviderBuffers() {
    Pages objects;
    Pages strings;
    auto* source = strings.End<char16_t>(3);
    source[0] = u'A';
    source[1] = 0x20ac;
    source[2] = 0;
    strings.ReadOnly(true);
    auto* text = objects.End<UText>(1);
    *text = {};
    text->magic = 0x345ad82c;
    text->sizeOfStruct = sizeof(UText);
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(nullptr, ProviderAllocate_nid_no_patch, ProviderReallocate_nid_no_patch, ProviderFree_nid_no_patch, &status);
    Require(status == 0);
    Require(utext_openUChars(text, source, -1, &status) == text && status == 0);
    Require(text->pFuncs->nativeLength(text) == 2);
    UText* clone = text->pFuncs->clone(nullptr, text, 1, &status);
    Require(clone && status == 0);
    Require(utext_close(text) == text);
    Require(clone->pFuncs->clone(text, clone, 1, &status) == text && status == 0);
    Require(utext_close(clone) == nullptr);
    objects.ReadOnly(true);
    Reject([&] { utext_close(text); });
    Require(text->chunkContents[0] == u'A' && text->chunkContents[1] == 0x20ac);
    objects.ReadOnly(false);
    Require(utext_close(text) == text);
    Reject([&] { utext_openUChars(objects.End<UText>(), u"A", 1, &status); });
    Reject([&] { utext_setup(nullptr, 0, objects.End<std::int32_t>()); });
    auto* invalid = objects.End<std::uint32_t>(1);
    *invalid = 0;
    Require(utext_close(reinterpret_cast<UText*>(invalid)) == reinterpret_cast<UText*>(invalid));
    u_cleanup_nid_postfix();
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string mode = argv[1];
    if (mode == "conversion") ConversionBuffers();
    else if (mode == "case") CaseBuffers();
    else if (mode == "differential") {
        CaseDifferential();
        ConversionDifferential();
    } else if (mode == "watch") WatchedOutput();
    else if (mode == "provider") ProviderBuffers();
    else Require(false);
    std::puts("ICU checked guest text tests passed");
}
