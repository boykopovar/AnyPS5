#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKBundleFrame content check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

std::string ToString(const void* ref) {
    if (!ref) {
        return {};
    }
    const auto* str = static_cast<const NKWebKit::String*>(ref);
    return str->Utf8();
}

template<class Function>
void RequireNotImplemented(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        Require(false);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

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

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }

    template<typename T>
    T* SplitReadWriteToReadOnly() const {
        return reinterpret_cast<T*>(base + pageSize - sizeof(T) / 2);
    }

    template<typename T>
    T* SplitReadOnlyToGuard() const {
        return reinterpret_cast<T*>(base + pageSize * 2 - sizeof(T) / 2);
    }
};

void TestGuestPointers(const void* frameRef, const void* nodeHandleRef, const void* urlRef) {
    GuardedPages pages;
    const void* unmappedRef = pages.Guard();
    const void* splitObjRef = pages.SplitReadOnlyToGuard<const NKWebKit::Object>();
    auto* unmappedDouble = reinterpret_cast<double*>(pages.Guard());
    auto* readOnlyDouble = reinterpret_cast<double*>(pages.ReadOnly());
    auto* splitDouble = pages.SplitReadWriteToReadOnly<double>();
    const std::size_t splitHead = sizeof(double) / 2;

    RequireNotImplemented([&] { WKBundleFrameCopyCounterValue(unmappedRef, nodeHandleRef); });
    RequireNotImplemented([&] { WKBundleFrameCopyCounterValue(splitObjRef, nodeHandleRef); });
    RequireNotImplemented([&] { WKBundleFrameCopyCounterValue(frameRef, unmappedRef); });
    RequireNotImplemented([&] { WKBundleFrameCopyCounterValue(frameRef, splitObjRef); });
    RequireNotImplemented([&] { WKBundleFrameCopyInnerText(unmappedRef); });
    RequireNotImplemented([&] { WKBundleFrameCopyInnerText(splitObjRef); });
    RequireNotImplemented([&] { WKBundleFrameCopyLayerTreeAsText(unmappedRef); });
    RequireNotImplemented([&] { WKBundleFrameCopyLayerTreeAsText(splitObjRef); });
    RequireNotImplemented([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(unmappedRef, urlRef); });
    RequireNotImplemented([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(frameRef, unmappedRef); });
    RequireNotImplemented([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(unmappedRef, urlRef); });
    RequireNotImplemented([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(frameRef, unmappedRef); });

    double r = 7.0;
    double g = 8.0;
    double b = 9.0;
    double a = 10.0;

    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(unmappedRef, &r, &g, &b, &a); });
    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(splitObjRef, &r, &g, &b, &a); });
    Require(r == 7.0 && g == 8.0 && b == 9.0 && a == 10.0);

    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, unmappedDouble, &g, &b, &a); });
    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, readOnlyDouble, &g, &b, &a); });
    std::memset(splitDouble, 0x5A, splitHead);
    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, splitDouble, &g, &b, &a); });
    for (std::size_t i = 0; i < splitHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitDouble)[i] == 0x5A);
    Require(g == 8.0 && b == 9.0 && a == 10.0);

    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, unmappedDouble, &b, &a); });
    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, readOnlyDouble, &b, &a); });
    std::memset(splitDouble, 0x6B, splitHead);
    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, splitDouble, &b, &a); });
    for (std::size_t i = 0; i < splitHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitDouble)[i] == 0x6B);
    Require(r == 7.0 && b == 9.0 && a == 10.0);

    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, unmappedDouble, &a); });
    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, readOnlyDouble, &a); });
    std::memset(splitDouble, 0x7C, splitHead);
    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, splitDouble, &a); });
    for (std::size_t i = 0; i < splitHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitDouble)[i] == 0x7C);
    Require(r == 7.0 && g == 8.0 && a == 10.0);

    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, &b, unmappedDouble); });
    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, &b, readOnlyDouble); });
    std::memset(splitDouble, 0x4D, splitHead);
    RequireNotImplemented([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, &b, splitDouble); });
    for (std::size_t i = 0; i < splitHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitDouble)[i] == 0x4D);
    Require(r == 7.0 && g == 8.0 && b == 9.0);
}

}

int main() {
    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    auto nodeHandle = NKWebKit::Make<NKWebKit::BundleNodeHandle>();
    auto url = NKWebKit::Make<NKWebKit::URL>("https://example.com/asset.png");

    const void* frameRef = frame.Get();
    const void* nodeHandleRef = nodeHandle.Get();
    const void* urlRef = url.Get();

    Require(ToString(WKBundleFrameCopyCounterValue(frameRef, nodeHandleRef)).empty());
    Require(ToString(WKBundleFrameCopyInnerText(frameRef)).empty());
    Require(ToString(WKBundleFrameCopyLayerTreeAsText(frameRef)).empty());
    Require(ToString(WKBundleFrameCopyMIMETypeForResourceWithURL(frameRef, urlRef)).empty());
    Require(ToString(WKBundleFrameCopySuggestedFilenameForResourceWithURL(frameRef, urlRef)).empty());

    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 0.0;
    Require(!WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, &b, &a));
    Require(r == 1.0);
    Require(g == 1.0);
    Require(b == 1.0);
    Require(a == 1.0);

    frame->counterValue = NKWebKit::String::FromUtf8("42");
    frame->innerText = NKWebKit::String::FromUtf8("Sample Inner Text");
    frame->layerTreeAsText = NKWebKit::String::FromUtf8("LayerTree(Root)");
    frame->mimeTypeForResource = NKWebKit::String::FromUtf8("image/png");
    frame->suggestedFilenameForResource = NKWebKit::String::FromUtf8("asset.png");
    frame->hasDocumentBackgroundColor = true;
    frame->documentBackgroundColor[0] = 0.1;
    frame->documentBackgroundColor[1] = 0.2;
    frame->documentBackgroundColor[2] = 0.3;
    frame->documentBackgroundColor[3] = 0.4;

    Require(ToString(WKBundleFrameCopyCounterValue(frameRef, nodeHandleRef)) == "42");
    Require(ToString(WKBundleFrameCopyInnerText(frameRef)) == "Sample Inner Text");
    Require(ToString(WKBundleFrameCopyLayerTreeAsText(frameRef)) == "LayerTree(Root)");
    Require(ToString(WKBundleFrameCopyMIMETypeForResourceWithURL(frameRef, urlRef)) == "image/png");
    Require(ToString(WKBundleFrameCopySuggestedFilenameForResourceWithURL(frameRef, urlRef)) == "asset.png");

    Require(WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, &b, &a));
    Require(r == 0.1);
    Require(g == 0.2);
    Require(b == 0.3);
    Require(a == 0.4);

    RequireInvalidArgument([] { WKBundleFrameCopyCounterValue(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopyCounterValue(frameRef, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopyCounterValue(nullptr, nodeHandleRef); });
    RequireInvalidArgument([] { WKBundleFrameCopyInnerText(nullptr); });
    RequireInvalidArgument([] { WKBundleFrameCopyLayerTreeAsText(nullptr); });
    RequireInvalidArgument([] { WKBundleFrameCopyMIMETypeForResourceWithURL(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(frameRef, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(nullptr, urlRef); });
    RequireInvalidArgument([] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(frameRef, nullptr); });
    RequireInvalidArgument([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(nullptr, urlRef); });

    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(nullptr, &r, &g, &b, &a); });
    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, nullptr, &g, &b, &a); });
    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, nullptr, &b, &a); });
    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, nullptr, &a); });
    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(frameRef, &r, &g, &b, nullptr); });

    auto dummy = NKWebKit::Make<NKWebKit::BundleScriptWorld>();
    RequireInvalidArgument([&] { WKBundleFrameCopyCounterValue(dummy.Get(), nodeHandleRef); });
    RequireInvalidArgument([&] { WKBundleFrameCopyCounterValue(frameRef, dummy.Get()); });
    RequireInvalidArgument([&] { WKBundleFrameCopyInnerText(dummy.Get()); });
    RequireInvalidArgument([&] { WKBundleFrameCopyLayerTreeAsText(dummy.Get()); });
    RequireInvalidArgument([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(dummy.Get(), urlRef); });
    RequireInvalidArgument([&] { WKBundleFrameCopyMIMETypeForResourceWithURL(frameRef, dummy.Get()); });
    RequireInvalidArgument([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(dummy.Get(), urlRef); });
    RequireInvalidArgument([&] { WKBundleFrameCopySuggestedFilenameForResourceWithURL(frameRef, dummy.Get()); });
    RequireInvalidArgument([&] { WKBundleFrameGetDocumentBackgroundColor(dummy.Get(), &r, &g, &b, &a); });

    TestGuestPointers(frameRef, nodeHandleRef, urlRef);

    return 0;
}
