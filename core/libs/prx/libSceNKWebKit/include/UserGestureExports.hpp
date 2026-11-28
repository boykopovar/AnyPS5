#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"
#include <atomic>

namespace WebCore {
struct Node;
struct UserGestureToken;
}  // namespace WebCore

namespace NKWebKit::UserGesture {
WebCore::UserGestureToken*& CurrentToken();
WTF::Seconds& MaximumIntervalForUserGestureForwardingForFetch();
bool& ProcessingUserTypingGesture();
WebCore::Node*& FocusedNodeAtGestureStart();
}  // namespace NKWebKit::UserGesture

namespace NKWebKit::Workers {
std::atomic<unsigned>& WorkerThreadCounter();
}  // namespace NKWebKit::Workers

extern "C" {

bool APS5_VABI _ZN7WebCore20UserGestureIndicator21processingUserGestureEv();
bool APS5_VABI _ZN7WebCore20UserGestureIndicator29processingUserGestureForMediaEv();
WebCore::UserGestureToken** APS5_VABI _ZN7WebCore20UserGestureIndicator18currentUserGestureEv(WebCore::UserGestureToken** result);
void APS5_VABI _ZN7WebCore16UserGestureToken60setMaximumIntervalForUserGestureForwardingForFetchForTestingEN3WTF7SecondsE(WTF::Seconds value);
bool APS5_VABI _ZN7WebCore26UserTypingGestureIndicator27processingUserTypingGestureEv();
WebCore::Node* APS5_VABI _ZN7WebCore26UserTypingGestureIndicator28focusedElementAtGestureStartEv();
unsigned APS5_VABI _ZN7WebCore12WorkerThread17workerThreadCountEv();
bool APS5_VABI WKBundleIsProcessingUserGesture(const void* bundle);

}
