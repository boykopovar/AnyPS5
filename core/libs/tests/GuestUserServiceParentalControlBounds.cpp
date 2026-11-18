#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename F>
void RequireNotImplemented(const char* expectedName, F&& func) {
    try {
        func();
        std::abort();
    } catch (const std::runtime_error& err) {
        const std::string_view message = err.what();
        const std::string_view expected = expectedName;
        if (message.find(expected) == std::string_view::npos ||
            message.find("not implemented") == std::string_view::npos) {
            std::abort();
        }
    }
}

void TestAgeLevelBounds() {
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    std::uint32_t ageLevel = 0x5a5a5a5au;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &ageLevel) == USER_SERVICE_OK);
    Require(ageLevel == 0xffffffffu);

    UserServiceSetUserConnected_nid_postfix(0, false);
    ageLevel = 0x12345678u;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(ageLevel == 0x12345678u);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

    UserServiceSetUserConnected_nid_postfix(0, true);
    ageLevel = 0;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &ageLevel) == USER_SERVICE_OK);
    Require(ageLevel == 0xffffffffu);

    for (int slot = 1; slot < USER_SERVICE_MAX_USERS; ++slot) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + slot;

        ageLevel = 0xa5a5a5a5u;
        Require(sceUserServiceGetAgeLevel(userId, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(ageLevel == 0xa5a5a5a5u);
        Require(sceUserServiceGetAgeLevel(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        UserServiceSetUserConnected_nid_postfix(slot, true);
        ageLevel = 0;
        Require(sceUserServiceGetAgeLevel(userId, &ageLevel) == USER_SERVICE_OK);
        Require(ageLevel == 0xffffffffu);
        Require(sceUserServiceGetAgeLevel(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        UserServiceSetUserConnected_nid_postfix(slot, false);
        ageLevel = 0xbeefcafeu;
        Require(sceUserServiceGetAgeLevel(userId, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(ageLevel == 0xbeefcafeu);
        Require(sceUserServiceGetAgeLevel(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    }

    const int invalidUserIds[] = {
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS + 1,
        USER_SERVICE_INITIAL_USER_ID + 100,
        USER_SERVICE_USER_ID_INVALID,
        0,
        1,
        -1,
        -100,
        0x12345678,
        0x20000000,
        0x30000000,
        0x7fffffff,
        static_cast<int>(0x80000000u),
        static_cast<int>(0xffffffffu)
    };

    for (int invalidId : invalidUserIds) {
        ageLevel = 0xdeadfadeu;
        Require(sceUserServiceGetAgeLevel(invalidId, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(ageLevel == 0xdeadfadeu);
        Require(sceUserServiceGetAgeLevel(invalidId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    }
}

void TestParentalControlUnimplementedAPIs() {
    RequireNotImplemented("sceUserServiceSetParentalDvdRegion", [] {
        return sceUserServiceSetParentalDvdRegion();
    });
    RequireNotImplemented("sceUserServiceSetParentalBdAge", [] {
        return sceUserServiceSetParentalBdAge();
    });
    RequireNotImplemented("sceUserServiceSetParentalGameAgeLevel", [] {
        return sceUserServiceSetParentalGameAgeLevel();
    });
    RequireNotImplemented("sceUserServiceSetParentalBrowser", [] {
        return sceUserServiceSetParentalBrowser();
    });
    RequireNotImplemented("sceUserServiceGetParentalDvdRegion", [] {
        return sceUserServiceGetParentalDvdRegion();
    });
    RequireNotImplemented("sceUserServiceGetParentalContentControl", [] {
        return sceUserServiceGetParentalContentControl();
    });
    RequireNotImplemented("sceUserServiceGetParentalDvd", [] {
        return sceUserServiceGetParentalDvd();
    });
    RequireNotImplemented("sceUserServiceSetParentalGameWhiteList", [] {
        return sceUserServiceSetParentalGameWhiteList();
    });
    RequireNotImplemented("sceUserServiceSetParentalMorpheus", [] {
        return sceUserServiceSetParentalMorpheus();
    });
    RequireNotImplemented("sceUserServiceSetParentalContentControl", [] {
        return sceUserServiceSetParentalContentControl();
    });
    RequireNotImplemented("sceUserServiceGetParentalBrowser", [] {
        return sceUserServiceGetParentalBrowser();
    });
    RequireNotImplemented("sceUserServiceSetParentalDvd", [] {
        return sceUserServiceSetParentalDvd();
    });
    RequireNotImplemented("sceUserServiceSetParentalGame", [] {
        return sceUserServiceSetParentalGame();
    });
    RequireNotImplemented("sceUserServiceGetParentalBdAge", [] {
        return sceUserServiceGetParentalBdAge();
    });
    RequireNotImplemented("sceUserServiceGetParentalGameWhiteList", [] {
        return sceUserServiceGetParentalGameWhiteList();
    });
    RequireNotImplemented("sceUserServiceGetParentalMorpheus", [] {
        return sceUserServiceGetParentalMorpheus();
    });
    RequireNotImplemented("sceUserServiceGetParentalGame", [] {
        return sceUserServiceGetParentalGame();
    });
    RequireNotImplemented("sceUserServiceGetParentalGameAgeLevel", [] {
        return sceUserServiceGetParentalGameAgeLevel();
    });
}

void TestAgeVerificationAndNpAgeUnimplementedAPIs() {
    RequireNotImplemented("sceUserServiceGetNpAge", [] {
        return sceUserServiceGetNpAge();
    });
    RequireNotImplemented("sceUserServiceSetNpAge", [] {
        return sceUserServiceSetNpAge();
    });
    RequireNotImplemented("sceUserServiceGetAgeVerified", [] {
        return sceUserServiceGetAgeVerified();
    });
    RequireNotImplemented("sceUserServiceSetAgeVerified", [] {
        return sceUserServiceSetAgeVerified();
    });
}

void TestLifecycleTransitions() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);

    std::uint32_t ageLevel = 0;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &ageLevel) == USER_SERVICE_OK);
    Require(ageLevel == 0xffffffffu);

    TestParentalControlUnimplementedAPIs();
    TestAgeVerificationAndNpAgeUnimplementedAPIs();

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
}

} // namespace

int main() {
    TestAgeLevelBounds();
    TestParentalControlUnimplementedAPIs();
    TestAgeVerificationAndNpAgeUnimplementedAPIs();
    TestLifecycleTransitions();
    return 0;
}
