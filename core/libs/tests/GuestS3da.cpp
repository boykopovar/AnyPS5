#include "prx/libSceS3da/include/Api.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <string>

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
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("s3da-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory));
    std::filesystem::current_path(directory);
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

    std::uint8_t info[64];
    for (std::uint8_t& byte : info) byte = 0xAB;
    for (const std::uint32_t port : {0u, 1u}) {
        Require(sceS3daSendData(port, dataBuffer, sizeof(dataBuffer)) == -1);
        Require(sceS3daSendData(port, dataBuffer, 1) == -1);
        Require(sceS3daSendData(port, dataBuffer, 0xA000u) == -1);
        Require(sceS3daGetInfo(port, reinterpret_cast<S3daInfo*>(info)) == -1);
        Require(sceS3daGetInfo(port, nullptr) == -1);
    }
    for (const std::uint8_t byte : info) Require(byte == 0xAB);

    std::filesystem::current_path(original);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return 0;
}
