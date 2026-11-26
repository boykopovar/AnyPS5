#include <cstdio>
#ifndef __linux__
int main() { std::puts("skipped, dma-buf imports are Linux only"); return 77; }
#else
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace AgcDriver {
struct VulkanDeviceTestAccess { static Graphics::Context ContextOf(const VulkanDevice& d) { return d.graphicsContext(); } };
}
namespace {
constexpr std::size_t Bytes = std::size_t{64} << 20u;
std::uint8_t* Mapping = nullptr;
int Backing = -1;
PFN_vkGetDeviceProcAddr OriginalProc = nullptr;
PFN_vkCreateBuffer OriginalCreateBuffer = nullptr;
PFN_vkDestroyBuffer OriginalDestroyBuffer = nullptr;
PFN_vkBindBufferMemory OriginalBindBufferMemory = nullptr;
std::atomic<bool> FailBinds{false};
std::atomic<int> LiveBuffers{0};
std::atomic<unsigned> FailedBinds{0};
std::atomic<unsigned> HostQueries{0};
void Require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
bool InRange(std::uintptr_t address, std::size_t bytes) {
 const auto begin = reinterpret_cast<std::uintptr_t>(Mapping);
 return address >= begin && bytes != 0 && bytes <= Bytes && address - begin <= Bytes - bytes;
}
bool Runs(std::uintptr_t address, std::size_t bytes, int* file, GuestArena::SharedBackingRun* runs, std::size_t capacity, std::size_t* count) {
 if (!InRange(address, bytes) || capacity == 0) return false;
 runs[0] = {address - reinterpret_cast<std::uintptr_t>(Mapping), bytes};
 *file = dup(Backing); *count = 1; return *file >= 0;
}
bool HostFallback(std::uintptr_t address, std::size_t bytes) { return InRange(address, bytes); }
VKAPI_ATTR VkResult VKAPI_CALL CreateBuffer(VkDevice d, const VkBufferCreateInfo* info, const VkAllocationCallbacks* allocator, VkBuffer* buffer) {
 const auto result = OriginalCreateBuffer(d, info, allocator, buffer);
 if (result == VK_SUCCESS) ++LiveBuffers;
 return result;
}
VKAPI_ATTR void VKAPI_CALL DestroyBuffer(VkDevice d, VkBuffer buffer, const VkAllocationCallbacks* allocator) {
 if (buffer != VK_NULL_HANDLE) --LiveBuffers;
 OriginalDestroyBuffer(d, buffer, allocator);
}
VKAPI_ATTR VkResult VKAPI_CALL BindBufferMemory(VkDevice d, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset) {
 if (FailBinds.load()) { ++FailedBinds; return VK_ERROR_OUT_OF_DEVICE_MEMORY; }
 return OriginalBindBufferMemory(d, buffer, memory, offset);
}
PFN_vkVoidFunction Resolve(VkDevice d, const char* name) {
 if (std::strcmp(name, "vkGetMemoryHostPointerPropertiesEXT") == 0) { ++HostQueries; FailBinds = false; }
 if (std::strcmp(name, "vkCreateBuffer") == 0) { OriginalCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(OriginalProc(d, name)); return reinterpret_cast<PFN_vkVoidFunction>(CreateBuffer); }
 if (std::strcmp(name, "vkDestroyBuffer") == 0) { OriginalDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(OriginalProc(d, name)); return reinterpret_cast<PFN_vkVoidFunction>(DestroyBuffer); }
 if (std::strcmp(name, "vkBindBufferMemory") == 0) { OriginalBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(OriginalProc(d, name)); return reinterpret_cast<PFN_vkVoidFunction>(BindBufferMemory); }
 return OriginalProc(d, name);
}
}
int main() {
 try {
  auto device = OpenVulkanTestDevice(); if (!device) return VulkanTestSkipped;
  auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
  if (!AgcDriver::Graphics::DmaBufImportSelected(context)) { std::puts("skipped, the device does not select watched dma-buf imports"); return VulkanTestSkipped; }
  Backing = memfd_create("flexible-bind-failure", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  Require(Backing >= 0 && ftruncate(Backing, Bytes) == 0 && fcntl(Backing, F_ADD_SEALS, F_SEAL_SHRINK) == 0, "cannot create the backing file");
  constexpr std::size_t Align = std::size_t{2} << 20u;
  void* reserved = mmap(nullptr, Bytes + Align, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  Require(reserved != MAP_FAILED, "cannot reserve the range");
  const auto raw = reinterpret_cast<std::uintptr_t>(reserved), aligned = (raw + Align - 1) & ~(Align - 1);
  if (aligned != raw) munmap(reserved, aligned - raw);
  if (aligned + Bytes != raw + Bytes + Align) munmap(reinterpret_cast<void*>(aligned + Bytes), raw + Align - aligned);
  Mapping = reinterpret_cast<std::uint8_t*>(aligned);
  Require(mmap(Mapping, Bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != MAP_FAILED, "cannot map the guest range");
  Mapping[5] = 0x5d;
  GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(Mapping, Bytes);
  GuestAllocations::Mutation().Add(Mapping, Bytes, true, true, true);
  GuestArena::GuestArenaSetSharedBackingRuns_nid_postfix(Runs);
  GuestArena::GuestArenaSetSharedBackingHostFallback_nid_no_patch(HostFallback);
  AgcDriver::Graphics::SetHostImportRangeWindow(0); context.sparseBinding = false; OriginalProc = context.deviceProc; context.deviceProc = Resolve;
  const int before = LiveBuffers.load();
  FailBinds = true;
  const AgcDriver::Graphics::HostImport* imported = nullptr;
  bool threw = false;
  try {
   std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
   imported = AgcDriver::Graphics::HostImportFor(context, aligned, Bytes, AgcDriver::Graphics::ImportRequester::Access);
  } catch (const std::exception& error) { std::cerr << "the failed dma-buf import was not retried: " << error.what() << '\n'; threw = true; }
  FailBinds = false;
  const bool hostImport = imported != nullptr && imported->buffer != VK_NULL_HANDLE && imported->memory != VK_NULL_HANDLE && !imported->dmaBuf && !imported->chunk;
  const int during = LiveBuffers.load();
  { std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex()); AgcDriver::Graphics::ClearHostImports(context.device); }
  GuestAllocations::Mutation().Remove(Mapping); GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(Mapping, Bytes); munmap(Mapping, Bytes); close(Backing);
  GuestArena::GuestArenaSetSharedBackingRuns_nid_postfix(nullptr); GuestArena::GuestArenaSetSharedBackingHostFallback_nid_no_patch(nullptr);
  Require(!threw, "a dma-buf import that failed after creating its buffer must fall back to a host-pointer import for flexible memory");
  Require(FailedBinds.load() != 0, "the dma-buf import never reached vkBindBufferMemory, so the partial failure was not exercised");
  Require(HostQueries.load() != 0, "the host-pointer import was not attempted");
  Require(hostImport, "the import is not a complete host-pointer import");
  Require(during == before + 1, "the failed dma-buf import left a buffer alive beside the host-pointer import");
  std::cout << "flexible dma-buf bind failure handled: " << FailedBinds.load() << " failed binds, host-pointer import, buffers " << before << " -> " << during << '\n';
  return 0;
 } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

#endif
