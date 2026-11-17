#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "Testing.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstdio>

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
