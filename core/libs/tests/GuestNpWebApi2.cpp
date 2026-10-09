#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceNpWebApi2Initialize(int, std::size_t);
int APS5_VABI sceNpWebApi2CreateUserContext(int, int);
int APS5_VABI sceNpWebApi2CreateRequest(int, const char*, const char*, const char*, const void*, std::int64_t*);
int APS5_VABI sceNpWebApi2SetRequestTimeout(std::int64_t, std::uint32_t);
int APS5_VABI sceNpWebApi2SendRequest(std::int64_t, const void*, std::size_t, NpWebApi2ResponseInformationOption*);
int APS5_VABI sceNpWebApi2ReadData(std::int64_t, void*, std::size_t);
int APS5_VABI sceNpWebApi2DeleteRequest(std::int64_t);
int APS5_VABI sceNpWebApi2DeleteUserContext(int);
int APS5_VABI sceNpWebApi2Terminate(int);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int unavailable = static_cast<int>(0x80553406);
    const int library = sceNpWebApi2Initialize(1, 0x10000);
    Require(library > 0);
    const int user = sceNpWebApi2CreateUserContext(library, 1);
    Require(user > 0 && user != library);
    std::int64_t request = 0;
    Require(sceNpWebApi2CreateRequest(user, "userProfile", "/v1/users/me/profile", "GET", nullptr, &request) == 0);
    Require(request > 0);
    Require(sceNpWebApi2SetRequestTimeout(request, 30000) == 0);
    Require(sceNpWebApi2SendRequest(request, nullptr, 0, nullptr) == unavailable);
    Require(sceNpWebApi2SetRequestTimeout(request, 0) == 0);
    Require(sceNpWebApi2SendRequest(request, nullptr, 0, nullptr) == unavailable);
    char data[16];
    Require(sceNpWebApi2ReadData(request, data, sizeof(data)) == unavailable);
    Require(sceNpWebApi2DeleteRequest(request) == 0);
    Require(sceNpWebApi2DeleteUserContext(user) == 0);
    Require(sceNpWebApi2Terminate(library) == 0);
}
