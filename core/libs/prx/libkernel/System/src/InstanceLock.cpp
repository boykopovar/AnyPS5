#include "prx/libkernel/System/include/InstanceLock.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

InstanceLockMode ParseInstanceLockMode(const char* value) {
    if (value == nullptr || value[0] == '\0') return InstanceLockMode::Off;
    const std::string_view text(value);
    if (text == "off") return InstanceLockMode::Off;
    if (text == "on") return InstanceLockMode::On;
    throw std::runtime_error(std::string(InstanceLockVariable) + " must be on or off, not '" + std::string(text) + "'");
}

std::filesystem::path InstanceLockPath(const std::filesystem::path& root) {
    return root / InstanceLockFileName;
}

void AcquireInstanceLock(IInstanceLockBackend& backend, const std::filesystem::path& root, std::string_view titleId) {
    const auto file = InstanceLockPath(root);
    if (backend.TryLock(file)) return;
    throw std::runtime_error("title " + std::string(titleId) + " is already running from " + root.string() + ": another process holds " + file.string() +
        ". Two copies of one title share this directory's save data and app data and can corrupt each other. Close the other copy, or set " +
        InstanceLockVariable + "=off to run both.");
}
