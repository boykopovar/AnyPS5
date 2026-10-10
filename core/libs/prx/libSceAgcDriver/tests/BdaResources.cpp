#include "BdaTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <new>
#include <string>

#ifndef _WIN32
#include <sys/mman.h>
#endif
namespace {

using namespace AgcDriver::Graphics;
using Role = ShaderRecompiler::DescriptorRole;

template<typename TAction>
void reject(TAction action, const char* reason) {
    try { action(); }
    catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find(reason) != std::string::npos, std::string("unexpected BDA test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected BDA rejection: ") + reason);
}

void importCrossingTests(const Context& context, const BdaTestAccess& access) {
    constexpr std::size_t half = 65536;
    void* block = ::operator new(2 * half, std::align_val_t{half});
    auto* guest = static_cast<std::uint8_t*>(block);
    std::memset(guest, 0x11, half);
    std::memset(guest + half, 0x22, half);
    const auto first = reinterpret_cast<std::uintptr_t>(block);
    const auto second = first + half;
    auto importing = context;
    importing.hostImportAlignment = half;
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        for (auto* range : {guest, guest + half}) {
            if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, half, true, false, true);
            else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
        }
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    registry(true);
    {
        GuestBufferMemory leased(importing);
        leased.AcquireRegistered();
        Require(HostImportCovers(importing, first, half) && HostImportCovers(importing, second, half), "the registered ranges were not imported");
        const auto secondImport = *HostImportFor(importing, second, half);
        leased.AddReadable(second - 16, 32);
        leased.AddReadable(second + 64, 16);
        leased.Upload(true);
        std::uint32_t adjustment = 0;
        const auto crossing = leased.Descriptor(second - 16, 32, adjustment);
        const auto crossingBytes = access.bytes(crossing.buffer);
        Require(crossing.offset + crossing.range <= crossingBytes.size(), "a view crossing into the next registered range is bound past its host import");
        Require(crossingBytes[crossing.offset + adjustment + 15] == std::byte{0x11} && crossingBytes[crossing.offset + adjustment + 16] == std::byte{0x22}, "a view crossing into the next registered range misses its bytes");
        const auto inside = leased.Descriptor(second + 64, 16, adjustment);
        const auto insideBytes = access.bytes(inside.buffer);
        Require(inside.offset + inside.range <= insideBytes.size() && (inside.buffer == secondImport.buffer ? inside.offset + adjustment == 64 : insideBytes[inside.offset + adjustment] == std::byte{0x22}), "a view in the second registered range is bound past its buffer or misses its bytes");
        for (const auto& range : leased.AddressRanges()) {
            if (range.end <= first || range.begin >= second + half) continue;
            const auto mapped = access.addressBytes(range.deviceAddress);
            Require(mapped.size() >= range.end - range.begin, "a BDA range runs past the end of its host import");
            if (range.begin <= second && second < range.end && range.deviceAddress != secondImport.address) Require(mapped[second - range.begin] == std::byte{0x22}, "the BDA range over the second registered range misses its bytes");
        }
        leased.WriteBack();
    }
    registry(false);
    Require(HostImportFor(importing, first, half) == nullptr && !HostImportCovers(importing, second, half), "the host imports outlived their ranges");
    ::operator delete(block, std::align_val_t{half});
}

ShaderRecompiler::DescriptorBinding binding(Role role, std::uint32_t slot) {
    return {ShaderRecompiler::DescriptorKind::StorageBuffer, role, 0, slot, 1, {}, false};
}

void importedHeapMirrorTests(const Context& context, const BdaTestAccess& access) {
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: heap mirrors of imported ranges not tested\n";
        return;
    }
    constexpr std::size_t bytes = 1u << 20u;
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, bytes);
#ifdef _WIN32
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#endif
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    auto importing = context;
    importing.hostImportAlignment = bytes;
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, bytes, true, false, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto build = [&](const Context& with) {
        GuestAllocations::GuestAllocationsEnd_nid_postfix(GuestAllocations::GuestAllocationsBegin_nid_postfix());
        GuestBufferMemory leased(with);
        leased.AcquireRegistered();
        leased.Upload(true);
        const auto ranges = leased.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == address && range.end == address + bytes; });
        const auto device = found != ranges.end() ? found->deviceAddress : 0;
        leased.WriteBack();
        return device;
    };
    const auto before = MirrorCounters();
    registry(true);
    Require(build(context) != 0, "the heap range is missing from the BDA table of a build without host imports");
    const auto mirrored = MirrorCounters();
    Require(mirrored.heapMirrors == before.heapMirrors + 1 && mirrored.heapBytes == before.heapBytes + bytes, "a range built without host imports was not heap mirrored");
    Require(build(importing) != 0 && HostImportCovers(importing, address, bytes), "the heap mirrored range was not imported once imports were available");
    const auto imported = MirrorCounters();
    Require(imported.heapMirrors == before.heapMirrors && imported.heapBytes == before.heapBytes, "a heap mirror outlived the host import that serves its range");
    Require(build(importing) != 0 && MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().rebuilds == imported.rebuilds, "an imported range was heap mirrored again");
    static_cast<std::uint8_t*>(block)[bytes / 2] = 0x22;
    const auto device = build(context);
    const auto again = MirrorCounters();
    Require(device != 0 && again.heapMirrors == before.heapMirrors + 1 && again.heapBytes == before.heapBytes + bytes && again.rebuilds == imported.rebuilds + 1, "a range no import serves was not heap mirrored again");
    Require(access.addressBytes(device)[bytes / 2] == std::byte{0x22} && access.addressBytes(device)[bytes / 2 + 1] == std::byte{0x11}, "a heap mirror made after its import missed the guest bytes");
    registry(false);
    Require(HostImportFor(importing, address, bytes) == nullptr && !HostImportCovers(importing, address, bytes), "the import outlived its range");
    Require(build(context) == 0, "the unregistered heap range is still in the BDA table");
    Require(MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().heapBytes == before.heapBytes, "the heap mirror outlived its range");
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(block, bytes);
#endif
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
}

void importedFreshTests(const Context& context) {
#ifndef _WIN32
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: imported never-collected pages not tested\n";
        return;
    }
    constexpr std::size_t blockBytes = 65536;
    void* area = mmap(nullptr, 2 * blockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(area != MAP_FAILED, "cannot map the imported block");
    const auto base = (reinterpret_cast<std::uintptr_t>(area) + blockBytes - 1) & ~(blockBytes - 1);
    std::memset(reinterpret_cast<void*>(base), 0x55, blockBytes);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<void*>(base), blockBytes);
    auto importing = context;
    importing.hostImportAlignment = blockBytes;
    const auto registry = [&](bool add) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, reinterpret_cast<void*>(base), blockBytes, true, true, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, reinterpret_cast<void*>(base));
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    registry(true);
    if (HostImportFor(importing, base, blockBytes) == nullptr) {
        std::cout << "the never-collected block was not imported: imported never-collected pages not tested\n";
    } else {
        const auto gpuWrite = AgcDriver::GuestMemory::MarkWritten(base, 4);
        Require(gpuWrite != 0, "a GPU write into an imported never-collected block was not stamped");
        reinterpret_cast<volatile std::uint8_t*>(base)[5 * 4096 + 8] = 0x66;
        static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(base, blockBytes));
        Require(AgcDriver::GuestMemory::WrittenSince(base + 5 * 4096, 4096, gpuWrite), "a CPU write to an imported page protected after the import was not seen");
    }
    registry(false);
    Require(HostImportFor(importing, base, blockBytes) == nullptr, "the import outlived its range");
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<void*>(base), blockBytes);
    munmap(area, 2 * blockBytes);
#else
    static_cast<void>(context);
#endif
}

void importWatchTests() {
#ifndef _WIN32
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        std::cout << "write watch unavailable: import watch not tested\n";
        return;
    }
    constexpr std::size_t bytes = 4 * 65536;
    void* block = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(block != MAP_FAILED, "cannot map the import watch block");
    auto* guest = static_cast<std::uint8_t*>(block);
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
    std::memset(block, 0x22, bytes);
    const auto written = [&] {
        std::size_t reported = 0;
        Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(address, bytes, [](void* context, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::size_t*>(context) += end - begin; }, &reported), "the write watch did not collect the block");
        return reported;
    };
    Require(AgcDriver::GuestMemory::ImportWatched(address, bytes, [] { return true; }), "importing a block nothing had collected failed");
    Require(written() == bytes, "an import write-protected pages nothing had collected");
    guest[3 * 4096] = 0x33;
    Require(AgcDriver::GuestMemory::ImportWatched(address, bytes, [] { return true; }), "importing a collected block failed");
    Require(written() == 0, "an import did not take the writes to pages a collect had protected");
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
    munmap(block, bytes);
    constexpr std::size_t blockBytes = 65536;
    void* area = mmap(nullptr, bytes + 2 * blockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(area != MAP_FAILED, "cannot map the GPU write block");
    const auto aligned = (reinterpret_cast<std::uintptr_t>(area) + blockBytes - 1) & ~(blockBytes - 1);
    const auto imported = aligned + 4096;
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<void*>(imported), bytes);
    Require(AgcDriver::GuestMemory::ImportWatched(imported, bytes, [] { return true; }), "importing a fresh range failed");
    const auto gpuWrite = AgcDriver::GuestMemory::MarkWritten(imported + 4096, 4096);
    Require(gpuWrite != 0, "a GPU write into the imported range was not stamped");
    static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(imported, bytes));
    Require(!AgcDriver::GuestMemory::WrittenSince(imported + 4096, 4096, gpuWrite), "a collect after a GPU write into an uncollected import read as a CPU write over the GPU's results");
    reinterpret_cast<volatile std::uint8_t*>(imported)[4096 + 100] = 0x44;
    static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(imported, bytes));
    Require(AgcDriver::GuestMemory::WrittenSince(imported + 4096, 4096, gpuWrite), "a CPU write after a GPU write into an import was not seen");
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<void*>(imported), bytes);
    munmap(area, bytes + 2 * blockBytes);

    void* labels = mmap(nullptr, 2 * blockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(labels != MAP_FAILED, "cannot map the label block");
    const auto label = (reinterpret_cast<std::uintptr_t>(labels) + blockBytes - 1) & ~(blockBytes - 1);
    auto* words = reinterpret_cast<volatile std::uint32_t*>(label);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<void*>(label), blockBytes);
    words[0] = 1;
    static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(label, 4));
    const auto before = AgcDriver::GuestMemory::TrackerGeneration();
    Require(AgcDriver::GuestMemory::MarkWritten(label, 4) != 0, "a GPU write next to never-collected pages was not stamped");
    Require(AgcDriver::GuestMemory::UnchangedSinceCollected(label, 4, before), "a GPU write next to never-collected pages read as a CPU write of its block");
    Require(AgcDriver::GuestMemory::StoredOver(label + 8 * 4096, 4096, before), "a never-collected page a GPU write collected is not reported as possibly stored");
    const std::array<std::uint64_t, 1> since{before};
    std::array<std::uint8_t, 1> changed{};
    std::array<std::uint8_t, 1> cpu{};
    Require(AgcDriver::GuestMemory::ChangedBlocks(label, blockBytes, since, changed, cpu) && changed[0] == AgcDriver::GuestMemory::BlockWritten && cpu[0] == 0, "collecting never-collected pages at a GPU write stamped their block as written by the CPU");
    const auto now = AgcDriver::GuestMemory::TrackerGeneration();
    Require(AgcDriver::GuestMemory::MarkWritten(label, 4) == now + 1, "a GPU write into a block with nothing left to collect advanced the generation more than once");
    words[1] = 2;
    Require(!AgcDriver::GuestMemory::UnchangedSinceCollected(label, 4, before), "a CPU write next to a GPU write was not seen");
    void* awaitedArea = mmap(nullptr, 2 * blockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(awaitedArea != MAP_FAILED, "cannot map the awaited block");
    const auto awaitedBlock = (reinterpret_cast<std::uintptr_t>(awaitedArea) + blockBytes - 1) & ~(blockBytes - 1);
    const auto awaited = awaitedBlock + 5 * 4096;
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<void*>(awaitedBlock), blockBytes);
    static_cast<void>(AgcDriver::GuestMemory::CollectWritesUncached(awaitedBlock, 4));
    const auto closed = AgcDriver::GuestMemory::TrackerGeneration();
    *reinterpret_cast<volatile std::uint32_t*>(awaited) = 7;
    Require(AgcDriver::GuestMemory::MarkWritten(awaitedBlock, 4) != 0, "a GPU write into the awaited block was not stamped");
    Require(!AgcDriver::GuestMemory::UnchangedSinceCollected(awaited, 4, closed), "a CPU store to a never-collected page that a GPU write elsewhere in its block collected was not seen");
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<void*>(awaitedBlock), blockBytes);
    munmap(awaitedArea, 2 * blockBytes);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<void*>(label), blockBytes);
    munmap(labels, 2 * blockBytes);
#endif
}

void heapMirrorTests(const Context& context, const BdaTestAccess& access) {
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: heap mirrors not tested\n";
        return;
    }
    constexpr std::size_t bytes = 2 * 65536;
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536);
#ifdef _WIN32
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#endif
    auto* guest = static_cast<std::uint8_t*>(block);
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    const auto registry = [&](bool add, bool writable) {
        auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
        if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, block, bytes, true, writable, true);
        else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, block);
        GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
    };
    const auto build = [&](std::uint64_t written, const std::function<void(GuestBufferMemory&)>& gpu) {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        if (written != 0) leased.AddWritable(written, 32);
        leased.Upload(true);
        const auto ranges = leased.AddressRanges();
        const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == address; });
        Require(found != ranges.end() && found->deviceAddress != 0u, "the heap range is missing from the BDA table");
        const auto device = found->deviceAddress;
        auto cursor = address;
        std::uint64_t writableBytes = 0;
        for (auto part = found; cursor < address + bytes && part != ranges.end(); ++part) {
            Require(part->begin == cursor && part->end > cursor && part->end <= address + bytes, "heap BDA ranges have gaps or overlap");
            Require(part->deviceAddress == device + cursor - address, "heap BDA ranges are not contiguous on the device");
            const bool writable = written != 0u && part->begin >= written && part->end <= written + 32u;
            const auto permissions = ShaderRecompiler::BdaAbi::Read | (writable ? ShaderRecompiler::BdaAbi::Write : 0u);
            Require(part->permissions == permissions, "heap BDA range has incorrect permissions");
            if (writable) writableBytes += part->end - part->begin;
            cursor = part->end;
        }
        Require(cursor == address + bytes && writableBytes == (written != 0u ? 32u : 0u), "heap BDA coverage or writable extent is incorrect");
        if (gpu) gpu(leased);
        leased.WriteBack();
        return device;
    };
    const auto sweep = [&] {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        leased.Upload(true);
        leased.WriteBack();
    };
    sweep();
    registry(true, false);
    const auto before = MirrorCounters();
    const auto first = build(0, {});
    const auto made = MirrorCounters();
    Require(made.heapMirrors == before.heapMirrors + 1 && made.heapBytes == before.heapBytes + bytes, "a read-only heap range was not mirrored");
    Require(access.addressBytes(first)[65536 + 3] == std::byte{0x11}, "the heap mirror was not filled");
    Require(build(0, {}) == first && MirrorCounters().blocksCopied == made.blocksCopied && MirrorCounters().heapRefills == made.heapRefills, "an unchanged heap range was read into its mirror again");
    guest[65536 + 3] = 0x22;
    Require(build(0, {}) == first && MirrorCounters().heapRefills == made.heapRefills + 1 && MirrorCounters().blocksCopied == made.blocksCopied + 1, "a written heap block was not read again alone");
    Require(access.addressBytes(first)[65536 + 3] == std::byte{0x22}, "the heap mirror missed the CPU write");
    const auto one = MirrorCounters();
    guest[5] = 0x33;
    guest[65536 + 7] = 0x44;
    Require(build(0, {}) == first && MirrorCounters().blocksCopied == one.blocksCopied + 2, "two written heap blocks were not read again");
    Require(access.addressBytes(first)[5] == std::byte{0x33} && access.addressBytes(first)[65536 + 7] == std::byte{0x44}, "the heap mirror missed the CPU writes");
    const auto quiet = MirrorCounters();
    Require(build(0, {}) == first && MirrorCounters().heapChecks == quiet.heapChecks && MirrorCounters().sweeps == quiet.sweeps && MirrorCounters().blocksCopied == quiet.blocksCopied, "an unchanged heap range was compared block by block or the mirrors were swept again");
    guest[65536 + 9] = 0x66;
    Require(build(0, {}) == first && MirrorCounters().heapChecks > quiet.heapChecks && MirrorCounters().blocksCopied == quiet.blocksCopied + 1 && access.addressBytes(first)[65536 + 9] == std::byte{0x66}, "a CPU write after an unchanged build was missed");
    registry(false, false);
    sweep();
    const auto swept = MirrorCounters();
    Require(swept.heapMirrors == before.heapMirrors && swept.heapBytes == before.heapBytes, "the heap mirror outlived its range");

    registry(true, true);
    const auto device = build(address + 16, [&](GuestBufferMemory& leased) {
        std::uint32_t adjustment = 0;
        const auto view = leased.Descriptor(address + 16, 32, adjustment);
        access.bytes(view.buffer)[view.offset + adjustment] = std::byte{0x77};
        guest[16 + 8] = 0x55;
    });
    Require(guest[16] == 0x77 && guest[16 + 8] == 0x55, "a writable heap mirror's write-back lost the GPU's store or rolled back the CPU's");
    Require(build(0, {}) == device && access.addressBytes(device)[16] == std::byte{0x77} && access.addressBytes(device)[16 + 8] == std::byte{0x55}, "the writable heap mirror missed the stores");
    Require(build(address + 40, [&](GuestBufferMemory& leased) {
        std::uint32_t adjustment = 0;
        const auto view = leased.Descriptor(address + 40, 8, adjustment);
        access.bytes(view.buffer)[view.offset + adjustment] = std::byte{0x78};
    }) == device && guest[40] == 0x78, "a writable heap mirror's second write-back lost the GPU's store");
    guest[65536 + 1] = 0x79;
    Require(build(0, {}) == device && access.addressBytes(device)[40] == std::byte{0x78} && access.addressBytes(device)[65536 + 1] == std::byte{0x79}, "the writable heap mirror missed a store after a write-back");
    registry(false, true);
    sweep();
    Require(MirrorCounters().heapMirrors == before.heapMirrors, "the writable heap mirror outlived its range");

#ifdef _WIN32
    {
        constexpr std::size_t half = 2 * 65536;
        void* pair = GuestArena::GuestArenaAllocate_nid_postfix(2 * half, 65536);
        GuestArena::GuestArenaCommit_nid_postfix(pair, 2 * half, PAGE_READWRITE, 2 * half);
        std::memset(pair, 0x11, 2 * half);
        auto* first = static_cast<std::uint8_t*>(pair);
        auto* second = first + half;
        const auto registerPair = [&](bool add) {
            auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
            for (auto* range : {first, second}) {
                if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, range, half, true, false, true);
                else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, range);
            }
            GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        };
        const auto firstDevice = [&] {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
            leased.Upload(true);
            const auto ranges = leased.AddressRanges();
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == reinterpret_cast<std::uintptr_t>(first); });
            Require(found != ranges.end(), "the first heap range is missing from the BDA table");
            const auto device = found->deviceAddress;
            leased.WriteBack();
            return device;
        };
        registerPair(true);
        const auto device = firstDevice();
        first[5] = 0x66;
        DWORD previous = 0;
        Require(VirtualProtect(second, 65536, PAGE_NOACCESS, &previous) != 0, "cannot protect the second heap range");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(second), 65536);
        bool threw = false;
        try {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(VirtualProtect(second, 65536, PAGE_READWRITE, &previous) != 0, "cannot unprotect the second heap range");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(second), 65536);
        Require(threw, "a build over an inaccessible heap mirror range did not fail");
        Require(firstDevice() == device && access.addressBytes(device)[5] == std::byte{0x66}, "an interrupted build left a changed heap block marked current");
        registerPair(false);
        sweep();
        GuestArena::GuestArenaReset_nid_postfix(pair, 2 * half);
        GuestArena::GuestArenaRelease_nid_postfix(pair, 2 * half);
    }
#endif
#ifdef _WIN32
    {
        constexpr std::size_t size = 2 * 65536;
        void* raw = GuestArena::GuestArenaAllocate_nid_postfix(size, 65536);
        GuestArena::GuestArenaCommit_nid_postfix(raw, size, PAGE_READWRITE, size);
        auto* bytes8 = static_cast<std::uint8_t*>(raw);
        std::memset(raw, 0x11, size);
        const auto base = reinterpret_cast<std::uintptr_t>(raw);
        const auto page = base + 65536;
        DWORD previous = 0;
        Require(VirtualProtect(reinterpret_cast<void*>(page), 4096, PAGE_READONLY, &previous) != 0, "cannot make the aliased page read-only");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(page, 4096);
        const auto registerRange = [&](bool add) {
            auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
            if (add) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, raw, size, true, true, true);
            else GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, raw);
            GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        };
        registerRange(true);
        const auto storeAt = [&](std::uint64_t at) {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
            leased.AddWritable(page - 16, 32);
            leased.Upload(true);
            const auto ranges = leased.AddressRanges();
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == base; });
            Require(found != ranges.end() && access.addressBytes(found->deviceAddress)[65536 + 8] == std::byte{0x11}, "a writable range with a read-only page was not mirrored with its bytes");
            std::uint32_t adjustment = 0;
            const auto view = leased.Descriptor(page - 16, 32, adjustment);
            access.bytes(view.buffer)[view.offset + adjustment + static_cast<std::size_t>(at - (page - 16))] = std::byte{0x77};
            leased.WriteBack();
        };
        const auto made = MirrorCounters().heapMirrors;
        storeAt(page - 8);
        Require(MirrorCounters().heapMirrors == made + 1 && bytes8[65536 - 8] == 0x77, "a store next to a read-only page was not written back");
        bool refused = false;
        try {
            storeAt(page + 4);
        } catch (const std::runtime_error& error) {
            refused = std::string(error.what()).find("aliased writes are not implemented") != std::string::npos;
        }
        Require(refused && bytes8[65536 + 4] == 0x11, "a GPU change of a read-only page was not refused");
        {
            GuestBufferMemory plain(context);
            plain.AddReadable(page, 16);
            plain.Upload(false);
            std::uint32_t adjustment = 0;
            const auto view = plain.Descriptor(page, 16, adjustment);
            Require(access.bytes(view.buffer)[view.offset + adjustment] == std::byte{0x11}, "a descriptor over a read-only page of a writable range read zeros");
            plain.WriteBack();
        }
        registerRange(false);
        sweep();
        Require(VirtualProtect(reinterpret_cast<void*>(page), 4096, PAGE_READWRITE, &previous) != 0, "cannot restore the aliased page");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(page, 4096);
        GuestArena::GuestArenaReset_nid_postfix(raw, size);
        GuestArena::GuestArenaRelease_nid_postfix(raw, size);
    }
#endif
    {
        constexpr std::size_t large = 36 * 65536;
        constexpr std::size_t small = 17 * 65536;
        constexpr std::size_t total = 2 * large + small;
        void* raw = GuestArena::GuestArenaAllocate_nid_postfix(total, 65536);
#ifdef _WIN32
        GuestArena::GuestArenaCommit_nid_postfix(raw, total, PAGE_READWRITE, total);
#endif
        auto* const heap = static_cast<std::uint8_t*>(raw);
        const std::array<std::uint8_t*, 3> heaps{heap, heap + large, heap + 2 * large};
        const std::array<std::size_t, 3> sizes{large, large, small};
        for (std::size_t index = 0; index < heaps.size(); ++index) std::memset(heaps[index], 0x21 + static_cast<int>(index), sizes[index]);
        const auto change = [&](std::initializer_list<std::size_t> added, std::initializer_list<std::size_t> removed) {
            auto* mutation = GuestAllocations::GuestAllocationsBegin_nid_postfix();
            for (const auto index : removed) GuestAllocations::GuestAllocationsRemove_nid_postfix(mutation, heaps[index]);
            for (const auto index : added) GuestAllocations::GuestAllocationsAdd_nid_postfix(mutation, heaps[index], sizes[index], true, false, true);
            GuestAllocations::GuestAllocationsEnd_nid_postfix(mutation);
        };
        const auto mirrored = [&](std::size_t index) {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
            leased.Upload(true);
            const auto ranges = leased.AddressRanges();
            const auto begin = reinterpret_cast<std::uintptr_t>(heaps[index]);
            const auto found = std::find_if(ranges.begin(), ranges.end(), [&](const auto& entry) { return entry.begin == begin && entry.end == begin + sizes[index]; });
            Require(found != ranges.end(), "a mirrored heap range is missing from the BDA table");
            const auto last = access.addressBytes(found->deviceAddress)[sizes[index] - 1];
            leased.WriteBack();
            return last;
        };
        sweep();
        const auto before = MirrorCounters();
        change({0}, {});
        Require(mirrored(0) == std::byte{0x21}, "the first heap range was not mirrored");
        const auto one = MirrorCounters();
        access.limitMemory(large - 1);
        change({1}, {0});
        Require(mirrored(1) == std::byte{0x22}, "a heap mirror past APS5_HEAP_MIRROR_MIB was not made in the memory of the expired mirror");
        const auto swept = MirrorCounters();
        Require(swept.heapMirrors == one.heapMirrors && swept.heapBytes == one.heapBytes, "the expired heap mirror was not swept before the allocation");
        access.limitMemory(small - 1);
        change({2}, {});
        char expected[64];
        std::snprintf(expected, sizeof(expected), "heap mirror of 0x%llx+0x%llx: ", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(heaps[2])), static_cast<unsigned long long>(small));
        const auto attempts = access.allocationAttempts();
        bool refused = false;
        try {
            mirrored(2);
        } catch (const std::runtime_error& error) {
            const std::string what = error.what();
            refused = what.find(expected) != std::string::npos && what.find("Vulkan result -2") != std::string::npos;
        }
        Require(refused, "a heap mirror the memory cannot hold did not fail its build with the Vulkan result");
        Require(access.allocationAttempts() == attempts + 1, "a refused heap mirror was allocated again");
        Require(MirrorCounters().heapMirrors == swept.heapMirrors && MirrorCounters().heapBytes == swept.heapBytes, "a refused heap mirror was registered");
        access.limitMemory(std::nullopt);
        Require(mirrored(2) == std::byte{0x23} && MirrorCounters().heapMirrors == swept.heapMirrors + 1, "a heap mirror was not made once the memory was free again");
        change({}, {1, 2});
        sweep();
        Require(MirrorCounters().heapMirrors == before.heapMirrors && MirrorCounters().heapBytes == before.heapBytes, "the heap mirrors outlived their ranges");
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(raw, total);
#endif
        GuestArena::GuestArenaRelease_nid_postfix(raw, total);
    }
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(block, bytes);
#endif
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
}

}

void gpuMappingTests(const Context& context) {
    constexpr std::size_t bytes = 65536;
    auto* cpu = static_cast<std::byte*>(::operator new(bytes, std::align_val_t{bytes}));
    auto* gpu = static_cast<std::byte*>(::operator new(bytes, std::align_val_t{bytes}));
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(cpu, bytes, true, true, false);
        mutation.Add(gpu, bytes, true, true, true);
    }
    {
        GuestBufferMemory leased(context);
        leased.AcquireRegistered();
        leased.Upload(true);
        const auto ranges = leased.AddressRanges();
        const auto has = [&](const std::byte* block) { return std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == reinterpret_cast<std::uintptr_t>(block); }); };
        Require(has(gpu), "a GPU-mapped range is missing from the BDA table");
        Require(!has(cpu), "a range mapped without GPU access is in the BDA table");
        leased.WriteBack();
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(cpu);
        mutation.Remove(gpu);
    }
    ::operator delete(cpu, std::align_val_t{bytes});
    ::operator delete(gpu, std::align_val_t{bytes});
}

void RunBdaResourceTests(const Context& context, const BdaTestAccess& access) {
    alignas(64) std::array<std::uint32_t, 16> guest{};
    guest[0] = 123;
    const auto address = reinterpret_cast<std::uintptr_t>(guest.data());
    GuestBufferMemory memory(context);
    memory.AddWritable(address, sizeof(guest));
    memory.AddWritable(address + 16, 16);
    memory.Upload(true);
    std::uint32_t adjustment = 0;
    const auto first = memory.Descriptor(address, sizeof(guest), adjustment);
    Require(adjustment == 0, "a view at its owner's start is bound off it");
    const auto alias = memory.Descriptor(address + 16, 16, adjustment);
    Require(first.buffer == alias.buffer && alias.offset + adjustment == 16 && alias.range == 16 + adjustment, "aliased guest buffers have different owners");
    const auto ranges = memory.AddressRanges();
    Require(ranges.size() == 1 && ranges[0].begin == address && ranges[0].end == address + sizeof(guest), "incorrect BDA range bounds");
    Require(ranges[0].deviceAddress != 0 && ranges[0].permissions == (ShaderRecompiler::BdaAbi::Read | ShaderRecompiler::BdaAbi::Write), "incorrect BDA address or permissions");
    std::uint32_t changed = 321;
    std::memcpy(access.bytes(alias.buffer).data() + alias.offset, &changed, sizeof(changed));
    memory.WriteBack();
    Require(guest[4] == changed, "aliased GPU write was not published");
    reject([&] { memory.WriteBack(); }, "cannot be committed twice");
    reject([&] { memory.AddWritable(address, sizeof(guest)); }, "frozen");
    GuestBufferMemory overflow(context);
    const std::array<std::byte, 8> source{};
    reject([&] { overflow.AddSnapshot({std::numeric_limits<std::uint64_t>::max() - 3, source}); }, "overflow");

    const std::array<GuestMemorySnapshot, 1> snapshots{{{0x7fff12340000ULL, source}}};
    ShaderRecompiler::RecompileResult shader;
    shader.bindings = {binding(Role::BdaPagetable, 4), binding(Role::FaultBuffer, 5)};
    CompiledShader compiled{ShaderRecompiler::ShaderStage::Compute, &shader, 0};
    reject([&] { ShaderResources resources(context, compiled, snapshots); }, "ABI version");
    shader.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    auto disabled = context;
    disabled.bufferDeviceAddress = false;
    reject([&] { ShaderResources resources(disabled, compiled, snapshots); }, "not enabled");
    {
        // A rect-list fault buffer must not require BDA or consume guest snapshots.
        ShaderRecompiler::RecompileResult control;
        control.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
        control.bindings = {binding(Role::FaultBuffer, 5)};
        auto writable = binding(Role::GuestBuffers, 6);
        writable.guestDescriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32) & 0xffffu, sizeof(guest), 0x31000000u};
        control.bindings.push_back(writable);
        const std::array<CompiledShader, 1> stages{{{ShaderRecompiler::ShaderStage::TessellationControl, &control, 0}}};
        const std::array<GuestMemorySnapshot, 1> unusedSnapshots{{{0, source}}};
        ShaderResources resources(disabled, stages, ColorTarget{}, 0, 0, 0, unusedSnapshots);
        Require(ShaderResources::NeverReusable(stages) && !resources.Reusable(), "a fault-buffer build is reusable");
        const auto fault = access.bytes(access.descriptor(5).buffer);
        for (const auto byte : fault) Require(byte == std::byte{}, "rect-list fault buffer was not initialized");
        const ShaderRecompiler::BdaAbi::Fault report{ShaderRecompiler::BdaAbi::FaultState::Ready, ShaderRecompiler::BdaAbi::FaultReason::InvalidRectangle, 0, 0, 0, 0, 0};
        std::memcpy(fault.data(), &report, sizeof(report));
        reject([&] { resources.WriteBack(); }, "rect-list requires");
        std::memset(fault.data(), 0, fault.size());
        changed = 456;
        std::memcpy(access.bytes(access.descriptor(6).buffer).data() + sizeof(std::uint32_t), &changed, sizeof(changed));
        resources.WriteBack();
        Require(guest[1] == changed, "rect-list fault-only path lost guest buffer writes");
    }
    {
        ShaderResources resources(context, compiled, snapshots);
        Require(ShaderResources::NeverReusable(std::span<const CompiledShader>(&compiled, 1)) && !resources.Reusable(), "an address-based build is reusable");
        ShaderRecompiler::RecompileResult plain;
        plain.bindings = {binding(Role::GuestBuffers, 6)};
        const CompiledShader plainStage{ShaderRecompiler::ShaderStage::Compute, &plain, 0};
        Require(!ShaderResources::NeverReusable(std::span<const CompiledShader>(&plainStage, 1)), "a build without address tables or a fault buffer is refused the resource cache");
        const auto table = access.bytes(access.descriptor(4).buffer);
        ShaderRecompiler::BdaAbi::Header header{};
        ShaderRecompiler::BdaAbi::Range range{};
        std::memcpy(&header, table.data(), sizeof(header));
        Require(header.version == ShaderRecompiler::BdaAbi::Version && header.count == 1 && header.entryBytes == sizeof(range), "BDA header layout mismatch");
        std::memcpy(&range, table.data() + sizeof(header), sizeof(range));
        Require(range.begin == snapshots[0].address && range.end == range.begin + source.size(), "64-bit guest address was truncated");
        const auto fault = access.bytes(access.descriptor(5).buffer);
        for (const auto byte : fault) Require(byte == std::byte{}, "fault buffer was not initialized");
        const ShaderRecompiler::BdaAbi::Fault denied{ShaderRecompiler::BdaAbi::FaultState::Ready, ShaderRecompiler::BdaAbi::FaultReason::Permission, snapshots[0].address + 4, 4, 0, 0x88, 0};
        std::memcpy(fault.data(), &denied, sizeof(denied));
        reject([&] { resources.WriteBack(); }, "read-only in the BDA table");
        std::memset(fault.data(), 0, fault.size());
        resources.WriteBack();
    }
    {
        auto writable = binding(Role::GuestBuffers, 6);
        writable.guestDescriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32) & 0xffffu, sizeof(guest), 0x31000000u};
        shader.bindings.push_back(writable);
        ShaderResources resources(context, compiled);
        changed = 999;
        std::memcpy(access.bytes(access.descriptor(6).buffer).data(), &changed, sizeof(changed));
        const ShaderRecompiler::BdaAbi::Fault report{ShaderRecompiler::BdaAbi::FaultState::Ready, ShaderRecompiler::BdaAbi::FaultReason::Unmapped, 0x7fff99880000ULL, 4, 0, 0x44, 0};
        std::memcpy(access.bytes(access.descriptor(5).buffer).data(), &report, sizeof(report));
        reject([&] { resources.WriteBack(); }, "BDA access failed");
        auto invalidRectangle = report;
        invalidRectangle.reason = ShaderRecompiler::BdaAbi::FaultReason::InvalidRectangle;
        std::memcpy(access.bytes(access.descriptor(5).buffer).data(), &invalidRectangle, sizeof(invalidRectangle));
        reject([&] { resources.WriteBack(); }, "rect-list requires");
        Require(guest[0] == 123, "failed GPU command published writes");
    }
    {
        auto aligned = context;
        aligned.limits.minStorageBufferOffsetAlignment = 16;
        GuestBufferMemory unaligned(aligned);
        unaligned.AddWritable(address, sizeof(guest));
        unaligned.Upload(true);
        std::uint32_t adjustment = 0;
        const auto view = unaligned.Descriptor(address + 4, 4, adjustment);
        Require(adjustment == 4 && view.offset == 0 && view.range == 8, "a view off the offset alignment binds from below it");
        reject([&] { unaligned.Descriptor(address + sizeof(guest), 4, adjustment); }, "exceeds its GPU owner");
        const auto odd = unaligned.Descriptor(address + 2, 6, adjustment);
        Require(adjustment == 2 && odd.offset == 0 && odd.range == 8, "a view off a DWORD boundary does not bind the DWORDs around it");
        const auto late = unaligned.Descriptor(address + 0x13, 8, adjustment);
        Require(adjustment == 3 && late.offset == 16 && late.range == 12, "a view off a DWORD boundary does not bind from the offset alignment below it");
        GuestBufferMemory lone(aligned);
        lone.AddReadable(address + 6, 5);
        lone.Upload(true);
        const auto copied = lone.Descriptor(address + 6, 5, adjustment);
        Require(adjustment == 2 && copied.offset == 0 && copied.range == 8, "a lone view off a DWORD boundary is not copied from the DWORD below it");
        Require(std::memcmp(access.bytes(copied.buffer).data(), reinterpret_cast<const void*>(address + 4), 8) == 0, "a lone view off a DWORD boundary copied other bytes");
        GuestBufferMemory tail(aligned);
        tail.AddWritable(address, 62);
        tail.Upload(true);
        reject([&] { tail.Descriptor(address + 58, 4, adjustment); }, "exceeds its GPU owner");
    }
    {
        // The cached address space: a second build in an unchanged registry takes the first one's
        // space, and a guest free of a range pinned only by the cache goes through the pin waiter's
        // drop (no GPU work to wait for) and empties the registry of it.
        void* block = GuestHeap::GuestHeapAllocate_nid_postfix(64);
        const auto blockAddress = reinterpret_cast<std::uintptr_t>(block);
        std::memset(block, 0x5a, 64);
        const auto registered = [&] {
            const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
            return std::any_of(lease.begin(), lease.end(), [&](const auto& range) { return range->address == blockAddress; });
        };
        Require(registered(), "guest heap block is not registered");
        const auto before = AddressSpaceCounters();
        for (int build = 0; build < 2; ++build) {
            GuestBufferMemory leased(context);
            leased.AcquireRegistered();
            Require(leased.HoldsLease(), "address-based build holds no lease");
            leased.Upload(true);
            std::uint32_t adjustment = 0;
            const auto view = leased.Descriptor(blockAddress, 64, adjustment);
            Require(view.range == 64 + adjustment, "leased block has no descriptor");
            const auto ranges = leased.AddressRanges();
            Require(std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) { return range.begin == blockAddress && range.end == blockAddress + 64; }), "leased block is missing from the BDA table");
            leased.WriteBack();
            Require(!leased.HoldsLease(), "write-back kept the lease");
        }
        const auto after = AddressSpaceCounters();
        if (after.enabled) Require(after.hits == before.hits + 1 && after.rebuiltFirst + after.rebuiltGeneration + after.rebuiltWaiterDrop + after.rebuiltEpoch + after.rebuiltDevice == before.rebuiltFirst + before.rebuiltGeneration + before.rebuiltWaiterDrop + before.rebuiltEpoch + before.rebuiltDevice + 1, "second build did not take the cached address space");
        GuestHeap::GuestHeapFree_nid_postfix(block);
        Require(!registered(), "freed guest heap block remains registered");
        const auto dropped = AddressSpaceCounters();
        if (dropped.enabled) Require(dropped.waiterDrops == after.waiterDrops + 1 && LeaseCounters().cacheDrops == dropped.waiterDrops, "the free did not drop the cached address space");
    }
    importCrossingTests(context, access);
    heapMirrorTests(context, access);
    importedHeapMirrorTests(context, access);
    importWatchTests();
    importedFreshTests(context);
    gpuMappingTests(context);
    Require(AddressCopyOverflow({{0x1000, 0x3000, 0x2000, "uncommitted pages"}}, 0x2000).empty(), "copies within the limit were refused");
    const auto copies = AddressCopyOverflow({{0x1000, 0x2000, 0x1000, "not mirrored"}, {0x10000, 0x30000, 0x18000, "uncommitted pages"}}, 0x2000);
    Require(!copies.empty() && copies.find("0x10000+0x20000 (0.1 MiB committed, uncommitted pages)") < copies.find("0x1000+0x1000"), "the copy limit does not name the largest copy first");
}
