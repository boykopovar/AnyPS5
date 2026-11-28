#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstdio>

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceInitializeForShellCore(reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234))) == USER_SERVICE_OK);
    Require(sceUserServiceInitializeForShellCore(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
    Require(sceUserServiceGetEvent(&event) != USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceTerminateForShellCore() == USER_SERVICE_OK);
    Require(sceUserServiceTerminateForShellCore() == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceTerminateForShellCore() == USER_SERVICE_OK);
    Require(sceUserServiceInitializeForShellCore(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    std::puts("UserService shell-core lifecycle tests passed");
    return 0;
}
