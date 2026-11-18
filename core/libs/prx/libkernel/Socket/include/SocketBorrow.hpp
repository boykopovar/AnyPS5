#pragma once
#include <cstdint>
#include <memory>

extern "C" int KernelBorrowSocketNative_nid_no_patch(std::uintptr_t native, int family, int type, bool nonblocking, std::shared_ptr<void>* owner);
extern "C" bool KernelReleaseBorrowedSocket_nid_no_patch(int descriptor, const std::shared_ptr<void>* owner);
extern "C" void KernelNoteSocketConnect_nid_no_patch(int descriptor, const std::shared_ptr<void>* owner);
