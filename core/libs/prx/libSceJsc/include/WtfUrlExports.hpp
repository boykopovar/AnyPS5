#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfUrlAbi.hpp"

extern "C" {

Jsc::WtfAbi::OptionalUInt16 APS5_VABI _ZNK3WTF3URL4portEv(const Jsc::WtfAbi::URL* self);
Jsc::WtfAbi::StringView APS5_VABI _ZNK3WTF3URL5queryEv(const Jsc::WtfAbi::URL* self);
bool APS5_VABI _ZNK3WTF3URL11isLocalFileEv(const Jsc::WtfAbi::URL* self);
bool APS5_VABI _ZNK3WTF3URL15protocolIsAboutEv(const Jsc::WtfAbi::URL* self);
Jsc::WtfAbi::StringView APS5_VABI _ZNK3WTF3URL38stringWithoutQueryOrFragmentIdentifierEv(const Jsc::WtfAbi::URL* self);
Jsc::WtfAbi::OptionalUInt16 APS5_VABI _ZN3WTF22defaultPortForProtocolENS_10StringViewE(Jsc::WtfAbi::StringView protocol);
bool APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(const Jsc::WtfAbi::TimeWithDynamicClockType* self, const Jsc::WtfAbi::TimeWithDynamicClockType* other);
Jsc::WtfAbi::TimeWithDynamicClockType APS5_VABI _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(const Jsc::WtfAbi::Seconds* self, const Jsc::WtfAbi::TimeWithDynamicClockType* other);
bool APS5_VABI _ZNK3WTF13StringBuilder10isAllASCIIEv(const Jsc::WtfAbi::StringBuilder* self);

}
