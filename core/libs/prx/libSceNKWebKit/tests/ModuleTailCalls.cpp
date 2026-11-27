#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ModuleTailCalls check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Allocate = void* (APS5_VABI*)(std::size_t);
using Release = void (APS5_VABI*)(void*);
using FinishCreation = void (APS5_VABI*)(void*, void*);
using Destroy = void (APS5_VABI*)(void*);

static bool ReachesStub(Destroy destroy, void* object) {
    try {
        destroy(object);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    for (const Allocate allocate : {reinterpret_cast<Allocate>(&_ZN7WebCore17FrameLoaderClientnaEm), reinterpret_cast<Allocate>(&_ZN7WebCore17FrameLoaderClientnwEm)}) {
        for (const std::size_t size : {std::size_t{1}, std::size_t{200}}) {
            void* memory = allocate(size);
            Require(memory != nullptr && _ZN3WTF14fastMallocSizeEPKv(memory) == size);
            std::memset(memory, 0x5a, size);
            _ZN3WTF8fastFreeEPv(memory);
        }
    }
    for (const Release release : {reinterpret_cast<Release>(&_ZN7WebCore17FrameLoaderClientdaEPv), reinterpret_cast<Release>(&_ZN7WebCore17FrameLoaderClientdlEPv), reinterpret_cast<Release>(&_ZN7WebCore17LibWebRTCProviderD0Ev)}) {
        release(_ZN3WTF10fastMallocEm(32));
        release(nullptr);
    }
    alignas(16) unsigned char cell[256] = {};
    unsigned char vm[64] = {};
    for (const FinishCreation finish : {reinterpret_cast<FinishCreation>(&_ZN7WebCore10JSDocument14finishCreationERN3JSC2VME), reinterpret_cast<FinishCreation>(&_ZN7WebCore13JSHTMLElement14finishCreationERN3JSC2VME), reinterpret_cast<FinishCreation>(&_ZN7WebCore15JSSVGSVGElement14finishCreationERN3JSC2VME), reinterpret_cast<FinishCreation>(&_ZN7WebCore17JSHTMLLinkElement14finishCreationERN3JSC2VME), reinterpret_cast<FinishCreation>(&_ZN7WebCore18JSHTMLImageElement14finishCreationERN3JSC2VME), reinterpret_cast<FinishCreation>(&_ZN7WebCore18JSHTMLInputElement14finishCreationERN3JSC2VME), reinterpret_cast<FinishCreation>(&_ZN7WebCore18JSHTMLMediaElement14finishCreationERN3JSC2VME), reinterpret_cast<FinishCreation>(&_ZN7WebCore18JSHTMLVideoElement14finishCreationERN3JSC2VME), reinterpret_cast<FinishCreation>(&_ZN7WebCore19JSHTMLSelectElement14finishCreationERN3JSC2VME), reinterpret_cast<FinishCreation>(&_ZN7WebCore9JSElement14finishCreationERN3JSC2VME)}) finish(cell, vm);
    WKBundleRemoveAllWebNotificationPermissions();
    WKBundleSetServiceWorkerProxyCreationCallback();
    WKContextTerminateServiceWorkers();
    WKSendDisplayConfigurationChangedMessageForTesting();
    WKTextCheckerSetTestingMode();
    _ZN7WebCore14JSWebAnimation14finishCreationERN3JSC2VME();
    _ZN7WebCore21JSTextTrackCueGeneric14finishCreationERN3JSC2VME();
    _ZN7WebCore6JSFile14finishCreationERN3JSC2VME();
    _ZN7WebCore6JSNode14finishCreationERN3JSC2VME();
    _ZN7WebCore7JSRange14finishCreationERN3JSC2VME();
    for (std::size_t i = 0; i < sizeof(cell); ++i) Require(cell[i] == 0);
    Require(NotImplementedCalls_nid_no_patch() == before);
    Require(ReachesStub(reinterpret_cast<Destroy>(&_ZN7WebCore11JSDOMWindowD1Ev), cell));
    Require(ReachesStub(reinterpret_cast<Destroy>(&_ZN7WebCore11JSDOMWindowD2Ev), cell));
    Require(NotImplementedCalls_nid_no_patch() == before + 2);
    return 0;
}
