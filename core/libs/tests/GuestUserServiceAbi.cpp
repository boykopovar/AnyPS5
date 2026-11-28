#include "prx/libSceUserService/UserService.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <type_traits>

using LoginSignature = int (APS5_VABI *)(int, std::uint64_t);
using LogoutSignature = int (APS5_VABI *)(int, int);
using DestroyUserSignature = int (APS5_VABI *)(int);

static_assert(std::is_same_v<decltype(&sceUserServiceLogin), LoginSignature>);
static_assert(std::is_same_v<decltype(&sceUserServiceLogout), LogoutSignature>);
static_assert(std::is_same_v<decltype(&sceUserServiceDestroyUser), DestroyUserSignature>);

int main() { return 0; }
