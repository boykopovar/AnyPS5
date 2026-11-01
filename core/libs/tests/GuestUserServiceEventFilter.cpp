#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI sceUserServiceRegisterEventFilter(std::uint64_t filterMask);
}

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);

    for (std::uint64_t invalidMask : {0ULL, 0x100000000ULL, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL}) {
        Require(sceUserServiceRegisterEventFilter(invalidMask) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    return 0;
}
