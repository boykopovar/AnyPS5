#include "prx/libkernel/System/include/InstanceLockHost.hpp"

#include <stdexcept>
#include <string>
#include <windows.h>

namespace {

std::string Describe(const std::filesystem::path& file, const char* action, DWORD error) {
    return std::string("cannot ") + action + " the instance lock " + file.string() + ": Windows error " + std::to_string(error) + ". Set " + InstanceLockVariable +
        "=off to skip this check.";
}

class WindowsInstanceLockBackend final : public IInstanceLockBackend {
public:
    WindowsInstanceLockBackend() = default;
    WindowsInstanceLockBackend(const WindowsInstanceLockBackend&) = delete;
    WindowsInstanceLockBackend& operator=(const WindowsInstanceLockBackend&) = delete;
    ~WindowsInstanceLockBackend() override {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    }

    bool TryLock(const std::filesystem::path& file) override {
        if (handle != INVALID_HANDLE_VALUE) throw std::logic_error("the instance lock is already held on " + file.string());
        const HANDLE opened = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (opened == INVALID_HANDLE_VALUE) throw std::runtime_error(Describe(file, "open", GetLastError()));
        OVERLAPPED region{};
        if (!LockFileEx(opened, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &region)) {
            const DWORD error = GetLastError();
            CloseHandle(opened);
            if (error == ERROR_LOCK_VIOLATION) return false;
            throw std::runtime_error(Describe(file, "lock", error));
        }
        handle = opened;
        return true;
    }

private:
    HANDLE handle = INVALID_HANDLE_VALUE;
};

}

std::unique_ptr<IInstanceLockBackend> MakeHostInstanceLockBackend() {
    return std::make_unique<WindowsInstanceLockBackend>();
}
