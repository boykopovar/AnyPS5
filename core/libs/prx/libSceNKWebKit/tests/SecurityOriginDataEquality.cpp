#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore::SecurityOriginData equality check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

using NKWebKit::WebCoreAbi::OpaqueOriginIdentifier;
using NKWebKit::WebCoreAbi::OptionalPort;
using NKWebKit::WebCoreAbi::SecurityOriginData;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;
using NKWebKit::WtfAbi::String;

static String Text(const char* text) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(text));
}

struct Origin {
    SecurityOriginData data;

    Origin(const char* protocol, const char* host, OptionalPort port) {
        new (&data.storage.tuple) SecurityOriginTuple{protocol ? Text(protocol) : String(), host ? Text(host) : String(), port};
        data.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
    }

    Origin(std::uint64_t object, std::uint64_t process) {
        data.storage.opaque = OpaqueOriginIdentifier{object, process};
        data.index = NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex;
    }

    explicit Origin(std::uint32_t index) {
        data.index = index;
    }

    ~Origin() {
        if (data.index == NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex) data.storage.tuple.~SecurityOriginTuple();
    }
};

static bool Equal(const Origin& a, const Origin& b) {
    const bool result = _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&a.data, &b.data);
    Require(result == _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&b.data, &a.data));
    return result;
}

template<typename TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    const OptionalPort none{0, false};
    const OptionalPort port443{443, true};
    const OptionalPort port8080{8080, true};

    Origin https("https", "example.com", none);
    Require(Equal(https, https));
    Require(Equal(https, Origin("https", "example.com", none)));
    Require(Equal(https, Origin("https", "example.com", OptionalPort{77, false})));
    Require(!Equal(https, Origin("http", "example.com", none)));
    Require(!Equal(https, Origin("https", "example.org", none)));
    Require(!Equal(https, Origin("https", "Example.com", none)));
    Require(!Equal(https, Origin("https", "example.com", port443)));
    Require(Equal(Origin("https", "example.com", port8080), Origin("https", "example.com", port8080)));
    Require(!Equal(Origin("https", "example.com", port8080), Origin("https", "example.com", port443)));
    Require(Equal(Origin(nullptr, nullptr, none), Origin(nullptr, nullptr, none)));
    Require(!Equal(Origin(nullptr, nullptr, none), Origin("", "", none)));

    Origin opaque(5, 9);
    Require(Equal(opaque, opaque));
    Require(Equal(opaque, Origin(5, 9)));
    Require(!Equal(opaque, Origin(5, 10)));
    Require(!Equal(opaque, Origin(6, 9)));
    Require(!Equal(opaque, Origin(nullptr, nullptr, none)));
    Require(!Equal(Origin(0, 0), Origin(nullptr, nullptr, none)));

    const std::uint32_t valueless = std::numeric_limits<std::uint32_t>::max();
    Require(Equal(Origin(valueless), Origin(valueless)));
    Require(!Equal(Origin(valueless), opaque));
    Require(!Equal(Origin(valueless), https));

    Origin invalid(2u);
    Require(Throws([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&invalid.data, &https.data); }));
    Require(Throws([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&https.data, &invalid.data); }));
    Require(_ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&invalid.data, &invalid.data));
    Require(Throws([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(nullptr, &https.data); }));
    Require(Throws([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&https.data, nullptr); }));

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
                Require(base != nullptr);
                DWORD previous = 0;
                Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
                const long size = sysconf(_SC_PAGESIZE);
                Require(size > 0);
                pageSize = static_cast<std::size_t>(size);
                void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                Require(mapped != MAP_FAILED);
                base = static_cast<unsigned char*>(mapped);
                Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
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
                Require(VirtualProtect(base, pageSize, PAGE_READONLY, &previous) != 0);
#else
                Require(mprotect(base, pageSize, PROT_READ) == 0);
#endif
            }

            unsigned char* Readable() const { return base; }
            unsigned char* Guard() const { return base + pageSize * 2; }
        };

        auto requireNotImplemented = [](auto&& function) {
            const auto before = NotImplementedCalls_nid_no_patch();
            bool threw = false;
            try {
                function();
            } catch (const std::runtime_error&) {
                threw = true;
            }
            Require(threw);
            Require(NotImplementedCalls_nid_no_patch() == before + 1);
        };

        GuardedPages pages;
        std::memcpy(pages.Readable(), &https.data, sizeof(SecurityOriginData));
        std::memcpy(pages.Readable() + 64, &opaque.data, sizeof(SecurityOriginData));
        pages.ProtectReadOnly();

        const auto* roHttps = reinterpret_cast<const SecurityOriginData*>(pages.Readable());
        const auto* roOpaque = reinterpret_cast<const SecurityOriginData*>(pages.Readable() + 64);
        Require(_ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(roHttps, &https.data));
        Require(_ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(roOpaque, &opaque.data));
        Require(!_ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(roHttps, roOpaque));

        const auto* guardOrigin = reinterpret_cast<const SecurityOriginData*>(pages.Guard());
        const auto* splitOrigin = reinterpret_cast<const SecurityOriginData*>(pages.Guard() - (sizeof(SecurityOriginData) - 4));
        requireNotImplemented([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(guardOrigin, &https.data); });
        requireNotImplemented([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&https.data, guardOrigin); });
        requireNotImplemented([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(guardOrigin, guardOrigin); });
        requireNotImplemented([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(splitOrigin, &https.data); });
        requireNotImplemented([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&https.data, splitOrigin); });

        using NKWebKit::WtfAbi::StringImpl;
        SecurityOriginData badProtocol{};
        badProtocol.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
        badProtocol.storage.tuple.protocol.impl = reinterpret_cast<StringImpl*>(pages.Guard());
        badProtocol.storage.tuple.host.impl = nullptr;
        badProtocol.storage.tuple.port = none;
        requireNotImplemented([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&badProtocol, &https.data); });
        requireNotImplemented([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&https.data, &badProtocol); });
        badProtocol.storage.tuple.protocol.impl = nullptr;

        StringImpl badChars{2, 8, pages.Guard(), 4};
        SecurityOriginData badHostChars{};
        badHostChars.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
        badHostChars.storage.tuple.protocol.impl = https.data.storage.tuple.protocol.impl;
        badHostChars.storage.tuple.host.impl = &badChars;
        badHostChars.storage.tuple.port = none;
        requireNotImplemented([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&badHostChars, &https.data); });
        requireNotImplemented([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&https.data, &badHostChars); });

        StringImpl splitChars{2, 8, pages.Guard() - 2, 4};
        badHostChars.storage.tuple.host.impl = &splitChars;
        requireNotImplemented([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&badHostChars, &https.data); });

        const char16_t wideProtoData[] = u"https";
        const char16_t wideHostData[] = u"example.com";
        StringImpl wideProto{2, 5, wideProtoData, 0};
        StringImpl wideHost{2, 11, wideHostData, 0};
        SecurityOriginData wideHttps{};
        wideHttps.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
        wideHttps.storage.tuple.protocol.impl = &wideProto;
        wideHttps.storage.tuple.host.impl = &wideHost;
        wideHttps.storage.tuple.port = none;
        Require(_ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&wideHttps, &https.data));
        Require(_ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&https.data, &wideHttps));

        badHostChars.storage.tuple.protocol.impl = nullptr;
        badHostChars.storage.tuple.host.impl = nullptr;
        wideHttps.storage.tuple.protocol.impl = nullptr;
        wideHttps.storage.tuple.host.impl = nullptr;
    }

    std::puts("libSceNKWebKit WebCore::SecurityOriginData equality passed");
    return 0;
}
