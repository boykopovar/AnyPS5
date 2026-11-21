#include "prx/libSceIcu/libSceIcu.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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
    std::fprintf(stderr, "ICU string comparison check failed at line %u\n", location.line());
    std::abort();
}

std::int32_t Compare(const std::u16string& first, std::int32_t firstLength, const std::u16string& second, std::int32_t secondLength, bool codePointOrder) {
    return u_strCompare_nid_postfix(first.c_str(), firstLength, second.c_str(), secondLength, codePointOrder);
}

std::int32_t Terminated(const std::u16string& first, const std::u16string& second, bool codePointOrder) {
    return Compare(first, -1, second, -1, codePointOrder);
}

std::int32_t Counted(const std::u16string& first, const std::u16string& second, bool codePointOrder) {
    return Compare(first, static_cast<std::int32_t>(first.size()), second, static_cast<std::int32_t>(second.size()), codePointOrder);
}

void Plain() {
    for (bool order : {false, true}) {
        Require(Terminated(u"abc", u"abc", order) == 0);
        Require(Counted(u"abc", u"abc", order) == 0);
        Require(Terminated(u"abc", u"abd", order) == -1);
        Require(Terminated(u"abd", u"abc", order) == 1);
        Require(Terminated(u"ab", u"abc", order) == -u'c');
        Require(Terminated(u"abc", u"ab", order) == u'c');
        Require(Terminated(u"", u"", order) == 0);
        Require(Terminated(u"a", u"", order) == u'a');
        Require(Counted(u"ab", u"abc", order) == -1);
        Require(Counted(u"abc", u"ab", order) == 1);
        Require(Counted(u"", u"a", order) == -1);
        Require(Counted(u"x", u"abc", order) == u'x' - u'a');
        Require(Compare(u"ab", -1, u"abc", 3, order) == -1);
        Require(Compare(u"abc", 3, u"ab", -1, order) == 1);
        Require(Compare(u"abc", 2, u"abd", -1, order) == -1);
        Require(Compare(u"abc", 2, u"ab", -1, order) == 0);
        Require(Compare(u"abcd", 3, u"abce", 3, order) == 0);
        Require(Compare(u"abcd", 4, u"abce", 3, order) == 1);
        const std::u16string first({0x41, 0, 0x42});
        const std::u16string second({0x41, 0, 0x43});
        Require(Compare(first, 3, second, 3, order) == -1);
        Require(Compare(first, 1, second, 1, order) == 0);
        Require(Terminated(first, second, order) == 0);
        Require(u_strCompare_nid_postfix(first.c_str(), -1, first.c_str(), -1, order) == 0);
        Require(u_strCompare_nid_postfix(first.c_str(), 1, first.c_str(), 3, order) == -1);
        Require(u_strCompare_nid_postfix(first.c_str(), 3, first.c_str(), 1, order) == 1);
        Require(u_strCompare_nid_postfix(first.c_str(), 3, first.c_str(), 3, order) == 0);
    }
}

void Surrogates() {
    const std::u16string bmp({0xe000});
    const std::u16string supplementary({0xd800, 0xdc00});
    for (bool terminated : {false, true}) {
        const auto compare = [&](const std::u16string& first, const std::u16string& second, bool order) {
            return terminated ? Terminated(first, second, order) : Counted(first, second, order);
        };
        Require(compare(bmp, supplementary, false) == 0xe000 - 0xd800);
        Require(compare(bmp, supplementary, true) == 0xe000 - 0x2800 - 0xd800);
        Require(compare(supplementary, bmp, true) == 0xd800 - (0xe000 - 0x2800));

        const std::u16string loneLead({0xd900});
        Require(compare(loneLead, supplementary, false) == 0xd900 - 0xd800);
        Require(compare(loneLead, supplementary, true) == 0xd900 - 0x2800 - 0xd800);

        const std::u16string loneTrail({0xdc00});
        Require(compare(loneTrail, supplementary, false) == 0xdc00 - 0xd800);
        Require(compare(loneTrail, supplementary, true) == 0xdc00 - 0x2800 - 0xd800);

        Require(compare(std::u16string({0xd800, 0xdc00}), std::u16string({0xd800, 0xdc01}), true) == -1);
        Require(compare(std::u16string({0xd800, 0xdc00}), std::u16string({0xd801, 0xdc00}), true) == -1);
        Require(compare(std::u16string({0xd800, 0x41}), std::u16string({0xd800, 0xdc00}), true) == 0x41 - 0xdc00);
        Require(compare(std::u16string({0xdc00, 0xdc00}), std::u16string({0xdc00, 0xe000}), true) == 0xdc00 - 0x2800 - (0xe000 - 0x2800));
        Require(compare(std::u16string({0xd800, 0xdc00}), std::u16string({0xdbff, 0xdfff}), true) == 0xd800 - 0xdbff);
        Require(compare(std::u16string({0x41, 0xd7ff}), std::u16string({0x41, 0xe000}), true) == 0xd7ff - 0xe000);
        Require(compare(std::u16string({0xe000}), std::u16string({0xffff}), true) == 0xe000 - 0xffff);
        Require(compare(std::u16string({0xd800}), std::u16string({0xdfff}), true) == 0xd800 - 0xdfff);
        Require(compare(std::u16string({0xdfff}), std::u16string({0xd800}), false) == 0xdfff - 0xd800);
    }
    Require(Counted(std::u16string({0xd800, 0xdc00}), std::u16string({0xd800}), true) == 1);
    Require(Counted(std::u16string({0xd800}), std::u16string({0xd800, 0xdc00}), true) == -1);
    Require(Compare(std::u16string({0xd900, 0xdc00}), 1, std::u16string({0xd800, 0xdc00}), 2, true) == 0xd900 - 0x2800 - 0xd800);
    Require(Compare(std::u16string({0xd900, 0xdc00}), 2, std::u16string({0xd800, 0xdc00}), 2, true) == 0xd900 - 0xd800);
}

void Arguments() {
    const std::u16string text(u"a");
    Require(u_strCompare_nid_postfix(nullptr, 1, text.c_str(), 1, 0) == 0);
    Require(u_strCompare_nid_postfix(text.c_str(), 1, nullptr, 1, 0) == 0);
    Require(u_strCompare_nid_postfix(nullptr, 0, nullptr, 0, 0) == 0);
    Require(u_strCompare_nid_postfix(text.c_str(), -2, text.c_str(), 1, 0) == 0);
    Require(u_strCompare_nid_postfix(text.c_str(), 1, text.c_str(), std::numeric_limits<std::int32_t>::min(), 0) == 0);
    Require(u_strCompare_nid_postfix(text.c_str(), -2, u"b", 1, 1) == 0);
}

template<typename TCall>
void RequireThrows(TCall call, std::source_location location = std::source_location::current()) {
    errno = 71;
    try {
        call();
    } catch (const std::invalid_argument&) {
        Require(errno == 71, location);
        return;
    }
    Require(false, location);
}

char16_t* GuardedPage(std::size_t& pageSize) {
#ifdef _WIN32
    SYSTEM_INFO info {};
    GetSystemInfo(&info);
    pageSize = info.dwPageSize;
    auto* base = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(base != nullptr);
    DWORD previous {};
    Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
    pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* base = static_cast<char*>(mapped);
    Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
#endif
    return reinterpret_cast<char16_t*>(base + pageSize);
}

void ReleasePage(char16_t* end, std::size_t pageSize) {
    auto* base = reinterpret_cast<char*>(end) - pageSize;
#ifdef _WIN32
    VirtualFree(base, 0, MEM_RELEASE);
#else
    munmap(base, pageSize * 2);
#endif
}

void GuestStrings() {
    const auto* unmapped = reinterpret_cast<const char16_t*>(std::uintptr_t {0x10});
    const char16_t text[] = u"abc";
    RequireThrows([&] { u_strCompare_nid_postfix(unmapped, -1, text, -1, 0); });
    RequireThrows([&] { u_strCompare_nid_postfix(text, -1, unmapped, -1, 1); });
    RequireThrows([&] { u_strCompare_nid_postfix(unmapped, 3, text, 3, 0); });
    RequireThrows([&] { u_strCompare_nid_postfix(text, 3, unmapped, -1, 1); });
    errno = 71;
    Require(u_strCompare_nid_postfix(unmapped, 0, text, 0, 0) == 0 && errno == 71);
    Require(u_strCompare_nid_postfix(unmapped, -1, unmapped, -1, 1) == 0 && errno == 71);

    std::size_t pageSize = 0;
    auto* end = GuardedPage(pageSize);
    auto* tail = end - 2;
    tail[0] = u'a';
    tail[1] = 0xd800;
    constexpr auto huge = std::numeric_limits<std::int32_t>::max();
    const char16_t other[] = u"ac";
    const char16_t longer[] = {u'a', 0xd800, u'c', 0};
    const char16_t pair[] = {u'a', 0xd800, 0xdc00, 0};
    Require(u_strCompare_nid_postfix(tail, huge, other, huge, 0) == 0xd800 - u'c');
    Require(u_strCompare_nid_postfix(tail, 2, pair, 3, 1) == -1);
    Require(u_strCompare_nid_postfix(tail, 2, pair, 2, 1) == 0);
    RequireThrows([&] { u_strCompare_nid_postfix(tail, huge, longer, huge, 0); });
    RequireThrows([&] { u_strCompare_nid_postfix(tail, -1, longer, -1, 0); });
    RequireThrows([&] { u_strCompare_nid_postfix(tail, -1, longer, 3, 0); });
    RequireThrows([&] { u_strCompare_nid_postfix(tail, huge, pair, huge, 1); });
    ReleasePage(end, pageSize);
}

}

int main() {
    Plain();
    Surrogates();
    Arguments();
    GuestStrings();
    std::puts("ICU string comparison tests passed");
}
