#ifndef CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_QUEUEENDOFPIPEACTION_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_QUEUEENDOFPIPEACTION_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcQueueEndOfPipeActionPatchAddress(std::uint32_t* cmd, const volatile Label* address);
int APS5_VABI sceAgcQueueEndOfPipeActionPatchData(uint32_t* cmd, uint64_t data);
int APS5_VABI sceAgcQueueEndOfPipeActionPatchGcrCntl(std::uint32_t* cmd, std::uint16_t gcrControl);
int APS5_VABI sceAgcQueueEndOfPipeActionPatchType(std::uint32_t* cmd, std::uint8_t action);

}

#endif
