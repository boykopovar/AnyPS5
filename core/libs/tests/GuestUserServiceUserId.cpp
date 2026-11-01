#include "prx/libSceUserService/libSceUserService.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

void CheckUserId(std::uint32_t bits) {
    const int userId = std::bit_cast<std::int32_t>(bits);
    const int guest = bits >= 0x20000000u && bits <= 0x3fffffffu;
    const int sharePlay = bits >= 0x30000000u && bits <= 0x3fffffffu;
    using Predicate = int (APS5_VABI *)(int);
    const Predicate isGuest = &sceUserServiceIsGuestUser;
    const Predicate isSharePlay = &sceUserServiceIsSharePlayClientUser;
    Require(isGuest(userId) == guest);
    Require(isSharePlay(userId) == sharePlay);
}

void CheckCategories() {
    for (std::uint32_t category = 0; category < 16; ++category) {
        for (std::uint32_t payload : {0u, 1u, 0x1234567u, 0x7ffffffu, 0xffffffeu, 0xfffffffu}) {
            CheckUserId((category << 28) | payload);
        }
    }
    for (std::uint32_t boundary : {0u, 0x10000000u, 0x20000000u, 0x30000000u, 0x40000000u, 0x80000000u}) {
        CheckUserId(boundary - 1);
        CheckUserId(boundary);
        CheckUserId(boundary + 1);
    }
}

}

int main() {
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);
    CheckCategories();
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    CheckCategories();
    UserServiceSetUserConnected_nid_postfix(0, false);
    UserServiceSetUserConnected_nid_postfix(1, true);
    CheckCategories();
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    CheckCategories();
    std::array<std::thread, 4> workers;
    for (auto& worker : workers) {
        worker = std::thread([] {
            for (int repeat = 0; repeat < 100; ++repeat) CheckCategories();
        });
    }
    for (auto& worker : workers) worker.join();
}
