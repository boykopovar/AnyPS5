#pragma once

#include "SceTypes.hpp"
#include <cstddef>

using StatsWriter = void(APS5_VABI*)(void*, const char*);

extern "C" {

void* APS5_VABI sceJemspaceCreate(const char* name, std::size_t limit, unsigned flags);
int APS5_VABI sceJemspaceDestroy(void* p);
void* APS5_VABI sceJemspaceMalloc(void* p, std::size_t size);
int APS5_VABI sceJemspaceFree(void*, void* allocation);
void* APS5_VABI sceJemspaceCalloc(void* p, std::size_t count, std::size_t size);
void* APS5_VABI sceJemspaceMemalign(void* p, std::size_t alignment, std::size_t size);
void* APS5_VABI sceJemspaceRealloc(void* p, void* allocation, std::size_t size);
void* APS5_VABI sceJemspaceReallocalign(void* p, void* allocation, std::size_t alignment, std::size_t size);
int APS5_VABI sceJemspacePosixMemalign(void* p, void** allocation, std::size_t alignment, std::size_t size);
std::size_t APS5_VABI sceJemspaceMallocUsableSize(const void* allocation);
int APS5_VABI sceJemspacePurge(void* p);
void APS5_VABI sceJemspacePrintStats(StatsWriter write, void* opaque, const char* options);
void APS5_VABI sceJemspaceGetStats(const void* p, void* stats);

}
