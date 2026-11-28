#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

WebCore::UserGestureToken* Sentinel() {
    return reinterpret_cast<WebCore::UserGestureToken*>(static_cast<std::uintptr_t>(0x5A5A5A5A5A5A5A5Aull));
}

void RequireNoGesture() {
    Require(!_ZN7WebCore20UserGestureIndicator21processingUserGestureEv());
    Require(!_ZN7WebCore20UserGestureIndicator29processingUserGestureForMediaEv());
    Require(!WKBundleIsProcessingUserGesture(nullptr));
    WebCore::UserGestureToken* slot = Sentinel();
    Require(_ZN7WebCore20UserGestureIndicator18currentUserGestureEv(&slot) == &slot);
    Require(slot == nullptr);
}

}

int main() {
    RequireNoGesture();

    _ZN3WTF20initializeMainThreadEv();
    Require(_ZN3WTF12isMainThreadEv());

    RequireNoGesture();
    Require(!_ZN7WebCore26UserTypingGestureIndicator27processingUserTypingGestureEv());
    Require(_ZN7WebCore26UserTypingGestureIndicator28focusedElementAtGestureStartEv() == nullptr);
    Require(_ZN7WebCore12WorkerThread17workerThreadCountEv() == 0);

    std::atomic<bool> workerChecked{false};
    std::thread worker([&] {
        Require(!_ZN3WTF12isMainThreadEv());
        RequireNoGesture();
        workerChecked.store(true);
    });
    worker.join();
    Require(workerChecked.load());

    Require(NKWebKit::UserGesture::MaximumIntervalForUserGestureForwardingForFetch().m_value == 10.0);
    _ZN7WebCore16UserGestureToken60setMaximumIntervalForUserGestureForwardingForFetchForTestingEN3WTF7SecondsE(WTF::Seconds{2.5});
    Require(NKWebKit::UserGesture::MaximumIntervalForUserGestureForwardingForFetch().m_value == 2.5);

    std::printf("All libSceNKWebKit UserGestureState tests passed!\n");
    return 0;
}
