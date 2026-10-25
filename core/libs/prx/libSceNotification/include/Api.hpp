#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNotificationSend(int type, std::uint8_t flag, std::uint8_t* message);
int APS5_VABI sceNotificationSendById(int type, std::uint8_t flag, std::uint8_t* id, std::uint8_t* jsonData);
int APS5_VABI sceNotificationShowPsButtonPersistentBanner(std::uint8_t* jsonParam);
void APS5_VABI sceNotificationHidePsButtonPersistentBanner();

}
