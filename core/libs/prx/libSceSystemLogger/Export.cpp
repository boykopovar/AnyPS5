#include "prx/libSceSystemLogger/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(SystemLoggerLogData) == 0x64);
static_assert(offsetof(SystemLoggerLogData, size) == 0x40);
static_assert(offsetof(SystemLoggerLogData, reserved) == 0x44);

namespace {

constexpr int SCE_SYSTEM_LOGGER_ERROR_NOT_INITIALIZED = static_cast<int>(0x81080004);
constexpr int SCE_SYSTEM_LOGGER_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x81080005);
constexpr int SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT = static_cast<int>(0x81080002);
constexpr std::size_t MAX_TAG_LENGTH = 15;
constexpr std::uint32_t MAX_ENTRY_COUNT = 0x20;
constexpr std::uint32_t MAX_ENTRY_SIZE = 0x40;

std::mutex loggerMutex;
bool loggerInitialized = false;

bool isTagCharacter(unsigned char character) {
    return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
}

} // namespace

extern "C" {

int APS5_VABI sceSystemLoggerInitialize() {
    std::lock_guard lock(loggerMutex);
    if (loggerInitialized) return SCE_SYSTEM_LOGGER_ERROR_ALREADY_INITIALIZED;
    loggerInitialized = true;
    return 0;
}

int APS5_VABI sceSystemLoggerSetLogData(const char* tag, uint32_t count, const SystemLoggerLogData* data) {
    std::lock_guard lock(loggerMutex);
    if (!loggerInitialized) return SCE_SYSTEM_LOGGER_ERROR_NOT_INITIALIZED;
    if (!tag) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    const int savedErrno = errno;
    std::string localTag;
    if (CopyGuestString_nid_no_patch(tag, MAX_TAG_LENGTH + 1, localTag) != 0) {
        errno = savedErrno;
        return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    }
    const std::size_t length = localTag.size();
    if (length > MAX_TAG_LENGTH) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    for (std::size_t index = 0; index < length; ++index) {
        if (!isTagCharacter(static_cast<unsigned char>(localTag[index]))) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    }
    if (count > MAX_ENTRY_COUNT) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    if (count != 0 && !data) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    std::array<SystemLoggerLogData, MAX_ENTRY_COUNT> entries;
    if (!GuestCopyIn_nid_no_patch(entries.data(), data, static_cast<std::size_t>(count) * sizeof(SystemLoggerLogData))) {
        errno = savedErrno;
        return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        if (entries[index].size > MAX_ENTRY_SIZE) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
        for (const std::uint8_t byte : entries[index].reserved) {
            if (byte != 0) return SCE_SYSTEM_LOGGER_ERROR_INVALID_ARGUMENT;
        }
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        std::fprintf(stderr, "[sceSystemLogger:%s] ", localTag.c_str());
        std::fwrite(entries[index].data, 1, entries[index].size, stderr);
        std::fputc('\n', stderr);
    }
    errno = savedErrno;
    return 0;
}

int APS5_VABI sceSystemLoggerTerminate() {
    std::lock_guard lock(loggerMutex);
    if (!loggerInitialized) return SCE_SYSTEM_LOGGER_ERROR_NOT_INITIALIZED;
    loggerInitialized = false;
    return 0;
}

}
