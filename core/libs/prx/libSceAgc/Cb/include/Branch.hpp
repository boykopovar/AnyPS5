#ifndef CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_BRANCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_BRANCH_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbBranch(CommandBuffer* buf, std::uint8_t mode, std::uint8_t compareFunction, const volatile std::uint64_t* compareAddress, std::uint64_t mask, std::uint64_t reference, std::uint8_t cachePolicy1, const volatile std::uint32_t* buffer1, std::uint32_t sizeInDwords1, std::uint8_t cachePolicy2, const volatile std::uint32_t* buffer2, std::uint32_t sizeInDwords2);
std::uint32_t APS5_VABI sceAgcCbBranchGetSize();
std::uint32_t* APS5_VABI sceAgcCbCondWrite(CommandBuffer* buf, std::uint32_t compareFunction, std::uint32_t writeSpace, const volatile void* writeAddress, std::uint32_t writeData, const volatile void* pollAddress, std::uint32_t reference, std::uint32_t mask);
std::uint32_t APS5_VABI sceAgcCbCondWriteGetSize();

}

#endif
