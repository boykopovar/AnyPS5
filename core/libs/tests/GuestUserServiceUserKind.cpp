#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) { if (!value) std::abort(); }

std::int32_t Id(std::uint32_t value) {
    std::int32_t id = 0;
    std::memcpy(&id, &value, sizeof(id));
    return id;
}

void CheckGuestUser() {
    const std::uint32_t guests[] = {0x20000000u, 0x20000001u, 0x2abcdef0u, 0x2fffffffu, 0x30000000u, 0x3fffffffu};
    const std::uint32_t others[] = {0u, 0x10000000u, 0x10000003u, 0x1fffffffu, 0x40000000u, 0x60000000u,
        0xa0000000u, 0xe0000000u, 0xffffffffu};
    for (const std::uint32_t id : guests) Require(sceUserServiceIsGuestUser(Id(id)) == 1);
    for (const std::uint32_t id : others) Require(sceUserServiceIsGuestUser(Id(id)) == 0);
    Require(sceUserServiceIsGuestUser(USER_SERVICE_INITIAL_USER_ID) == 0);
    Require(sceUserServiceIsGuestUser(USER_SERVICE_USER_ID_INVALID) == 0);
}

void CheckSharePlayClientUser() {
    const std::uint32_t clients[] = {0x30000000u, 0x30000001u, 0x3abcdef0u, 0x3fffffffu};
    const std::uint32_t others[] = {0u, 0x10000000u, 0x20000000u, 0x2fffffffu, 0x40000000u, 0x70000000u,
        0xb0000000u, 0xf0000000u, 0xffffffffu};
    for (const std::uint32_t id : clients) Require(sceUserServiceIsSharePlayClientUser(Id(id)) == 1);
    for (const std::uint32_t id : others) Require(sceUserServiceIsSharePlayClientUser(Id(id)) == 0);
    Require(sceUserServiceIsSharePlayClientUser(USER_SERVICE_INITIAL_USER_ID) == 0);
}

void CheckSpartacusEnabled() {
    const std::int32_t ids[] = {USER_SERVICE_INITIAL_USER_ID, USER_SERVICE_USER_ID_INVALID, 0, 123, Id(0x20000000u)};
    for (const std::int32_t id : ids) {
        Require(sceUserServiceGetIsSpartacusEnabled(id, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        unsigned char bytes[2] = {0x5a, 0x5a};
        bool* enabled = reinterpret_cast<bool*>(bytes);
        Require(sceUserServiceGetIsSpartacusEnabled(id, enabled) == USER_SERVICE_OK);
        Require(bytes[0] == 1);
        Require(bytes[1] == 0x5a);
    }
}

}

int main() {
    CheckGuestUser();
    CheckSharePlayClientUser();
    CheckSpartacusEnabled();
    return 0;
}
