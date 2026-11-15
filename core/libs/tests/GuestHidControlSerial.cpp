#include "prx/libSceHidControl/libSceHidControl.hpp"
#include "SceTypes.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x803b3001);
constexpr std::uint64_t QueryCommand = 0xc0184852;

struct Query {
    std::uint8_t kind;
    std::uint8_t reserved0[3];
    std::uint32_t device;
    std::uint32_t value;
    std::uint32_t reserved1;
    void* output;
};
static_assert(sizeof(Query) == 24);

int nextResult = 0;
int ioctlCalls = 0;

void Require(bool ok, const char* message) {
    if (ok) return;
    std::fprintf(stderr, "HID SERIAL: %s\n", message);
    std::abort();
}

}

extern "C" {

int APS5_VABI open_nid_postfix(const char* path, int, int) {
    Require(std::strcmp(path, "/dev/hid") == 0, "device path");
    return 3;
}

int APS5_VABI ioctl_nid_postfix(int descriptor, std::uint64_t request, void* argument) {
    Require(descriptor == 3 && request == QueryCommand, "ioctl request");
    auto* query = static_cast<Query*>(argument);
    Require(query->kind == 0x0b && query->device == 7, "serial query layout");
    ++ioctlCalls;
    if (nextResult < 0) return nextResult;
    for (std::size_t index = 0; index < 0x11; ++index)
        static_cast<std::uint8_t*>(query->output)[index] = static_cast<std::uint8_t>(index + 1);
    return nextResult;
}

}

int main() {
    Require(sceHidControlInit() == 0, "initialization");
    std::array<std::uint8_t, 0x20> serial{};
    serial.fill(0x5a);

    nextResult = -9;
    Require(sceHidControlGetControllerSerialNumberInformation(7, serial.data()) == -9,
            "negative ioctl result");
    for (auto value : serial) Require(value == 0x5a, "negative result preserves output");

    nextResult = 1;
    errno = 71;
    Require(sceHidControlGetControllerSerialNumberInformation(7, serial.data()) == 1 && errno == 71,
            "positive ioctl result and errno");
    for (std::size_t index = 0; index < serial.size(); ++index)
        Require(serial[index] == (index < 0x11 ? index + 1 : 0), "complete serial reply");

    errno = 71;
    Require(sceHidControlGetControllerSerialNumberInformation(7, reinterpret_cast<void*>(0x10)) == ErrorInvalidArgument && errno == 71,
            "unwritable guest output");
    Require(ioctlCalls == 3, "all three calls reached the controlled ioctl");
    std::puts("HidControl serial tests passed");
    return 0;
}
