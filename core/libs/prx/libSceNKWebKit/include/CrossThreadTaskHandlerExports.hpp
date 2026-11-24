#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/CrossThreadTaskHandlerAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfFunctionAbi.hpp"

extern "C" {

void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerC1EPKcNS0_25AutodrainedPoolForRunLoopE(NKWebKit::WtfAbi::CrossThreadTaskHandler* self, const char* threadName, int useAutodrainedPool);
void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerC2EPKcNS0_25AutodrainedPoolForRunLoopE(NKWebKit::WtfAbi::CrossThreadTaskHandler* self, const char* threadName, int useAutodrainedPool);
void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerD0Ev(NKWebKit::WtfAbi::CrossThreadTaskHandler* self);
void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerD1Ev(NKWebKit::WtfAbi::CrossThreadTaskHandler* self);
void APS5_VABI _ZN3WTF22CrossThreadTaskHandlerD2Ev(NKWebKit::WtfAbi::CrossThreadTaskHandler* self);
void APS5_VABI _ZN3WTF22CrossThreadTaskHandler8postTaskEONS_15CrossThreadTaskE(NKWebKit::WtfAbi::CrossThreadTaskHandler* self, NKWebKit::WtfAbi::VoidFunction* task);
void APS5_VABI _ZN3WTF22CrossThreadTaskHandler13postTaskReplyEONS_15CrossThreadTaskE(NKWebKit::WtfAbi::CrossThreadTaskHandler* self, NKWebKit::WtfAbi::VoidFunction* task);
void APS5_VABI _ZN3WTF22CrossThreadTaskHandler4killEv(NKWebKit::WtfAbi::CrossThreadTaskHandler* self);

}
