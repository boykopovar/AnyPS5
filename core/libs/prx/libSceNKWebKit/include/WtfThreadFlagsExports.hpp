#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

namespace NKWebKit::WtfAbi {

enum class GCThreadType : std::uint8_t {
    None = 0,
    Main,
    Helper,
};

}  // namespace NKWebKit::WtfAbi

extern "C" {

bool APS5_VABI _ZN3WTF6Thread27exchangeIsCompilationThreadEb(bool newValue);
bool APS5_VABI _ZN3WTF19isCompilationThreadEv();
void APS5_VABI _ZN3WTF6Thread16registerGCThreadENS_12GCThreadTypeE(NKWebKit::WtfAbi::GCThreadType gcThreadType);
bool APS5_VABI _ZN3WTF6Thread13mayBeGCThreadEv();

}
