#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

struct Ngs2JobSchedulerOption;

extern "C" {

int APS5_VABI sceNgs2JobSchedulerResetOption(Ngs2JobSchedulerOption*);
int APS5_VABI sceNgs2ModuleArrayEnumItems(const void* array, void* out, std::uint32_t max);
int APS5_VABI sceNgs2ModuleEnumConfigs(void** out, std::uint32_t max);
int APS5_VABI sceNgs2ModuleQueueEnumItems(const void* queue, void* out, std::uint32_t max);
int APS5_VABI sceNgs2RackQueryInfo(uintptr_t rack_handle, std::uint32_t info_id, void* info, std::size_t info_size);
int APS5_VABI sceNgs2RackRunCommands(uintptr_t rack_handle, const void* commands, std::size_t count);
int APS5_VABI sceNgs2StreamCreate(uintptr_t system_handle, const void* option, const void* buffer_info, uintptr_t* handle);
int APS5_VABI sceNgs2StreamCreateWithAllocator(uintptr_t system_handle, const void* option, const void* allocator, uintptr_t* handle);
int APS5_VABI sceNgs2StreamDestroy(uintptr_t stream_handle, void* buffer_info);
int APS5_VABI sceNgs2StreamQueryBufferSize(const void* option, void* buffer_info);
int APS5_VABI sceNgs2StreamQueryInfo(uintptr_t stream_handle, std::uint32_t info_id, void* info, std::size_t info_size);
int APS5_VABI sceNgs2StreamResetOption(void* option);
int APS5_VABI sceNgs2StreamRunCommands(uintptr_t stream_handle, const void* commands, std::size_t count);
int APS5_VABI sceNgs2SystemQueryInfo(uintptr_t system_handle, std::uint32_t info_id, void* info, std::size_t info_size);
int APS5_VABI sceNgs2SystemRunCommands(uintptr_t system_handle, const void* commands, std::size_t count);

}
