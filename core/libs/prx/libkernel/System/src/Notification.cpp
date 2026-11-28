#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include <string>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

std::mutex g_outputMutex;

bool WriteAllToDescriptor(int descriptor, const char* data, std::size_t length) {
    const int savedErrno = errno;
    std::size_t written = 0;
    while (written < length) {
#ifdef _WIN32
        const std::size_t remaining = length - written;
        const auto chunk = static_cast<unsigned>(remaining > 0x40000000u ? 0x40000000u : remaining);
        const int count = _write(descriptor, data + written, chunk);
#else
        const auto count = ::write(descriptor, data + written, length - written);
#endif
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            errno = savedErrno;
            return false;
        }
        written += static_cast<std::size_t>(count);
    }
    errno = savedErrno;
    return true;
}

}

extern "C" int APS5_VABI sceKernelDebugOutText(int channel, const char* text) {
    if (channel < 0) return static_cast<int>(0x80020016u);
    if (!text) return static_cast<int>(0x8002000eu);
    constexpr std::size_t DebugTextLimit = 0x10000;
    std::string copied;
    const int copyError = CopyGuestString_nid_no_patch(text, DebugTextLimit, copied);
    if (copyError == 63) {
        copied.resize(DebugTextLimit);
        if (!GuestCopyIn_nid_no_patch(copied.data(), text, DebugTextLimit)) return static_cast<int>(0x8002000eu);
    } else if (copyError != 0) {
        return static_cast<int>(0x8002000eu);
    }
    const std::lock_guard lock(g_outputMutex);
    if (const int host = File::HostStandardError(); host >= 0) {
        const auto line = "[debug:" + std::to_string(channel) + "] " + copied;
        return WriteAllToDescriptor(host, line.data(), line.size()) ? 0 : static_cast<int>(0x80020005u);
    }
    if (std::fprintf(stderr, "[debug:%d] %s", channel, copied.c_str()) < 0 || std::fflush(stderr) != 0)
        return static_cast<int>(0x80020005u);
    return 0;
}

extern "C" int APS5_VABI sceKernelSendNotificationRequest(int device, const void* request, std::size_t size, int flags) {
    constexpr auto invalidArgument = static_cast<int>(0x80020016u);
    constexpr auto badAddress = static_cast<int>(0x8002000eu);
    constexpr auto notSupported = static_cast<int>(0x8002002du);
    constexpr auto ioError = static_cast<int>(0x80020005u);
    constexpr std::size_t requestSize = 3120;
    constexpr std::size_t messageOffset = 45;
    constexpr std::size_t messageCapacity = 1024;
    constexpr std::int32_t systemTextType = 5;

    if (size != requestSize || device < 0)
        return invalidArgument;
    if (request == nullptr)
        return badAddress;
    if (device != 0 || flags != 0)
        return notSupported;

    std::array<std::uint8_t, requestSize> localRequest{};
    if (!GuestCopyIn_nid_no_patch(localRequest.data(), request, requestSize))
        return badAddress;

    std::int32_t type = 0;
    std::memcpy(&type, localRequest.data(), sizeof(type));
    if ((type != 0 && type != systemTextType) || localRequest[messageOffset - 1] != 0)
        return notSupported;

    const auto* message = reinterpret_cast<const char*>(localRequest.data() + messageOffset);
    const auto* end = static_cast<const char*>(std::memchr(message, '\0', messageCapacity));
    if (end == nullptr)
        return invalidArgument;

    const std::lock_guard lock(g_outputMutex);
    std::array<char, messageCapacity + 32> output{};
    const auto length = std::snprintf(output.data(), output.size(), "[notification] %.*s\n", static_cast<int>(end - message), message);
    if (length < 0 || static_cast<std::size_t>(length) >= output.size())
        return ioError;
    const auto totalLength = static_cast<std::size_t>(length);
    if (const int host = File::HostStandardError(); host >= 0)
        return WriteAllToDescriptor(host, output.data(), totalLength) ? 0 : ioError;
#ifdef _WIN32
    std::size_t written = 0;
    while (written < totalLength) {
        DWORD count = 0;
        if (!WriteFile(GetStdHandle(STD_ERROR_HANDLE), output.data() + written, static_cast<DWORD>(totalLength - written), &count, nullptr) || count == 0)
            return ioError;
        written += static_cast<std::size_t>(count);
    }
    return 0;
#else
    return WriteAllToDescriptor(STDERR_FILENO, output.data(), totalLength) ? 0 : ioError;
#endif
}
