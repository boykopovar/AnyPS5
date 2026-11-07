#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "nid/Sha1.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class Masks {
public:
    Masks(std::uint16_t guest, mode_t host) : guest(umask_nid_postfix(guest)), host(::umask(host)) {}
    ~Masks() { ::umask(host); umask_nid_postfix(guest); }
    Masks(const Masks&) = delete;
    Masks& operator=(const Masks&) = delete;
private:
    std::uint16_t guest;
    mode_t host;
};

class Object {
public:
    explicit Object(const char* suffix) : name("/anyps5-permissions-" + std::to_string(getpid()) + suffix) {
        const auto digest = Nid::Sha1(std::vector<std::uint8_t>(name.begin(), name.end()));
        for (auto byte : digest) {
            hostName += "0123456789abcdef"[byte >> 4];
            hostName += "0123456789abcdef"[byte & 15];
        }
        ::shm_unlink(hostName.c_str());
    }
    ~Object() { if (descriptor >= 0) ::close(descriptor); ::shm_unlink(hostName.c_str()); }
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
    std::string name;
    int descriptor = -1;
private:
    std::string hostName = "/anyps5-shm-";
};

static void CheckMode(const char* suffix, std::uint16_t guestMask, mode_t hostMask, int requested, mode_t expected) {
    Masks masks(guestMask, hostMask);
    Object object(suffix);
    object.descriptor = shm_open_nid_postfix(object.name.c_str(), 2 | 0x200 | 0x800, requested);
    Require(object.descriptor >= 0, "shared-memory creation failed");
    struct stat info{};
    Require(::fstat(object.descriptor, &info) == 0, "stat failed");
    if ((info.st_mode & 07777) != expected) {
        std::fprintf(stderr, "requested=%o guest-mask=%o host-mask=%o actual=%o expected=%o\n", requested, guestMask, hostMask, info.st_mode & 07777, expected);
        throw std::runtime_error("shared-memory mode did not follow the guest umask");
    }
    const auto observedHostMask = ::umask(hostMask);
    Require(observedHostMask == hostMask, "guest creation changed host umask");
    if (expected == 0600) {
        const int reopened = shm_open_nid_postfix(object.name.c_str(), 2 | 0x200, 0777);
        Require(reopened >= 0, "existing-object reopen failed");
        const int result = ::fstat(reopened, &info);
        ::close(reopened);
        Require(result == 0 && (info.st_mode & 07777) == expected, "reopen changed existing permissions");
    }
}

int main() {
    try {
        CheckMode("-guest-restricts", 0077, 0000, 0666, 0600);
        CheckMode("-host-restricts", 0000, 0077, 0666, 0666);
        CheckMode("-guest-denies", 0777, 0000, 0777, 0000);
        CheckMode("-special-bits", 0022, 0077, 07777, 0755);
        std::puts("guest shared-memory permissions passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
