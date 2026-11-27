#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler;
using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisablerVtable;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WebCoreAbi::ResourceResponseStatusLayout;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WebCoreAbi::StoredCredentialsPolicy;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "CrossOriginAccessControl guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

struct GuardedPages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 3) == 0);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template <typename Action>
bool ThrowsNotImplemented(Action&& action, const char* symbol) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(symbol) + " not implemented";
    }
    return false;
}

}  // namespace

int main() {
    GuardedPages pages;
    auto* readOnly = reinterpret_cast<CrossOriginAccessControlCheckDisabler*>(pages.ReadOnly());
    auto* guard = reinterpret_cast<CrossOriginAccessControlCheckDisabler*>(pages.Guard());
    auto* dangling = reinterpret_cast<CrossOriginAccessControlCheckDisabler*>(0x10);
    unsigned char* split = pages.ReadOnly() - 8;

    std::memset(split, 0x5A, 8);
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(readOnly); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev(guard); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(reinterpret_cast<CrossOriginAccessControlCheckDisabler*>(split)); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev"));
    for (std::size_t i = 0; i < 8; ++i) Require(split[i] == 0x5A);

    CrossOriginAccessControlCheckDisabler valid{};
    _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev(&valid);
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisablerC1ERKS0_(readOnly, &valid); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisablerC1ERKS0_"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_(&valid, guard); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_(readOnly, &valid); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_(&valid, guard); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(readOnly, false); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb(guard, false); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb"));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv(dangling)); },
        "_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv"));
    Require(ThrowsNotImplemented([&] { static_cast<void>(_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv(guard)); },
        "_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev(readOnly); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev(guard); },
        "_ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev"));

    alignas(ResourceResponseStatusLayout) unsigned char responseRaw[sizeof(ResourceResponseStatusLayout)]{};
    alignas(SecurityOrigin) unsigned char originRaw[sizeof(SecurityOrigin)]{};
    alignas(LegacyResourceRequestBase) unsigned char requestRaw[sizeof(LegacyResourceRequestBase)]{};
    auto* response = reinterpret_cast<const ResourceResponseBase*>(responseRaw);
    auto* origin = reinterpret_cast<const SecurityOrigin*>(originRaw);
    auto* request = reinterpret_cast<const LegacyResourceRequestBase*>(requestRaw);

    constexpr const char* kPassesSymbol =
        "_ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE";
    constexpr const char* kPreflightSymbol =
        "_ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE";

    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            reinterpret_cast<const ResourceResponseBase*>(pages.Guard()), StoredCredentialsPolicy::DoNotUse, origin, nullptr));
    }, kPassesSymbol));
    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            response, StoredCredentialsPolicy::DoNotUse, reinterpret_cast<const SecurityOrigin*>(pages.Guard()), nullptr));
    }, kPassesSymbol));
    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            response, StoredCredentialsPolicy::Use, origin, guard));
    }, kPassesSymbol));

    CrossOriginAccessControlCheckDisabler badVtableDisabler{reinterpret_cast<const CrossOriginAccessControlCheckDisablerVtable*>(pages.Guard()), true};
    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore24passesAccessControlCheckERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            response, StoredCredentialsPolicy::Use, origin, &badVtableDisabler));
    }, kPassesSymbol));

    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            reinterpret_cast<const LegacyResourceRequestBase*>(pages.Guard()), response, StoredCredentialsPolicy::DoNotUse, origin, nullptr));
    }, kPreflightSymbol));
    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            request, reinterpret_cast<const ResourceResponseBase*>(pages.Guard()), StoredCredentialsPolicy::DoNotUse, origin, nullptr));
    }, kPreflightSymbol));
    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            request, response, StoredCredentialsPolicy::DoNotUse, reinterpret_cast<const SecurityOrigin*>(pages.Guard()), nullptr));
    }, kPreflightSymbol));

    return 0;
}
