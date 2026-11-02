#include "prx/libSceS3da/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

#ifndef _WIN32
extern "C" [[noreturn]] void NotImplemented_nid_no_patch(const char* name) {
    (void)name;
    std::abort();
}
#endif

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

constexpr int ERROR_INVALID_ARGUMENT = static_cast<int>(0x81600002u);
constexpr int ERROR_ALREADY_OPEN = static_cast<int>(0x81600006u);

}

int main() {
    std::uint8_t dataBuffer[64]{};

    for (const std::uint32_t port : {2u, 3u, 32u, 0xFFFFFFFFu}) {
        Require(sceS3daOpen(port) == ERROR_INVALID_ARGUMENT);
        Require(sceS3daClose(port) == ERROR_INVALID_ARGUMENT);
        Require(sceS3daGetInfo(port, nullptr) == ERROR_INVALID_ARGUMENT);
        Require(sceS3daSendData(port, dataBuffer, sizeof(dataBuffer)) == ERROR_INVALID_ARGUMENT);
        Require(sceS3daSendData(port, nullptr, sizeof(dataBuffer)) == ERROR_INVALID_ARGUMENT);
        Require(sceS3daSendData(port, dataBuffer, 0) == ERROR_INVALID_ARGUMENT);
        Require(sceS3daSendData(port, dataBuffer, 0xA001u) == ERROR_INVALID_ARGUMENT);
    }

    Require(sceS3daClose(0) == ERROR_INVALID_ARGUMENT);
    Require(sceS3daClose(1) == ERROR_INVALID_ARGUMENT);

    Require(sceS3daOpen(0) == 0);
    Require(sceS3daOpen(0) == ERROR_ALREADY_OPEN);
    Require(sceS3daClose(0) == 0);
    Require(sceS3daClose(0) == ERROR_INVALID_ARGUMENT);

    Require(sceS3daOpen(1) == 1);
    Require(sceS3daOpen(1) == ERROR_ALREADY_OPEN);
    Require(sceS3daClose(1) == 0);
    Require(sceS3daClose(1) == ERROR_INVALID_ARGUMENT);

    for (const std::uint32_t port : {0u, 1u}) {
        Require(sceS3daSendData(port, nullptr, sizeof(dataBuffer)) == ERROR_INVALID_ARGUMENT);
        Require(sceS3daSendData(port, dataBuffer, 0) == ERROR_INVALID_ARGUMENT);
        Require(sceS3daSendData(port, dataBuffer, 0xA001u) == ERROR_INVALID_ARGUMENT);
    }

    return 0;
}
