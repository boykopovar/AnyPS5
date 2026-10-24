#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSysmoduleGetModuleInfoForUnwind(std::uint64_t addr, int flags, ModuleInfoForUnwind* info);
int APS5_VABI sceSysmoduleGetModuleHandleInternal(std::uint32_t id, std::int32_t* handle);
int APS5_VABI sceSysmoduleIsLoaded(std::uint16_t id);
int APS5_VABI sceSysmoduleLoadModule(std::uint16_t id);
int APS5_VABI sceSysmoduleLoadModuleInternalWithArg(std::uint32_t id, int argc, void* argv, std::uint64_t unk, int* ret);
int APS5_VABI sceSysmoduleUnloadModule(std::uint16_t id);
int APS5_VABI sceSysmoduleLoadModuleInternal(std::uint32_t id);
int APS5_VABI sceSysmoduleUnloadModuleInternal(std::uint32_t id);
int APS5_VABI sceSysmoduleUnloadModuleInternalWithArg(std::uint32_t id, int argc, void* argv, std::uint64_t unk, int* ret);
int APS5_VABI sceSysmoduleIsLoadedInternal(std::uint32_t id);
int APS5_VABI sceSysmoduleIsCalledFromSysModule();
int APS5_VABI sceSysmoduleIsCameraPreloaded();
int APS5_VABI sceSysmoduleLoadModuleByNameInternal(std::uint8_t* fname, std::uint64_t a1, std::uint64_t a2, std::uint64_t a3, std::uint64_t a4, std::uint64_t a5);
int APS5_VABI sceSysmoduleMapLibcForLibkernel();
int APS5_VABI sceSysmodulePreloadModuleForLibkernel();
int APS5_VABI sceSysmoduleUnloadModuleByNameInternal();

}
