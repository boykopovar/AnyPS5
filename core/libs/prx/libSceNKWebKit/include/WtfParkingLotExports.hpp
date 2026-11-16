#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfParkingLotAbi.hpp"

extern "C" {

NKWebKit::WtfAbi::ParkResult APS5_VABI _ZN3WTF10ParkingLot21parkConditionallyImplEPKvRKNS_12ScopedLambdaIFbvEEERKNS3_IFvvEEERKNS_24TimeWithDynamicClockTypeE(const void* address, const NKWebKit::WtfAbi::ValidationLambda* validation, const NKWebKit::WtfAbi::BeforeSleepLambda* beforeSleep, const WTF::TimeWithDynamicClockType* timeout);
NKWebKit::WtfAbi::UnparkResult APS5_VABI _ZN3WTF10ParkingLot9unparkOneEPKv(const void* address);
void APS5_VABI _ZN3WTF10ParkingLot13unparkOneImplEPKvRKNS_12ScopedLambdaIFlNS0_12UnparkResultEEEE(const void* address, const NKWebKit::WtfAbi::UnparkCallbackLambda* callback);
unsigned APS5_VABI _ZN3WTF10ParkingLot11unparkCountEPKvj(const void* address, unsigned count);
void APS5_VABI _ZN3WTF10ParkingLot9unparkAllEPKv(const void* address);
void APS5_VABI _ZN3WTF4Lock8lockSlowEv(NKWebKit::WtfAbi::Lock* self);
void APS5_VABI _ZN3WTF4Lock10unlockSlowEv(NKWebKit::WtfAbi::Lock* self);
void APS5_VABI _ZN3WTF4Lock13safepointSlowEv(NKWebKit::WtfAbi::Lock* self);
void APS5_VABI _ZN3WTF8WordLock8lockSlowEv(NKWebKit::WtfAbi::WordLock* self);
void APS5_VABI _ZN3WTF8WordLock10unlockSlowEv(NKWebKit::WtfAbi::WordLock* self);
void APS5_VABI _ZN3WTF13ReadWriteLock8readLockEv(NKWebKit::WtfAbi::ReadWriteLock* self);
void APS5_VABI _ZN3WTF13ReadWriteLock10readUnlockEv(NKWebKit::WtfAbi::ReadWriteLock* self);
void APS5_VABI _ZN3WTF13ReadWriteLock9writeLockEv(NKWebKit::WtfAbi::ReadWriteLock* self);
void APS5_VABI _ZN3WTF13ReadWriteLock11writeUnlockEv(NKWebKit::WtfAbi::ReadWriteLock* self);
void APS5_VABI _ZN3WTF15BinarySemaphore6signalEv(NKWebKit::WtfAbi::BinarySemaphore* self);
bool APS5_VABI _ZN3WTF15BinarySemaphore9waitUntilERKNS_24TimeWithDynamicClockTypeE(NKWebKit::WtfAbi::BinarySemaphore* self, const WTF::TimeWithDynamicClockType* timeout);

}
