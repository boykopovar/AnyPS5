#include "prx/libSceNotification/include/Api.hpp"
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNotificationSend(int type, std::uint8_t flag, std::uint8_t* message) {
    (void)type;
    (void)flag;
    (void)message;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNotificationSendById(int type, std::uint8_t flag, std::uint8_t* id, std::uint8_t* jsonData) {
    (void)type;
    (void)flag;
    (void)id;
    (void)jsonData;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNotificationShowPsButtonPersistentBanner(std::uint8_t* jsonParam) {
    (void)jsonParam;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceNotificationHidePsButtonPersistentBanner() {
    NotImplemented_nid_no_patch(__func__);
}

}
