#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/DisplayListSetStateAbi.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore11DisplayList8SetStateC1ERKNS_20GraphicsContextStateEN3WTF9OptionSetINS2_6ChangeEEE(NKWebKit::DisplayListSetStateAbi::SetState* self, const NKWebKit::DisplayListSetStateAbi::State* state, std::uint32_t changeFlags);
void APS5_VABI _ZN7WebCore11DisplayList8SetStateC2ERKNS_20GraphicsContextStateEN3WTF9OptionSetINS2_6ChangeEEE(NKWebKit::DisplayListSetStateAbi::SetState* self, const NKWebKit::DisplayListSetStateAbi::State* state, std::uint32_t changeFlags);
void APS5_VABI _ZN7WebCore11DisplayList8SetStateC1ERKNS_26GraphicsContextStateChangeE(NKWebKit::DisplayListSetStateAbi::SetState* self, const NKWebKit::DisplayListSetStateAbi::StateChange* change);
void APS5_VABI _ZN7WebCore11DisplayList8SetStateC2ERKNS_26GraphicsContextStateChangeE(NKWebKit::DisplayListSetStateAbi::SetState* self, const NKWebKit::DisplayListSetStateAbi::StateChange* change);
void APS5_VABI _ZN7WebCore11DisplayList8SetStateD0Ev(NKWebKit::DisplayListSetStateAbi::SetState* self);
void APS5_VABI _ZN7WebCore11DisplayList8SetStateD1Ev(NKWebKit::DisplayListSetStateAbi::SetState* self);
void APS5_VABI _ZN7WebCore11DisplayList8SetStateD2Ev(NKWebKit::DisplayListSetStateAbi::SetState* self);

}
