#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

namespace NKWebKit::InspectorAbi {

struct FrontendRouter;

struct FrontendDispatcher {
    FrontendRouter* frontendRouter;
};

static_assert(sizeof(FrontendDispatcher) == 8);

}  // namespace NKWebKit::InspectorAbi

extern "C" {

void APS5_VABI _ZN9Inspector21CSSFrontendDispatcher17styleSheetChangedERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* styleSheetId);
void APS5_VABI _ZN9Inspector21CSSFrontendDispatcher17styleSheetRemovedERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* styleSheetId);
void APS5_VABI _ZN9Inspector21CSSFrontendDispatcher23mediaQueryResultChangedEv(const NKWebKit::InspectorAbi::FrontendDispatcher* self);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher15documentUpdatedEv(const NKWebKit::InspectorAbi::FrontendDispatcher* self);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher16attributeRemovedEiRKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int nodeId, const NKWebKit::WtfAbi::String* name);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher16childNodeRemovedEii(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int parentNodeId, int nodeId);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher16shadowRootPoppedEii(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int hostId, int rootId);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher17attributeModifiedEiRKN3WTF6StringES4_(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int nodeId, const NKWebKit::WtfAbi::String* name, const NKWebKit::WtfAbi::String* value);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher19didAddEventListenerEi(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int nodeId);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher20pseudoElementRemovedEii(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int parentId, int pseudoElementId);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher21characterDataModifiedEiRKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int nodeId, const NKWebKit::WtfAbi::String* characterData);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher21childNodeCountUpdatedEii(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int nodeId, int childNodeCount);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher23willRemoveEventListenerEi(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int nodeId);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher25customElementStateChangedEiNS_8Protocol3DOM18CustomElementStateE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int nodeId, int customElementState);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher34powerEfficientPlaybackStateChangedEidb(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int nodeId, double timestamp, bool isPowerEfficient);
void APS5_VABI _ZN9Inspector21DOMFrontendDispatcher7inspectEi(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int nodeId);
void APS5_VABI _ZN9Inspector22HeapFrontendDispatcher16trackingCompleteEdRKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, double timestamp, const NKWebKit::WtfAbi::String* snapshotData);
void APS5_VABI _ZN9Inspector22PageFrontendDispatcher13frameDetachedERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* frameId);
void APS5_VABI _ZN9Inspector22PageFrontendDispatcher14loadEventFiredEd(const NKWebKit::InspectorAbi::FrontendDispatcher* self, double timestamp);
void APS5_VABI _ZN9Inspector22PageFrontendDispatcher19frameStartedLoadingERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* frameId);
void APS5_VABI _ZN9Inspector22PageFrontendDispatcher19frameStoppedLoadingERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* frameId);
void APS5_VABI _ZN9Inspector22PageFrontendDispatcher20domContentEventFiredEd(const NKWebKit::InspectorAbi::FrontendDispatcher* self, double timestamp);
void APS5_VABI _ZN9Inspector22PageFrontendDispatcher24frameScheduledNavigationERKN3WTF6StringEd(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* frameId, double delay);
void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher13canvasRemovedERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* canvasId);
void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher16extensionEnabledERKN3WTF6StringES4_(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* canvasId, const NKWebKit::WtfAbi::String* extension);
void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher16recordingStartedERKN3WTF6StringENS_8Protocol9Recording9InitiatorE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* canvasId, int initiator);
void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher18clientNodesChangedERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* canvasId);
void APS5_VABI _ZN9Inspector24CanvasFrontendDispatcher19canvasMemoryChangedERKN3WTF6StringEd(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* canvasId, double memoryCost);
void APS5_VABI _ZN9Inspector24TargetFrontendDispatcher15targetDestroyedERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* targetId);
void APS5_VABI _ZN9Inspector24TargetFrontendDispatcher25dispatchMessageFromTargetERKN3WTF6StringES4_(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* targetId, const NKWebKit::WtfAbi::String* message);
void APS5_VABI _ZN9Inspector24TargetFrontendDispatcher26didCommitProvisionalTargetERKN3WTF6StringES4_(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* oldTargetId, const NKWebKit::WtfAbi::String* newTargetId);
void APS5_VABI _ZN9Inspector24WorkerFrontendDispatcher13workerCreatedERKN3WTF6StringES4_S4_(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* workerId, const NKWebKit::WtfAbi::String* url, const NKWebKit::WtfAbi::String* name);
void APS5_VABI _ZN9Inspector24WorkerFrontendDispatcher16workerTerminatedERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* workerId);
void APS5_VABI _ZN9Inspector24WorkerFrontendDispatcher25dispatchMessageFromWorkerERKN3WTF6StringES4_(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* workerId, const NKWebKit::WtfAbi::String* message);
void APS5_VABI _ZN9Inspector25NetworkFrontendDispatcher12dataReceivedERKN3WTF6StringEdii(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* requestId, double timestamp, int dataLength, int encodedDataLength);
void APS5_VABI _ZN9Inspector25NetworkFrontendDispatcher15webSocketClosedERKN3WTF6StringEd(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* requestId, double timestamp);
void APS5_VABI _ZN9Inspector25NetworkFrontendDispatcher16webSocketCreatedERKN3WTF6StringES4_(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* requestId, const NKWebKit::WtfAbi::String* url);
void APS5_VABI _ZN9Inspector25NetworkFrontendDispatcher19webSocketFrameErrorERKN3WTF6StringEdS4_(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* requestId, double timestamp, const NKWebKit::WtfAbi::String* errorMessage);
void APS5_VABI _ZN9Inspector26DebuggerFrontendDispatcher19globalObjectClearedEv(const NKWebKit::InspectorAbi::FrontendDispatcher* self);
void APS5_VABI _ZN9Inspector26DebuggerFrontendDispatcher19scriptFailedToParseERKN3WTF6StringES4_iiS4_(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* url, const NKWebKit::WtfAbi::String* scriptSource, int startLine, int errorLine, const NKWebKit::WtfAbi::String* errorMessage);
void APS5_VABI _ZN9Inspector26DebuggerFrontendDispatcher25playBreakpointActionSoundEi(const NKWebKit::InspectorAbi::FrontendDispatcher* self, int breakpointActionId);
void APS5_VABI _ZN9Inspector26DebuggerFrontendDispatcher7resumedEv(const NKWebKit::InspectorAbi::FrontendDispatcher* self);
void APS5_VABI _ZN9Inspector26TimelineFrontendDispatcher16recordingStartedEd(const NKWebKit::InspectorAbi::FrontendDispatcher* self, double startTime);
void APS5_VABI _ZN9Inspector26TimelineFrontendDispatcher16recordingStoppedEd(const NKWebKit::InspectorAbi::FrontendDispatcher* self, double endTime);
void APS5_VABI _ZN9Inspector26TimelineFrontendDispatcher18autoCaptureStartedEv(const NKWebKit::InspectorAbi::FrontendDispatcher* self);
void APS5_VABI _ZN9Inspector27AnimationFrontendDispatcher13targetChangedERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* animationId);
void APS5_VABI _ZN9Inspector27AnimationFrontendDispatcher13trackingStartEd(const NKWebKit::InspectorAbi::FrontendDispatcher* self, double timestamp);
void APS5_VABI _ZN9Inspector27AnimationFrontendDispatcher16trackingCompleteEd(const NKWebKit::InspectorAbi::FrontendDispatcher* self, double timestamp);
void APS5_VABI _ZN9Inspector27AnimationFrontendDispatcher18animationDestroyedERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* animationId);
void APS5_VABI _ZN9Inspector27InspectorFrontendDispatcher25evaluateForTestInFrontendERKN3WTF6StringE(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* script);
void APS5_VABI _ZN9Inspector27LayerTreeFrontendDispatcher18layerTreeDidChangeEv(const NKWebKit::InspectorAbi::FrontendDispatcher* self);
void APS5_VABI _ZN9Inspector32ScriptProfilerFrontendDispatcher13trackingStartEd(const NKWebKit::InspectorAbi::FrontendDispatcher* self, double timestamp);
void APS5_VABI _ZN9Inspector34ApplicationCacheFrontendDispatcher19networkStateUpdatedEb(const NKWebKit::InspectorAbi::FrontendDispatcher* self, bool isNowOnline);
void APS5_VABI _ZN9Inspector34ApplicationCacheFrontendDispatcher29applicationCacheStatusUpdatedERKN3WTF6StringES4_i(const NKWebKit::InspectorAbi::FrontendDispatcher* self, const NKWebKit::WtfAbi::String* frameId, const NKWebKit::WtfAbi::String* manifestURL, int status);

}
