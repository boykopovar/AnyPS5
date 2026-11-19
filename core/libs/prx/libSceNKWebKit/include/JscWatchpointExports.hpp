#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/JscWatchpointAbi.hpp"

extern "C" {

void APS5_VABI _ZN3JSC13WatchpointSetC1ENS_15WatchpointStateE(NKWebKit::JscAbi::WatchpointSet* self, NKWebKit::JscAbi::WatchpointState state);
void APS5_VABI _ZN3JSC13WatchpointSetC2ENS_15WatchpointStateE(NKWebKit::JscAbi::WatchpointSet* self, NKWebKit::JscAbi::WatchpointState state);
void APS5_VABI _ZN3JSC13WatchpointSetD1Ev(NKWebKit::JscAbi::WatchpointSet* self);
void APS5_VABI _ZN3JSC13WatchpointSetD2Ev(NKWebKit::JscAbi::WatchpointSet* self);
void APS5_VABI _ZN3JSC13WatchpointSet11fireAllSlowERNS_2VMEPNS_22DeferredWatchpointFireE(NKWebKit::JscAbi::WatchpointSet* self, void* vm, NKWebKit::JscAbi::DeferredWatchpointFire* deferredWatchpoints);

}
