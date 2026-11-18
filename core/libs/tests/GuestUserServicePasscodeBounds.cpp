#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
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
void RequireNotImplemented(std::string_view expectedName, F&& func) {
    try {
        func();
        std::abort();
    } catch (const std::runtime_error& err) {
        const std::string_view message = err.what();
        if (message.find(expectedName) == std::string_view::npos ||
            message.find("not implemented") == std::string_view::npos) {
            std::abort();
        }
    }
}

void TestPasscodeUninitialized() {
    RequireNotImplemented("sceUserServiceGetPasscode", [] {
        return sceUserServiceGetPasscode();
    });
    RequireNotImplemented("sceUserServiceSetPasscode", [] {
        return sceUserServiceSetPasscode();
    });
}

void TestPasscodeLifecycle() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);

    RequireNotImplemented("sceUserServiceGetPasscode", [] {
        return sceUserServiceGetPasscode();
    });
    RequireNotImplemented("sceUserServiceSetPasscode", [] {
        return sceUserServiceSetPasscode();
    });

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);

    RequireNotImplemented("sceUserServiceGetPasscode", [] {
        return sceUserServiceGetPasscode();
    });
    RequireNotImplemented("sceUserServiceSetPasscode", [] {
        return sceUserServiceSetPasscode();
    });
}

void TestPasscodeFunctionPointers() {
    using PasscodeGetter = int (APS5_VABI *)();
    using PasscodeSetter = int (APS5_VABI *)();

    const PasscodeGetter getter = &sceUserServiceGetPasscode;
    const PasscodeSetter setter = &sceUserServiceSetPasscode;

    Require(getter != nullptr);
    Require(setter != nullptr);

    RequireNotImplemented("sceUserServiceGetPasscode", [getter] {
        return getter();
    });
    RequireNotImplemented("sceUserServiceSetPasscode", [setter] {
        return setter();
    });
}

void TestPasscodeRepeatedInvocations() {
    for (int i = 0; i < 32; ++i) {
        RequireNotImplemented("sceUserServiceGetPasscode", [] {
            return sceUserServiceGetPasscode();
        });
        RequireNotImplemented("sceUserServiceSetPasscode", [] {
            return sceUserServiceSetPasscode();
        });
    }
}

} // namespace

int main() {
    TestPasscodeUninitialized();
    TestPasscodeLifecycle();
    TestPasscodeFunctionPointers();
    TestPasscodeRepeatedInvocations();
    return 0;
}
