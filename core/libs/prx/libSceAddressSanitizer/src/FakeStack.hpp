#pragma once

#include <cstddef>
#include <cstdint>

namespace AsanRuntime::FakeStack {

void* Malloc_nid_no_patch(std::size_t classId, std::size_t size, bool always, std::uintptr_t realStack);
void Free_nid_no_patch(std::uintptr_t pointer, std::size_t classId, std::size_t size);
void* Current_nid_no_patch();
void* AddrIsIn_nid_no_patch(void* fakeStack, void* address, void** begin, void** end);
void HandleNoReturn_nid_no_patch();
void StartSwitchFiber_nid_no_patch(void** save, const void* bottom, std::size_t size);
void FinishSwitchFiber_nid_no_patch(void* save, const void** bottomOld, std::size_t* sizeOld);

}  // namespace AsanRuntime::FakeStack
