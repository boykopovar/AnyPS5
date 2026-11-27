#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GLContext extension check failed at line %u\n", location.line());
        std::abort();
    }
}

using Query = bool (APS5_VABI *)(const char*, const char*);
static_assert(std::is_same_v<decltype(&_ZN7WebCore9GLContext20isExtensionSupportedEPKcS2_), Query>);
constexpr Query IsSupported = &_ZN7WebCore9GLContext20isExtensionSupportedEPKcS2_;

struct GuardedPages {
    char* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 2) == 0);
#endif
    }

    char* Guard() const { return base + pageSize; }
};

void RequireThrows(const char* list, const char* extension) {
    bool threw = false;
    try {
        IsSupported(list, extension);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

void RequireThrowsNotImplemented(const char* list, const char* extension) {
    bool matched = false;
    try {
        IsSupported(list, extension);
    } catch (const std::runtime_error& error) {
        matched = std::string(error.what()) == "_ZN7WebCore9GLContext20isExtensionSupportedEPKcS2_ not implemented";
    }
    Require(matched);
}

}

int main() {
    try {
        Require(IsSupported("EXT_x", "EXT_x"));
        Require(IsSupported("EXT_a EXT_x EXT_z", "EXT_x"));
        Require(IsSupported("EXT_a EXT_x EXT_z", "EXT_a"));
        Require(IsSupported("EXT_a EXT_x EXT_z", "EXT_z"));
        Require(!IsSupported("EXT_xy", "EXT_x"));
        Require(!IsSupported("EXT_xy EXT_z", "EXT_x"));
        Require(IsSupported("EXT_xy EXT_x", "EXT_x"));
        Require(IsSupported("PREFIX_EXT_x", "EXT_x"));
        Require(!IsSupported("EXT_X", "EXT_x"));
        Require(!IsSupported("", "EXT_x"));
        Require(!IsSupported("EXT_a EXT_z", "EXT_x"));
        Require(!IsSupported("EXT_x\tEXT_z", "EXT_x"));
        Require(IsSupported("  EXT_x  EXT_z ", "EXT_x"));
        Require(IsSupported("EXT_a EXT_z", "EXT_a EXT_z"));
        Require(!IsSupported(nullptr, "EXT_x"));
        Require(!IsSupported(nullptr, nullptr));
        Require(!IsSupported(nullptr, ""));
        Require(IsSupported("", ""));
        Require(IsSupported(" ", ""));
        Require(IsSupported(" EXT_x", ""));
        RequireThrows("EXT_x", nullptr);
        RequireThrows("", nullptr);
        RequireThrows("EXT_x", "");
        RequireThrows("EXT_x ", "");
        RequireThrows("\tEXT_x", "");
        RequireThrows("EXT_x", reinterpret_cast<const char*>(1));
        RequireThrows("EXT_x EXT_z", reinterpret_cast<const char*>(0xdeadbeefull));

        GuardedPages pages;
        RequireThrowsNotImplemented(reinterpret_cast<const char*>(0x10), "EXT_x");
        RequireThrowsNotImplemented(reinterpret_cast<const char*>(0xdeadbeefull), "EXT_x");
        RequireThrowsNotImplemented(reinterpret_cast<const char*>(0x10), "");
        RequireThrowsNotImplemented(pages.Guard(), "EXT_x");
        RequireThrowsNotImplemented(pages.Guard(), "");

        char* unterminatedList = pages.Guard() - 5;
        std::memcpy(unterminatedList, "EXT_x", 5);
        RequireThrowsNotImplemented(unterminatedList, "EXT_x");
        RequireThrowsNotImplemented(unterminatedList, "");

        char* terminatedList = pages.Guard() - 6;
        std::memcpy(terminatedList, "EXT_x", 6);
        Require(IsSupported(terminatedList, "EXT_x"));
        Require(!IsSupported(terminatedList, "EXT_y"));

        std::string largeList;
        largeList.reserve(70000);
        for (int i = 0; i < 8750; ++i) {
            largeList.append("EXT_pad ");
        }
        largeList.append("EXT_tail");
        Require(IsSupported(largeList.c_str(), "EXT_tail"));
        Require(!IsSupported(largeList.c_str(), "EXT_missing"));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GLContext extension query unexpectedly threw: %s\n", error.what());
        return 1;
    }
}
