#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "nid/Sha1.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using Unlink = int (*)(const char*);
constexpr std::int64_t Page = 0x4000;
constexpr int ReadWriteCreate = 0xa02;

struct Object {
    std::string name = "/anyps5-concurrent-" + std::to_string(getpid());
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
};

bool Race(int descriptor) {
    for (int iteration = 0; iteration < 500; ++iteration) {
        const auto size = (iteration & 1) ? 0 : Page;
        if (ftruncate_nid_postfix(descriptor, size) != 0) return false;
        const auto written = pwrite_nid_disambig1_nid_postfix(descriptor, "x", 1, Page / 2);
        if (written < 0) return false;
        FileStat info{};
        if (fstat_nid_disambig1_nid_postfix(descriptor, &info) != 0 ||
            (info.st_size != 0 && info.st_size != Page)) return false;
    }
    return true;
}

bool NoDescriptorFallback(int descriptor) {
    if (ftruncate_nid_postfix(descriptor, Page) != 0) return false;
    const pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        const struct rlimit limit{0, 0};
        if (setrlimit(RLIMIT_NOFILE, &limit) != 0) _exit(2);
        std::int64_t written = -2;
        try {
            written = pwrite_nid_disambig1_nid_postfix(descriptor, "x", 1, Page);
        } catch (const std::exception& exception) {
            std::fprintf(stderr, "low-RLIMIT child: fd=%d pwrite threw=%s guest_errno=%d native_errno=%d\n",
                         descriptor, exception.what(), *__error_nid_postfix(), errno);
            _exit(3);
        } catch (...) {
            std::fprintf(stderr, "low-RLIMIT child: fd=%d pwrite threw unknown exception guest_errno=%d native_errno=%d\n",
                         descriptor, *__error_nid_postfix(), errno);
            _exit(3);
        }
        const int guestError = *__error_nid_postfix();
        const int nativeError = errno;
        struct stat info{};
        if (::fstat(descriptor, &info) != 0) {
            std::fprintf(stderr, "low-RLIMIT child: fd=%d pwrite=%lld guest_errno=%d native_errno=%d fstat_errno=%d\n",
                         descriptor, static_cast<long long>(written), guestError, nativeError, errno);
            _exit(4);
        }
        if (written != 0 && !(written == -1 && guestError == 24)) {
            std::fprintf(stderr, "low-RLIMIT child: fd=%d pwrite=%lld guest_errno=%d native_errno=%d st_size=%lld\n",
                         descriptor, static_cast<long long>(written), guestError, nativeError, static_cast<long long>(info.st_size));
            _exit(5);
        }
        if (info.st_size != Page) {
            std::fprintf(stderr, "low-RLIMIT child: fd=%d pwrite=%lld guest_errno=%d native_errno=%d st_size=%lld expected=%lld\n",
                         descriptor, static_cast<long long>(written), guestError, nativeError,
                         static_cast<long long>(info.st_size), static_cast<long long>(Page));
            _exit(6);
        }
        _exit(0);
    }
    int status = 0;
    return waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool ChildrenSucceeded(const pid_t* children, int count) {
    bool okay = true;
    for (int index = 0; index < count; ++index) {
        int status = 0;
        const auto result = waitpid(children[index], &status, 0);
        okay = result == children[index] && WIFEXITED(status) && WEXITSTATUS(status) == 0 && okay;
    }
    return okay;
}

void Run() {
    Object object;
    const auto unlinkObject = reinterpret_cast<Unlink>(dlsym(RTLD_DEFAULT, "shm_unlink_nid_postfix"));
    if (unlinkObject == nullptr) throw std::runtime_error("guest shm_unlink export is missing");
    const int descriptor = shm_open_nid_postfix(object.name.c_str(), ReadWriteCreate, 0600);
    if (descriptor < 0) throw std::runtime_error("shared object creation failed");

    pid_t children[4]{};
    int childCount = 0;
    bool okay = true;
    for (int index = 0; index < 4; ++index) {
        const pid_t child = fork();
        if (child < 0) {
            okay = false;
            break;
        }
        if (child == 0) {
            const int own = (index & 1) ? shm_open_nid_postfix(object.name.c_str(), 2, 0600) : descriptor;
            const bool childOkay = own >= 0 && Race(own);
            if ((index & 1) && own >= 0) close_nid_postfix(own);
            _exit(childOkay ? 0 : 1);
        }
        children[childCount++] = child;
    }
    okay = Race(descriptor) && okay;
    okay = ChildrenSucceeded(children, childCount) && okay;
    if (!okay) throw std::runtime_error("interleaved resize/write produced an invalid shared-object size");
    if (!NoDescriptorFallback(descriptor))
        throw std::runtime_error("EOF write with no spare descriptors grew the shared object or returned an unexpected error");

    if (fchmod_nid_postfix(descriptor, 0400) != 0)
        throw std::runtime_error("changing shared-object permissions to read-only failed");
    if (::geteuid() != 0) {
        *__error_nid_postfix() = 0;
        if (unlinkObject(object.name.c_str()) != -1 || *__error_nid_postfix() != 13)
            throw std::runtime_error("unlink of a read-only shared object did not return EACCES");
    } else {
        std::fprintf(stderr, "running as root, which ignores file permissions: the read-only unlink case not tested\n");
    }
    if (fchmod_nid_postfix(descriptor, 0600) != 0)
        throw std::runtime_error("restoring shared-object permissions failed");
    if (unlinkObject(object.name.c_str()) != 0)
        throw std::runtime_error("unlink after restoring write permission failed");
    if (close_nid_postfix(descriptor) != 0)
        throw std::runtime_error("shared-object close failed");
    std::puts("cross-process truncate/write, descriptor sharing and unlink permissions passed");
}
}

int main() {
    try {
        Run();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
