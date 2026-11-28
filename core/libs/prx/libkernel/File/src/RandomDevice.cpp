#include "prx/libkernel/File/include/RandomDevice.hpp"
#include "prx/libc/include/HostEntropy.hpp"

#include <mutex>
#include <set>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

std::mutex g_mutex;
std::set<int> g_descriptors;
std::set<int> g_nullDescriptors;

}

namespace File {

bool IsRandomDevicePath(std::string_view path) {
    return path == "/dev/random" || path == "/dev/urandom";
}

int OpenRandomDevice() {
#ifdef _WIN32
    const int fd = ::_open("NUL", _O_RDONLY | _O_BINARY);
#else
    const int fd = ::open("/dev/null", O_RDONLY);
#endif
    if (fd < 0) return -1;
    std::lock_guard lock(g_mutex);
    g_descriptors.insert(fd);
    return fd;
}

int OpenNullDevice(int flags) {
    const int access = flags & 3;
#ifdef _WIN32
    const int fd = ::_open("NUL", (access == 0 ? _O_RDONLY : access == 1 ? _O_WRONLY : _O_RDWR) | _O_BINARY);
#else
    const int fd = ::open("/dev/null", access == 0 ? O_RDONLY : access == 1 ? O_WRONLY : O_RDWR);
#endif
    if (fd < 0) return -1;
    std::lock_guard lock(g_mutex);
    g_nullDescriptors.insert(fd);
    return fd;
}

bool IsNullDevice(int fd) {
    std::lock_guard lock(g_mutex);
    return g_nullDescriptors.contains(fd);
}

bool IsRandomDevice(int fd) {
    std::lock_guard lock(g_mutex);
    return g_descriptors.contains(fd);
}

bool ReadRandomDevice(int fd, void* buf, std::size_t nbytes) {
    std::lock_guard lock(g_mutex);
    if (!g_descriptors.contains(fd)) return false;
    HostEntropy_nid_no_patch(buf, nbytes);
    return true;
}

void RememberRandomDevice(int fd) {
    std::lock_guard lock(g_mutex);
    g_descriptors.insert(fd);
}

void ForgetRandomDevice(int fd) {
    std::lock_guard lock(g_mutex);
    g_descriptors.erase(fd);
    g_nullDescriptors.erase(fd);
}

}
