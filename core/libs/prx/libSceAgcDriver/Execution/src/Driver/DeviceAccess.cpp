#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/DeviceAccess.hpp"

namespace AgcDriver::DriverDetail {

std::shared_ptr<VulkanDevice> DevicePointer::Load() const { return pointer.load(std::memory_order_acquire); }

DevicePointer::operator std::shared_ptr<VulkanDevice>() const { return Load(); }

DevicePointer& DevicePointer::operator=(std::shared_ptr<VulkanDevice> value) {
    pointer.store(std::move(value), std::memory_order_release);
    return *this;
}

void DevicePointer::Reset() { *this = nullptr; }

VulkanDevice* DevicePointer::operator->() const { return Load().get(); }

DevicePointer::operator bool() const { return Load() != nullptr; }

bool DevicePointer::operator==(std::nullptr_t) const { return Load() == nullptr; }

void DeviceUseGate::lock_shared() {
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return !replacing; });
    ++users;
}

void DeviceUseGate::unlock_shared() {
    std::lock_guard lock(mutex);
    if (--users == 0)
        changed.notify_all();
}

void DeviceUseGate::lock() {
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return !replacing; });
    replacing = true;
    changed.wait(lock, [&] { return users == 0; });
}

void DeviceUseGate::unlock() {
    std::lock_guard lock(mutex);
    replacing = false;
    changed.notify_all();
}

}
