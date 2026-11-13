#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoNativeExtEssential_CreateDecInstance(const void* param, void** instance);
int APS5_VABI sceVideoNativeExtEssential_DestroyDecInstance(void* const* instance);
int APS5_VABI sceVideoNativeExtEssential_Finalize();
int APS5_VABI sceVideoNativeExtEssential_Initialize(const void* param);
int APS5_VABI sceVideoNativeExtEssential_IsInit();

}
