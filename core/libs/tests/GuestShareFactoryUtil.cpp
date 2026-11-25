#include "prx/libSceShareFactoryUtil/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr std::int32_t ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x81630002);
constexpr std::int32_t ERROR_NOT_LAUNCHED = static_cast<std::int32_t>(0x81630003);
constexpr std::int32_t ERROR_INVALID_PARAM = static_cast<std::int32_t>(0x81630005);

std::size_t g_lastReadBytes = 0;

void RecordReadAndClobberErrno(std::uintptr_t, std::size_t bytes) {
    g_lastReadBytes = bytes;
    errno = 99;
}

void ClobberErrno(std::uintptr_t, std::size_t) { errno = 99; }

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void RequireAllByte(const std::array<std::uint8_t, 0x114>& buf, std::uint8_t expected) {
    for (std::uint8_t b : buf) {
        Require(b == expected);
    }
}

}

int main() {
    std::array<std::uint8_t, 0x1100> param{};
    std::array<std::uint8_t, 16> smallParam{};
    std::array<std::uint8_t, 0x114> status{};
    status.fill(0x5a);
    const auto fullParamSize = static_cast<std::uint32_t>(param.size());
    const auto smallParamSize = static_cast<std::uint32_t>(smallParam.size());

    Require(sceShareFactoryUtilLaunchShareFactory(nullptr, 0) == ERROR_NOT_INITIALIZED);
    Require(sceShareFactoryUtilLaunchShareFactory(param.data(), 0) == ERROR_NOT_INITIALIZED);
    Require(sceShareFactoryUtilLaunchShareFactory(param.data(), fullParamSize) == ERROR_NOT_INITIALIZED);
    Require(sceShareFactoryUtilBackToGameFromShareFactory(nullptr) == ERROR_NOT_INITIALIZED);
    Require(sceShareFactoryUtilBackToGameFromShareFactory(param.data()) == ERROR_NOT_INITIALIZED);
    Require(sceShareFactoryUtilGetLaunchStatus(nullptr, param.data()) == ERROR_INVALID_PARAM);
    Require(sceShareFactoryUtilGetLaunchStatus(status.data(), nullptr) == ERROR_INVALID_PARAM);
    Require(sceShareFactoryUtilGetLaunchStatus(status.data(), param.data()) == ERROR_NOT_INITIALIZED);
    Require(sceShareFactoryUtilGetLaunchStatusOfShareFactory(nullptr, param.data()) == ERROR_INVALID_PARAM);
    Require(sceShareFactoryUtilGetLaunchStatusOfShareFactory(status.data(), nullptr) == ERROR_INVALID_PARAM);
    Require(sceShareFactoryUtilGetLaunchStatusOfShareFactory(status.data(), param.data()) == ERROR_NOT_INITIALIZED);
    RequireAllByte(status, 0x5a);

    Require(sceShareFactoryUtilTerminate() == 0);
    Require(sceShareFactoryUtilInitialize() == 0);
    Require(sceShareFactoryUtilInitialize() == 0);
    Require(sceShareFactoryUtilTerminate() == 0);
    Require(sceShareFactoryUtilLaunchShareFactory(nullptr, fullParamSize) == ERROR_INVALID_PARAM);
    Require(sceShareFactoryUtilLaunchShareFactory(param.data(), 0) == ERROR_INVALID_PARAM);
    Require(sceShareFactoryUtilLaunchShareFactory(param.data(), fullParamSize + 1) == ERROR_INVALID_PARAM);

    Require(sceShareFactoryUtilBackToGameFromShareFactory(nullptr) == ERROR_INVALID_PARAM);
    Require(sceShareFactoryUtilGetLaunchStatus(nullptr, param.data()) == ERROR_INVALID_PARAM);
    Require(sceShareFactoryUtilGetLaunchStatus(status.data(), nullptr) == ERROR_INVALID_PARAM);
    Require(sceShareFactoryUtilGetLaunchStatusOfShareFactory(nullptr, param.data()) == ERROR_INVALID_PARAM);
    Require(sceShareFactoryUtilGetLaunchStatusOfShareFactory(status.data(), nullptr) == ERROR_INVALID_PARAM);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(RecordReadAndClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    const auto* badParam = reinterpret_cast<const void*>(0x10);
    auto* badStatus = reinterpret_cast<void*>(0x10);

    errno = 71;
    Require(sceShareFactoryUtilLaunchShareFactory(badParam, fullParamSize) == ERROR_INVALID_PARAM && errno == 71);
    errno = 71;
    Require(sceShareFactoryUtilLaunchShareFactory(badParam, smallParamSize) == ERROR_INVALID_PARAM && errno == 71);
    errno = 71;
    g_lastReadBytes = 0;
    Require(sceShareFactoryUtilLaunchShareFactory(smallParam.data(), smallParamSize) == ERROR_NOT_LAUNCHED && errno == 71);
    Require(g_lastReadBytes == smallParamSize);
    errno = 71;
    g_lastReadBytes = 0;
    Require(sceShareFactoryUtilLaunchShareFactory(param.data(), fullParamSize) == ERROR_NOT_LAUNCHED && errno == 71);
    Require(g_lastReadBytes == fullParamSize);

    errno = 71;
    Require(sceShareFactoryUtilBackToGameFromShareFactory(badParam) == ERROR_INVALID_PARAM && errno == 71);
    errno = 71;
    Require(sceShareFactoryUtilBackToGameFromShareFactory(param.data()) == ERROR_NOT_LAUNCHED && errno == 71);

    errno = 71;
    Require(sceShareFactoryUtilGetLaunchStatus(badStatus, param.data()) == ERROR_INVALID_PARAM && errno == 71);
    errno = 71;
    Require(sceShareFactoryUtilGetLaunchStatus(status.data(), badParam) == ERROR_INVALID_PARAM && errno == 71);
    errno = 71;
    Require(sceShareFactoryUtilGetLaunchStatus(status.data(), param.data()) == ERROR_NOT_LAUNCHED && errno == 71);

    errno = 71;
    Require(sceShareFactoryUtilGetLaunchStatusOfShareFactory(badStatus, param.data()) == ERROR_INVALID_PARAM && errno == 71);
    errno = 71;
    Require(sceShareFactoryUtilGetLaunchStatusOfShareFactory(status.data(), badParam) == ERROR_INVALID_PARAM && errno == 71);
    errno = 71;
    Require(sceShareFactoryUtilGetLaunchStatusOfShareFactory(status.data(), param.data()) == ERROR_NOT_LAUNCHED && errno == 71);
    RequireAllByte(status, 0x5a);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);

    for (int i = 0; i < 16; ++i) {
        Require(sceShareFactoryUtilInitialize() == 0);
    }
    for (int i = 0; i < 16; ++i) {
        Require(sceShareFactoryUtilLaunchShareFactory(param.data(), fullParamSize) == ERROR_NOT_LAUNCHED);
        Require(sceShareFactoryUtilTerminate() == 0);
    }

    Require(sceShareFactoryUtilTerminate() == 0);
    Require(sceShareFactoryUtilLaunchShareFactory(param.data(), fullParamSize) == ERROR_NOT_INITIALIZED);
    Require(sceShareFactoryUtilGetLaunchStatus(status.data(), param.data()) == ERROR_NOT_INITIALIZED);
    return 0;
}
