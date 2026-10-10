#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceSysmoduleIsLoaded(std::uint16_t id);
int APS5_VABI sceSysmoduleLoadModule(std::uint16_t id);
int APS5_VABI sceSysmoduleUnloadModule(std::uint16_t id);
int APS5_VABI sceSysmoduleLoadModuleInternal(std::uint32_t id);
int APS5_VABI sceSysmoduleUnloadModuleInternal(std::uint32_t id);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr std::uint16_t kFiberModuleId = 0x0006;
constexpr std::uint16_t kUltModuleId = 0x0007;
constexpr int kModuleUnloaded = static_cast<int>(0x805A1001);

}

int main() {
    Require(sceSysmoduleIsLoaded(kFiberModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModuleInternal(kFiberModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kFiberModuleId) == 0);
    Require(sceSysmoduleLoadModuleInternal(kFiberModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kFiberModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kFiberModuleId) == 0);
    Require(sceSysmoduleUnloadModuleInternal(kFiberModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kFiberModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModuleInternal(kFiberModuleId) == kModuleUnloaded);

    Require(sceSysmoduleIsLoaded(kUltModuleId) == kModuleUnloaded);
    Require(sceSysmoduleUnloadModule(kUltModuleId) == kModuleUnloaded);
    Require(sceSysmoduleLoadModule(kUltModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kUltModuleId) == 0);
    Require(sceSysmoduleUnloadModule(kUltModuleId) == 0);
    Require(sceSysmoduleIsLoaded(kUltModuleId) == kModuleUnloaded);
}
