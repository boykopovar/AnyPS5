#include "prx/libSceIcu/libSceIcu.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    std::fprintf(stderr, "ICU substring search check failed at line %u\n", location.line());
    std::abort();
}

constexpr std::ptrdiff_t None = -1;

std::ptrdiff_t First(const std::u16string& text, const std::u16string& sub) {
    const auto* found = u_strstr_nid_postfix(text.c_str(), sub.c_str());
    return found ? found - text.c_str() : None;
}

std::ptrdiff_t Last(const std::u16string& text, const std::u16string& sub) {
    const auto* found = u_strrstr_nid_postfix(text.c_str(), sub.c_str());
    return found ? found - text.c_str() : None;
}

void Check(const std::u16string& text, const std::u16string& sub, std::ptrdiff_t first, std::ptrdiff_t last,
           std::source_location location = std::source_location::current()) {
    if (First(text, sub) == first && Last(text, sub) == last) return;
    std::fprintf(stderr, "ICU substring search check failed at line %u\n", location.line());
    std::abort();
}

void Plain() {
    Check(u"abcabc", u"bc", 1, 4);
    Check(u"abcabc", u"abc", 0, 3);
    Check(u"abcabc", u"c", 2, 5);
    Check(u"abcabc", u"abcabc", 0, 0);
    Check(u"abcabc", u"abcabcd", None, None);
    Check(u"abcabc", u"x", None, None);
    Check(u"abcabc", u"cb", None, None);
    Check(u"aaa", u"aa", 0, 1);
    Check(u"aaa", u"a", 0, 2);
    Check(u"ab", u"abc", None, None);
    Check(u"", u"a", None, None);
    Check(u"abc", u"", 0, 0);
    Check(u"", u"", 0, 0);
    Check(std::u16string({0x41, 0, 0x42}), u"B", None, None);
    Check(std::u16string({0x41, 0x42, 0x43}), std::u16string({0x42, 0x43}), 1, 1);

    const char16_t text[] = u"xyz";
    Require(u_strstr_nid_postfix(text, nullptr) == text);
    Require(u_strrstr_nid_postfix(text, nullptr) == text);
    Require(u_strstr_nid_postfix(nullptr, nullptr) == nullptr);
    Require(u_strrstr_nid_postfix(nullptr, nullptr) == nullptr);
    Require(u_strstr_nid_postfix(nullptr, text) == nullptr);
    Require(u_strrstr_nid_postfix(nullptr, text) == nullptr);
}

void Surrogates() {
    const std::u16string pair({0xd83d, 0xde00});
    Check(std::u16string({0x41, 0xd83d, 0xde00, 0x42}), pair, 1, 1);
    Check(std::u16string({0xd83d, 0xde00, 0xd83d, 0xde00}), pair, 0, 2);

    Check(std::u16string({0xd800, 0xdc00}), std::u16string({0xdc00}), None, None);
    Check(std::u16string({0xd800, 0xdc00}), std::u16string({0xd800}), None, None);
    Check(std::u16string({0xd800, 0xdc00, 0xdc00}), std::u16string({0xdc00}), 2, 2);
    Check(std::u16string({0xdc00, 0xdc00, 0xd800, 0xdc00}), std::u16string({0xdc00}), 0, 1);
    Check(std::u16string({0xd800, 0xdc00, 0xd800}), std::u16string({0xd800}), 2, 2);
    Check(std::u16string({0xd800, 0xd800, 0xdc00}), std::u16string({0xd800}), 0, 0);
    Check(std::u16string({0x41, 0xd800, 0x42}), std::u16string({0x41, 0xd800}), 0, 0);
    Check(std::u16string({0x41, 0xd800, 0xdc00}), std::u16string({0x41, 0xd800}), None, None);
    Check(std::u16string({0xd800, 0xdc00, 0x41}), std::u16string({0xdc00, 0x41}), None, None);
    Check(std::u16string({0xdc00, 0x41, 0xd800, 0xdc00, 0x41}), std::u16string({0xdc00, 0x41}), 0, 0);
    Check(std::u16string({0xd800, 0xdc00, 0x41, 0xdc00, 0x41}), std::u16string({0xdc00, 0x41}), 3, 3);
    Check(std::u16string({0xd800, 0xdc00, 0x41, 0xdc00, 0x41}), std::u16string({0xd800, 0xdc00, 0x41}), 0, 0);
    Check(std::u16string({0xdc00, 0xd800}), std::u16string({0xdc00}), 0, 0);
}

template <typename TCall>
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
    const char16_t text[] = u"abcabc";
    const char16_t sub[] = u"bc";
    RequireThrows([&] { u_strstr_nid_postfix(text, unmapped); });
    RequireThrows([&] { u_strrstr_nid_postfix(text, unmapped); });
    RequireThrows([&] { u_strstr_nid_postfix(unmapped, sub); });
    RequireThrows([&] { u_strrstr_nid_postfix(unmapped, sub); });
    const char16_t empty[] = u"";
    errno = 71;
    Require(u_strstr_nid_postfix(unmapped, empty) == unmapped && errno == 71);
    Require(u_strrstr_nid_postfix(unmapped, empty) == unmapped && errno == 71);
    Require(u_strstr_nid_postfix(unmapped, nullptr) == unmapped && errno == 71);
    Require(u_strrstr_nid_postfix(unmapped, nullptr) == unmapped && errno == 71);

    std::size_t textPageSize = 0;
    std::size_t subPageSize = 0;
    auto* textEnd = GuardedPage(textPageSize);
    auto* subEnd = GuardedPage(subPageSize);
    auto* guestText = textEnd - std::size(text);
    auto* guestSub = subEnd - std::size(sub);
    std::memcpy(guestText, text, sizeof(text));
    std::memcpy(guestSub, sub, sizeof(sub));
    errno = 71;
    Require(u_strstr_nid_postfix(guestText, guestSub) == guestText + 1 && errno == 71);
    Require(u_strrstr_nid_postfix(guestText, guestSub) == guestText + 4 && errno == 71);
    auto* odd = reinterpret_cast<char16_t*>(reinterpret_cast<char*>(textEnd) - sizeof(text) - 1);
    std::memcpy(odd, text, sizeof(text));
    Require(u_strstr_nid_postfix(odd, guestSub) == odd + 1 && u_strrstr_nid_postfix(odd, guestSub) == odd + 4 && errno == 71);
    guestText = textEnd - 2;
    guestText[0] = u'b';
    guestText[1] = u'c';
    RequireThrows([&] { u_strstr_nid_postfix(guestText, guestSub); });
    RequireThrows([&] { u_strrstr_nid_postfix(guestText, guestSub); });
    ReleasePage(subEnd, subPageSize);
    ReleasePage(textEnd, textPageSize);
}

}

int main() {
    Plain();
    Surrogates();
    GuestStrings();
    std::puts("ICU substring search tests passed");
}
