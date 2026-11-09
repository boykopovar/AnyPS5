#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePfsGetBlockTableReadCommand();
int APS5_VABI scePfsGetHeaderReadCommand();
int APS5_VABI scePfsGetPreadBlockRange();
int APS5_VABI scePfsGetSingleBlockReadCommand();
int APS5_VABI scePfsGetUncompressedSize();
int APS5_VABI scePfsGetWorkBufferSize();
int APS5_VABI scePfsPread();
int APS5_VABI scePfsPreadDecompressBlock();
int APS5_VABI scePfsValidate();

}
