#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKBundlePageSimulateMouseDown(const void* pageRef, int button, NKWebKit::PointValue position, int clickCount, std::uint32_t modifiers, double syntheticClickType) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    ++page->simulatedMouseDownCount;
    page->lastMousePosition = position;
    page->lastMouseButton = button;
    page->lastMouseClickCount = clickCount;
    page->lastMouseModifiers = modifiers;
    page->lastSyntheticClickType = syntheticClickType;
}

void APS5_VABI WKBundlePageSimulateMouseMotion(const void* pageRef, NKWebKit::PointValue position, std::uint32_t modifiers) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    ++page->simulatedMouseMotionCount;
    page->lastMousePosition = position;
    page->lastMouseModifiers = modifiers;
}

void APS5_VABI WKBundlePageSimulateMouseUp(const void* pageRef, int button, NKWebKit::PointValue position, int clickCount, std::uint32_t modifiers) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    ++page->simulatedMouseUpCount;
    page->lastMousePosition = position;
    page->lastMouseButton = button;
    page->lastMouseClickCount = clickCount;
    page->lastMouseModifiers = modifiers;
}

void APS5_VABI WKBundlePageStartMonitoringScrollOperations(const void* pageRef, bool clearLatchingState) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->isMonitoringScrollOperations = true;
    page->clearLatchingState = clearLatchingState;
}

bool APS5_VABI WKBundlePageRegisterScrollOperationCompletionCallback(const void* pageRef, void (*callback)(void*), bool expectWheelEndOrCancel, bool expectMomentumEnd, void* context) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    if (!callback || !page->isMonitoringScrollOperations) {
        return false;
    }
    page->scrollOperationCompletionCallback = callback;
    page->expectWheelEndOrCancel = expectWheelEndOrCancel;
    page->expectMomentumEnd = expectMomentumEnd;
    page->scrollOperationCompletionContext = context;
    return true;
}

void APS5_VABI WKBundlePageCallAfterTasksAndTimers(const void* pageRef, void (*callback)(void*), void* context) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    if (!callback) {
        return;
    }
    page->tasksAndTimersCallback = callback;
    page->tasksAndTimersContext = context;
}

bool APS5_VABI WKBundlePageCanHandleRequest(const void* urlRequestRef) {
    if (!urlRequestRef) {
        return false;
    }
    const auto* object = NKWebKit::ToObject(urlRequestRef, __func__);
    if (object->Type() != NKWebKit::ObjectType::URLRequest) {
        return false;
    }
    const auto* request = static_cast<const NKWebKit::URLRequest*>(object);
    return request->url && !request->url->text.empty();
}

const void* APS5_VABI WKBundlePageCopyGroupIdentifier(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(page->groupIdentifier ? page->groupIdentifier->characters : std::u16string()));
}

}
