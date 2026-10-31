#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI _ZN17SceCloudMessaging19UnregisterMessagingEPKNS_21MessagingRegisterInfoE(void);
int APS5_VABI _ZN17SceCloudMessaging10InitializeEi(void);
int APS5_VABI _ZN17SceCloudMessaging13CreateMessageEPKNS_7MessageEPj(void);
int APS5_VABI _ZN17SceCloudMessaging16SendMessageAsyncEjPKNS_17MessageResultInfoE(void);
int APS5_VABI _ZN17SceCloudMessaging9TerminateEv(void);
int APS5_VABI _ZN17SceCloudMessaging17RegisterMessagingEPKNS_21MessagingRegisterInfoE(void);

}
