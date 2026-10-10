#if defined(__linux__)

#include <cerrno>
#include <system_error>
#include <sys/resource.h>

namespace {

struct OpenFileLimit {
    OpenFileLimit() {
        rlimit limit{};
        if (getrlimit(RLIMIT_NOFILE, &limit) != 0) throw std::system_error(errno, std::generic_category(), "read open file limit");
        if (limit.rlim_cur >= limit.rlim_max) return;
        limit.rlim_cur = limit.rlim_max;
        if (setrlimit(RLIMIT_NOFILE, &limit) != 0) throw std::system_error(errno, std::generic_category(), "raise open file limit");
    }
} openFileLimit;

}

#endif
