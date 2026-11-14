#include "prx/libSceVcodec/libSceVcodec.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

constexpr int ErrorPermission = static_cast<int>(0x80bf0000);
constexpr int ErrorInvalidArgument = static_cast<int>(0x80bf0001);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "VCODEC: %s\n", message);
    std::abort();
}

}

int main() {
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("vcodec-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "create the test directory");
    std::filesystem::current_path(directory);

    Require(sceVcodecOpenHwBudget(0, 1) == ErrorPermission, "OpenHwBudget without /dev/uvd_bgt maps EBADF");
    Require(sceVcodecOpenHwBudgetByAppid(1, 1) == ErrorPermission, "OpenHwBudgetByAppid without /dev/uvd_bgt maps EBADF");
    Require(sceVcodecCloseHwBudget(1) == ErrorPermission, "CloseHwBudget without /dev/uvd_bgt maps EBADF");
    Require(libSceVcodecUnknown_X0qscgxx1MI_nid_no_patch(1, 0) == ErrorPermission, "X0qscgxx1MI without /dev/uvd_bgt maps EBADF");
    Require(sceVcodecSetHwBudgetSize(1, 0x1000) == ErrorPermission, "SetHwBudgetSize without /dev/uvd_bgt maps EBADF");
    Require(sceVcodecSetHwBudgetType(1, 2, 3) == ErrorPermission, "SetHwBudgetType without /dev/uvd_bgt maps EBADF");

    std::uint64_t result = 0x5a5a5a5a5a5a5a5aull;
    Require(libSceVcodecUnknown_Ipf4PM5PIHE_nid_no_patch(&result, nullptr) == ErrorInvalidArgument, "Ipf4PM5PIHE null config");
    Require(result == 0, "Ipf4PM5PIHE clears its result before the config check");
    Require(libSceVcodecUnknown_Ipf4PM5PIHE_nid_no_patch(nullptr, nullptr) == ErrorInvalidArgument, "Ipf4PM5PIHE null result and config");
    const std::array<std::uint32_t, 5> config{1, 1920, 1080, 0, 200};
    result = 0x5a5a5a5a5a5a5a5aull;
    Require(libSceVcodecUnknown_Ipf4PM5PIHE_nid_no_patch(&result, config.data()) == ErrorPermission, "Ipf4PM5PIHE without /dev/uvd_bgt maps EBADF");
    Require(result == 0, "a failed Ipf4PM5PIHE request leaves the cleared result");
    Require(libSceVcodecUnknown_Ipf4PM5PIHE_nid_no_patch(nullptr, config.data()) == ErrorPermission, "Ipf4PM5PIHE with no result pointer");
    errno = 71;
    Require(libSceVcodecUnknown_Ipf4PM5PIHE_nid_no_patch(reinterpret_cast<std::uint64_t*>(0x10), config.data()) == ErrorInvalidArgument, "Ipf4PM5PIHE unmapped result");
    Require(errno == 71, "Ipf4PM5PIHE errno preserved on unmapped result");
    errno = 71;
    Require(libSceVcodecUnknown_Ipf4PM5PIHE_nid_no_patch(&result, reinterpret_cast<const void*>(0x10)) == ErrorInvalidArgument, "Ipf4PM5PIHE unmapped config");
    Require(errno == 71, "Ipf4PM5PIHE errno preserved on unmapped config");

    std::array<std::uint8_t, 0x1308> status{};
    status.fill(0x5a);
    Require(libSceVcodecUnknown_Ac6aYjr1pY8_nid_no_patch(nullptr) == ErrorPermission, "Ac6aYjr1pY8 without /dev/uvd_bgt maps EBADF");
    Require(libSceVcodecUnknown_Ac6aYjr1pY8_nid_no_patch(status.data()) == ErrorPermission, "Ac6aYjr1pY8 with an output fails at the request");
    for (const std::uint8_t byte : status) Require(byte == 0x5a, "a failed Ac6aYjr1pY8 request writes nothing");

    std::filesystem::current_path(original);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::puts("Vcodec tests passed");
    return 0;
}
