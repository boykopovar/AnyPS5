#include "prx/libkernel/File/include/HostStreams.hpp"
#include <array>
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
#ifdef _WIN32
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidParameter);
    const bool open = _get_osfhandle(fd) != -1;
    _set_thread_local_invalid_parameter_handler(previous);
    return open;
#else
    return ::fcntl(fd, F_GETFD) != -1;
#endif
}

const std::array<bool, 3> g_hostStreams = {OpenAtStart(0), OpenAtStart(1), OpenAtStart(2)};

}

namespace File {

bool IsHostStream(int fd) {
    return fd >= 0 && fd < static_cast<int>(g_hostStreams.size()) && g_hostStreams[static_cast<std::size_t>(fd)];
}

}
