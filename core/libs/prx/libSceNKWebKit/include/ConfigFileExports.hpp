#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/ConfigFileAbi.hpp"

extern "C" {

void APS5_VABI _ZN3JSC10ConfigFileC1EPKc(NKWebKit::JscAbi::ConfigFile* self, const char* filename);
void APS5_VABI _ZN3JSC10ConfigFileC2EPKc(NKWebKit::JscAbi::ConfigFile* self, const char* filename);
void APS5_VABI _ZN3JSC10ConfigFile5parseEv(NKWebKit::JscAbi::ConfigFile* self);
void APS5_VABI _ZN3JSC10ConfigFile14setProcessNameEPKc(const char* processName);
void APS5_VABI _ZN3JSC10ConfigFile20setParentProcessNameEPKc(const char* parentProcessName);

}
