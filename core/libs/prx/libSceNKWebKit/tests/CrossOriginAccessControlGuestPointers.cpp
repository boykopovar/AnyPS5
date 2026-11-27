#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
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

    const auto makeString = [](const char* text) {
        return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
    };
    ResourceResponseStatusLayout okResponse{};
    okResponse.httpStatusCode = 200;
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&okResponse.head, NKWebKit::HttpAbi::HTTPHeaderName::AccessControlAllowOrigin, makeString("*"));
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&okResponse.head, NKWebKit::HttpAbi::HTTPHeaderName::AccessControlAllowMethods, makeString("PUT"));
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&okResponse.head, NKWebKit::HttpAbi::HTTPHeaderName::AccessControlAllowHeaders, makeString("X-Test"));
    auto& responseHeaders = okResponse.head.httpHeaderFields;
    SecurityOrigin validOrigin{};
    auto* okResponsePtr = &okResponse.head;

    const auto checkNestedPreflightFault = [&](LegacyResourceRequestBase& badReq) {
        badReq.resourceRequestUpdated = false;
        Require(ThrowsNotImplemented([&] {
            static_cast<void>(_ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
                &badReq, okResponsePtr, StoredCredentialsPolicy::DoNotUse, &validOrigin, nullptr));
        }, kPreflightSymbol));
        Require(!badReq.resourceRequestUpdated);
    };

    const NKWebKit::WtfAbi::String validMethod = makeString("PUT");
    const NKWebKit::WtfAbi::String validUrlText = makeString("https://api.example/resource");
    NKWebKit::WtfAbi::StringImpl nullDataImpl{2, 4, nullptr, 4};
    NKWebKit::WtfAbi::StringImpl guardDataImpl{2, 4, pages.Guard(), 4};

    {
        LegacyResourceRequestBase badMethodReq{};
        badMethodReq.url.impl = validUrlText.impl;
        badMethodReq.httpMethod.impl = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(pages.Guard());
        checkNestedPreflightFault(badMethodReq);
        badMethodReq.httpMethod.impl = &nullDataImpl;
        checkNestedPreflightFault(badMethodReq);
        badMethodReq.httpMethod.impl = &guardDataImpl;
        checkNestedPreflightFault(badMethodReq);
        badMethodReq.httpMethod.impl = nullptr;
    }
    {
        LegacyResourceRequestBase badUrlReq{};
        badUrlReq.httpMethod.impl = validMethod.impl;
        badUrlReq.url.impl = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(pages.Guard());
        checkNestedPreflightFault(badUrlReq);
        badUrlReq.url.impl = &nullDataImpl;
        checkNestedPreflightFault(badUrlReq);
        badUrlReq.httpMethod.impl = nullptr;
    }
    {
        LegacyResourceRequestBase badCommonBufReq{};
        badCommonBufReq.httpMethod.impl = validMethod.impl;
        badCommonBufReq.url.impl = validUrlText.impl;
        auto& reqMap = *reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&badCommonBufReq.httpHeaderFields);
        reqMap.common.buffer = reinterpret_cast<NKWebKit::HttpAbi::CommonHeader*>(pages.Guard());
        reqMap.common.capacity = 1;
        reqMap.common.size = 1;
        checkNestedPreflightFault(badCommonBufReq);

        NKWebKit::HttpAbi::CommonHeader badCommonEntry{NKWebKit::HttpAbi::HTTPHeaderName::Accept, NKWebKit::WtfAbi::String{}};
        badCommonEntry.value.impl = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(pages.Guard());
        reqMap.common.buffer = &badCommonEntry;
        checkNestedPreflightFault(badCommonBufReq);
        badCommonEntry.value.impl = nullptr;

        reqMap.common = {};
        reqMap.uncommon.buffer = reinterpret_cast<NKWebKit::HttpAbi::UncommonHeader*>(pages.Guard());
        reqMap.uncommon.capacity = 1;
        reqMap.uncommon.size = 1;
        checkNestedPreflightFault(badCommonBufReq);

        NKWebKit::HttpAbi::UncommonHeader badUncommonEntry{};
        badUncommonEntry.key.impl = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(pages.Guard());
        reqMap.uncommon.buffer = &badUncommonEntry;
        checkNestedPreflightFault(badCommonBufReq);
        badUncommonEntry.key.impl = validMethod.impl;
        badUncommonEntry.value.impl = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(pages.Guard());
        checkNestedPreflightFault(badCommonBufReq);
        badUncommonEntry.key.impl = nullptr;
        badUncommonEntry.value.impl = nullptr;
        badCommonBufReq.httpMethod.impl = nullptr;
    }

    if (responseHeaders.common.buffer) {
        std::destroy_n(responseHeaders.common.buffer, responseHeaders.common.size);
        _ZN3WTF8fastFreeEPv(responseHeaders.common.buffer);
    }
    if (responseHeaders.uncommon.buffer) {
        std::destroy_n(responseHeaders.uncommon.buffer, responseHeaders.uncommon.size);
        _ZN3WTF8fastFreeEPv(responseHeaders.uncommon.buffer);
    }

    return 0;
}
