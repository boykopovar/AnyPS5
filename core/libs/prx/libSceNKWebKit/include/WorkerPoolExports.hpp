#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WorkerPoolAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfFunctionAbi.hpp"

extern "C" {

void APS5_VABI _ZN3WTF10WorkerPoolC1ENS_12ASCIILiteralEjNS_7SecondsE(NKWebKit::WtfAbi::WorkerPool* self, const char* name, unsigned numberOfWorkers, double timeout);
void APS5_VABI _ZN3WTF10WorkerPoolC2ENS_12ASCIILiteralEjNS_7SecondsE(NKWebKit::WtfAbi::WorkerPool* self, const char* name, unsigned numberOfWorkers, double timeout);
void APS5_VABI _ZN3WTF10WorkerPoolD1Ev(NKWebKit::WtfAbi::WorkerPool* self);
void APS5_VABI _ZN3WTF10WorkerPoolD2Ev(NKWebKit::WtfAbi::WorkerPool* self);
void APS5_VABI _ZN3WTF10WorkerPool8postTaskEONS_8FunctionIFvvEEE(NKWebKit::WtfAbi::WorkerPool* self, NKWebKit::WtfAbi::VoidFunction* task);

}
