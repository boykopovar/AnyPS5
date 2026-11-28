#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/InspectorCallStackAbi.hpp"

extern "C" {

void APS5_VABI _ZN9Inspector15AsyncStackTrace18didCancelAsyncCallEv(NKWebKit::InspectorAbi::AsyncStackTrace* self);
void APS5_VABI _ZN9Inspector15AsyncStackTrace20didDispatchAsyncCallEv(NKWebKit::InspectorAbi::AsyncStackTrace* self);
void APS5_VABI _ZN9Inspector15AsyncStackTrace21willDispatchAsyncCallEm(NKWebKit::InspectorAbi::AsyncStackTrace* self, std::size_t maxDepth);
void APS5_VABI _ZN9Inspector15AsyncStackTrace6removeEv(NKWebKit::InspectorAbi::AsyncStackTrace* self);
void APS5_VABI _ZN9Inspector15AsyncStackTrace8truncateEm(NKWebKit::InspectorAbi::AsyncStackTrace* self, std::size_t maxDepth);
void APS5_VABI _ZN9Inspector15AsyncStackTraceD1Ev(NKWebKit::InspectorAbi::AsyncStackTrace* self);
void APS5_VABI _ZN9Inspector15AsyncStackTraceD2Ev(NKWebKit::InspectorAbi::AsyncStackTrace* self);
void APS5_VABI _ZN9Inspector15ScriptCallFrameC1ERKS0_(NKWebKit::InspectorAbi::ScriptCallFrame* self, const NKWebKit::InspectorAbi::ScriptCallFrame* other);
void APS5_VABI _ZN9Inspector15ScriptCallFrameC2ERKS0_(NKWebKit::InspectorAbi::ScriptCallFrame* self, const NKWebKit::InspectorAbi::ScriptCallFrame* other);
void APS5_VABI _ZN9Inspector15ScriptCallFrameD1Ev(NKWebKit::InspectorAbi::ScriptCallFrame* self);
void APS5_VABI _ZN9Inspector15ScriptCallFrameD2Ev(NKWebKit::InspectorAbi::ScriptCallFrame* self);
NKWebKit::InspectorAbi::ScriptCallFrame& APS5_VABI _ZN9Inspector15ScriptCallFrameaSERKS0_(NKWebKit::InspectorAbi::ScriptCallFrame* self, const NKWebKit::InspectorAbi::ScriptCallFrame* other);
NKWebKit::InspectorAbi::ScriptCallStackRef APS5_VABI _ZN9Inspector15ScriptCallStack6createEv();
void APS5_VABI _ZN9Inspector15ScriptCallStackC1Ev(NKWebKit::InspectorAbi::ScriptCallStack* self);
void APS5_VABI _ZN9Inspector15ScriptCallStackC2Ev(NKWebKit::InspectorAbi::ScriptCallStack* self);
void APS5_VABI _ZN9Inspector15ScriptCallStackD1Ev(NKWebKit::InspectorAbi::ScriptCallStack* self);
void APS5_VABI _ZN9Inspector15ScriptCallStackD2Ev(NKWebKit::InspectorAbi::ScriptCallStack* self);
bool APS5_VABI _ZNK9Inspector15AsyncStackTrace8isLockedEv(const NKWebKit::InspectorAbi::AsyncStackTrace* self);
bool APS5_VABI _ZNK9Inspector15AsyncStackTrace9isPendingEv(const NKWebKit::InspectorAbi::AsyncStackTrace* self);
bool APS5_VABI _ZNK9Inspector15ScriptCallFrame8isNativeEv(const NKWebKit::InspectorAbi::ScriptCallFrame* self);
const NKWebKit::InspectorAbi::ScriptCallFrame* APS5_VABI _ZNK9Inspector15ScriptCallStack23firstNonNativeCallFrameEv(const NKWebKit::InspectorAbi::ScriptCallStack* self);
const NKWebKit::InspectorAbi::ScriptCallFrame& APS5_VABI _ZNK9Inspector15ScriptCallStack2atEm(const NKWebKit::InspectorAbi::ScriptCallStack* self, std::size_t index);
bool APS5_VABI _ZNK9Inspector15ScriptCallStack7isEqualEPS0_(const NKWebKit::InspectorAbi::ScriptCallStack* self, NKWebKit::InspectorAbi::ScriptCallStack* other);

}
