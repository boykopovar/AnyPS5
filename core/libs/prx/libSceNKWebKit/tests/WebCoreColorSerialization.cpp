#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore color serialization check failed at line %d\n", line);
        std::abort();
    }
}
#define REQUIRE(value) Require((value), __LINE__)

std::string Text(const NKWebKit::WtfAbi::String& value) {
    REQUIRE(value.impl != nullptr);
    REQUIRE((value.impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(value.impl->data), value.impl->length);
}

WebCore::Color Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
    constexpr std::uint64_t colorSpaceSrgb = 17ULL << 56;
    constexpr std::uint64_t valid = 1ULL << 50;
    return WebCore::Color{colorSpaceSrgb | valid | (static_cast<std::uint64_t>(red) << 24)
                          | (static_cast<std::uint64_t>(green) << 16)
                          | (static_cast<std::uint64_t>(blue) << 8) | alpha};
}

} // namespace

int main() {
    const auto opaque = Color(12, 34, 56, 255);
    REQUIRE(Text(_ZN7WebCore20serializationForHTMLERKNS_5ColorE(&opaque)) == "#0c2238");

    const auto transparent = Color(12, 34, 56, 0);
    REQUIRE(Text(_ZN7WebCore20serializationForHTMLERKNS_5ColorE(&transparent)) == "rgba(12, 34, 56, 0)");

    const auto translucent = Color(12, 34, 56, 128);
    REQUIRE(Text(_ZN7WebCore20serializationForHTMLERKNS_5ColorE(&translucent)) == "rgba(12, 34, 56, 0.5)");

    const WebCore::Color invalid{0};
    REQUIRE(Text(_ZN7WebCore20serializationForHTMLERKNS_5ColorE(&invalid)) == "rgba(0, 0, 0, 0)");

    {
        struct GuardedPages {
            unsigned char* base = nullptr;
            std::size_t pageSize = 0;

            GuardedPages() {
#ifdef _WIN32
                SYSTEM_INFO info{};
                GetSystemInfo(&info);
                pageSize = info.dwPageSize;
                base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
                REQUIRE(base != nullptr);
                DWORD previous = 0;
                REQUIRE(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
                const long size = sysconf(_SC_PAGESIZE);
                REQUIRE(size > 0);
                pageSize = static_cast<std::size_t>(size);
                void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                REQUIRE(mapped != MAP_FAILED);
                base = static_cast<unsigned char*>(mapped);
                REQUIRE(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
            }

            ~GuardedPages() {
                if (!base) return;
#ifdef _WIN32
                VirtualFree(base, 0, MEM_RELEASE);
#else
                munmap(base, pageSize * 3);
#endif
            }

            void ProtectReadOnly() {
#ifdef _WIN32
                DWORD previous = 0;
                REQUIRE(VirtualProtect(base, pageSize, PAGE_READONLY, &previous) != 0);
#else
                REQUIRE(mprotect(base, pageSize, PROT_READ) == 0);
#endif
            }

            unsigned char* Readable() const { return base; }
            unsigned char* Guard() const { return base + pageSize * 2; }
        };

        auto requireNotImplemented = [](auto&& fn) {
            const auto before = NotImplementedCalls_nid_no_patch();
            bool threw = false;
            try {
                fn();
            } catch (const std::runtime_error&) {
                threw = true;
            }
            REQUIRE(threw && NotImplementedCalls_nid_no_patch() == before + 1);
        };

        bool threwInvalid = false;
        try {
            _ZN7WebCore20serializationForHTMLERKNS_5ColorE(nullptr);
        } catch (const std::invalid_argument&) {
            threwInvalid = true;
        }
        REQUIRE(threwInvalid);

        GuardedPages pages;
        std::memcpy(pages.Readable(), &opaque, sizeof(opaque));
        pages.ProtectReadOnly();
        REQUIRE(Text(_ZN7WebCore20serializationForHTMLERKNS_5ColorE(reinterpret_cast<const WebCore::Color*>(pages.Readable()))) == "#0c2238");

        requireNotImplemented([&] {
            _ZN7WebCore20serializationForHTMLERKNS_5ColorE(reinterpret_cast<const WebCore::Color*>(pages.Guard()));
        });
        requireNotImplemented([&] {
            _ZN7WebCore20serializationForHTMLERKNS_5ColorE(reinterpret_cast<const WebCore::Color*>(pages.Guard() - 4));
        });
    }
    return 0;
}
