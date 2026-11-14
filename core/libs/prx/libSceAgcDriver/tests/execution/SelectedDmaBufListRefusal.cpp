#include <cstdio>
#ifndef __linux__
int main() { std::puts("skipped, dma-buf run-list imports are Linux only"); return 77; }
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
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace AgcDriver {
struct VulkanDeviceTestAccess { static Graphics::Context ContextOf(const VulkanDevice& d) { return d.graphicsContext(); } };
}
namespace {
constexpr std::size_t Chunk = std::size_t{64} << 20u;
std::uint8_t* Mapping = nullptr;
int Backing = -1;
PFN_vkGetDeviceProcAddr OriginalProc = nullptr;
std::atomic<unsigned> HostQueries{0};
void Require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
bool Runs(std::uintptr_t address, std::size_t bytes, int* file, GuestArena::SharedBackingRun* runs, std::size_t capacity, std::size_t* count) {
 const auto begin = reinterpret_cast<std::uintptr_t>(Mapping);
 if (address < begin || bytes > 2 * Chunk || address - begin > 2 * Chunk - bytes) return false;
 std::size_t offset = address - begin, done = 0, used = 0;
 while (done < bytes) {
  const auto pos = offset + done, segment = pos / Chunk, within = pos % Chunk, take = std::min(bytes - done, Chunk - within);
  if (used >= capacity) return false;
  runs[used++] = {segment * 2 * Chunk + within, take}; done += take;
 }
 *file = dup(Backing); *count = used; return *file >= 0;
}
PFN_vkVoidFunction Resolve(VkDevice d, const char* name) {
 if (std::strcmp(name, "vkGetMemoryHostPointerPropertiesEXT") == 0) ++HostQueries;
 return OriginalProc(d, name);
}
}
int main(int argc, char** argv) {
 if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--access") != 0)) return 1;
 try {
  auto device = OpenVulkanTestDevice(); if (!device) return VulkanTestSkipped;
  auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
  if (!AgcDriver::Graphics::DmaBufImportSelected(context)) { std::puts("skipped, the device imports no guest memory through dma-buf"); return VulkanTestSkipped; }
  std::uint64_t limit = 0; std::ifstream("/sys/module/udmabuf/parameters/size_limit_mb") >> limit;
  if (limit == 0 || limit >= 128) { std::printf("skipped, the device imports no dma-buf run list this test can refuse: udmabuf size_limit_mb is %llu, the test needs 1 to 127\n", static_cast<unsigned long long>(limit)); return VulkanTestSkipped; }
  Backing = memfd_create("selected-list-refusal", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  Require(Backing >= 0 && ftruncate(Backing, 3 * Chunk) == 0 && fcntl(Backing, F_ADD_SEALS, F_SEAL_SHRINK) == 0, "cannot create scattered backing");
  constexpr std::size_t Align = 65536;
  void* reserved = mmap(nullptr, 2 * Chunk + Align, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  Require(reserved != MAP_FAILED, "cannot reserve scattered range");
  const auto raw = reinterpret_cast<std::uintptr_t>(reserved), aligned = (raw + Align - 1) & ~(Align - 1);
  if (aligned != raw) munmap(reserved, aligned - raw);
  if (aligned + 2 * Chunk != raw + 2 * Chunk + Align) munmap(reinterpret_cast<void*>(aligned + 2 * Chunk), raw + Align - aligned);
  Mapping = reinterpret_cast<std::uint8_t*>(aligned);
  for (std::size_t i = 0; i < 2; ++i) Require(mmap(Mapping + i * Chunk, Chunk, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, Backing, i * 2 * Chunk) != MAP_FAILED, "cannot map scattered segment");
  GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(Mapping, 2 * Chunk);
  GuestAllocations::Mutation().Add(Mapping, 2 * Chunk, true, true, true);
  GuestArena::GuestArenaSetSharedBackingRuns_nid_postfix(Runs);
  AgcDriver::Graphics::SetHostImportRangeWindow(0); context.sparseBinding = false; OriginalProc = context.deviceProc; context.deviceProc = Resolve;
  const bool color = argc == 1; bool refused = false; const AgcDriver::Graphics::HostImport* imported = nullptr;
  try {
   std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
   imported = AgcDriver::Graphics::HostImportFor(context, aligned, 2 * Chunk, color ? AgcDriver::Graphics::ImportRequester::ColorTarget : AgcDriver::Graphics::ImportRequester::Access);
  } catch (const std::exception& error) { std::cerr << "selected list refusal propagated unexpectedly: " << error.what() << '\n'; refused = true; }
  const bool watched = AgcDriver::GuestMemory::Watched(aligned, 2 * Chunk); const auto queries = HostQueries.load();
  { std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex()); AgcDriver::Graphics::ClearHostImports(context.device); }
  GuestAllocations::Mutation().Remove(Mapping); GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(Mapping, 2 * Chunk); munmap(Mapping, 2 * Chunk); close(Backing); GuestArena::GuestArenaSetSharedBackingRuns_nid_postfix(nullptr);
  Require(!refused, "UDMABUF_CREATE_LIST refusal must follow the pre-import fallback policy");
  if (color) Require(imported == nullptr && queries == 0 && watched, "scattered color target lost its watched fallback guard");
  else Require(queries != 0, "scattered non-color import did not attempt host-pointer fallback");
  std::cout << "selected list refusal handled: " << (color ? "color guard" : "host fallback") << ", host queries=" << queries << '\n'; return 0;
 } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

#endif
