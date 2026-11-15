#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePfsGetBlockTableReadCommand(const void* header, void* command);
int APS5_VABI scePfsGetHeaderReadCommand(void* command);
int APS5_VABI scePfsGetPreadBlockRange(std::uint64_t size, std::uint64_t offset, std::uint32_t* firstBlockOut, std::uint32_t* lastBlockOut, const void* header, const void* blockTable);
int APS5_VABI scePfsGetSingleBlockReadCommand(const void* header, const void* blockTable, int blockIndex, void* command);
int APS5_VABI scePfsGetUncompressedSize(const void* header, std::uint64_t* sizeOut);
std::int64_t APS5_VABI scePfsGetWorkBufferSize(void);
std::int64_t APS5_VABI scePfsPread(void* buffer, std::uint64_t size, std::uint64_t offset, const void* header, void* workBuffer);
int APS5_VABI scePfsPreadDecompressBlock();
int APS5_VABI scePfsValidate(const void* header, int mode);

}
