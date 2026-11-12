#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/LogMacros.hpp"

#include "prx/libSceNotification/include/Api.hpp"

namespace {

bool ReadGuestMessage_nid_no_patch(const std::uint8_t* source, std::size_t capacity, std::string& destination) {
    if (source == nullptr) return false;
    const int savedErrno = errno;
    const int result = CopyGuestString_nid_no_patch(reinterpret_cast<const char*>(source), capacity + 1, destination);
    errno = savedErrno;
    return result == 0 && !destination.empty();
}

}

extern "C" {

int APS5_VABI sceNotificationSend(int type, std::uint8_t flag, std::uint8_t* message) {
    std::string text;
    if (!ReadGuestMessage_nid_no_patch(message, Notification::MessageCapacity, text)) return Notification::InvalidMessage;
    APS5_LOG_OUT("type=%d flag=%u message=%s", type, flag, text.c_str());
    return 0;
}

int APS5_VABI sceNotificationSendById(int type, std::uint8_t flag, std::uint8_t* id, std::uint8_t* jsonData) {
    std::string name;
    std::string body;
    if (!ReadGuestMessage_nid_no_patch(id, Notification::IdCapacity, name) ||
        !ReadGuestMessage_nid_no_patch(jsonData, Notification::MessageCapacity, body))
        return Notification::InvalidMessage;
    APS5_LOG_OUT("type=%d flag=%u id=%s json=%s", type, flag, name.c_str(), body.c_str());
    return 0;
}

int APS5_VABI sceNotificationShowPsButtonPersistentBanner(std::uint8_t* jsonParam) {
    const auto* json = jsonParam != nullptr ? reinterpret_cast<const char*>(jsonParam) : "{}";
    APS5_LOG_OUT("banner=%s", json);
    return 0;
}

void APS5_VABI sceNotificationHidePsButtonPersistentBanner() {
}

}
