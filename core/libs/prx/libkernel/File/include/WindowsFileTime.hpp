#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_WINDOWSFILETIME_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_WINDOWSFILETIME_HPP

#include "SceTypes.hpp"
#include <windows.h>
#include <cerrno>
#include <limits>
#include <system_error>

namespace File {

class WindowsFileTime {
    static constexpr std::int64_t epochSeconds = 11644473600LL;
    static constexpr std::int64_t ticksPerSecond = 10000000;

public:
    static KernelTimespec Decode(std::int64_t ticks) {
        auto seconds = ticks / ticksPerSecond - epochSeconds;
        auto remainder = ticks % ticksPerSecond;
        if (remainder < 0) {
            --seconds;
            remainder += ticksPerSecond;
        }
        return {seconds, remainder * 100};
    }

    static bool Encode(const KernelTimeval& time, FILETIME& result) {
        if (time.tv_usec < 0 || time.tv_usec >= 1000000 || time.tv_sec < -epochSeconds ||
            time.tv_sec > std::numeric_limits<std::int64_t>::max() / ticksPerSecond - epochSeconds) return false;
        const auto seconds = time.tv_sec + epochSeconds;
        const auto fraction = time.tv_usec * 10;
        if (seconds > (std::numeric_limits<std::int64_t>::max() - fraction) / ticksPerSecond) return false;
        const auto ticks = static_cast<std::uint64_t>(seconds * ticksPerSecond + fraction);
        if (ticks == 0) return false;
        result = {static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
        return true;
    }

    static int Failure(DWORD error) {
        const auto condition = std::system_category().default_error_condition(static_cast<int>(error));
        if (condition == std::errc::no_such_file_or_directory) errno = ENOENT;
        else if (condition == std::errc::permission_denied) errno = EACCES;
        else if (condition == std::errc::bad_file_descriptor) errno = EBADF;
        else if (condition == std::errc::not_enough_memory) errno = ENOMEM;
        else if (condition == std::errc::too_many_files_open) errno = EMFILE;
        else if (condition == std::errc::invalid_argument) errno = EINVAL;
        else errno = EIO;
        return -1;
    }
};

}

#endif
