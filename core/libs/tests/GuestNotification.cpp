#include "prx/libSceNotification/Notification.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NOTIFICATION: %s\n", message);
        std::abort();
    }
}

std::uint8_t* Pointer(std::string& value) {
    return reinterpret_cast<std::uint8_t*>(value.data());
}

void TestSend() {
    std::string message = "Hello from the guest test";
    Require(sceNotificationSend(0, 0, Pointer(message)) == 0, "send a notification");
    std::string typed = "typed";
    Require(sceNotificationSend(1, 0x5a, Pointer(typed)) == 0, "send a notification of another type and flag");
    std::string longest(Notification::MessageCapacity, 'a');
    Require(sceNotificationSend(0, 0, Pointer(longest)) == 0, "send a message of the full 0x1800 bytes");
    std::string overlong(Notification::MessageCapacity + 1, 'b');
    Require(sceNotificationSend(0, 0, Pointer(overlong)) == Notification::InvalidMessage,
            "send a message over the 0x1800-byte limit");
    std::string empty;
    Require(sceNotificationSend(0, 0, Pointer(empty)) == Notification::InvalidMessage, "send an empty message");
    Require(sceNotificationSend(0, 0, nullptr) == Notification::InvalidMessage, "send a null message");
}

void TestSendById() {
    std::string id = "PPSA99000";
    std::string json = R"({"text":"Installed"})";
    Require(sceNotificationSendById(0, 0, Pointer(id), Pointer(json)) == 0, "send a notification by id");
    std::string longestId(Notification::IdCapacity, 'c');
    std::string longestJson(Notification::MessageCapacity, 'd');
    Require(sceNotificationSendById(0, 0, Pointer(longestId), Pointer(longestJson)) == 0,
            "send by id with a full-size id and body");
    std::string overlongId(Notification::IdCapacity + 1, 'e');
    Require(sceNotificationSendById(0, 0, Pointer(overlongId), Pointer(json)) == Notification::InvalidMessage,
            "send by id with an id over the 0x20-byte limit");
    std::string overlongJson(Notification::MessageCapacity + 1, 'f');
    Require(sceNotificationSendById(0, 0, Pointer(id), Pointer(overlongJson)) == Notification::InvalidMessage,
            "send by id with a body over the 0x1800-byte limit");
    std::string emptyId;
    std::string emptyJson;
    Require(sceNotificationSendById(0, 0, Pointer(emptyId), Pointer(json)) == Notification::InvalidMessage,
            "send by id with an empty id");
    Require(sceNotificationSendById(0, 0, Pointer(id), Pointer(emptyJson)) == Notification::InvalidMessage,
            "send by id with an empty body");
    Require(sceNotificationSendById(0, 0, nullptr, Pointer(json)) == Notification::InvalidMessage,
            "send by id with a null id");
    Require(sceNotificationSendById(0, 0, Pointer(id), nullptr) == Notification::InvalidMessage,
            "send by id with a null body");
}

void TestBanner() {
    std::string config = R"({"text":"Update ready"})";
    Require(sceNotificationShowPsButtonPersistentBanner(Pointer(config)) == 0, "show the persistent banner");
    Require(sceNotificationShowPsButtonPersistentBanner(nullptr) == 0,
            "show the persistent banner with a null configuration");
}

void TestHide() {
    sceNotificationHidePsButtonPersistentBanner();
}

}

int main() {
    TestSend();
    TestSendById();
    TestBanner();
    TestHide();
    return 0;
}
