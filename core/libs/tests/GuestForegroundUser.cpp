#include "prx/libSceUserService/UserService.hpp"
#include "SceTypes.hpp"
#include <cstdlib>

extern "C" int APS5_VABI sceUserServiceGetForegroundUser(int* user_id);

int main() {
    if (sceUserServiceGetForegroundUser(nullptr) != USER_SERVICE_ERROR_INVALID_ARGUMENT)
        std::abort();
    int user_id = USER_SERVICE_USER_ID_INVALID;
    if (sceUserServiceGetForegroundUser(&user_id) != USER_SERVICE_OK ||
        user_id != USER_SERVICE_INITIAL_USER_ID)
        std::abort();
}
