#include <cstdint>
#include <cstddef>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Module/EhFrame.hpp"
#include "prx/libkernel/Module/ModuleArgs.hpp"
#ifdef _WIN32
#include <mutex>
#include <unordered_map>
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#else
#include <string_view>
#include "prx/libc/include/specifics/linux/HostMapping.hpp"
#endif

#ifdef _WIN32
namespace {
struct UnwindImage {
  DWORD size = 0;
  bool hasTables = false;
  EhFrame::Tables tables;
  std::string name;
};

std::mutex unwindImagesMutex;
std::unordered_map<std::uintptr_t, UnwindImage>& UnwindImages() {
  static auto& images = *new std::unordered_map<std::uintptr_t, UnwindImage>();
  return images;
}

void WriteUnwindInfo(const UnwindImage& image, const std::uint8_t* base, ModuleInfoForUnwind* info) {
  if (image.hasTables) {
    info->eh_frame_hdr_addr = image.tables.header;
    info->eh_frame_addr = image.tables.frames;
    info->eh_frame_size = image.tables.framesSize;
    info->seg0_addr = reinterpret_cast<std::uint64_t>(base);
    info->seg0_size = image.size;
  }
  std::strncpy(info->name, image.name.c_str(), sizeof(info->name) - 1);
  info->name[sizeof(info->name) - 1] = '\0';
}

void FillUnwindImage(void* allocationBase, ModuleInfoForUnwind* info) {
  const auto* base = static_cast<const std::uint8_t*>(allocationBase);
  const auto key = reinterpret_cast<std::uintptr_t>(base);
  {
    std::lock_guard lock(unwindImagesMutex);
    const auto found = UnwindImages().find(key);
    if (found != UnwindImages().end()) {
      WriteUnwindInfo(found->second, base, info);
      return;
    }
  }
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) throw std::runtime_error("sceKernelGetModuleInfoForUnwind: image without a DOS header");
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) throw std::runtime_error("sceKernelGetModuleInfoForUnwind: image without an NT header");
  UnwindImage fresh;
  fresh.size = nt->OptionalHeader.SizeOfImage;
  fresh.hasTables = EhFrame::ReadEhmeta(key, *nt, "sceKernelGetModuleInfoForUnwind", fresh.tables);
  char path[4096] = {};
  const DWORD length = GetMappedFileNameA(GetCurrentProcess(), allocationBase, path, sizeof(path) - 1);
  fresh.name.assign(path, length);
  std::lock_guard lock(unwindImagesMutex);
  const auto& image = UnwindImages().insert_or_assign(key, std::move(fresh)).first->second;
  WriteUnwindInfo(image, base, info);
}

bool FillKnownUnwindImage(std::uint64_t address, ModuleInfoForUnwind* info) {
  PVOID allocationBase = nullptr;
  if (RtlPcToFileHeader(reinterpret_cast<PVOID>(address), &allocationBase) == nullptr || allocationBase == nullptr) return false;
  const auto* base = static_cast<const std::uint8_t*>(allocationBase);
  std::lock_guard lock(unwindImagesMutex);
  const auto found = UnwindImages().find(reinterpret_cast<std::uintptr_t>(base));
  if (found == UnwindImages().end() || !found->second.hasTables) return false;
  info->st_size = sizeof(ModuleInfoForUnwind);
  WriteUnwindInfo(found->second, base, info);
  return true;
}
}

extern "C" void ForgetUnloadedUnwindImages_nid_no_patch() {
  std::vector<std::uintptr_t> bases;
  {
    std::lock_guard lock(unwindImagesMutex);
    for (const auto& image : UnwindImages()) bases.push_back(image.first);
  }
  std::vector<std::uintptr_t> unloaded;
  for (const auto base : bases) {
    HMODULE loaded = nullptr;
    const bool present = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(base), &loaded) != 0 &&
                         reinterpret_cast<std::uintptr_t>(loaded) == base;
    if (!present) unloaded.push_back(base);
  }
  if (unloaded.empty()) return;
  std::lock_guard lock(unwindImagesMutex);
  for (const auto base : unloaded) UnwindImages().erase(base);
}
#elif !defined(__APPLE__)
extern "C" int APS5_VABI sceKernelGetModuleInfoFromAddr(std::uint64_t address, int flags, ModuleInfoEx* info);

namespace {
struct ImageSearch {
  std::uint64_t address;
  bool relinked;
};

int FindRelinkedImage(dl_phdr_info* image, std::size_t, void* data) {
  auto& search = *static_cast<ImageSearch*>(data);
  bool contains = false;
  for (std::uint16_t index = 0; index < image->dlpi_phnum; ++index) {
    const auto& header = image->dlpi_phdr[index];
    const auto start = image->dlpi_addr + header.p_vaddr;
    if (header.p_type == PT_LOAD && search.address >= start && search.address - start < header.p_memsz) contains = true;
  }
  if (!contains) return 0;
  const std::string_view name = image->dlpi_name != nullptr ? image->dlpi_name : "";
  search.relinked = name.empty() || name.ends_with(".guest.prx");
  return 1;
}

bool IsRelinkedImage(std::uint64_t address) {
  ImageSearch search{address, false};
  dl_iterate_phdr(FindRelinkedImage, &search);
  return search.relinked;
}

void FillGuestUnwindInfo(std::uint64_t address, ModuleInfoForUnwind* info) {
  ModuleInfoEx module{};
  module.st_size = sizeof(ModuleInfoEx);
  if (sceKernelGetModuleInfoFromAddr(address, 2, &module) != 0)
    throw std::runtime_error("sceKernelGetModuleInfoForUnwind: failed to query guest module information");
  info->eh_frame_hdr_addr = module.eh_frame_hdr_addr;
  info->eh_frame_addr = module.eh_frame_addr;
  info->eh_frame_size = module.eh_frame_size;
}
}
#endif

extern "C" {
void* APS5_VABI dlopen_nid_postfix(const char* path, int flags);
void* APS5_VABI dlsym_nid_postfix(void* handle, const char* name);
int APS5_VABI dlclose_nid_postfix(void* handle);
const char* APS5_VABI sceKernelGetFsSandboxRandomWord();
}

namespace {
constexpr int kRtldNow = 2;

std::optional<std::string> SystemLibraryPath(std::string_view path) {
    const std::string sandbox = std::string("/") + sceKernelGetFsSandboxRandomWord() + "/common/lib/";
    for (const std::string_view prefix : {std::string_view("/system/common/lib/"), std::string_view(sandbox)}) {
        if (!path.starts_with(prefix)) continue;
        const auto name = path.substr(prefix.size());
        if (name.size() <= 5 || !name.ends_with(".sprx") || name.find('/') != std::string_view::npos) return std::nullopt;
        return "libs/" + std::string(name.substr(0, name.size() - 5)) + ".prx";
    }
    return std::nullopt;
}
}

extern "C" {

int APS5_VABI sceKernelDlsym(KernelModule handle, const char* symbol, void** addr) {
 if (!symbol || !addr) return SCE_KERNEL_ERROR_EFAULT;
 void* module = handle == 0 ? dlopen_nid_postfix(nullptr, kRtldNow) : reinterpret_cast<void*>(static_cast<intptr_t>(handle));
 void* found = dlsym_nid_postfix(module, symbol);
 if (handle == 0 && module) dlclose_nid_postfix(module);
 if (!found) return SCE_KERNEL_ERROR_ESRCH;
 *addr = found;
 return 0;
}

int APS5_VABI sceKernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
  (void)flags;
  if (!info) return SCE_KERNEL_ERROR_EFAULT;
#ifdef _WIN32
  if (FillKnownUnwindImage(addr, info)) return 0;
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) return SCE_KERNEL_ERROR_ESRCH;
  info->st_size = sizeof(ModuleInfoForUnwind);
  info->eh_frame_hdr_addr = 0;
  info->eh_frame_addr = 0;
  info->eh_frame_size = 0;
  info->seg0_addr = reinterpret_cast<std::uint64_t>(mbi.BaseAddress);
  info->seg0_size = mbi.RegionSize;
  if (mbi.Type == MEM_IMAGE) {
    FillUnwindImage(mbi.AllocationBase, info);
    return 0;
  }
  char path[4096] = {};
  const DWORD len = GetMappedFileNameA(GetCurrentProcess(), mbi.BaseAddress, path, sizeof(path) - 1);
  path[len] = '\0';
  std::strncpy(info->name, path, sizeof(info->name) - 1);
  info->name[sizeof(info->name) - 1] = '\0';
  return 0;
#elif defined(__APPLE__)
  struct Search {
    std::uint64_t address;
    ModuleInfoForUnwind* info;
    bool found;
  } search {addr, info, false};
  dl_iterate_phdr([](dl_phdr_info* image, std::size_t, void* data) {
    auto& search = *static_cast<Search*>(data);
    const Elf64_Phdr* first = nullptr;
    const Elf64_Phdr* frames = nullptr;
    bool contains = false;
    for (std::uint16_t index = 0; index < image->dlpi_phnum; ++index) {
      const auto& header = image->dlpi_phdr[index];
      const auto start = image->dlpi_addr + header.p_vaddr;
      if (header.p_type == PT_LOAD && first == nullptr) first = &header;
      if (header.p_type == PT_LOAD && search.address >= start && search.address - start < header.p_memsz) contains = true;
      if (header.p_type == PT_GNU_EH_FRAME) frames = &header;
    }
    if (!contains) return 0;
    auto* info = search.info;
    info->st_size = sizeof(ModuleInfoForUnwind);
    std::strncpy(info->name, image->dlpi_name != nullptr ? image->dlpi_name : "", sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    info->eh_frame_hdr_addr = frames != nullptr ? image->dlpi_addr + frames->p_vaddr : 0;
    info->eh_frame_addr = 0;
    info->eh_frame_size = 0;
    info->seg0_addr = image->dlpi_addr + first->p_vaddr;
    info->seg0_size = first->p_memsz;
    search.found = true;
    return 1;
  }, &search);
  if (search.found) return 0;
  Dl_info image {};
  if (!dladdr(reinterpret_cast<void*>(addr), &image) || image.dli_fbase == nullptr) return SCE_KERNEL_ERROR_ESRCH;
  unsigned long textSize = 0;
  getsegmentdata(static_cast<const mach_header_64*>(image.dli_fbase), "__TEXT", &textSize);
  info->st_size = sizeof(ModuleInfoForUnwind);
  std::strncpy(info->name, image.dli_fname != nullptr ? image.dli_fname : "", sizeof(info->name) - 1);
  info->name[sizeof(info->name) - 1] = '\0';
  info->eh_frame_hdr_addr = 0;
  info->eh_frame_addr = 0;
  info->eh_frame_size = 0;
  info->seg0_addr = reinterpret_cast<std::uint64_t>(image.dli_fbase);
  info->seg0_size = textSize;
  return 0;
#else
  const auto mapping = HostMapping::Find(addr, "sceKernelGetModuleInfoForUnwind");
  if (!mapping) return SCE_KERNEL_ERROR_ESRCH;
  info->st_size = sizeof(ModuleInfoForUnwind);
  std::strncpy(info->name, mapping->path.c_str(), sizeof(info->name) - 1);
  info->name[sizeof(info->name) - 1] = '\0';
  info->eh_frame_hdr_addr = 0;
  info->eh_frame_addr = 0;
  info->eh_frame_size = 0;
  info->seg0_addr = mapping->start;
  info->seg0_size = mapping->end - mapping->start;
  if (IsRelinkedImage(addr)) FillGuestUnwindInfo(addr, info);
  return 0;
#endif
}

namespace {

thread_local PendingModuleArgs pendingModuleArgs;
thread_local int pendingModuleInitResult = 0;

}

extern "C" {

const void* __aps5_get_pending_module_args_nid_no_patch() {
    return &pendingModuleArgs;
}

void __aps5_set_module_init_result_nid_no_patch(int result) {
    pendingModuleInitResult = result;
}

}

KernelModule APS5_VABI sceKernelLoadStartModule(const char* module_file_name, size_t args, const void* argp, uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
 (void)flags;
 (void)opt;
 if (res) *res = 0;
 if (!module_file_name) return static_cast<KernelModule>(SCE_KERNEL_ERROR_EFAULT);
 pendingModuleArgs = {args, argp};
 pendingModuleInitResult = 0;
 const auto systemLibrary = SystemLibraryPath(module_file_name);
 void* handle = dlopen_nid_postfix(systemLibrary ? systemLibrary->c_str() : module_file_name, kRtldNow);
 const int started = pendingModuleInitResult;
 pendingModuleArgs = {};
 pendingModuleInitResult = 0;
 if (res) *res = started;
 if (!handle) return static_cast<KernelModule>(SCE_KERNEL_ERROR_ENOENT);
 return static_cast<KernelModule>(reinterpret_cast<intptr_t>(handle));
}

int APS5_VABI sceKernelStopUnloadModule(KernelModule handle, size_t args, const void* argp, uint32_t flags, const KernelUnloadModuleOpt* opt, int* res) {
 (void)args;
 (void)argp;
 (void)flags;
 (void)opt;
 if (res) *res = 0;
 return dlclose_nid_postfix(reinterpret_cast<void*>(static_cast<intptr_t>(handle))) == 0 ? 0 : SCE_KERNEL_ERROR_ESRCH;
}

}

extern "C" {

int APS5_VABI __elf_phdr_match_addr_nid_postfix(dl_phdr_info* phdrInfo, void* addr) {
    if (phdrInfo == nullptr) throw std::invalid_argument("__elf_phdr_match_addr: phdr_info is null");
    const auto address = reinterpret_cast<std::uintptr_t>(addr);

    const auto* module = reinterpret_cast<const ModuleInfoEx*>(phdrInfo);
    if (module->st_size == sizeof(ModuleInfoEx)) {
        for (std::uint32_t i = 0; i < module->segment_count && i < std::size(module->segments); ++i) {
            const auto& segment = module->segments[i];
            if ((segment.prot & 4) == 0 || segment.size < sizeof(addr)) continue;
            if (segment.address <= address && address - segment.address <= segment.size - sizeof(addr)) return 1;
        }
        return 0;
    }

    for (std::uint16_t i = 0; i < phdrInfo->dlpi_phnum; ++i) {
        const Elf64_Phdr& header = phdrInfo->dlpi_phdr[i];
        if (header.p_type != PT_LOAD || (header.p_flags & PF_X) == 0) continue;
        const std::uintptr_t begin = phdrInfo->dlpi_addr + header.p_vaddr;
        if (begin <= address && address + sizeof(addr) < begin + header.p_memsz) return 1;
    }
    return 0;
}

}
