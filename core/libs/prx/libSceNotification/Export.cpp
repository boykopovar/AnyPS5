#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/LogMacros.hpp"

#include "prx/libSceNotification/include/Api.hpp"

extern "C" {

int APS5_VABI sceNotificationSend(int type, std::uint8_t flag, std::uint8_t* message) {
    if (message == nullptr) {
        return Notification::InvalidMessage;
    }
    const auto* text = reinterpret_cast<const char*>(message);
    const void* end = std::memchr(text, '\0', Notification::MessageCapacity + 1);
    if (end == nullptr || end == text) {
        return Notification::InvalidMessage;
    }
    APS5_LOG_OUT("type=%d flag=%u message=%s", type, flag, text);
    return 0;
}

int APS5_VABI sceNotificationSendById(int type, std::uint8_t flag, std::uint8_t* id, std::uint8_t* jsonData) {
    if (id == nullptr || jsonData == nullptr) {
        return Notification::InvalidMessage;
    }
    const auto* name = reinterpret_cast<const char*>(id);
    const auto* body = reinterpret_cast<const char*>(jsonData);
    const void* nameEnd = std::memchr(name, '\0', Notification::IdCapacity + 1);
    const void* bodyEnd = std::memchr(body, '\0', Notification::MessageCapacity + 1);
    if (nameEnd == nullptr || nameEnd == name || bodyEnd == nullptr || bodyEnd == body) {
        return Notification::InvalidMessage;
    }
    APS5_LOG_OUT("type=%d flag=%u id=%s json=%s", type, flag, name, body);
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
