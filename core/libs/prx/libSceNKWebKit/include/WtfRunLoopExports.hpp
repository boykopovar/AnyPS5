#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfFunctionAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfRunLoopAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

extern "C" {

NKWebKit::WtfAbi::RunLoop* APS5_VABI _ZN3WTF7RunLoop4mainEv();
NKWebKit::WtfAbi::RunLoop* APS5_VABI _ZN3WTF7RunLoop7currentEv();
void APS5_VABI _ZN3WTF7RunLoop8dispatchEONS_8FunctionIFvvEEE(NKWebKit::WtfAbi::RunLoop* self, NKWebKit::WtfAbi::VoidFunction* function);
NKWebKit::WtfAbi::RunLoopDispatchTimerRef* APS5_VABI _ZN3WTF7RunLoop13dispatchAfterENS_7SecondsEONS_8FunctionIFvvEEE(NKWebKit::WtfAbi::RunLoopDispatchTimerRef* result, NKWebKit::WtfAbi::RunLoop* self, WTF::Seconds delay, NKWebKit::WtfAbi::VoidFunction* function);
void APS5_VABI _ZN3WTF7RunLoop3runEv();
void APS5_VABI _ZN3WTF7RunLoop4stopEv(NKWebKit::WtfAbi::RunLoop* self);
NKWebKit::WtfAbi::RunLoopCycleResult APS5_VABI _ZN3WTF7RunLoop5cycleEj(unsigned mode);
void APS5_VABI _ZN3WTF7RunLoop17setWakeUpCallbackEONS_8FunctionIFvvEEE(NKWebKit::WtfAbi::VoidFunction* function);
void APS5_VABI _ZN3WTF7RunLoop38suspendFunctionDispatchForCurrentCycleEv(NKWebKit::WtfAbi::RunLoop* self);

void APS5_VABI _ZN3WTF7RunLoop9TimerBaseC2ERS0_(NKWebKit::WtfAbi::RunLoopTimerBase* self, NKWebKit::WtfAbi::RunLoop* runLoop);
void APS5_VABI _ZN3WTF7RunLoop9TimerBaseD1Ev(NKWebKit::WtfAbi::RunLoopTimerBase* self);
void APS5_VABI _ZN3WTF7RunLoop9TimerBaseD2Ev(NKWebKit::WtfAbi::RunLoopTimerBase* self);
void APS5_VABI _ZN3WTF7RunLoop9TimerBase5startENS_7SecondsEb(NKWebKit::WtfAbi::RunLoopTimerBase* self, WTF::Seconds interval, bool repeat);
void APS5_VABI _ZN3WTF7RunLoop9TimerBase4stopEv(NKWebKit::WtfAbi::RunLoopTimerBase* self);
bool APS5_VABI _ZNK3WTF7RunLoop9TimerBase8isActiveEv(const NKWebKit::WtfAbi::RunLoopTimerBase* self);
WTF::Seconds APS5_VABI _ZNK3WTF7RunLoop9TimerBase16secondsUntilFireEv(const NKWebKit::WtfAbi::RunLoopTimerBase* self);

}
