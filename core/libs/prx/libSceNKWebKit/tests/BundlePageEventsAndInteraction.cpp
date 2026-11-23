#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "BundlePageEventsAndInteraction check failed at line %u\n", location.line());
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

void OnScrollCompleted(void* context) {
    auto* flag = static_cast<bool*>(context);
    if (flag) {
        *flag = true;
    }
}

void OnTasksAndTimers(void* context) {
    auto* counter = static_cast<int*>(context);
    if (counter) {
        ++(*counter);
    }
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::BundlePage>();
    const void* pageRef = page.Get();

    Require(page->simulatedMouseDownCount == 0);
    Require(page->simulatedMouseMotionCount == 0);
    Require(page->simulatedMouseUpCount == 0);
    Require(!page->isMonitoringScrollOperations);

    NKWebKit::PointValue downPos{120.5, 240.25};
    WKBundlePageSimulateMouseDown(pageRef, 1, downPos, 2, 0x4u, 1.0);
    Require(page->simulatedMouseDownCount == 1);
    Require(page->lastMouseButton == 1);
    Require(page->lastMousePosition.x == 120.5 && page->lastMousePosition.y == 240.25);
    Require(page->lastMouseClickCount == 2);
    Require(page->lastMouseModifiers == 0x4u);
    Require(page->lastSyntheticClickType == 1.0);

    NKWebKit::PointValue movePos{150.0, 300.0};
    WKBundlePageSimulateMouseMotion(pageRef, movePos, 0x2u);
    Require(page->simulatedMouseMotionCount == 1);
    Require(page->lastMousePosition.x == 150.0 && page->lastMousePosition.y == 300.0);
    Require(page->lastMouseModifiers == 0x2u);

    NKWebKit::PointValue upPos{160.0, 310.0};
    WKBundlePageSimulateMouseUp(pageRef, 1, upPos, 2, 0x1u);
    Require(page->simulatedMouseUpCount == 1);
    Require(page->lastMouseButton == 1);
    Require(page->lastMousePosition.x == 160.0 && page->lastMousePosition.y == 310.0);
    Require(page->lastMouseClickCount == 2);
    Require(page->lastMouseModifiers == 0x1u);

    bool scrollCalled = false;
    Require(!WKBundlePageRegisterScrollOperationCompletionCallback(pageRef, OnScrollCompleted, true, false, &scrollCalled));
    Require(!scrollCalled);

    WKBundlePageStartMonitoringScrollOperations(pageRef, true);
    Require(page->isMonitoringScrollOperations);
    Require(page->clearLatchingState);

    Require(!WKBundlePageRegisterScrollOperationCompletionCallback(pageRef, nullptr, true, false, &scrollCalled));

    Require(WKBundlePageRegisterScrollOperationCompletionCallback(pageRef, OnScrollCompleted, true, false, &scrollCalled));
    Require(!scrollCalled);
    Require(page->expectWheelEndOrCancel);
    Require(!page->expectMomentumEnd);
    Require(page->scrollOperationCompletionCallback == OnScrollCompleted);
    Require(page->scrollOperationCompletionContext == &scrollCalled);

    WKBundlePageStartMonitoringScrollOperations(pageRef, false);
    Require(page->isMonitoringScrollOperations);
    Require(!page->clearLatchingState);

    int timerCounter = 0;
    WKBundlePageCallAfterTasksAndTimers(pageRef, OnTasksAndTimers, &timerCounter);
    Require(page->tasksAndTimersCallback == OnTasksAndTimers);
    Require(page->tasksAndTimersContext == &timerCounter);
    Require(timerCounter == 0);

    WKBundlePageCallAfterTasksAndTimers(pageRef, nullptr, nullptr);

    auto emptyRequest = NKWebKit::Make<NKWebKit::URLRequest>();
    Require(!WKBundlePageCanHandleRequest(emptyRequest.Get()));
    Require(!WKBundlePageCanHandleRequest(nullptr));

    auto validUrl = NKWebKit::Make<NKWebKit::URL>("https://example.com");
    auto validRequest = NKWebKit::Make<NKWebKit::URLRequest>(validUrl);
    Require(WKBundlePageCanHandleRequest(validRequest.Get()));

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    Require(!WKBundlePageCanHandleRequest(frame.Get()));

    const void* emptyGroupRef = WKBundlePageCopyGroupIdentifier(pageRef);
    Require(emptyGroupRef != nullptr);
    auto* emptyGroup = NKWebKit::Cast<NKWebKit::String>(emptyGroupRef, "TestEmptyGroup");
    Require(emptyGroup->characters.empty());

    page->groupIdentifier = NKWebKit::Make<NKWebKit::String>(std::u16string(u"TestGroup123"));
    const void* groupRef = WKBundlePageCopyGroupIdentifier(pageRef);
    Require(groupRef != nullptr);
    auto* group = NKWebKit::Cast<NKWebKit::String>(groupRef, "TestGroup");
    Require(group->characters == u"TestGroup123");

    RequireInvalidArgument([&] { WKBundlePageSimulateMouseDown(nullptr, 0, downPos, 1, 0, 0.0); });
    RequireInvalidArgument([&] { WKBundlePageSimulateMouseMotion(nullptr, movePos, 0); });
    RequireInvalidArgument([&] { WKBundlePageSimulateMouseUp(nullptr, 0, upPos, 1, 0); });
    RequireInvalidArgument([&] { WKBundlePageStartMonitoringScrollOperations(nullptr, true); });
    RequireInvalidArgument([&] { WKBundlePageRegisterScrollOperationCompletionCallback(nullptr, OnScrollCompleted, true, false, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageCallAfterTasksAndTimers(nullptr, OnTasksAndTimers, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageCopyGroupIdentifier(nullptr); });

    RequireInvalidArgument([&] { WKBundlePageSimulateMouseDown(frame.Get(), 0, downPos, 1, 0, 0.0); });
    RequireInvalidArgument([&] { WKBundlePageSimulateMouseMotion(frame.Get(), movePos, 0); });
    RequireInvalidArgument([&] { WKBundlePageSimulateMouseUp(frame.Get(), 0, upPos, 1, 0); });
    RequireInvalidArgument([&] { WKBundlePageStartMonitoringScrollOperations(frame.Get(), true); });
    RequireInvalidArgument([&] { WKBundlePageRegisterScrollOperationCompletionCallback(frame.Get(), OnScrollCompleted, true, false, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageCallAfterTasksAndTimers(frame.Get(), OnTasksAndTimers, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageCopyGroupIdentifier(frame.Get()); });

    return 0;
}
