#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

Jsc::WtfAbi::PrintStream& APS5_VABI _ZN3WTF8dataFileEv();
void APS5_VABI _ZN3WTF15FilePrintStreamD0Ev(Jsc::WtfAbi::FilePrintStream* self);
void APS5_VABI _ZN3WTF15FilePrintStreamD1Ev(Jsc::WtfAbi::FilePrintStream* self);
void APS5_VABI _ZN3WTF15FilePrintStreamD2Ev(Jsc::WtfAbi::FilePrintStream* self);

}
