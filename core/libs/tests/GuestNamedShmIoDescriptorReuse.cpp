#include "nid/Sha1.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <thread>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

struct GuestIovec { void* base; std::size_t length; };
extern "C" {
int APS5_VABI shm_open_nid_postfix(const char*, int, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI dup2_nid_postfix(int, int);
int APS5_VABI ftruncate_nid_postfix(int, std::int64_t);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
std::int64_t APS5_VABI pwrite_nid_disambig1_nid_postfix(int, const void*, std::size_t, std::int64_t);
std::int64_t APS5_VABI lseek_nid_postfix(int, std::int64_t, int);
std::int64_t APS5_VABI writev_nid_postfix(int, const GuestIovec*, int);
std::int64_t APS5_VABI pwritev_nid_postfix(int, const GuestIovec*, int, std::int64_t);
int* APS5_VABI __error_nid_postfix();
}
namespace {
int original = -1;
int replacement = -1;
bool armed = false;
bool replaceAtProbe = false;
bool replaceBeforeProbe = false;
bool swapped = false;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Object {
    explicit Object(const char* suffix) : name("/anyps5-io-reuse-" + std::to_string(getpid()) + suffix) {
        const auto digest = Nid::Sha1(std::vector<std::uint8_t>(name.begin(), name.end()));
        for (const auto byte : digest) {
            hostName += "0123456789abcdef"[byte >> 4];
            hostName += "0123456789abcdef"[byte & 15];
        }
        descriptor = shm_open_nid_postfix(name.c_str(), 2 | 0x200 | 0x800, 0600);
        Require(descriptor >= 0, "object creation failed");
        Require(::ftruncate(descriptor, 0x4000) == 0, "object sizing failed");
        Require(::pwrite(descriptor, "safe", 4, 0) == 4, "object seeding failed");
    }
    ~Object() { if (descriptor >= 0) ::close(descriptor); ::shm_unlink(hostName.c_str()); }
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
    std::string name;
    std::string hostName = "/anyps5-shm-";
    int descriptor = -1;
};
void Run(int operation, int boundary) {
    replaceAtProbe = boundary == 1;
    replaceBeforeProbe = boundary == 2;
    Object first(("-first-" + std::to_string(operation)).c_str());
    Object second(("-second-" + std::to_string(operation)).c_str());
    if (replaceBeforeProbe) {
        char path[] = "/tmp/anyps5-io-reuse-XXXXXX";
        const int temporary = ::mkstemp(path);
        Require(temporary >= 0, "ordinary replacement creation failed");
        Require(::unlink(path) == 0, "ordinary replacement unlink failed");
        Require(::ftruncate(temporary, 0x4000) == 0 && ::pwrite(temporary, "safe", 4, 0) == 4,
                "ordinary replacement initialization failed");
        Require(::dup2(temporary, second.descriptor) == second.descriptor, "ordinary replacement installation failed");
        ::close(temporary);
    }
    original = first.descriptor;
    replacement = second.descriptor;
    swapped = false;
    armed = true;
    char payload[] = "bad!";
    GuestIovec vector{payload, 4};
    std::int64_t result = 0;
    switch (operation) {
    case 0: result = write_nid_postfix(original, payload, 4); break;
    case 1: result = pwrite_nid_disambig1_nid_postfix(original, payload, 4, 0); break;
    case 2: result = ftruncate_nid_postfix(original, 7); break;
    case 3: result = lseek_nid_postfix(original, 7, SEEK_SET); break;
    case 4: result = writev_nid_postfix(original, &vector, 1); break;
    case 5: result = pwritev_nid_postfix(original, &vector, 1, 0); break;
    }
    armed = false;
    Require(swapped, "descriptor replacement did not run at the selected acquisition boundary");
    struct stat status{};
    char bytes[4]{};
    Require(::fstat(replacement, &status) == 0 && status.st_size == 0x4000, "replacement object size changed");
    Require(::pread(replacement, bytes, 4, 0) == 4 && std::memcmp(bytes, "safe", 4) == 0, "replacement object data changed");
    Require(::lseek(replacement, 0, SEEK_CUR) == 0, "replacement object offset changed");
    Require(result == -1 && *__error_nid_postfix() == 9, "descriptor identity change was not rejected with EBADF");
}
}
static bool ReplaceDescriptor() {
    swapped = true;
    bool replaced = false;
    std::thread reuse([&] {
        replaced = close_nid_postfix(original) == 0 && dup2_nid_postfix(replacement, original) == original;
    });
    reuse.join();
    return replaced;
}
extern "C" ssize_t readlink(const char* path, char* buffer, std::size_t length) {
    if (armed && replaceBeforeProbe && !swapped) {
        if (!ReplaceDescriptor()) return -1;
    }
    const auto result = static_cast<ssize_t>(::syscall(SYS_readlink, path, buffer, length));
    if (result >= 0 && armed && replaceAtProbe && !swapped) {
        if (!ReplaceDescriptor()) return -1;
    }
    return result;
}
extern "C" ssize_t __readlink_chk(const char* path, char* buffer, std::size_t length, std::size_t) {
    return readlink(path, buffer, length);
}
extern "C" int flock(int descriptor, int operation) {
    const int result = static_cast<int>(::syscall(SYS_flock, descriptor, operation));
    if (result == 0 && operation == LOCK_EX && armed && !replaceAtProbe && !replaceBeforeProbe && !swapped) {
        if (!ReplaceDescriptor()) return -1;
    }
    return result;
}
int main() {
    int failures = 0;
    for (int scenario = 0; scenario < 18; ++scenario) {
        const int operation = scenario % 6;
        try {
            Run(operation, scenario / 6);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "scenario %d: %s\n", scenario, error.what());
            ++failures;
        }
    }
    if (failures != 0) return 1;
    std::puts("guest shared-memory I/O descriptor reuse passed");
    return 0;
}
