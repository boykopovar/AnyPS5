#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
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

using NKWebKit::HttpAbi::CommonHeader;
using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "CrossOriginRequest guest pointer check failed at line %d\n", line);
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

String MakeString(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

}  // namespace

int main() {
    GuardedPages pages;
    constexpr const char* kIsSimple =
        "_ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE";
    constexpr const char* kClean =
        "_ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE";
    constexpr const char* kUpdate =
        "_ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE";

    const String getMethod = MakeString("GET");
    HTTPHeaderMap emptyHeaders{};

    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
            *reinterpret_cast<const String*>(pages.Guard()), emptyHeaders));
    }, kIsSimple));

    alignas(String) unsigned char badMethodRaw[sizeof(String)]{};
    const auto* badMethodImpl = reinterpret_cast<const StringImpl*>(pages.Guard());
    std::memcpy(badMethodRaw, &badMethodImpl, sizeof(badMethodImpl));
    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
            *reinterpret_cast<const String*>(badMethodRaw), emptyHeaders));
    }, kIsSimple));

    StringImpl guardDataMethod{2, 3, pages.Guard(), 4};
    const auto* guardDataMethodPtr = &guardDataMethod;
    std::memcpy(badMethodRaw, &guardDataMethodPtr, sizeof(guardDataMethodPtr));
    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
            *reinterpret_cast<const String*>(badMethodRaw), emptyHeaders));
    }, kIsSimple));

    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
            getMethod, *reinterpret_cast<const HTTPHeaderMap*>(pages.Guard())));
    }, kIsSimple));

    HTTPHeaderMap badCommonBuf{{reinterpret_cast<CommonHeader*>(pages.Guard()), 1, 1}, {nullptr, 0, 0}};
    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
            getMethod, badCommonBuf));
    }, kIsSimple));

    CommonHeader badHeaderEntry{HTTPHeaderName::Accept, String{}};
    badHeaderEntry.value.impl = reinterpret_cast<StringImpl*>(pages.Guard());
    HTTPHeaderMap badHeaderValue{{&badHeaderEntry, 1, 1}, {nullptr, 0, 0}};
    Require(ThrowsNotImplemented([&] {
        static_cast<void>(_ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
            getMethod, badHeaderValue));
    }, kIsSimple));
    badHeaderEntry.value.impl = nullptr;

    auto* readOnlyReq = reinterpret_cast<ResourceRequestBase*>(pages.ReadOnly());
    auto* guardReq = reinterpret_cast<ResourceRequestBase*>(pages.Guard());
    auto* danglingReq = reinterpret_cast<ResourceRequestBase*>(0x10);
    unsigned char* splitReq = pages.ReadOnly() - 32;
    std::memset(splitReq, 0x5A, 32);

    Require(ThrowsNotImplemented([&] { _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(readOnlyReq, 0); }, kClean));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(guardReq, 0); }, kClean));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(danglingReq, 0); }, kClean));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(reinterpret_cast<ResourceRequestBase*>(splitReq), 0); }, kClean));
    for (std::size_t i = 0; i < 32; ++i) Require(splitReq[i] == 0x5A);

    ResourceRequestBase badHeadersReq{};
    badHeadersReq.resourceRequestUpdated = true;
    badHeadersReq.platformRequestUpdated = true;
    auto& reqHeaders = *reinterpret_cast<HTTPHeaderMap*>(&badHeadersReq.httpHeaderFields);
    reqHeaders.common = {reinterpret_cast<CommonHeader*>(pages.Guard()), 1, 1};
    Require(ThrowsNotImplemented([&] { _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(&badHeadersReq, 0); }, kClean));
    Require(badHeadersReq.platformRequestUpdated);
    reqHeaders.common = {};

    SecurityOrigin validOrigin{};
    validOrigin.data.index = NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex;
    Require(ThrowsNotImplemented([&] { _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(readOnlyReq, &validOrigin, 0); }, kUpdate));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(guardReq, &validOrigin, 0); }, kUpdate));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(danglingReq, &validOrigin, 0); }, kUpdate));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(reinterpret_cast<ResourceRequestBase*>(splitReq), &validOrigin, 0); }, kUpdate));
    for (std::size_t i = 0; i < 32; ++i) Require(splitReq[i] == 0x5A);

    ResourceRequestBase validReq{};
    validReq.allowCookies = true;
    validReq.resourceRequestUpdated = true;
    validReq.platformRequestUpdated = true;
    Require(ThrowsNotImplemented([&] { _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(&validReq, reinterpret_cast<SecurityOrigin*>(pages.Guard()), 0); }, kUpdate));
    Require(validReq.allowCookies && validReq.platformRequestUpdated);

    SecurityOrigin badTupleOrigin{};
    badTupleOrigin.data.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
    badTupleOrigin.data.storage.tuple.protocol.impl = reinterpret_cast<StringImpl*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(&validReq, &badTupleOrigin, 0); }, kUpdate));
    Require(validReq.allowCookies && validReq.platformRequestUpdated);
    badTupleOrigin.data.storage.tuple.protocol.impl = nullptr;

    validReq.url.impl = reinterpret_cast<StringImpl*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(&validReq, &validOrigin, 0); }, kUpdate));
    Require(validReq.allowCookies && validReq.platformRequestUpdated);
    validReq.url.impl = nullptr;

    return 0;
}
