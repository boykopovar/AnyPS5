#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTWRITEWATCH_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTWRITEWATCH_HPP

#include <cstddef>
#include <cstdint>

namespace GuestWriteWatch {

extern "C" {

bool GuestWriteWatchAvailable_nid_postfix();
void GuestWriteWatchRegister_nid_postfix(const void* pointer, std::size_t bytes);
bool GuestWriteWatchUnregister_nid_postfix(const void* pointer, std::size_t bytes);
bool GuestWriteWatchCovers_nid_postfix(std::uintptr_t address, std::size_t bytes);
bool GuestWriteWatchCollect_nid_postfix(std::uintptr_t address, std::size_t bytes, void (*written)(void* context, std::uintptr_t begin, std::uintptr_t end), void* context);
void GuestWriteWatchBeginHostWrite_nid_postfix(const void* pointer, std::size_t bytes);
void GuestWriteWatchEndHostWrite_nid_postfix(const void* pointer, std::size_t bytes);
void GuestWriteWatchProtectionChanged_nid_postfix(const void* pointer, std::size_t bytes, int protection);
int GuestWriteWatchProtection_nid_postfix(std::uintptr_t address, std::uintptr_t limit, std::uintptr_t* runEnd);
bool GuestWriteWatchCollectArmed_nid_postfix(std::uintptr_t address, std::size_t bytes, void (*written)(void* context, std::uintptr_t begin, std::uintptr_t end), void* context);
bool GuestWriteWatchCollectFresh_nid_postfix(std::uintptr_t address, std::size_t bytes, void (*written)(void* context, std::uintptr_t begin, std::uintptr_t end), void* context);

}

}

#endif
