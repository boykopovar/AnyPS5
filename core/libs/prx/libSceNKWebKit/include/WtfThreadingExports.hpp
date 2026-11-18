#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

namespace NKWebKit::WtfAbi {

struct Mutex {
    PthreadMutex mutex;
};

struct ThreadCondition {
    PthreadCond condition;
};

static_assert(sizeof(Mutex) == 8);
static_assert(sizeof(ThreadCondition) == 8);

}

extern "C" {

void APS5_VABI _ZN3WTF5MutexC1Ev(NKWebKit::WtfAbi::Mutex* self);
void APS5_VABI _ZN3WTF5MutexC2Ev(NKWebKit::WtfAbi::Mutex* self);
void APS5_VABI _ZN3WTF5MutexD1Ev(NKWebKit::WtfAbi::Mutex* self);
void APS5_VABI _ZN3WTF5MutexD2Ev(NKWebKit::WtfAbi::Mutex* self);
void APS5_VABI _ZN3WTF5Mutex4lockEv(NKWebKit::WtfAbi::Mutex* self);
bool APS5_VABI _ZN3WTF5Mutex7tryLockEv(NKWebKit::WtfAbi::Mutex* self);
void APS5_VABI _ZN3WTF5Mutex6unlockEv(NKWebKit::WtfAbi::Mutex* self);

void APS5_VABI _ZN3WTF15ThreadConditionD1Ev(NKWebKit::WtfAbi::ThreadCondition* self);
void APS5_VABI _ZN3WTF15ThreadConditionD2Ev(NKWebKit::WtfAbi::ThreadCondition* self);
void APS5_VABI _ZN3WTF15ThreadCondition4waitERNS_5MutexE(NKWebKit::WtfAbi::ThreadCondition* self, NKWebKit::WtfAbi::Mutex* mutex);
bool APS5_VABI _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(NKWebKit::WtfAbi::ThreadCondition* self, NKWebKit::WtfAbi::Mutex* mutex, WTF::WallTime absoluteTime);
void APS5_VABI _ZN3WTF15ThreadCondition6signalEv(NKWebKit::WtfAbi::ThreadCondition* self);
void APS5_VABI _ZN3WTF15ThreadCondition9broadcastEv(NKWebKit::WtfAbi::ThreadCondition* self);

}
