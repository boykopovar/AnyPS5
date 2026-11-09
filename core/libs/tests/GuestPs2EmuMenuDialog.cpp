#include "prx/libScePs2EmuMenuDialog/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kStatusNone = 0;
constexpr int kErrUnavailable = static_cast<int>(0x80B8000Fu);

using OneArg = int(APS5_VABI*)(void*);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "PS2EmuMenuDialog: %s\n", message);
        std::abort();
    }
}

void CheckUntouched(OneArg fn, int expected, const char* message) {
    std::uint8_t buffer[32];
    for (std::size_t i = 0; i < sizeof(buffer); ++i) buffer[i] = static_cast<std::uint8_t>(0xA5 ^ i);
    void* const args[] = {nullptr, buffer, reinterpret_cast<void*>(~std::uintptr_t{0})};
    for (void* arg : args) {
        Require(fn(arg) == expected, message);
    }
    for (std::size_t i = 0; i < sizeof(buffer); ++i) {
        Require(buffer[i] == static_cast<std::uint8_t>(0xA5 ^ i), message);
    }
}

}

int main() {
    for (int round = 0; round < 2; ++round) {
        CheckUntouched(reinterpret_cast<OneArg>(&scePs2EmuMenuDialogInitialize), kErrUnavailable, "initialize");
        CheckUntouched(reinterpret_cast<OneArg>(&scePs2EmuMenuDialogOpen), kErrUnavailable, "open");
        CheckUntouched(reinterpret_cast<OneArg>(&scePs2EmuMenuDialogGetResult), kErrUnavailable, "get result");
        CheckUntouched(reinterpret_cast<OneArg>(&scePs2EmuMenuDialogClose), kErrUnavailable, "close");
        CheckUntouched(reinterpret_cast<OneArg>(&scePs2EmuMenuDialogTerminate), kErrUnavailable, "terminate");
        CheckUntouched(reinterpret_cast<OneArg>(&scePs2EmuMenuDialogGetStatus), kStatusNone, "get status");
        CheckUntouched(reinterpret_cast<OneArg>(&scePs2EmuMenuDialogUpdateStatus), kStatusNone, "update status");
    }
}
