#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePlayGoClose(int handle);
int APS5_VABI scePlayGoGetChunkId(int handle, uint16_t* outChunkIdList, uint32_t numberOfEntries, uint32_t* outEntries);
int APS5_VABI scePlayGoGetEta(int handle, const uint16_t* chunkIds, uint32_t numberOfEntries, int64_t* outEta);
int APS5_VABI scePlayGoGetInstallChunkId(int handle, uint16_t* outChunkIdList, uint32_t numberOfEntries, uint32_t* outEntries);
int APS5_VABI scePlayGoGetInstallSpeed(int handle, int32_t* outSpeed);
int APS5_VABI scePlayGoGetLanguageMask(int handle, uint64_t* outLanguageMask);
int APS5_VABI scePlayGoGetLocus(int handle, const uint16_t* chunkIds, uint32_t numberOfEntries, int8_t* outLoci);
int APS5_VABI scePlayGoGetOptionalChunk(int handle, std::uint32_t type, PlayGoOptionalChunk* option);
int APS5_VABI scePlayGoGetProgress(int handle, const uint16_t* chunkIds, uint32_t numberOfEntries, PlayGoProgress* outProgress);
int APS5_VABI scePlayGoGetSupportedOptionalChunk(int handle, std::uint32_t type, PlayGoOptionalChunk* option);
int APS5_VABI scePlayGoGetToDoList(int handle, PlayGoToDo* outTodoList, uint32_t numberOfEntries, uint32_t* outEntries);
int APS5_VABI scePlayGoInitialize(const PlayGoInitParams* init);
int APS5_VABI scePlayGoOpen(int* outHandle, const void* param);
int APS5_VABI scePlayGoPrefetch(int handle, const uint16_t* chunkIds, uint32_t numberOfEntries, int8_t minimumLocus);
int APS5_VABI scePlayGoPrefetchOptionalChunk(int handle, int32_t type, const PlayGoOptionalChunk* option);
int APS5_VABI scePlayGoSetInstallSpeed(int handle, int32_t speed);
int APS5_VABI scePlayGoSetToDoList(int handle, const PlayGoToDo* todoList, uint32_t numberOfEntries);
int APS5_VABI scePlayGoTerminate(void);
int APS5_VABI scePlayGoRequestNextChunk();
int APS5_VABI scePlayGoSetLanguageMask(int handle, uint64_t languageMask);
int APS5_VABI scePlayGoSnapshot();

}
