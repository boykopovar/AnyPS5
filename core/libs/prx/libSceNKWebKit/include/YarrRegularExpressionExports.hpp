#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/YarrRegularExpressionExports.hpp"

extern "C" {
void APS5_VABI _ZN3JSC4Yarr17RegularExpressionD1Ev(Jsc::YarrAbi::RegularExpression* self);
void APS5_VABI _ZN3JSC4Yarr17RegularExpressionD2Ev(Jsc::YarrAbi::RegularExpression* self);
Jsc::YarrAbi::RegularExpression* APS5_VABI _ZN3JSC4Yarr17RegularExpressionaSERKS1_(Jsc::YarrAbi::RegularExpression* self, const Jsc::YarrAbi::RegularExpression* other);
}
