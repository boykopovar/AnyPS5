#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

namespace NKWebKit::JscClassInfoAbi {

struct ClassInfoHead {
    const char* className;
    const ClassInfoHead* parentClass;
};

static_assert(offsetof(ClassInfoHead, className) == 0);
static_assert(offsetof(ClassInfoHead, parentClass) == 8);

}

extern "C" {

void APS5_VABI _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(const NKWebKit::JscClassInfoAbi::ClassInfoHead* self, Jsc::WtfAbi::PrintStream* out);

}
