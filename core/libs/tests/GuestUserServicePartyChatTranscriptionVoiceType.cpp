#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

extern "C" {
int APS5_VABI sceUserServiceGetPartyChatTranscriptionVoiceType_nid_no_patch();
}

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void AuditPartyChatTranscriptionUnimplemented() {
    bool threwGetVoice = false;
    try {
        sceUserServiceGetPartyChatTranscriptionVoiceType_nid_no_patch();
    } catch (const std::runtime_error&) {
        threwGetVoice = true;
    }
    Require(threwGetVoice);

    bool threwSetVoice = false;
    try {
        sceUserServiceSetPartyChatTranscriptionVoiceType();
    } catch (const std::runtime_error&) {
        threwSetVoice = true;
    }
    Require(threwSetVoice);

    bool threwGetLang = false;
    try {
        sceUserServiceGetPartyChatTranscriptionLanguageType();
    } catch (const std::runtime_error&) {
        threwGetLang = true;
    }
    Require(threwGetLang);

    bool threwSetLang = false;
    try {
        sceUserServiceSetPartyChatTranscriptionLanguageType();
    } catch (const std::runtime_error&) {
        threwSetLang = true;
    }
    Require(threwSetLang);

    bool threwGetFlag = false;
    try {
        sceUserServiceGetPartyChatTranscriptionFlag();
    } catch (const std::runtime_error&) {
        threwGetFlag = true;
    }
    Require(threwGetFlag);

    bool threwSetFlag = false;
    try {
        sceUserServiceSetPartyChatTranscriptionFlag();
    } catch (const std::runtime_error&) {
        threwSetFlag = true;
    }
    Require(threwSetFlag);
}

void AuditChatTranscriptionAccessibilityBounds() {
    Require(sceUserServiceGetAccessibilityChatTranscription(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    std::int32_t voiceType = -1;
    Require(sceUserServiceGetAccessibilityChatTranscription(USER_SERVICE_INITIAL_USER_ID, &voiceType) == USER_SERVICE_OK);
    Require(voiceType == 0);

    for (const int userId : {USER_SERVICE_USER_ID_INVALID,
                             0,
                             1,
                             123,
                             -1,
                             -2,
                             -100,
                             0x12345678,
                             USER_SERVICE_INITIAL_USER_ID - 1,
                             USER_SERVICE_INITIAL_USER_ID + 1,
                             USER_SERVICE_INITIAL_USER_ID + 2,
                             USER_SERVICE_INITIAL_USER_ID + 3,
                             USER_SERVICE_INITIAL_USER_ID + 4,
                             USER_SERVICE_INITIAL_USER_ID + 100,
                             -2147483647 - 1,
                             2147483647}) {
        std::int32_t transcription = 0x5a5a;
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, &transcription) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(transcription == 0x5a5a);
    }
}

void AuditMultiUserLoginTransitions() {
    for (int userIndex = 1; userIndex < USER_SERVICE_MAX_USERS; ++userIndex) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + userIndex;
        UserServiceSetUserConnected_nid_postfix(userIndex, true);

        Require(sceUserServiceGetAccessibilityChatTranscription(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        std::int32_t activeTranscription = -1;
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, &activeTranscription) == USER_SERVICE_OK);
        Require(activeTranscription == 0);

        UserServiceSetUserConnected_nid_postfix(userIndex, false);

        std::int32_t unloggedTranscription = 0x5a5a;
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, &unloggedTranscription) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(unloggedTranscription == 0x5a5a);
    }
}

} // namespace

int main() {
    AuditPartyChatTranscriptionUnimplemented();
    AuditChatTranscriptionAccessibilityBounds();
    AuditMultiUserLoginTransitions();
    return 0;
}
