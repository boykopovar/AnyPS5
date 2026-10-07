#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" int APS5_VABI sceKernelDebugOutText(int channel, const char* text) {
    if (channel < 0)
        return static_cast<int>(0x80020016u);
    if (!text)
        return static_cast<int>(0x8002000eu);
    static std::mutex outputMutex;
    const std::lock_guard lock(outputMutex);
    if (std::fprintf(stderr, "[debug:%d] %s", channel, text) < 0 || std::fflush(stderr) != 0)
        return static_cast<int>(0x80020005u);
    return 0;
}

extern "C" int APS5_VABI sceKernelSendNotificationRequest(int device, const void* request, std::size_t size,
                                                          int flags) {
    constexpr auto invalidArgument = static_cast<int>(0x80020016u);
    constexpr auto badAddress = static_cast<int>(0x8002000eu);
    constexpr auto notSupported = static_cast<int>(0x8002002du);
    constexpr auto ioError = static_cast<int>(0x80020005u);
    constexpr std::size_t requestSize = 3120;
    constexpr std::size_t messageOffset = 45;
    constexpr std::size_t messageCapacity = 1024;

    if (size != requestSize || device < 0)
        return invalidArgument;
    if (request == nullptr)
        return badAddress;
    if (device != 0 || flags != 0)
        return notSupported;

    std::int32_t type;
    std::memcpy(&type, request, sizeof(type));
    if (type != 0 || static_cast<const unsigned char*>(request)[messageOffset - 1] != 0)
        return notSupported;

    const auto* message = static_cast<const char*>(request) + messageOffset;
    const auto* end = static_cast<const char*>(std::memchr(message, '\0', messageCapacity));
    if (end == nullptr)
        return invalidArgument;

    static std::mutex outputMutex;
    const std::lock_guard lock(outputMutex);
    std::array<char, messageCapacity + 32> output{};
    const auto length =
        std::snprintf(output.data(), output.size(), "[notification] %.*s\n", static_cast<int>(end - message), message);
    if (length < 0 || static_cast<std::size_t>(length) >= output.size())
        return ioError;
    std::size_t written = 0;
    while (written < static_cast<std::size_t>(length)) {
#ifdef _WIN32
        DWORD count = 0;
        if (!WriteFile(GetStdHandle(STD_ERROR_HANDLE), output.data() + written, static_cast<DWORD>(length - written),
                       &count, nullptr) ||
            count == 0)
            return ioError;
#else
        const auto count = ::write(STDERR_FILENO, output.data() + written, length - written);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return ioError;
#endif
        written += static_cast<std::size_t>(count);
    }
    return 0;
}
