#include "prx/libkernel/File/include/RandomDevice.hpp"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <random>
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

bool IsRandomDevice(int fd) {
    std::lock_guard lock(g_mutex);
    return g_descriptors.contains(fd);
}

bool ReadRandomDevice(int fd, void* buf, std::size_t nbytes) {
    std::lock_guard lock(g_mutex);
    if (!g_descriptors.contains(fd)) return false;
    static std::random_device device;
    auto* bytes = static_cast<std::uint8_t*>(buf);
    for (std::size_t offset = 0; offset < nbytes; offset += sizeof(std::uint32_t)) {
        const std::uint32_t value = device();
        std::memcpy(bytes + offset, &value, nbytes - offset < sizeof(value) ? nbytes - offset : sizeof(value));
    }
    return true;
}

void RememberRandomDevice(int fd) {
    std::lock_guard lock(g_mutex);
    g_descriptors.insert(fd);
}

void ForgetRandomDevice(int fd) {
    std::lock_guard lock(g_mutex);
    g_descriptors.erase(fd);
}

}
