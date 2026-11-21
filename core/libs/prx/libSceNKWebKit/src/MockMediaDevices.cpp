#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace NKWebKit {

struct MockMediaDeviceManager {
    static MockMediaDeviceManager& Instance() {
        static MockMediaDeviceManager manager;
        return manager;
    }

    void Clear() {
        devices.clear();
        cleared = true;
    }

    void Reset() {
        devices.clear();
        cleared = false;
        reset = true;
    }

    void Remove(const std::string& persistentId) {
        std::erase(devices, persistentId);
    }

    std::vector<std::string> devices;
    bool cleared = false;
    bool reset = false;
};

} // namespace NKWebKit

extern "C" {

void APS5_VABI WKClearMockMediaDevices() {
    NKWebKit::MockMediaDeviceManager::Instance().Clear();
}

void APS5_VABI WKRemoveMockMediaDevice(const void* persistentIdRef) {
    if (!persistentIdRef) {
        return;
    }
    const auto* str = NKWebKit::Cast<NKWebKit::String>(persistentIdRef, __func__);
    NKWebKit::MockMediaDeviceManager::Instance().Remove(str->Utf8());
}

void APS5_VABI WKResetMockMediaDevices() {
    NKWebKit::MockMediaDeviceManager::Instance().Reset();
}

}
