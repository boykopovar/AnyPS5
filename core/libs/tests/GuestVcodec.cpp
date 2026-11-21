#include "prx/libSceVcodec/libSceVcodec.hpp"
#include "prx/libSceVcodec/StatusConversion.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

constexpr int ErrorPermission = static_cast<int>(0x80bf0000);
constexpr int ErrorInvalidArgument = static_cast<int>(0x80bf0001);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "VCODEC: %s\n", message);
    std::abort();
}

void Set32(SceVcodec::Internal::StatusRequest& request, std::size_t offset, std::uint32_t value) {
    std::memcpy(request.data() + offset, &value, sizeof(value));
}

void Set64(SceVcodec::Internal::StatusRequest& request, std::size_t offset, std::uint64_t value) {
    std::memcpy(request.data() + offset, &value, sizeof(value));
}

float GetFloat(const SceVcodec::Internal::StatusOutput& output, std::size_t offset) {
    float value = 0;
    std::memcpy(&value, output.data() + offset, sizeof(value));
    return value;
}

}

int main() {
    SceVcodec::Internal::StatusRequest raw{};
    SceVcodec::Internal::StatusOutput translated{};
    Set32(raw, 0x20, 0x12345678);
    Set32(raw, 0x24, 10);
    Set64(raw, 0x28, 20);
    Set64(raw, 0x30, 40);
    Set64(raw, 0x38, 30000);
    Set64(raw, 0x40, 5000);
    Set64(raw, 0x48, 70000);
    Set64(raw, 0x50, 90000);
    Set64(raw, 0x58, 110000);
    Set64(raw, 0x60, 13000);
    Set64(raw, 0x1b48, 1000);
    const std::size_t secondTable = 0x888;
    for (std::size_t i = 0; i < 16; ++i) raw[secondTable + i] = static_cast<std::uint8_t>(i + 1);
    Set32(raw, secondTable + 0x10, 5);
    Set32(raw, secondTable + 0x14, 9);
    Set32(raw, secondTable + 0x18, 20);
    Set64(raw, secondTable + 0x20, 30);
    Set64(raw, secondTable + 0x28, 50);
    Set64(raw, secondTable + 0x30, 60000);
    Set64(raw, secondTable + 0x38, 7000);
    Set64(raw, secondTable + 0x40, 180000);
    Set64(raw, secondTable + 0x48, 11000);
    Set64(raw, secondTable + 0x50, 240000);
    Set64(raw, secondTable + 0x58, 260000);
    for (std::size_t i = 0; i < 16; ++i) raw[secondTable + 0x60 + i] = static_cast<std::uint8_t>(0xa0 + i);
    Set64(raw, secondTable + 0x70, 0x8877665544332211ull);
    const std::size_t absentEntry = secondTable + 0x78;
    Set32(raw, absentEntry + 0x10, 0xffffffff);
    SceVcodec::Internal::ConvertStatus(raw, translated);
    std::uint32_t firstCopiedField = 0;
    std::memcpy(&firstCopiedField, translated.data() + 0x20, sizeof(firstCopiedField));
    Require(firstCopiedField == 0x12345678, "Ac6aYjr1pY8 copies the first budget record");
    Require(GetFloat(translated, 0x28) == 2.0F && GetFloat(translated, 0x2c) == 4.0F, "Ac6aYjr1pY8 normalizes activity rates by elapsed time");
    Require(GetFloat(translated, 0x30) == 3.0F && GetFloat(translated, 0x34) == 5.0F, "Ac6aYjr1pY8 converts first-table timing values");
    Require(GetFloat(translated, 0x38) == 7.0F && GetFloat(translated, 0x3c) == 9.0F && GetFloat(translated, 0x40) == 11.0F && GetFloat(translated, 0x44) == 13.0F, "Ac6aYjr1pY8 converts all first-table metrics");
    const std::size_t secondOutput = 0x5e8;
    Require(GetFloat(translated, secondOutput + 0x18) == 20000.0F, "Ac6aYjr1pY8 converts the second-table activity rate");
    Require(GetFloat(translated, secondOutput + 0x1c) == 3.0F && GetFloat(translated, secondOutput + 0x20) == 5.0F, "Ac6aYjr1pY8 normalizes second-table counters");
    Require(GetFloat(translated, secondOutput + 0x24) == 3.0F && GetFloat(translated, secondOutput + 0x28) == 7.0F && GetFloat(translated, secondOutput + 0x2c) == 9.0F, "Ac6aYjr1pY8 converts second-table timing values");
    Require(GetFloat(translated, secondOutput + 0x30) == 11.0F && GetFloat(translated, secondOutput + 0x34) == 12.0F && GetFloat(translated, secondOutput + 0x38) == 13.0F, "Ac6aYjr1pY8 converts remaining second-table metrics");
    Require(translated[secondOutput + 0x3c] == 0xa0 && translated[secondOutput + 0x4c] == 0x11, "Ac6aYjr1pY8 copies opaque second-table fields");
    const std::size_t absentOutput = secondOutput + 0x54;
    Require(translated[absentOutput] == 0xff && translated[absentOutput + 0x17] == 0xff, "Ac6aYjr1pY8 marks unavailable second-table entries");
    Require(translated[absentOutput + 0x18] == 0 && translated[absentOutput + 0x4b] == 0xff && translated[absentOutput + 0x4c] == 0, "Ac6aYjr1pY8 leaves unavailable entry metrics zero and copies its sentinel fields");
    Set64(raw, 0x1b48, 0);
    Set32(raw, secondTable + 0x18, 0);
    SceVcodec::Internal::ConvertStatus(raw, translated);
    Require(GetFloat(translated, 0x28) == 0 && GetFloat(translated, 0x2c) == 0, "Ac6aYjr1pY8 leaves activity rates zero when elapsed time is zero");
    Require(GetFloat(translated, secondOutput + 0x18) == 0 && GetFloat(translated, secondOutput + 0x1c) == 0 && GetFloat(translated, secondOutput + 0x20) == 0, "Ac6aYjr1pY8 leaves second-table rates zero when elapsed time is zero");
    Require(GetFloat(translated, secondOutput + 0x24) == 0 && GetFloat(translated, secondOutput + 0x38) == 0, "Ac6aYjr1pY8 leaves per-sample metrics zero when their sample count is zero");

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
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
    errno = 71;
    Require(libSceVcodecUnknown_Ipf4PM5PIHE_nid_no_patch(&result, nullptr) == ErrorInvalidArgument, "Ipf4PM5PIHE null config");
    Require(errno == 71, "Ipf4PM5PIHE errno preserved on initial result clear");
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
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
