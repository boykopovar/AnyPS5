#include "prx/libkernel/System/include/InstanceLockHost.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <unistd.h>

namespace {

std::string Describe(const std::filesystem::path& file, const char* action, int error) {
    return std::string("cannot ") + action + " the instance lock " + file.string() + ": " + std::strerror(error) + ". Set " + InstanceLockVariable + "=off to skip this check.";
}

class PosixInstanceLockBackend final : public IInstanceLockBackend {
public:
    PosixInstanceLockBackend() = default;
    PosixInstanceLockBackend(const PosixInstanceLockBackend&) = delete;
    PosixInstanceLockBackend& operator=(const PosixInstanceLockBackend&) = delete;
    ~PosixInstanceLockBackend() override {
        if (descriptor >= 0) ::close(descriptor);
    }

    bool TryLock(const std::filesystem::path& file) override {
        if (descriptor >= 0) throw std::logic_error("the instance lock is already held on " + file.string());
        int opened;
        do {
            opened = ::open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        } while (opened < 0 && errno == EINTR);
        if (opened < 0) throw std::runtime_error(Describe(file, "open", errno));
        int result;
        do {
            result = ::flock(opened, LOCK_EX | LOCK_NB);
        } while (result != 0 && errno == EINTR);
        if (result != 0) {
            const int error = errno;
            ::close(opened);
            if (error == EWOULDBLOCK) return false;
            throw std::runtime_error(Describe(file, "lock", error));
        }
        descriptor = opened;
        return true;
    }

private:
    int descriptor = -1;
};

}

std::unique_ptr<IInstanceLockBackend> MakeHostInstanceLockBackend() {
    return std::make_unique<PosixInstanceLockBackend>();
}
