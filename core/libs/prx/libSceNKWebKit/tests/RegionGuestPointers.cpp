#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Region guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

struct RegionHolder {
    alignas(alignof(WebCore::Region)) unsigned char storage[sizeof(WebCore::Region)];
    WebCore::Region* ptr() { return reinterpret_cast<WebCore::Region*>(storage); }
};

static WebCore::Region* MapRegionPage(const void* bytes) {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    std::memcpy(page, bytes, sizeof(WebCore::Region));
    return static_cast<WebCore::Region*>(page);
}

static void ProtectPage(void* page, bool writable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, writable ? PAGE_READWRITE : PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, writable ? PROT_READ | PROT_WRITE : PROT_READ) == 0);
#endif
}

static void UnmapPage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

static WebCore::IntRect Rect(int x, int y, int width, int height) {
    return WebCore::IntRect{WebCore::IntPoint{x, y}, WebCore::IntSize{width, height}};
}

static bool SameRegion(const WebCore::Region* region, const unsigned char* bytes) {
    return std::memcmp(region, bytes, sizeof(WebCore::Region)) == 0;
}

int main() {
    auto* unreadable = reinterpret_cast<WebCore::Region*>(0x10);
    const auto* unreadableRect = reinterpret_cast<const WebCore::IntRect*>(0x10);
    const auto* unreadableSize = reinterpret_cast<const WebCore::IntSize*>(0x10);
    const auto* unreadablePoint = reinterpret_cast<const WebCore::IntPoint*>(0x10);

    RegionHolder square;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(square.ptr(), Rect(0, 0, 10, 10));
    WebCore::Region* readOnly = MapRegionPage(square.storage);
    ProtectPage(readOnly, false);

    RegionHolder shaped;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(shaped.ptr(), Rect(0, 0, 10, 10));
    const WebCore::IntRect tall = Rect(0, 0, 4, 30);
    RegionHolder tallRegion;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(tallRegion.ptr(), tall);
    _ZN7WebCore6Region5uniteERKS0_(shaped.ptr(), *tallRegion.ptr());
    Require(shaped.ptr()->m_shape != nullptr);
    WebCore::Region* readOnlyShaped = MapRegionPage(shaped.storage);
    ProtectPage(readOnlyShaped, false);
    const WTF::Vector<WebCore::IntRect, 1> shapedRects = _ZNK7WebCore6Region5rectsEv(readOnlyShaped);
    Require(shapedRects.size() == 2);

    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6RegionC1Ev(unreadable); return 0; }, "_ZN7WebCore6RegionC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6RegionC2Ev(readOnly); return 0; }, "_ZN7WebCore6RegionC1Ev"));
    Require(SameRegion(readOnly, square.storage));

    RegionHolder target;
    std::memset(target.storage, 0xa5, sizeof(target.storage));
    unsigned char untouched[sizeof(WebCore::Region)];
    std::memcpy(untouched, target.storage, sizeof(untouched));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6RegionC1ERKNS_7IntRectE(target.ptr(), *unreadableRect); return 0; },
        "_ZN7WebCore6RegionC1ERKNS_7IntRectE"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6RegionC1ERKNS_7IntRectE(readOnly, Rect(1, 1, 1, 1)); return 0; },
        "_ZN7WebCore6RegionC1ERKNS_7IntRectE"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6RegionC2ERKS0_(target.ptr(), *unreadable); return 0; }, "_ZN7WebCore6RegionC1ERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6RegionC1ERKS0_(readOnly, *shaped.ptr()); return 0; }, "_ZN7WebCore6RegionC1ERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6RegionC1EOS0_(target.ptr(), std::move(*readOnlyShaped)); return 0; },
        "_ZN7WebCore6RegionC1EOS0_"));
    Require(SameRegion(target.ptr(), untouched) && SameRegion(readOnlyShaped, shaped.storage) && SameRegion(readOnly, square.storage));

    _ZN7WebCore6RegionC1ERKS0_(target.ptr(), *readOnlyShaped);
    Require(target.ptr()->m_shape != nullptr && target.ptr()->m_shape != shaped.ptr()->m_shape);
    Require(_ZNK7WebCore6Region8containsERKS0_(target.ptr(), *readOnlyShaped) && _ZNK7WebCore6Region8containsERKS0_(readOnlyShaped, *target.ptr()));
    std::memcpy(untouched, target.storage, sizeof(untouched));

    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region5uniteERKS0_(target.ptr(), *unreadable); return 0; }, "_ZN7WebCore6Region5uniteERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region5uniteERKS0_(unreadable, *target.ptr()); return 0; }, "_ZN7WebCore6Region5uniteERKS0_"));
    RegionHolder distant;
    _ZN7WebCore6RegionC1ERKNS_7IntRectE(distant.ptr(), Rect(50, 50, 5, 5));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region5uniteERKS0_(readOnlyShaped, *distant.ptr()); return 0; }, "_ZN7WebCore6Region5uniteERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region8subtractERKS0_(readOnlyShaped, *square.ptr()); return 0; },
        "_ZN7WebCore6Region8subtractERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region8subtractERKS0_(target.ptr(), *unreadable); return 0; },
        "_ZN7WebCore6Region8subtractERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region9intersectERKS0_(readOnlyShaped, *distant.ptr()); return 0; },
        "_ZN7WebCore6Region9intersectERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region9intersectERKS0_(readOnly, *square.ptr()); return 0; },
        "_ZN7WebCore6Region9intersectERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region9intersectERKS0_(unreadable, *square.ptr()); return 0; },
        "_ZN7WebCore6Region9intersectERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region9translateERKNS_7IntSizeE(readOnlyShaped, WebCore::IntSize{1, 1}); return 0; },
        "_ZN7WebCore6Region9translateERKNS_7IntSizeE"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region9translateERKNS_7IntSizeE(target.ptr(), *unreadableSize); return 0; },
        "_ZN7WebCore6Region9translateERKNS_7IntSizeE"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE(target.ptr()->m_shape.get(), *unreadableSize); return 0; },
        "_ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE"));
    auto* unreadableShape = reinterpret_cast<WebCore::Region::Shape*>(0x10);
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE(unreadableShape, WebCore::IntSize{1, 1}); return 0; },
        "_ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE"));
#ifdef _WIN32
    void* readOnlyShapePage = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(readOnlyShapePage != nullptr);
#else
    void* readOnlyShapePage = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(readOnlyShapePage != MAP_FAILED);
#endif
    std::memcpy(readOnlyShapePage, shaped.ptr()->m_shape.get(), sizeof(WebCore::Region::Shape));
    auto* readOnlyShape = static_cast<WebCore::Region::Shape*>(readOnlyShapePage);
    if (shaped.ptr()->m_shape->m_segments.data() == reinterpret_cast<const int*>(reinterpret_cast<const unsigned char*>(shaped.ptr()->m_shape.get()) + 16)) {
        int* inlineSegs = reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(readOnlyShape) + 16);
        std::memcpy(readOnlyShape, &inlineSegs, sizeof(inlineSegs));
    }
    if (shaped.ptr()->m_shape->m_spans.data() == reinterpret_cast<const WebCore::Region::Span*>(reinterpret_cast<const unsigned char*>(shaped.ptr()->m_shape.get()) + 160)) {
        auto* inlineSpans = reinterpret_cast<WebCore::Region::Span*>(reinterpret_cast<unsigned char*>(readOnlyShape) + 160);
        std::memcpy(reinterpret_cast<unsigned char*>(readOnlyShape) + 144, &inlineSpans, sizeof(inlineSpans));
    }
    ProtectPage(readOnlyShapePage, false);
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE(readOnlyShape, WebCore::IntSize{2, 3}); return 0; },
        "_ZN7WebCore6Region5Shape9translateERKNS_7IntSizeE"));
    UnmapPage(readOnlyShapePage);

    RegionHolder badShapeRegion;
    const WebCore::IntRect badBounds = Rect(0, 0, 20, 20);
    std::memcpy(badShapeRegion.storage, &badBounds, sizeof(badBounds));
    std::memcpy(badShapeRegion.storage + 16, &unreadableShape, sizeof(unreadableShape));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore6Region5rectsEv(badShapeRegion.ptr()); }, "_ZNK7WebCore6Region5rectsEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore6Region8containsERKNS_8IntPointE(badShapeRegion.ptr(), WebCore::IntPoint{5, 5}); },
        "_ZNK7WebCore6Region8containsERKNS_8IntPointE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore6Region8containsERKS0_(badShapeRegion.ptr(), *square.ptr()); },
        "_ZNK7WebCore6Region8containsERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore6Region10intersectsERKS0_(badShapeRegion.ptr(), *square.ptr()); },
        "_ZNK7WebCore6Region10intersectsERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6Region9translateERKNS_7IntSizeE(badShapeRegion.ptr(), WebCore::IntSize{1, 1}); return 0; },
        "_ZN7WebCore6Region9translateERKNS_7IntSizeE"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore6Region9totalAreaEv(badShapeRegion.ptr()); },
        "_ZNK7WebCore6Region9totalAreaEv"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6RegionC1ERKS0_(target.ptr(), *badShapeRegion.ptr()); return 0; },
        "_ZN7WebCore6RegionC1ERKS0_"));

    auto* unreadableStream = reinterpret_cast<NKWebKit::WtfAbi::TextStream*>(0x10);
    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(*unreadableStream, *square.ptr()); },
        "_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE"));
    NKWebKit::WtfAbi::TextStream localStream{};
    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(localStream, *unreadable); },
        "_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE"));
    Require(localStream.indent == 0 && localStream.text.length == 0);
    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(localStream, *badShapeRegion.ptr()); },
        "_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE"));
    Require(localStream.indent == 0 && localStream.text.length == 0);
#ifdef _WIN32
    void* readOnlyStreamPage = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(readOnlyStreamPage != nullptr);
#else
    void* readOnlyStreamPage = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(readOnlyStreamPage != MAP_FAILED);
#endif
    std::memset(readOnlyStreamPage, 0, sizeof(NKWebKit::WtfAbi::TextStream));
    ProtectPage(readOnlyStreamPage, false);
    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(*static_cast<NKWebKit::WtfAbi::TextStream*>(readOnlyStreamPage), *square.ptr()); },
        "_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE"));
    UnmapPage(readOnlyStreamPage);
    Require(SameRegion(target.ptr(), untouched) && SameRegion(readOnlyShaped, shaped.storage));
    const WTF::Vector<WebCore::IntRect, 1> afterRects = _ZNK7WebCore6Region5rectsEv(readOnlyShaped);
    Require(afterRects.size() == shapedRects.size());
    for (std::size_t i = 0; i < afterRects.size(); ++i)
        Require(std::memcmp(&afterRects[i], &shapedRects[i], sizeof(WebCore::IntRect)) == 0);

    _ZN7WebCore6Region5uniteERKS0_(readOnlyShaped, *square.ptr());
    _ZN7WebCore6Region8subtractERKS0_(readOnlyShaped, *distant.ptr());
    Require(SameRegion(readOnlyShaped, shaped.storage) && SameRegion(readOnly, square.storage));

    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore6Region5rectsEv(unreadable); }, "_ZNK7WebCore6Region5rectsEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore6Region10intersectsERKS0_(target.ptr(), *unreadable); },
        "_ZNK7WebCore6Region10intersectsERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore6Region8containsERKS0_(unreadable, *target.ptr()); },
        "_ZNK7WebCore6Region8containsERKS0_"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore6Region8containsERKNS_8IntPointE(target.ptr(), *unreadablePoint); },
        "_ZNK7WebCore6Region8containsERKNS_8IntPointE"));
    Require(_ZNK7WebCore6Region10intersectsERKS0_(readOnly, *readOnlyShaped));
    Require(_ZNK7WebCore6Region8containsERKNS_8IntPointE(readOnlyShaped, WebCore::IntPoint{2, 20}));
    Require(!_ZNK7WebCore6Region8containsERKNS_8IntPointE(readOnlyShaped, WebCore::IntPoint{8, 20}));

    RegionHolder assigned;
    _ZN7WebCore6RegionC1Ev(assigned.ptr());
    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCore6RegionaSERKS0_(readOnly, *shaped.ptr()); }, "_ZN7WebCore6RegionaSERKS0_"));
    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCore6RegionaSERKS0_(assigned.ptr(), *unreadable); }, "_ZN7WebCore6RegionaSERKS0_"));
    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCore6RegionaSEOS0_(assigned.ptr(), std::move(*readOnlyShaped)); }, "_ZN7WebCore6RegionaSEOS0_"));
    Require(FaultsAsNotImplemented([&] { return &_ZN7WebCore6RegionaSEOS0_(readOnly, std::move(target.ptr()[0])); }, "_ZN7WebCore6RegionaSEOS0_"));
    Require(SameRegion(readOnly, square.storage) && SameRegion(readOnlyShaped, shaped.storage) && SameRegion(target.ptr(), untouched));
    Require(assigned.ptr()->isEmpty() && assigned.ptr()->m_shape == nullptr);
    Require(&_ZN7WebCore6RegionaSERKS0_(assigned.ptr(), *readOnlyShaped) == assigned.ptr());
    Require(_ZNK7WebCore6Region8containsERKS0_(assigned.ptr(), *readOnlyShaped) && assigned.ptr()->m_shape != shaped.ptr()->m_shape);

    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6RegionD2Ev(unreadable); return 0; }, "_ZN7WebCore6RegionD1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN7WebCore6RegionD1Ev(readOnlyShaped); return 0; }, "_ZN7WebCore6RegionD1Ev"));
    Require(SameRegion(readOnlyShaped, shaped.storage));

    _ZN7WebCore6RegionD1Ev(assigned.ptr());
    Require(assigned.ptr()->m_shape == nullptr);
    _ZN7WebCore6RegionD1Ev(target.ptr());
    _ZN7WebCore6RegionD1Ev(distant.ptr());
    _ZN7WebCore6RegionD1Ev(tallRegion.ptr());
    _ZN7WebCore6RegionD1Ev(shaped.ptr());
    _ZN7WebCore6RegionD1Ev(square.ptr());
    UnmapPage(readOnlyShaped);
    UnmapPage(readOnly);
    return 0;
}
