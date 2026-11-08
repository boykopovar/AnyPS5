#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "UserService terminate bounds test failure at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);
    return 0;
}
