#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static unsigned char* MapTwoPages() {
#ifdef _WIN32
    void* pages = VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(pages != nullptr);
#else
    void* pages = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(pages != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(pages);
}

static void ProtectPage(unsigned char* page, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, readable ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
    Require(mprotect(page, 4096, readable ? PROT_READ : PROT_NONE) == 0);
#endif
}

static void UnmapTwoPages(unsigned char* pages) {
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 8192) == 0);
#endif
}

int main() {
    const void* viewRef = WKViewCreate(nullptr);
    Require(viewRef != nullptr);
    Require(WKGetTypeID(viewRef) == static_cast<std::uint32_t>(NKWebKit::ObjectType::View));

    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    Require(view->configuration.Get() == nullptr);
    Require(view->viewClient == nullptr);
    Require(view->popupMenuClient == nullptr);
    Require(view->lastPopupMenuSelectedIndex == -1);

    const void* configRef = WKPageConfigurationCreate();
    Require(configRef != nullptr);
    const void* viewWithConfigRef = WKViewCreate(configRef);
    Require(viewWithConfigRef != nullptr);
    auto* viewWithConfig = NKWebKit::Cast<NKWebKit::View>(viewWithConfigRef, __func__);
    Require(viewWithConfig->configuration.Get() != nullptr);

    int dummyViewClient = 101;
    WKViewSetViewClient(viewRef, &dummyViewClient);
    Require(view->viewClient == &dummyViewClient);

    int dummyPopupMenuClient = 202;
    WKViewSetViewPopupMenuClient(viewRef, &dummyPopupMenuClient);
    Require(view->popupMenuClient == &dummyPopupMenuClient);

    WKViewValueChangedForPopupMenu(viewRef, 5);
    Require(view->lastPopupMenuSelectedIndex == 5);

    int copied = -1;
    int actual = -1;
    WKViewGetPunchHoles(viewRef, nullptr, 0, &copied, &actual);
    Require(copied == 0);
    Require(actual == 0);

    NKWebKit::PunchHole hole1;
    hole1.quad[0][0] = 10.0f;
    hole1.quad[0][1] = 20.0f;
    hole1.opacity = 0.5f;
    hole1.canvasHandle = 100;
    hole1.flags = 1;

    NKWebKit::PunchHole hole2;
    hole2.quad[1][0] = 30.0f;
    hole2.quad[1][1] = 40.0f;
    hole2.opacity = 0.8f;
    hole2.canvasHandle = 200;
    hole2.flags = 2;

    view->punchHoles.push_back(hole1);
    view->punchHoles.push_back(hole2);

    WKPunchHole outHoles[4] = {};
    copied = -1;
    actual = -1;
    WKViewGetPunchHoles(viewRef, outHoles, 1, &copied, &actual);
    Require(copied == 1);
    Require(actual == 2);
    Require(outHoles[0].quad[0][0] == 10.0f);
    Require(outHoles[0].quad[0][1] == 20.0f);
    Require(outHoles[0].opacity == 0.5f);
    Require(outHoles[0].canvasHandle == 100);
    Require(outHoles[0].flags == 1);

    copied = -1;
    actual = -1;
    WKViewGetPunchHoles(viewRef, outHoles, 4, &copied, &actual);
    Require(copied == 2);
    Require(actual == 2);
    Require(outHoles[0].canvasHandle == 100);
    Require(outHoles[1].canvasHandle == 200);
    Require(outHoles[1].opacity == 0.8f);
    Require(outHoles[1].quad[1][0] == 30.0f);
    Require(outHoles[1].quad[1][1] == 40.0f);

    unsigned char* pages = MapTwoPages();
    auto* spanningHoles = reinterpret_cast<WKPunchHole*>(pages + 4096 - sizeof(WKPunchHole));
    copied = -1;
    actual = -1;
    WKViewGetPunchHoles(viewRef, spanningHoles, 2, &copied, &actual);
    Require(copied == 2);
    Require(actual == 2);
    Require(spanningHoles[0].canvasHandle == 100);
    Require(spanningHoles[1].canvasHandle == 200);

    ProtectPage(pages + 4096, false);
    copied = -1;
    actual = -1;
    RequireThrows([&] { WKViewGetPunchHoles(viewRef, spanningHoles, 2, &copied, &actual); });
    Require(copied == -1);
    Require(actual == -1);

    auto* guardedCount = reinterpret_cast<int*>(pages + 4096);
    RequireThrows([&] { WKViewGetPunchHoles(viewRef, outHoles, 4, guardedCount, &actual); });
    RequireThrows([&] { WKViewGetPunchHoles(viewRef, outHoles, 4, &copied, guardedCount); });

    auto* badCount = reinterpret_cast<int*>(std::uintptr_t{0x10});
    RequireThrows([&] { WKViewGetPunchHoles(viewRef, outHoles, 4, badCount, &actual); });
    RequireThrows([&] { WKViewGetPunchHoles(viewRef, outHoles, 4, &copied, badCount); });

    UnmapTwoPages(pages);

    const void* dummyObj = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKViewCreate(dummyObj); });

    RequireThrows([&] { WKViewGetPunchHoles(nullptr, outHoles, 4, &copied, &actual); });
    RequireThrows([&] { WKViewGetPunchHoles(dummyObj, outHoles, 4, &copied, &actual); });
    RequireThrows([&] { WKViewGetPunchHoles(viewRef, outHoles, 4, nullptr, &actual); });
    RequireThrows([&] { WKViewGetPunchHoles(viewRef, outHoles, 4, &copied, nullptr); });
    RequireThrows([&] { WKViewGetPunchHoles(viewRef, nullptr, 2, &copied, &actual); });
    RequireThrows([&] { WKViewGetPunchHoles(viewRef, outHoles, -1, &copied, &actual); });

    RequireThrows([&] { WKViewSetViewClient(nullptr, &dummyViewClient); });
    RequireThrows([&] { WKViewSetViewClient(dummyObj, &dummyViewClient); });

    RequireThrows([&] { WKViewSetViewPopupMenuClient(nullptr, &dummyPopupMenuClient); });
    RequireThrows([&] { WKViewSetViewPopupMenuClient(dummyObj, &dummyPopupMenuClient); });

    RequireThrows([&] { WKViewValueChangedForPopupMenu(nullptr, 0); });
    RequireThrows([&] { WKViewValueChangedForPopupMenu(dummyObj, 0); });

    WKRelease(dummyObj);
    WKRelease(viewWithConfigRef);
    WKRelease(configRef);
    WKRelease(viewRef);

    return 0;
}
