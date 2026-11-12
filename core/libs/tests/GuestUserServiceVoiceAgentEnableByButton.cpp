#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch(int userId, bool enable);
}

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

template <typename TCall>
bool ReportsNotImplemented(const char* expectedFunc, TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(expectedFunc) + " not implemented";
    }
    return false;
}

} // namespace

int main() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);

    for (int user : {USER_SERVICE_INITIAL_USER_ID, 0x10000001, 0x10000002, 0x10000003}) {
        Require(ReportsNotImplemented("sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch", [user] {
            sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch(user, true);
        }));
        Require(ReportsNotImplemented("sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch", [user] {
            sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch(user, false);
        }));
    }

    for (int invalidUser : {-1, 0, 1, 100, 0x20000000, USER_SERVICE_USER_ID_INVALID}) {
        Require(ReportsNotImplemented("sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch", [invalidUser] {
            sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch(invalidUser, true);
        }));
        Require(ReportsNotImplemented("sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch", [invalidUser] {
            sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch(invalidUser, false);
        }));
    }

    Require(ReportsNotImplemented("sceUserServiceGetVoiceAgentEnableByButton", [] {
        sceUserServiceGetVoiceAgentEnableByButton();
    }));

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);

    Require(ReportsNotImplemented("sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch", [] {
        sceUserServiceSetVoiceAgentEnableByButton_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, true);
    }));
    Require(ReportsNotImplemented("sceUserServiceGetVoiceAgentEnableByButton", [] {
        sceUserServiceGetVoiceAgentEnableByButton();
    }));

    return 0;
}
