#include "prx/libkernel/File/include/HostStreams.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#include <io.h>
#include <stdlib.h>
#else
#include <fcntl.h>
#endif

namespace {

#ifdef _WIN32
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
void IgnoreInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {}
#endif

bool OpenAtStart(int fd) {
    const int savedErrno = errno;
#ifdef _WIN32
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidParameter);
    const std::intptr_t handle = _get_osfhandle(fd);
    _set_thread_local_invalid_parameter_handler(previous);
    errno = savedErrno;
    return handle != -1 && handle != -2 && handle != 0;
#else
    const bool open = ::fcntl(fd, F_GETFD) != -1;
    errno = savedErrno;
    return open;
#endif
}

const std::array<bool, 3> g_hostStreams = {OpenAtStart(0), OpenAtStart(1), OpenAtStart(2)};

}

namespace File {

bool IsHostStream(int fd) {
    return fd >= 0 && fd < static_cast<int>(g_hostStreams.size()) && g_hostStreams[static_cast<std::size_t>(fd)];
}

}
