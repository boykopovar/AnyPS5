#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>

extern "C" {
int APS5_VABI sceSysmoduleIsLoaded(std::uint16_t id);
int APS5_VABI sceSysmoduleLoadModule(std::uint16_t id);
int APS5_VABI sceSysmoduleUnloadModule(std::uint16_t id);
int APS5_VABI sceSysmoduleLoadModuleInternal(std::uint32_t id);
int APS5_VABI sceSysmoduleUnloadModuleInternal(std::uint32_t id);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr std::uint16_t fiberModuleId = 0x0006;
constexpr std::uint16_t ultModuleId = 0x0007;
constexpr int moduleUnloaded = static_cast<int>(0x805A1001);

class ModuleUnloadGuard {
public:
    explicit ModuleUnloadGuard(std::uint16_t id) : id(id) {}

    ~ModuleUnloadGuard() {
        for (int attempt = 0; attempt < 16 && sceSysmoduleIsLoaded(id) == 0; ++attempt) sceSysmoduleUnloadModuleInternal(id);
    }

    ModuleUnloadGuard(const ModuleUnloadGuard&) = delete;
    ModuleUnloadGuard& operator=(const ModuleUnloadGuard&) = delete;

private:
    std::uint16_t id;
};

const Case fiberInitiallyUnloaded{"IsLoaded_FiberBeforeLoad_ReportsUnloaded", [] {
    RequireEqual(sceSysmoduleIsLoaded(fiberModuleId), moduleUnloaded, "fiber module state");
}};

const Case internalLoadCounting{"LoadModuleInternal_LoadedTwice_StaysLoadedUntilUnloadedTwice", [] {
    const ModuleUnloadGuard guard(fiberModuleId);
    RequireEqual(sceSysmoduleLoadModuleInternal(fiberModuleId), 0, "first load");
    RequireEqual(sceSysmoduleIsLoaded(fiberModuleId), 0, "state after the first load");
    RequireEqual(sceSysmoduleLoadModuleInternal(fiberModuleId), 0, "second load");
    RequireEqual(sceSysmoduleUnloadModuleInternal(fiberModuleId), 0, "first unload");
    RequireEqual(sceSysmoduleIsLoaded(fiberModuleId), 0, "state after the first unload");
    RequireEqual(sceSysmoduleUnloadModuleInternal(fiberModuleId), 0, "second unload");
    RequireEqual(sceSysmoduleIsLoaded(fiberModuleId), moduleUnloaded, "state after the second unload");
}};

const Case internalUnloadWhenUnloaded{"UnloadModuleInternal_NotLoaded_ReportsUnloaded", [] {
    RequireEqual(sceSysmoduleUnloadModuleInternal(fiberModuleId), moduleUnloaded, "unload of an unloaded module");
}};

const Case ultInitiallyUnloaded{"IsLoaded_UltBeforeLoad_ReportsUnloaded", [] {
    RequireEqual(sceSysmoduleIsLoaded(ultModuleId), moduleUnloaded, "ult module state");
}};

const Case unloadWhenUnloaded{"UnloadModule_NotLoaded_ReportsUnloaded", [] {
    RequireEqual(sceSysmoduleUnloadModule(ultModuleId), moduleUnloaded, "unload of an unloaded module");
}};

const Case loadAndUnload{"LoadModule_LoadedOnce_UnloadRestoresUnloadedState", [] {
    const ModuleUnloadGuard guard(ultModuleId);
    RequireEqual(sceSysmoduleLoadModule(ultModuleId), 0, "load");
    RequireEqual(sceSysmoduleIsLoaded(ultModuleId), 0, "state after load");
    RequireEqual(sceSysmoduleUnloadModule(ultModuleId), 0, "unload");
    RequireEqual(sceSysmoduleIsLoaded(ultModuleId), moduleUnloaded, "state after unload");
}};

} // namespace
