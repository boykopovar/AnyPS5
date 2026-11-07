#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_JUMPBUFFER_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_JUMPBUFFER_HPP

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace JumpBuffer {

using Sigprocmask = int (APS5_VABI*)(int how, const void* set, void* previousSet);
using StackRestore = void (*)(std::uintptr_t stack, std::uint32_t context);

struct Context {
    std::uintptr_t stack;
    std::uint32_t identity;
    std::uint32_t signature;
};

static_assert(sizeof(Context) == 16);

extern "C" {

void GuestSigprocmaskRegister_nid_no_patch(Sigprocmask sigprocmask);
void GuestIoJumpRegister_nid_no_patch(StackRestore restore);
std::uint32_t GuestContextCurrent_nid_no_patch();
void GuestContextSet_nid_no_patch(std::uint32_t context);
Context GuestContextCapture_nid_no_patch(std::uintptr_t stack);
Context GuestContextCreate_nid_no_patch(std::uintptr_t stack);
std::uint32_t GuestContextIdentify_nid_no_patch(const Context& context, std::uintptr_t stack);
void APS5_VABI JumpBufferSaveContext_nid_no_patch(unsigned char* buffer);
void APS5_VABI JumpBufferSaveMask_nid_no_patch(unsigned char* buffer);
void APS5_VABI JumpBufferRestoreMask_nid_no_patch(const unsigned char* buffer);
void APS5_VABI GuestIoStackRestore_nid_no_patch(std::uintptr_t stack, std::uint32_t context);

}

}

#endif
