#include "prx/libSceNpManager/libSceNpManager.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

constexpr int PushNotInitialized = static_cast<int>(0x8055A002u);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

struct Entry {
    decltype(&sceNpPushStartNotification) function;
    const char* name;
};

const Entry entries[] = {
        {libSceNpManagerUnknown_QZJ5fC0tP8M_nid_no_patch, "libSceNpManagerUnknown_QZJ5fC0tP8M_nid_no_patch"},
        {libSceNpManagerUnknown__minus_5ZuQ1HlZNY_nid_no_patch, "libSceNpManagerUnknown__minus_5ZuQ1HlZNY_nid_no_patch"},
        {sceNpPushSetNpCommunicationId, "sceNpPushSetNpCommunicationId"},
        {sceNpPushUnsetNpCommunicationId, "sceNpPushUnsetNpCommunicationId"},
        {sceNpPushIntBeginInactive, "sceNpPushIntBeginInactive"},
        {sceNpPushIntEndInactive, "sceNpPushIntEndInactive"},
        {sceNpPushIntRegisterNotificationPacketCallback, "sceNpPushIntRegisterNotificationPacketCallback"},
        {sceNpPushIntUnregisterNotificationPacketCallback, "sceNpPushIntUnregisterNotificationPacketCallback"},
        {sceNpPushRegisterExtendedDataFilter, "sceNpPushRegisterExtendedDataFilter"},
        {sceNpPushRegisterNotificationExCallback, "sceNpPushRegisterNotificationExCallback"},
        {sceNpPushStartNotification, "sceNpPushStartNotification"},
        {sceNpPushStartNotificationA, "sceNpPushStartNotificationA"},
        {sceNpPushStopNotification, "sceNpPushStopNotification"},
        {sceNpPushStopNotificationA, "sceNpPushStopNotificationA"},
        {sceNpPushUnregisterExtendedDataFilter, "sceNpPushUnregisterExtendedDataFilter"},
        {sceNpPushUnregisterNotificationCallback, "sceNpPushUnregisterNotificationCallback"},
};

void RequireAnswers() {
    for (const Entry& entry : entries) Require(entry.function() == PushNotInitialized, entry.name);
}

void RequireThrows() {
    for (const Entry& entry : entries) {
        try {
            entry.function();
            Require(false, entry.name);
        } catch (const std::runtime_error& error) {
            Require(std::string(error.what()) == std::string(entry.name) + " not implemented", entry.name);
        }
    }
}

} // namespace

int main() {
    RequireAnswers();
    Require(sceNpPushInit() == 0, "sceNpPushInit failed");
    RequireThrows();
    Require(sceNpPushTerm() == 0, "sceNpPushTerm failed");
    RequireAnswers();
    return 0;
}
