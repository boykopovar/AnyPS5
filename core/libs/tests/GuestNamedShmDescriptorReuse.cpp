#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "nid/Sha1.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace {
constexpr int ReadWrite = 2;
constexpr int Create = 0x200;
constexpr int Exclusive = 0x800;
constexpr int Shared = 0x1;
constexpr std::size_t Page = 0x4000;
int armedDescriptor = -1;
int replacementDescriptor = -1;
bool armed = false;
bool swapped = false;

int CreateOrdinaryFile() {
    char path[] = "/tmp/anyps5-descriptor-reuse-XXXXXX";
    const int descriptor = ::mkstemp(path);
    if (descriptor < 0) throw std::runtime_error("ordinary file creation failed");
    if (::unlink(path) != 0) {
        ::close(descriptor);
        throw std::runtime_error("ordinary file unlink failed");
    }
    constexpr char contents[] = "ordinary file contents";
    if (::write(descriptor, contents, sizeof(contents)) != static_cast<ssize_t>(sizeof(contents)) ||
        ::lseek(descriptor, 0, SEEK_SET) != 0) {
        ::close(descriptor);
        throw std::runtime_error("ordinary file initialization failed");
    }
    return descriptor;
}

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string(message) + " (guest errno=" + std::to_string(*__error_nid_postfix()) + ")");
}

struct Object {
    std::string name = "/anyps5-descriptor-reuse-" + std::to_string(getpid());
    std::string hostName = "/anyps5-shm-";
    Object() {
        constexpr char hex[] = "0123456789abcdef";
        const auto digest = Nid::Sha1(std::vector<std::uint8_t>(name.begin(), name.end()));
        for (const auto byte : digest) {
            hostName += hex[byte >> 4];
            hostName += hex[byte & 15];
        }
        ::shm_unlink(hostName.c_str());
    }
    ~Object() { ::shm_unlink(hostName.c_str()); }
    int Open(int flags) const { return shm_open_nid_postfix(name.c_str(), flags, 0600); }
};
}

extern "C" int fcntl(int descriptor, int command, ...) {
    long argument = 0;
    switch (command) {
    case F_GETFD:
    case F_GETFL:
    case F_GETOWN:
#ifdef F_GETSIG
    case F_GETSIG:
#endif
#ifdef F_GETLEASE
    case F_GETLEASE:
#endif
#ifdef F_GETPIPE_SZ
    case F_GETPIPE_SZ:
#endif
#ifdef F_GET_SEALS
    case F_GET_SEALS:
#endif
        break;
#ifdef F_GETLK
    case F_GETLK:
    case F_SETLK:
    case F_SETLKW: {
        va_list args;
        va_start(args, command);
        argument = reinterpret_cast<long>(va_arg(args, void*));
        va_end(args);
        break;
    }
#endif
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETFL:
    case F_SETOWN:
#ifdef F_SETSIG
    case F_SETSIG:
#endif
#ifdef F_SETLEASE
    case F_SETLEASE:
#endif
#ifdef F_NOTIFY
    case F_NOTIFY:
#endif
#ifdef F_SETPIPE_SZ
    case F_SETPIPE_SZ:
#endif
#ifdef F_ADD_SEALS
    case F_ADD_SEALS:
#endif
    {
        va_list args;
        va_start(args, command);
        argument = va_arg(args, int);
        va_end(args);
        break;
    }
    default:
        errno = EINVAL;
        return -1;
    }
    const long result = ::syscall(SYS_fcntl, descriptor, command, argument);
    if (command == F_GETFL && armed && !swapped) {
        swapped = true;
        if (::syscall(SYS_dup2, replacementDescriptor, armedDescriptor) != armedDescriptor) return -1;
    }
    return static_cast<int>(result);
}

int main() {
    try {
        Object object;
        const int descriptor = object.Open(ReadWrite | Create | Exclusive);
        Require(descriptor >= 0 && ftruncate_nid_postfix(descriptor, Page) == 0, "shared-memory setup failed");
        Require(pwrite_nid_disambig1_nid_postfix(descriptor, "original", 8, 0) == 8, "shared-memory seed failed");
        const int verifier = object.Open(ReadWrite);
        Require(verifier >= 0, "shared-memory verifier open failed");
        const int replacement = CreateOrdinaryFile();
        Require(replacement >= 0, "ordinary-file setup failed");
        armedDescriptor = descriptor;
        replacementDescriptor = replacement;
        armed = true;
        auto* mapped = static_cast<char*>(mmap_nid_postfix(nullptr, Page, 3, Shared, descriptor, 0));
        armed = false;
        Require(swapped, "native F_GETFL interposer did not replace the guest descriptor");
        Require(mapped != reinterpret_cast<char*>(static_cast<std::uintptr_t>(-1)), "guest mmap failed after descriptor replacement");
        Require(std::memcmp(mapped, "original", 8) == 0, "mapping followed the replacement descriptor instead of the pinned shared-memory object");
        mapped[0] = 0x70;
        char byte = 0;
        Require(pread_nid_postfix(verifier, &byte, 1, 0) == 1 && byte == 0x70, "mapping did not update the original shared-memory object");
        Require(munmap_nid_postfix(mapped, Page) == 0, "guest mapping cleanup failed");
        Require(close_nid_postfix(descriptor) == 0 && close_nid_postfix(verifier) == 0, "guest descriptor cleanup failed");
        Require(::close(replacement) == 0, "replacement descriptor cleanup failed");
        std::puts("named shared-memory descriptor reuse checks passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
