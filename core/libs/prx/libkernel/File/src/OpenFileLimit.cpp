#if defined(__linux__) || defined(__APPLE__)

#include <cerrno>
#include <cstddef>
#ifdef __linux__
#include <cstdio>
#endif
#include <system_error>
#include <sys/resource.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace {

struct OpenFileLimit {
    OpenFileLimit() {
        rlimit limit{};
        if (getrlimit(RLIMIT_NOFILE, &limit) != 0) throw std::system_error(errno, std::generic_category(), "read open file limit");
        rlim_t usable = limit.rlim_max;
#ifdef __linux__
        if (std::FILE* nrOpenFile = std::fopen("/proc/sys/fs/nr_open", "r")) {
            unsigned long long nrOpen = 0;
            if (std::fscanf(nrOpenFile, "%llu", &nrOpen) == 1 && nrOpen > 0) {
                const rlim_t cap = static_cast<rlim_t>(nrOpen);
                if (usable == RLIM_INFINITY || cap < usable) usable = cap;
            }
            std::fclose(nrOpenFile);
        }
#elif defined(__APPLE__)
        int perProcess = 0;
        std::size_t size = sizeof(perProcess);
        if (sysctlbyname("kern.maxfilesperproc", &perProcess, &size, nullptr, 0) != 0) throw std::system_error(errno, std::generic_category(), "read kern.maxfilesperproc");
        if (perProcess > 0 && (usable == RLIM_INFINITY || static_cast<rlim_t>(perProcess) < usable)) usable = static_cast<rlim_t>(perProcess);
#endif
        if (usable == RLIM_INFINITY) return;
        if (limit.rlim_cur == usable && limit.rlim_max == usable) return;
        limit.rlim_cur = usable;
        limit.rlim_max = usable;
        if (setrlimit(RLIMIT_NOFILE, &limit) != 0) throw std::system_error(errno, std::generic_category(), "raise open file limit");
    }
} openFileLimit;

}

#elif defined(_WIN32)

#include <cerrno>
#include <cstdio>
#include <system_error>

namespace {

struct OpenFileLimit {
    OpenFileLimit() {
        if (_getmaxstdio() >= 8192) return;
        if (_setmaxstdio(8192) != 8192) throw std::system_error(errno, std::generic_category(), "raise open stream limit");
    }
} openFileLimit;

}

#endif
