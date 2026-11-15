#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemIntCloseStream();
int APS5_VABI sceNpUniversalDataSystemIntDeleteUserData();
int APS5_VABI sceNpUniversalDataSystemIntGetNpCommIdByNpTitleId();
int APS5_VABI sceNpUniversalDataSystemIntGetNpTitleIdListByNpCommId();
int APS5_VABI sceNpUniversalDataSystemIntGetObjectState();
int APS5_VABI sceNpUniversalDataSystemIntOpenObjectsDefinitionConfStream();
int APS5_VABI sceNpUniversalDataSystemIntOpenObjectsDefinitionConfStreamByObjectId();
int APS5_VABI sceNpUniversalDataSystemIntOpenStreamByUri();
int APS5_VABI sceNpUniversalDataSystemIntReadStream();

}
