#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/ParallelEnvironmentAbi.hpp"

extern "C" {

void APS5_VABI _ZN3WTF19ParallelEnvironmentC1EPFvPvEmi(
    NKWebKit::WtfAbi::ParallelEnvironment* self, NKWebKit::WtfAbi::ParallelEnvironmentFunction threadFunction, std::size_t sizeOfParameter, int requestedJobNumber);
void APS5_VABI _ZN3WTF19ParallelEnvironmentC2EPFvPvEmi(
    NKWebKit::WtfAbi::ParallelEnvironment* self, NKWebKit::WtfAbi::ParallelEnvironmentFunction threadFunction, std::size_t sizeOfParameter, int requestedJobNumber);
void APS5_VABI _ZN3WTF19ParallelEnvironment7executeEPv(NKWebKit::WtfAbi::ParallelEnvironment* self, void* parameters);

}
