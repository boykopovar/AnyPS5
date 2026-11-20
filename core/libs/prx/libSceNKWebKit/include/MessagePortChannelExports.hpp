#pragma once

#include "prx/libSceNKWebKit/src/MessagePortChannelAbi.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

NKWebKit::WebCoreAbi::OptionalProcessIdentifier APS5_VABI _ZN7WebCore18MessagePortChannel14processForPortERKNS_21MessagePortIdentifierE(NKWebKit::WebCoreAbi::MessagePortChannel* self, const NKWebKit::WebCoreAbi::MessagePortIdentifier* port);
bool APS5_VABI _ZNK7WebCore18MessagePortChannel31hasAnyMessagesPendingOrInFlightEv(const NKWebKit::WebCoreAbi::MessagePortChannel* self);

}
