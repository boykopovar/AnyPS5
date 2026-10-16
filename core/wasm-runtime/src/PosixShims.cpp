#include <atomic>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

std::atomic<unsigned> g_memoryFiles{0};

timespec ToTimespec(const timeval& value) {
    return {value.tv_sec, static_cast<long>(value.tv_usec) * 1000};
}

}  // namespace

extern "C" {

int memfd_create(const char* name, unsigned flags) {
    static_cast<void>(flags);
    const auto path = "/tmp/.memfd-" + std::to_string(g_memoryFiles.fetch_add(1)) + "-" + (name ? name : "");
    const int descriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (descriptor >= 0) ::unlink(path.c_str());
    return descriptor;
}

int futimes(int descriptor, const timeval times[2]) {
    if (!times) return ::futimens(descriptor, nullptr);
    const timespec converted[2] = {ToTimespec(times[0]), ToTimespec(times[1])};
    return ::futimens(descriptor, converted);
}

int lutimes(const char* path, const timeval times[2]) {
    if (!times) return ::utimensat(AT_FDCWD, path, nullptr, AT_SYMLINK_NOFOLLOW);
    const timespec converted[2] = {ToTimespec(times[0]), ToTimespec(times[1])};
    return ::utimensat(AT_FDCWD, path, converted, AT_SYMLINK_NOFOLLOW);
}

int sched_getcpu() {
    return 0;
}

}
