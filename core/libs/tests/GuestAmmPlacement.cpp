#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAmpr/include/Api.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#else
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#endif

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static bool HostReserved(std::uintptr_t start, std::size_t bytes) {
    const std::uintptr_t end = start + bytes;
    std::uintptr_t cursor = start;
#ifdef _WIN32
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) != sizeof(info) || info.State == MEM_FREE) return false;
        cursor = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    }
#elif defined(__APPLE__)
    while (cursor < end) {
        mach_vm_address_t address = cursor;
        mach_vm_size_t size = 0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        if (mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS || address > cursor) return false;
        cursor = static_cast<std::uintptr_t>(address + size);
    }
#else
    std::map<std::uintptr_t, std::uintptr_t> mappings;
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        std::istringstream fields(line);
        std::uintptr_t begin = 0;
        std::uintptr_t last = 0;
        char dash = 0;
        if (fields >> std::hex >> begin >> dash >> last && dash == '-') mappings.emplace(begin, last);
    }
    for (const auto& [begin, last] : mappings) {
        if (last <= cursor) continue;
        if (begin > cursor) return false;
        cursor = std::max(cursor, last);
        if (cursor >= end) break;
    }
#endif
    return cursor >= end;
}

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool FixedMapThrows(void* address, std::size_t bytes, bool reserve) {
    return Throws([&] {
        void* target = address;
        if (reserve) sceKernelReserveVirtualRange(&target, bytes, 0x10, 0);
        else sceKernelMapFlexibleMemory(&target, bytes, 3, 0x10);
    });
}

template <typename TCall>
static bool NotImplemented(TCall call) {
    const auto before = NotImplementedCalls_nid_no_patch();
    try {
        call();
    } catch (const std::runtime_error&) {
        return NotImplementedCalls_nid_no_patch() == before + 1;
    }
    return false;
}

template <typename TCall>
static bool RejectsArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template <typename TCall>
static bool RejectsRange(TCall call) {
    const auto before = NotImplementedCalls_nid_no_patch();
    try {
        call();
    } catch (const std::runtime_error& error) {
        return NotImplementedCalls_nid_no_patch() == before && std::string(error.what()) == "invalid guest allocation range";
    }
    return false;
}

static void* const MapFailed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
constexpr int PrivateAnonymous = 0x1002;
constexpr int Fixed = 0x10;
constexpr int NoOverwrite = 0x80;
constexpr int Shared = 0x1;
constexpr int Private = 0x2;
constexpr int KernelInvalid = static_cast<int>(0x80020016);
constexpr int KernelNoEntry = static_cast<int>(0x80020002);
constexpr int KernelFault = static_cast<int>(0x8002000E);

static bool MmapFails(void* address, std::size_t bytes, int flags, int descriptor, int error) {
    *__error_nid_postfix() = 0;
    return mmap_nid_postfix(address, bytes, 3, flags, descriptor, 0) == MapFailed && *__error_nid_postfix() == error;
}

static void CheckTitleMapsIntoWindow(std::uint64_t start, std::uint64_t page) {
    void* const inside = reinterpret_cast<void*>(start + 16 * page);
    void* const misaligned = reinterpret_cast<void*>(start + 16 * page + 1);
    Require(Throws([&] { mmap_nid_postfix(inside, page, 3, PrivateAnonymous | Fixed, -1, 0); }), "a fixed anonymous mmap inside the AMM window did not throw");
    Require(Throws([&] {
        void* out = nullptr;
        sceKernelMmap(inside, page, 3, PrivateAnonymous | Fixed, -1, 0, &out);
    }), "a fixed anonymous sceKernelMmap inside the AMM window did not throw");
    Require(MmapFails(misaligned, page, PrivateAnonymous | Fixed, -1, 22), "a misaligned fixed anonymous mmap inside the AMM window did not fail with EINVAL");

    const auto path = std::filesystem::path("anyps5-amm-window-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
    {
        std::ofstream stream(path, std::ios::binary);
        const std::vector<char> seed(2 * page, 0x5a);
        stream.write(seed.data(), static_cast<std::streamsize>(seed.size()));
    }
    const int fd = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(fd >= 0, "the file for the file-backed AMM window maps did not open");
    const bool sharedThrows = Throws([&] { mmap_nid_postfix(inside, page, 3, Shared | Fixed, fd, 0); });
    const bool privateThrows = Throws([&] { mmap_nid_postfix(inside, page, 3, Private | Fixed, fd, 0); });
    const bool noOverwriteThrows = Throws([&] { mmap_nid_postfix(inside, page, 3, Shared | Fixed | NoOverwrite, fd, 0); });
    const bool kernelThrows = Throws([&] {
        void* out = nullptr;
        sceKernelMmap(inside, page, 3, Shared | Fixed, fd, 0, &out);
    });
    const bool misalignedFails = MmapFails(misaligned, page, Shared | Fixed, fd, 22);
    sceKernelClose(fd);
    std::error_code removed;
    std::filesystem::remove(path, removed);
    Require(sharedThrows, "a fixed shared file mmap inside the AMM window did not throw");
    Require(privateThrows, "a fixed private file mmap inside the AMM window did not throw");
    Require(noOverwriteThrows, "a fixed no-overwrite file mmap inside the AMM window did not throw");
    Require(kernelThrows, "a fixed shared file sceKernelMmap inside the AMM window did not throw");
    Require(misalignedFails, "a misaligned fixed file mmap inside the AMM window did not fail with EINVAL");

    std::int64_t physical = -1;
    Require(sceKernelAllocateDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), page, page, 0, &physical) == 0, "direct memory for the AMM window maps was not allocated");
    Require(Throws([&] {
        void* target = inside;
        sceKernelMapDirectMemory(&target, page, 3, Fixed, physical, 0);
    }), "a fixed direct map inside the AMM window did not throw");
    Require(Throws([&] {
        void* target = inside;
        sceKernelMapDirectMemory(&target, page, 3, Fixed | NoOverwrite, physical, 0);
    }), "a fixed no-overwrite direct map inside the AMM window did not throw");
    Require(Throws([&] {
        void* target = inside;
        sceKernelReserveVirtualRange(&target, page, Fixed | NoOverwrite, 0);
    }), "a fixed no-overwrite reserve inside the AMM window did not throw");

    void* wrapped = reinterpret_cast<void*>(std::uintptr_t{0x10000});
    Require(sceKernelReserveVirtualRange(&wrapped, std::numeric_limits<std::size_t>::max() - 0xffff, Fixed, 0) == KernelInvalid, "a fixed reserve whose end wraps did not fail with EINVAL");

    std::uintptr_t arenaBase = 0;
    std::size_t arenaBytes = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
    const auto inArena = [&](const void* address) {
        const auto value = reinterpret_cast<std::uintptr_t>(address);
        return value >= arenaBase && value - arenaBase <= arenaBytes - page;
    };
    void* flexible = inside;
    Require(sceKernelMapFlexibleMemory(&flexible, page, 3, 0) == 0 && inArena(flexible), "a flexible map hinted inside the AMM window was not placed in the guest arena");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(flexible), page) == 0, "the hinted flexible map was not released");
    void* reserved = inside;
    Require(sceKernelReserveVirtualRange(&reserved, page, 0, 0) == 0 && inArena(reserved), "a reserve hinted inside the AMM window was not placed in the guest arena");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(reserved), page) == 0, "the hinted reserve was not released");
    void* direct = inside;
    Require(sceKernelMapDirectMemory(&direct, page, 3, 0, physical, 0) == 0 && inArena(direct), "a direct map hinted inside the AMM window was not placed in the guest arena");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(direct), page) == 0, "the hinted direct map was not released");
    void* anonymous = mmap_nid_postfix(inside, page, 3, PrivateAnonymous, -1, 0);
    Require(anonymous != MapFailed && inArena(anonymous), "an anonymous mmap hinted inside the AMM window was not placed in the guest arena");
    Require(munmap_nid_postfix(anonymous, page) == 0, "the hinted anonymous mmap was not released");
    Require(sceKernelReleaseDirectMemory(physical, page) == 0, "the direct memory for the AMM window maps was not released");
}

static std::filesystem::path WriteSeedFile(const char* name, std::size_t bytes) {
    const auto path = std::filesystem::path(std::string(name) + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
    std::ofstream stream(path, std::ios::binary);
    const std::vector<char> seed(bytes, 0x5a);
    stream.write(seed.data(), static_cast<std::streamsize>(seed.size()));
    return path;
}

static void CheckTitleChangesInWindow(std::uint64_t start, std::uint64_t page) {
    void* const inside = reinterpret_cast<void*>(start + 16 * page);
    const auto address = reinterpret_cast<std::uint64_t>(inside);
    void* const below = reinterpret_cast<void*>(start - page);
    Require(NotImplemented([&] { sceKernelMunmap(address, page); }), "sceKernelMunmap inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { munmap_nid_postfix(inside, page); }), "munmap inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelMunmap(start - page, 2 * page); }), "sceKernelMunmap from the user range into the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelMprotect(inside, page, 1); }), "sceKernelMprotect inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { mprotect_nid_postfix(inside, page, 1); }), "mprotect inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { mprotect_nid_postfix(below, 2 * page, 1); }), "mprotect from the user range into the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelMtypeprotect(inside, page, 0, 1); }), "sceKernelMtypeprotect inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelMemoryPoolCommit(inside, page, 0, 3, 0); }), "sceKernelMemoryPoolCommit inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelMemoryPoolDecommit(inside, page, 0); }), "sceKernelMemoryPoolDecommit inside the AMM window did not call NotImplemented");
    KernelMemoryPoolBatchEntry entry{};
    entry.op = 3;
    entry.protect.addr = inside;
    entry.protect.len = page;
    entry.protect.prot = 1;
    int done = -1;
    Require(NotImplemented([&] { sceKernelMemoryPoolBatch(&entry, 1, &done, 0); }), "a sceKernelMemoryPoolBatch protection inside the AMM window did not call NotImplemented");
    void* const untouched = reinterpret_cast<void*>(std::uintptr_t{0x1234});
    void* out = untouched;
    Require(NotImplemented([&] { sceKernelMemoryPoolReserve(inside, page, 0, Fixed, &out); }), "a fixed sceKernelMemoryPoolReserve inside the AMM window did not call NotImplemented");
    Require(out == untouched, "a fixed sceKernelMemoryPoolReserve inside the AMM window wrote its output");
    void* const readOnly = mmap_nid_postfix(nullptr, page, 1, PrivateAnonymous, -1, 0);
    Require(readOnly != MapFailed, "the read-only output page was not mapped");
    Require(NotImplemented([&] { sceKernelMemoryPoolReserve(inside, page, 0, Fixed, static_cast<void**>(readOnly)); }), "a fixed sceKernelMemoryPoolReserve inside the AMM window with a read-only output did not call NotImplemented");
    Require(sceKernelMemoryPoolReserve(nullptr, page, 0, 0, static_cast<void**>(readOnly)) == KernelFault, "a sceKernelMemoryPoolReserve with a read-only output did not fail with EFAULT");
    Require(munmap_nid_postfix(readOnly, page) == 0, "the read-only output page was not released");
    Require(NotImplemented([&] { sceKernelMlock_nid_postfix(inside, page); }), "sceKernelMlock inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelMunlock_nid_postfix(inside, page); }), "sceKernelMunlock inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { mlock_nid_postfix(below, 2 * page); }), "mlock from the user range into the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { munlock_nid_postfix(inside, page); }), "munlock inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelSetVirtualRangeName(inside, page, "amm"); }), "sceKernelSetVirtualRangeName inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelSetVirtualRangeName(below, 2 * page, "amm"); }), "sceKernelSetVirtualRangeName from the user range into the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelClearVirtualRangeName(inside, page); }), "sceKernelClearVirtualRangeName inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelClearVirtualRangeName(below, 2 * page); }), "sceKernelClearVirtualRangeName from the user range into the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelReleaseFlexibleMemory(inside, page); }), "sceKernelReleaseFlexibleMemory inside the AMM window did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelReleaseFlexibleMemory(below, 2 * page); }), "sceKernelReleaseFlexibleMemory from the user range into the AMM window did not call NotImplemented");
    Require(sceKernelReleaseFlexibleMemory(inside, 0) == 0, "an empty sceKernelReleaseFlexibleMemory inside the AMM window did not succeed");
    Require(sceKernelSetVirtualRangeName(inside, page, nullptr) == KernelFault, "sceKernelSetVirtualRangeName inside the AMM window with a null name did not fail with EFAULT");

    Require(sceKernelMunmap(address, page + 1) == KernelInvalid, "a sceKernelMunmap of a partial page inside the AMM window did not fail with EINVAL");
    Require(RejectsRange([&] { sceKernelMunmap(start - page, std::numeric_limits<std::uint64_t>::max() - (page - 1)); }), "a sceKernelMunmap from the user range whose end wraps did not reject its range as on the base");
    Require(RejectsRange([&] { sceKernelMunmap(start + 16 * page, std::numeric_limits<std::uint64_t>::max() - (page - 1)); }), "a sceKernelMunmap from inside the AMM window whose end wraps did not reject its range as on the base");
    Require(sceKernelMtypeprotect(inside, page, 0, 0x8) == KernelInvalid, "a sceKernelMtypeprotect with an undefined protection did not fail with EINVAL");
    Require(sceKernelMemoryPoolCommit(reinterpret_cast<char*>(inside) + 1, page, 0, 3, 0) == KernelInvalid, "a misaligned sceKernelMemoryPoolCommit did not fail with EINVAL");
    Require(sceKernelMemoryPoolCommit(inside, page, 0, 0x8, 0) == KernelInvalid, "a sceKernelMemoryPoolCommit with an undefined protection did not fail with EINVAL");
    Require(sceKernelMemoryPoolReserve(inside, page, 3 * page, Fixed, &out) == KernelInvalid, "a fixed sceKernelMemoryPoolReserve with a non-power-of-two alignment did not fail with EINVAL");
}

static void CheckArgumentErrorsBeforeWindow(std::uint64_t start, std::uint64_t end, std::uint64_t page) {
    constexpr std::uint64_t huge = 2ull << 20;
    constexpr int AlignedSuper = 0x1000000;
    void* const inside = reinterpret_cast<void*>(start + 16 * page);
    Require(MmapFails(nullptr, end, PrivateAnonymous | Fixed, -1, 22), "a fixed anonymous mmap at null across the AMM window did not fail with EINVAL");

    const auto path = WriteSeedFile("anyps5-amm-window-order-", 2 * page);
    const int readOnly = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    const int readWrite = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    const int directory = sceKernelOpen(".", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    const bool opened = readOnly >= 0 && readWrite >= 0 && directory >= 0;
    const bool nullFails = MmapFails(nullptr, end, Shared | Fixed, readWrite, 22);
    const bool badDescriptorFails = MmapFails(inside, page, Shared | Fixed, 4093, 9);
    const bool accessFails = MmapFails(inside, page, Shared | Fixed, readOnly, 13);
    const bool directoryFails = MmapFails(inside, page, Private | Fixed, directory, 45);
    const bool superpageFails = MmapFails(inside, page, Shared | Fixed | AlignedSuper, readWrite, 22);
    sceKernelClose(directory);
    sceKernelClose(readWrite);
    sceKernelClose(readOnly);
    std::error_code removed;
    std::filesystem::remove(path, removed);
    Require(opened, "the files for the AMM window argument checks did not open");
    Require(nullFails, "a fixed file mmap at null across the AMM window did not fail with EINVAL");
    Require(badDescriptorFails, "a fixed file mmap inside the AMM window with a closed descriptor did not fail with EBADF");
    Require(accessFails, "a fixed writable shared mmap of a read-only descriptor inside the AMM window did not fail with EACCES");
    Require(directoryFails, "a fixed mmap of a directory inside the AMM window did not fail with EOPNOTSUPP");
    Require(superpageFails, "a fixed superpage file mmap inside the AMM window off a 2 MiB boundary did not fail with EINVAL");

    std::int64_t physical = -1;
    Require(sceKernelAllocateDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), page, page, 0, &physical) == 0, "direct memory for the AMM window argument checks was not allocated");
    Require(RejectsArgument([&] {
        void* target = inside;
        sceKernelMapDirectMemory(&target, page, 3, Fixed, physical, huge);
    }), "a fixed direct map inside the AMM window off its alignment was not rejected as an argument error");
    Require(RejectsArgument([&] {
        void* target = inside;
        sceKernelMapDirectMemory(&target, page, 3, Fixed | 0x20, physical, 0);
    }), "a fixed direct map inside the AMM window with an unsupported flag was not rejected as an argument error");
    Require(RejectsArgument([&] {
        void* target = inside;
        sceKernelMapFlexibleMemory(&target, page, 3, Fixed | 0x20);
    }), "a fixed flexible map inside the AMM window with an unsupported flag was not rejected as an argument error");
    Require(sceKernelReleaseDirectMemory(physical, page) == 0, "the direct memory for the AMM window argument checks was not released");
    void* target = inside;
    Require(sceKernelReserveVirtualRange(&target, page, Fixed, huge) == KernelInvalid, "a fixed reserve inside the AMM window off its alignment did not fail with EINVAL");
    target = inside;
    Require(sceKernelReserveVirtualRange(&target, page, Fixed, 3 * page) == KernelInvalid, "a fixed reserve inside the AMM window with a non-power-of-two alignment did not fail with EINVAL");
}

static void CheckAlignedHintBelowWindow(std::uint64_t start, std::uintptr_t arenaBase, std::size_t arenaBytes) {
    constexpr std::uint64_t huge = 2ull << 20;
    std::int64_t physical = -1;
    Require(sceKernelAllocateDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), huge, huge, 0, &physical) == 0, "direct memory for the hint below the AMM window was not allocated");
    void* hinted = reinterpret_cast<void*>(start - 0x4000);
    Require(sceKernelMapDirectMemory(&hinted, huge, 3, 0, physical, huge) == 0, "a 2 MiB-aligned direct map hinted one page below the AMM window failed");
    const auto placed = reinterpret_cast<std::uintptr_t>(hinted);
    Require(placed >= arenaBase && placed - arenaBase <= arenaBytes - huge && placed % huge == 0, "a 2 MiB-aligned direct map hinted one page below the AMM window was not placed in the guest arena");
    Require(sceKernelMunmap(placed, huge) == 0, "the direct map hinted below the AMM window was not released");
    Require(sceKernelReleaseDirectMemory(physical, huge) == 0, "the direct memory for the hint below the AMM window was not released");
}

static void SubmitAmm(void (*record)(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::int64_t), std::uint64_t address, std::uint64_t size, std::int64_t direct) {
    alignas(8) std::array<std::uint8_t, 4096> memory{};
    Apr::CommandBufferObject buffer{};
    std::uint64_t gatherState = 0;
    std::uint64_t scatterState = 0;
    Require(sceAmprCommandBufferConstructor(&buffer) == 0 && sceAmprAprCommandBufferConstructor(&buffer, &gatherState, &scatterState) == 0, "an AMM command buffer was not constructed");
    Require(sceAmprCommandBufferSetBuffer(&buffer, memory.data(), static_cast<std::uint32_t>(memory.size())) == 0 && sceAmprAmmCommandBufferConstructor(&buffer) == 0, "an AMM command buffer was not set up");
    record(&buffer, address, size, direct);
    std::uint32_t id = 0;
    Require(sceAmprAmmSubmitCommandBuffer3(memory.data(), sceAmprCommandBufferGetCurrentOffset(&buffer), 0, &id) == 0 && sceAmprAmmWaitCommandBufferCompletion(id) == 0, "an AMM command buffer did not complete");
}

static void CheckReleaseOfAmmMappedMemory(std::uint64_t start, std::uint64_t page) {
    const std::uint64_t address = start + 64 * page;
    std::int64_t direct = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), page, page, 0, &direct) == 0 && direct >= 0, "direct memory for the AMM map was not allocated");
    SubmitAmm([](Apr::CommandBufferObject* buffer, std::uint64_t at, std::uint64_t size, std::int64_t offset) {
        Require(sceAmprAmmCommandBufferMapDirect(buffer, at, static_cast<std::uint64_t>(offset), size, 0, 3) == 0, "the AMM direct map was not recorded");
    }, address, page, direct);
    *reinterpret_cast<volatile std::uint64_t*>(address) = 0x5a5a;
    Require(NotImplemented([&] { sceKernelReleaseDirectMemory(direct, page); }), "releasing direct memory mapped in the AMM window did not call NotImplemented");
    Require(*reinterpret_cast<volatile std::uint64_t*>(address) == 0x5a5a, "a refused release unmapped the AMM page");
    SubmitAmm([](Apr::CommandBufferObject* buffer, std::uint64_t at, std::uint64_t size, std::int64_t) {
        Require(sceAmprAmmCommandBufferUnmap(buffer, at, size) == 0, "the AMM unmap was not recorded");
    }, address, page, direct);
    Require(sceKernelReleaseDirectMemory(direct, page) == 0, "direct memory unmapped from the AMM window was not released");
}

static void CheckRefusedReleaseKeepsUserView(std::uint64_t start, std::uint64_t page) {
    const std::uint64_t address = start + 128 * page;
    std::int64_t direct = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), page, page, 0, &direct) == 0 && direct >= 0, "direct memory for the user and AMM views was not allocated");
    void* user = nullptr;
    Require(sceKernelMapDirectMemory(&user, page, 3, 0, direct, 0) == 0 && reinterpret_cast<std::uint64_t>(user) < address, "the user view below the AMM window was not mapped");
    auto& userValue = *static_cast<volatile std::uint64_t*>(user);
    auto& ammValue = *reinterpret_cast<volatile std::uint64_t*>(address);
    userValue = 0x6b6b;
    SubmitAmm([](Apr::CommandBufferObject* buffer, std::uint64_t at, std::uint64_t size, std::int64_t offset) {
        Require(sceAmprAmmCommandBufferMapDirect(buffer, at, static_cast<std::uint64_t>(offset), size, 0, 3) == 0, "the AMM view was not recorded");
    }, address, page, direct);
    Require(ammValue == 0x6b6b, "the AMM view does not alias the user view");
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, page) == GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(user), page), "the AMM view and the user view of one direct memory range differ in write watching");
    Require(NotImplemented([&] { sceKernelReleaseDirectMemory(direct, page); }), "releasing direct memory with a user view and an AMM view did not call NotImplemented");
    Require(userValue == 0x6b6b && ammValue == 0x6b6b, "a refused release unmapped a view");
    void* again = nullptr;
    Require(sceKernelMapDirectMemory(&again, page, 3, 0, direct, 0) == 0, "direct memory was not mappable after a refused release");
    Require(*static_cast<volatile std::uint64_t*>(again) == 0x6b6b, "a new view after a refused release does not alias the kept views");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(again), page) == 0, "the view mapped after a refused release was not released");
    SubmitAmm([](Apr::CommandBufferObject* buffer, std::uint64_t at, std::uint64_t size, std::int64_t) {
        Require(sceAmprAmmCommandBufferUnmap(buffer, at, size) == 0, "the AMM unmap was not recorded");
    }, address, page, direct);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(user), page) == 0, "the user view was not released");
    Require(sceKernelReleaseDirectMemory(direct, page) == 0, "direct memory unmapped from both views was not released");
}

static void CheckUnmappedWindowUnwatched(std::uint64_t start) {
    constexpr std::uint64_t bytes = 1ull << 30;
    const std::uint64_t address = start + (64ull << 30);
    SubmitAmm([](Apr::CommandBufferObject* buffer, std::uint64_t at, std::uint64_t size, std::int64_t) {
        Require(sceAmprAmmCommandBufferUnmap(buffer, at, size) == 0, "the large AMM unmap was not recorded");
    }, address, bytes, 0);
    Require(!GuestWriteWatch::GuestWriteWatchOverlaps_nid_postfix(address, bytes), "a large AMM unmap registered its inaccessible range with write watch");
}

static bool HostAccessible(std::uintptr_t address) {
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT) return false;
    return (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
#elif defined(__APPLE__)
    mach_vm_address_t region = address;
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    if (mach_vm_region(mach_task_self(), &region, &size, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS || region > address) return false;
    return info.protection != VM_PROT_NONE;
#else
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        std::istringstream fields(line);
        std::uintptr_t begin = 0;
        std::uintptr_t last = 0;
        char dash = 0;
        std::string permissions;
        if (fields >> std::hex >> begin >> dash >> last >> permissions && dash == '-' && begin <= address && address < last) return permissions.size() >= 2 && (permissions[0] == 'r' || permissions[1] == 'w');
    }
    return false;
#endif
}

static void IgnoreTrappedRead(std::uintptr_t, std::uintptr_t) {}

static bool ArmReadTrap(std::uintptr_t address, std::size_t bytes, const char* check, const char* failure) {
    bool skip = !GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix() || !GuestWriteWatch::GuestWriteWatchSetReadTrapResolver_nid_postfix(&IgnoreTrappedRead);
#ifdef _WIN32
    skip = skip || !GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, bytes);
#endif
    if (skip) {
        std::printf("skipped: %s, the host does not trap reads of these pages\n", check);
        return false;
    }
    Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(address, bytes) && GuestWriteWatch::GuestWriteWatchTrapReads_nid_postfix(address, bytes), failure);
    return true;
}

static void CheckTrappedAmmUnmapInaccessible(std::uint64_t start, std::uint64_t page) {
    const std::uint64_t address = start + 192 * page;
    std::int64_t direct = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), page, page, 0, &direct) == 0 && direct >= 0, "direct memory for the trapped AMM page was not allocated");
    SubmitAmm([](Apr::CommandBufferObject* buffer, std::uint64_t at, std::uint64_t size, std::int64_t offset) {
        Require(sceAmprAmmCommandBufferMapDirect(buffer, at, static_cast<std::uint64_t>(offset), size, 0, 3) == 0, "the trapped AMM map was not recorded");
    }, address, page, direct);
    *reinterpret_cast<volatile std::uint64_t*>(address) = 0x7c7c;
    const bool trapped = ArmReadTrap(address, page, "the read trap on an AMM page before its unmap", "the read trap on an AMM page was not armed");
    SubmitAmm([](Apr::CommandBufferObject* buffer, std::uint64_t at, std::uint64_t size, std::int64_t) {
        Require(sceAmprAmmCommandBufferUnmap(buffer, at, size) == 0, "the trapped AMM unmap was not recorded");
    }, address, page, direct);
    Require(!HostAccessible(address) && !HostAccessible(address + page - 1), trapped ? "an AMM unmap of a read-trapped page left it accessible" : "an AMM unmap left its page accessible");
    Require(!GuestWriteWatch::GuestWriteWatchOverlaps_nid_postfix(address, page), "an AMM unmap of a read-trapped page left it write watched");
    Require(sceKernelReleaseDirectMemory(direct, page) == 0, "direct memory of the trapped AMM page was not released");
}

static void CheckPoolReserveIntoWatchedOutput(std::uint64_t page) {
    constexpr std::size_t bytes = 0x10000;
    std::int64_t direct = -1;
    Require(sceKernelAllocateDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), bytes, bytes, 0, &direct) == 0, "direct memory for the watched output was not allocated");
    void* first = nullptr;
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&first, bytes, 3, 0, direct, bytes) == 0 && sceKernelMapDirectMemory(&second, bytes, 3, 0, direct, bytes) == 0, "the watched output views were not mapped");
    auto** const out = static_cast<void**>(first);
    *out = nullptr;
    const auto address = reinterpret_cast<std::uintptr_t>(first);
#ifdef _WIN32
    for (int pass = 0; pass < 4 && GuestWritable_nid_no_patch(out, sizeof(*out)); ++pass) {
        std::array<void*, 64> pages{};
        std::size_t count = pages.size();
        GuestArena::GuestArenaCollectWrites_nid_postfix(address, bytes, pages.data(), &count, true);
    }
    Require(!GuestWritable_nid_no_patch(out, sizeof(*out)), "the shared output view was not armed for write tracking");
#else
    ArmReadTrap(address, bytes, "the read trap on a sceKernelMemoryPoolReserve output", "the read trap on a sceKernelMemoryPoolReserve output was not armed");
#endif
    Require(sceKernelMemoryPoolReserve(nullptr, page, 0, 0, out) == 0, "a sceKernelMemoryPoolReserve into a write-watched output outside the AMM window failed");
    void* const reserved = *static_cast<void* volatile*>(second);
    Require(reserved != nullptr && sceKernelMunmap(reinterpret_cast<std::uint64_t>(reserved), page) == 0, "the reserve into a write-watched output was not reported or released");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(first), bytes) == 0 && sceKernelMunmap(reinterpret_cast<std::uint64_t>(second), bytes) == 0, "the watched output views were not released");
    Require(sceKernelReleaseDirectMemory(direct, bytes) == 0, "direct memory for the watched output was not released");
}

static void CheckCoveredReserveIgnoresAlignment(std::uint64_t page) {
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, 2 * page, 0, 0) == 0, "the user range for the covered reserve was not reserved");
    const auto address = reinterpret_cast<std::uintptr_t>(reserved);
    const std::size_t alignment = static_cast<std::size_t>(address & (~address + 1)) << 1;
    void* target = reserved;
    Require(sceKernelReserveVirtualRange(&target, 2 * page, Fixed, alignment) == 0 && target == reserved, "a fixed reserve of a covered user range with an alignment its address does not satisfy did not remap the range");
    Require(sceKernelMunmap(address, 2 * page) == 0, "the covered user range was not released");
}

static void CheckFindNextBelowWindow(std::uint64_t start, std::uint64_t page) {
    VirtualQueryInfo direct{};
    VirtualQueryInfo next{};
    Require(sceKernelVirtualQuery(reinterpret_cast<void*>(start), 0, &direct, sizeof(direct)) == 0, "a query of the AMM window start failed");
    Require(sceKernelVirtualQuery(reinterpret_cast<void*>(start - page), 1, &next, sizeof(next)) == 0, "a find-next query below the AMM window failed");
    Require(direct.is_committed == 0 && direct.is_flexible == 0, "a query of the untouched AMM window start did not report a reservation");
    Require(std::memcmp(&direct, &next, sizeof(direct)) == 0, "a find-next query below the AMM window does not report what a query of the window start reports");
}

static void CheckSuperpagePrivateFileMapInReservation(std::uint64_t page) {
#ifdef _WIN32
    constexpr int AlignedSuper = 0x1000000;
    constexpr std::uint64_t huge = 2ull << 20;
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, 2 * page, 0, 0) == 0, "the user reservation for the superpage private file mmap was not reserved");
    const auto base = reinterpret_cast<std::uintptr_t>(reserved);
    void* const target = reinterpret_cast<void*>(base % huge == 0 ? base + page : base);
    const auto path = WriteSeedFile("anyps5-amm-super-private-", page);
    const int fd = sceKernelOpen(path.string().c_str(), SCE_KERNEL_O_RDONLY, 0);
    void* const mapped = fd >= 0 ? mmap_nid_postfix(target, page, 1, Private | Fixed | AlignedSuper, fd, 0) : MapFailed;
    const bool seeded = mapped == target && *static_cast<volatile char*>(mapped) == 0x5a;
    if (fd >= 0) sceKernelClose(fd);
    std::error_code removed;
    std::filesystem::remove(path, removed);
    Require(fd >= 0, "the file for the superpage private file mmap did not open");
    Require(seeded, "a fixed superpage private file mmap at a page-aligned address in a user reservation was not mapped from the file");
    Require(sceKernelMunmap(base, 2 * page) == 0, "the user reservation for the superpage private file mmap was not released");
#else
    static_cast<void>(page);
#endif
}

static void CheckReleaseOfPooledMemory(std::uint64_t page) {
    const auto size = static_cast<std::int64_t>(sceKernelGetDirectMemorySize());
    std::int64_t pooled = -1;
    Require(sceAmprAmmGiveDirectMemory(0, size, 2 * page, page, 1, &pooled) == 0 && pooled >= 0, "direct memory was not given to the AMM pool");
    Require(NotImplemented([&] { sceKernelReleaseDirectMemory(pooled, page); }), "releasing direct memory in the AMM pool did not call NotImplemented");
    Require(NotImplemented([&] { sceKernelCheckedReleaseDirectMemory(pooled + static_cast<std::int64_t>(page), page); }), "a checked release of direct memory in the AMM pool did not call NotImplemented");
    const auto beyond = static_cast<std::size_t>(size - pooled) + page;
    Require(sceKernelCheckedReleaseDirectMemory(pooled, beyond) == KernelNoEntry, "a checked release from the AMM pool past the direct memory end did not fail with ENOENT");
    Require(sceKernelCheckedReleaseDirectMemory(pooled, std::numeric_limits<std::size_t>::max() - (page - 1)) == KernelNoEntry, "a checked release from the AMM pool whose end wraps did not fail with ENOENT");
    std::int64_t mappable = -1;
    Require(sceAmprAmmGiveDirectMemory(0, size, page, page, 0, &mappable) == 0 && mappable >= 0, "direct memory was not given to the AMM for maps");
    Require(sceKernelReleaseDirectMemory(mappable, page) == 0, "direct memory given to the AMM for maps was not released");
    std::int64_t plain = -1;
    Require(sceKernelAllocateDirectMemory(0, size, page, page, 0, &plain) == 0 && plain >= 0, "direct memory outside the AMM pool was not allocated");
    Require(sceKernelCheckedReleaseDirectMemory(plain, page) == 0, "a checked release of direct memory outside the AMM pool failed");
}

template <typename TCondition>
static bool Eventually(TCondition condition) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!condition()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

static void CheckReleaseWaitingForMapLeavesDisjointRelease(std::uint64_t page) {
    const auto size = static_cast<std::int64_t>(sceKernelGetDirectMemorySize());
    std::int64_t held = -1;
    std::int64_t other = -1;
    Require(sceKernelAllocateDirectMemory(0, size, page, page, 0, &held) == 0 && sceKernelAllocateDirectMemory(0, size, page, page, 0, &other) == 0, "direct memory for the concurrent releases was not allocated");
    const auto heldFirst = static_cast<std::uint64_t>(held);
    const auto otherFirst = static_cast<std::uint64_t>(other);
    void* mapped = nullptr;
    int mappedResult = -1;
    int heldReleased = -1;
    int otherReleased = -1;
    std::atomic<int> finished{0};
    bool mapHeld = false;
    bool releaseWaiting = false;
    bool claimed = false;
    std::thread mapper;
    std::thread heldReleaser;
    std::thread otherReleaser;
    {
        const GuestAllocations::Mutation mutation;
        mapper = std::thread([&] { mappedResult = sceKernelMapDirectMemory(&mapped, page, 3, 0, held, 0); ++finished; });
        mapHeld = Eventually([&] { return DirectMemoryMapHeld(heldFirst, heldFirst + page); });
        heldReleaser = std::thread([&] { heldReleased = sceKernelReleaseDirectMemory(held, page); ++finished; });
        releaseWaiting = Eventually([&] { return DirectMemoryReleaseWaitersForTest() != 0; });
        otherReleaser = std::thread([&] { otherReleased = sceKernelReleaseDirectMemory(other, page); ++finished; });
        claimed = Eventually([&] { return DirectMemoryReleaseClaimed(otherFirst, otherFirst + page, nullptr); });
    }
    if (!Eventually([&] { return finished.load() == 3; })) {
        std::fprintf(stderr, "the concurrent map and releases did not finish (mapHeld %d, releaseWaiting %d)\n", mapHeld, releaseWaiting);
        std::fflush(stderr);
        std::_Exit(1);
    }
    mapper.join();
    heldReleaser.join();
    otherReleaser.join();
    Require(mapHeld, "the direct map did not hold its direct memory while the guest allocations were locked");
    Require(releaseWaiting, "the release of mapped direct memory did not wait for the map");
    Require(claimed, "a release waiting for a direct map blocked the release of disjoint direct memory");
    Require(mappedResult == 0 && heldReleased == 0 && otherReleased == 0, "the concurrent map and releases did not succeed");
}

static void CheckCheckedReleaseRefusesCompetingClaim(std::uint64_t page) {
    const auto size = static_cast<std::int64_t>(sceKernelGetDirectMemorySize());
    std::int64_t claimed = -1;
    Require(sceKernelAllocateDirectMemory(0, size, page, page, 0, &claimed) == 0, "direct memory for the competing release claim was not allocated");
    const auto first = static_cast<std::uint64_t>(claimed);
    Require(DirectMemoryTryAddReleaseClaim(first, first + page), "a release claim on allocated direct memory was not added");
    const auto tried = DirectMemoryTryClaimRelease(claimed, page, true);
    const auto awaited = DirectMemoryAwaitReleaseClaim(claimed, page, true);
    const auto checked = sceKernelCheckedReleaseDirectMemory(claimed, page);
    DirectMemoryRemoveReleaseClaim(first);
    Require(tried == DirectMemoryReleaseClaim::Refused, "a checked release claim on direct memory claimed by another release was not refused");
    Require(awaited == DirectMemoryReleaseClaim::Refused, "a checked release waiting on direct memory claimed by another release did not report the refusal");
    Require(checked == KernelNoEntry, "a checked release of direct memory claimed by another release did not fail with ENOENT");
    Require(sceKernelCheckedReleaseDirectMemory(claimed, page) == 0, "a checked release after the competing claim ended failed");
}

static void CheckPoolGiftArgumentErrors(std::uint64_t page) {
    const auto size = static_cast<std::int64_t>(sceKernelGetDirectMemorySize());
    const auto signedPage = static_cast<std::int64_t>(page);
    struct Gift {
        std::int64_t searchStart;
        std::int64_t searchEnd;
        std::uint64_t size;
        std::uint64_t alignment;
        const char* message;
    };
    const Gift gifts[] = {
        {0, size, 0, page, "giving zero bytes of direct memory to the AMM pool did not fail with EINVAL"},
        {0, size, page + 1, page, "giving an unaligned size of direct memory to the AMM pool did not fail with EINVAL"},
        {0, size, page, page + 1, "giving direct memory to the AMM pool with an unaligned alignment did not fail with EINVAL"},
        {-signedPage, size, page, page, "giving direct memory to the AMM pool from a negative search start did not fail with EINVAL"},
        {signedPage, signedPage, page, page, "giving direct memory to the AMM pool from an empty search range did not fail with EINVAL"},
    };
    for (const auto& gift : gifts) {
        std::int64_t pool = -1;
        int result = 0;
        Require(!Throws([&] { result = sceAmprAmmGiveDirectMemory(gift.searchStart, gift.searchEnd, gift.size, gift.alignment, 1, &pool); }) && result == KernelInvalid && pool == -1, gift.message);
    }
}

static const char* gCapturedConstraintMessage = nullptr;
static void* gCapturedConstraintPointer = nullptr;
static int gCapturedConstraintError = 0;
static int gCapturedConstraintCalls = 0;

static void APS5_VABI TestConstraintHandler(const char* message, void* pointer, int error) {
    gCapturedConstraintMessage = message;
    gCapturedConstraintPointer = pointer;
    gCapturedConstraintError = error;
    ++gCapturedConstraintCalls;
}

int main() {
    try {
        std::uintptr_t arenaBase = 0;
        std::size_t arenaBytes = 0;
        GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
        Require(arenaBase != 0 && arenaBytes != 0, "the guest arena is unavailable");
        const std::uint64_t arenaEnd = arenaBase + arenaBytes;
        constexpr std::uint64_t windowBytes = 512ull << 30;
        constexpr std::uint64_t page = 0x4000;

        std::uintptr_t windowBase = 0;
        std::size_t reservedBytes = 0;
        int reserveError = -1;
        GuestArena::GuestArenaMapperWindow_nid_no_patch(&windowBase, &reservedBytes, &reserveError);
#ifdef ANYPS5_ENABLE_TSAN
        constexpr std::uintptr_t expectedWindow = 0x7e8000000000ull;
#else
        constexpr std::uintptr_t expectedWindow = 0x80000000000ull + GuestArena::GuestAddressOffset;
#endif
        if (windowBase == 0) {
            std::printf("skipped: the host could not reserve the AMM window at 0x%llx (OS error %d)\n", static_cast<unsigned long long>(expectedWindow), reserveError);
            CheckPoolGiftArgumentErrors(page);
            std::uint64_t ignored = 0;
            Require(Throws([&] { sceAmprAmmGetVirtualAddressRanges(&ignored, &ignored, &ignored, &ignored); }), "an AMM range query without the AMM window did not throw");
            std::int64_t pool = -1;
            Require(Throws([&] { sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), page, page, 1, &pool); }) && pool == -1, "giving direct memory to the AMM pool without the AMM window did not throw before allocating");
            std::int64_t mapped = -1;
            Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), page, page, 0, &mapped) == 0 && mapped >= 0, "giving direct memory to the AMM for maps without the AMM window did not allocate it");
            Require(sceKernelReleaseDirectMemory(mapped, page) == 0, "direct memory given for maps without the AMM window was not released");
            return 77;
        }
        Require(windowBase == expectedWindow && reservedBytes == windowBytes && reserveError == 0, "the AMM window was not reserved at startup");
        Require(HostReserved(windowBase, reservedBytes), "the host does not hold the AMM window reserved");

        std::uint64_t start = 0;
        std::uint64_t end = 0;
        std::uint64_t multimapStart = 0;
        std::uint64_t multimapEnd = 0;
        Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0, "AMM ranges were not reported");
        Require(start == windowBase, "the AMM range does not start at the console address");
        Require(end - start == 480ull << 30 && end == multimapStart && multimapEnd - multimapStart == 32ull << 30, "AMM ranges are not a 480 GiB range and an adjacent 32 GiB multimap range");
        Require(multimapEnd == windowBase + windowBytes, "the multimap range does not end at the AMM window end");
        Require(start >= arenaEnd, "AMM ranges overlap the guest arena");
        Require(HostReserved(start, multimapEnd - start), "the host released the AMM window");

        CheckFindNextBelowWindow(start, page);
        constexpr std::uint64_t topBytes = 64ull << 20;
        void* top = reinterpret_cast<void*>(arenaEnd - topBytes);
        Require(sceKernelReserveVirtualRange(&top, topBytes, 0x90, 0) == 0 && reinterpret_cast<std::uint64_t>(top) == arenaEnd - topBytes, "a fixed no-overwrite reserve at the top of the guest arena failed after AMM init");
        Require(sceKernelMunmap(arenaEnd - topBytes, topBytes) == 0, "the reserve at the top of the guest arena was not released");

        Require(FixedMapThrows(reinterpret_cast<void*>(multimapEnd - page), page, true), "a fixed reserve inside the AMM window did not throw");
        Require(FixedMapThrows(reinterpret_cast<void*>(start), page, false), "a fixed flexible map inside the AMM window did not throw");
        Require(FixedMapThrows(reinterpret_cast<void*>(start - page), 2 * page, true), "a fixed reserve across the AMM window start did not throw");
        CheckTitleMapsIntoWindow(start, page);
        CheckTitleChangesInWindow(start, page);
        CheckArgumentErrorsBeforeWindow(start, multimapEnd, page);
        CheckAlignedHintBelowWindow(start, arenaBase, arenaBytes);
        CheckReleaseOfAmmMappedMemory(start, page);
        CheckRefusedReleaseKeepsUserView(start, page);
        CheckUnmappedWindowUnwatched(start);
        CheckTrappedAmmUnmapInaccessible(start, page);
        CheckPoolReserveIntoWatchedOutput(page);
        CheckCoveredReserveIgnoresAlignment(page);
        CheckSuperpagePrivateFileMapInReservation(page);
        CheckPoolGiftArgumentErrors(page);
        CheckReleaseOfPooledMemory(page);
        CheckReleaseWaitingForMapLeavesDisjointRelease(page);
        CheckCheckedReleaseRefusesCompetingClaim(page);
        Require(HostReserved(start, multimapEnd - start), "the host released the AMM window after the title calls");

        std::uint64_t start2 = 0;
        std::uint64_t end2 = 0;
        std::uint64_t multimapStart2 = 0;
        std::uint64_t multimapEnd2 = 0;
        Require(sceAmprAmmGetVirtualAddressRanges(&start2, &end2, &multimapStart2, &multimapEnd2) == 0, "second AMM range query failed");
        Require(start2 == start && end2 == end && multimapStart2 == multimapStart && multimapEnd2 == multimapEnd, "AMM ranges changed on repeated query");

        void* const initialHandler = SonySetConstraintHandlerS_nid_no_patch(nullptr);
        Require(initialHandler != nullptr, "initial constraint handler was null");
        SonyInvokeConstraintHandler_nid_no_patch(nullptr, nullptr, 0);

        gCapturedConstraintMessage = nullptr;
        gCapturedConstraintPointer = nullptr;
        gCapturedConstraintError = 0;
        gCapturedConstraintCalls = 0;
        void* const previousDefault = SonySetConstraintHandlerS_nid_no_patch(reinterpret_cast<void*>(&TestConstraintHandler));
        Require(previousDefault == initialHandler, "SonySetConstraintHandlerS did not return the previous default handler");

        const char* const testMessage = "constraint check";
        int dummyTarget = 42;
        SonyInvokeConstraintHandler_nid_no_patch(testMessage, &dummyTarget, 22);
        Require(gCapturedConstraintCalls == 1, "custom constraint handler was not invoked");
        Require(gCapturedConstraintMessage == testMessage, "custom constraint handler did not receive message");
        Require(gCapturedConstraintPointer == &dummyTarget, "custom constraint handler did not receive pointer");
        Require(gCapturedConstraintError == 22, "custom constraint handler did not receive error");

        void* const previousCustom = SonySetConstraintHandlerS_nid_no_patch(nullptr);
        Require(previousCustom == reinterpret_cast<void*>(&TestConstraintHandler), "SonySetConstraintHandlerS did not return the previous custom handler");

        std::puts("AMM ranges placed in the AMM window outside the guest arena");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
