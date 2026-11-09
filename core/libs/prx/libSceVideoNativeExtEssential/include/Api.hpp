#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoNativeExtEssential_CreateDecInstance();
int APS5_VABI sceVideoNativeExtEssential_DestroyDecInstance();
int APS5_VABI sceVideoNativeExtEssential_Finalize();
int APS5_VABI sceVideoNativeExtEssential_Initialize();
int APS5_VABI sceVideoNativeExtEssential_IsInit();

}
