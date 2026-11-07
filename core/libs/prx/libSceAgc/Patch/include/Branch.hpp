#ifndef CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_BRANCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_BRANCH_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcBranchPatchSetCompareAddress(std::uint32_t* cmd, const volatile std::uint64_t* compareAddress);
int APS5_VABI sceAgcBranchPatchSetThenTarget(std::uint32_t* cmd, const volatile std::uint32_t* target, std::uint32_t sizeInDwords);
int APS5_VABI sceAgcBranchPatchSetElseTarget(std::uint32_t* cmd, const volatile std::uint32_t* target, std::uint32_t sizeInDwords);
int APS5_VABI sceAgcBranchPatchSetThenTarget_0300(std::uint32_t* cmd, std::uint8_t cachePolicy, const volatile std::uint32_t* target, std::uint32_t sizeInDwords);
int APS5_VABI sceAgcBranchPatchSetElseTarget_0300(std::uint32_t* cmd, std::uint8_t cachePolicy, const volatile std::uint32_t* target, std::uint32_t sizeInDwords);

}

#endif
