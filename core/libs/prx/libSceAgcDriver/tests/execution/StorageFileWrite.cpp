#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

class FileStream;

namespace {

using AgcDriver::Graphics::Require;
constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 120;
constexpr std::size_t SurfaceBytes = 0x20000;
constexpr std::size_t SourceOffset = 13;
constexpr std::size_t TransferBytes = SurfaceBytes - 37;
constexpr std::int64_t FileOffset = 17;
alignas(256) constexpr std::array<std::uint32_t, 11> WriteCode{
    0xd7460004u, 0x0401060eu, 0xd7460005u, 0x0405060fu, 0x7e100d04u, 0x7e120d05u,
    0x7e140c0du, 0x7e1602f2u, 0xf0200f08u, 0x00000804u, 0xbf810000u,
};

class GuestSurface {
public:
    GuestSurface() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, SurfaceBytes));
        GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "file write: cannot allocate the guest surface");
        const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
        const auto aligned = (begin + SurfaceBytes - 1) & ~(std::uintptr_t{SurfaceBytes - 1});
        if (aligned != begin) munmap(mapped, aligned - begin);
        const auto tail = begin + SurfaceBytes * 2 - aligned - SurfaceBytes;
        if (tail != 0) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), tail);
        block = reinterpret_cast<std::uint8_t*>(aligned);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
#endif
        GuestAllocations::Mutation mutation;
        mutation.Add(block, SurfaceBytes, true, true, true);
    }

    ~GuestSurface() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(block);
        }
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, SurfaceBytes);
#else
        munmap(block, SurfaceBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, SurfaceBytes);
#endif
    }

    GuestSurface(const GuestSurface&) = delete;
    GuestSurface& operator=(const GuestSurface&) = delete;
    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

void Produce(std::uint8_t* texels) {
    std::memset(texels, 0, SurfaceBytes);
    const auto address = reinterpret_cast<std::uintptr_t>(texels);
    const std::array<std::uint32_t, 14> data{
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (71u << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (0x09u << 20u) | (9u << 28u),
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 7u,
    };
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{8, 8, 1};
    registers(0x207, shape);
    const auto code = reinterpret_cast<std::uintptr_t>(WriteCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (7u << 7u) | (1u << 11u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, Width / 8u, Height / 8u, 1u, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverSuspendPoint_nid_postfix();
    AgcDriverWaitIdle_nid_postfix();
    Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "file write: the GPU producer left no pending image");
}

class OutputFile {
public:
    OutputFile() : name("anyps5-storage-file-write-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin") {}
    ~OutputFile() {
        std::error_code error;
        std::filesystem::remove(name, error);
    }
    const std::string& Name() const { return name; }
    int Open() const {
        const auto descriptor = sceKernelOpen(name.c_str(), SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, 0600);
        Require(descriptor >= 0, "file write: cannot open the output");
        return descriptor;
    }
    std::vector<std::byte> Read(std::size_t offset) const {
        std::ifstream file(name, std::ios::binary | std::ios::ate);
        Require(file.tellg() == static_cast<std::streamoff>(offset + TransferBytes), "file write: incorrect output length");
        file.seekg(static_cast<std::streamoff>(offset));
        std::vector<std::byte> result(TransferBytes);
        Require(static_cast<bool>(file.read(reinterpret_cast<char*>(result.data()), result.size())), "file write: short output readback");
        return result;
    }

private:
    std::string name;
};

bool Pending(GuestSurface& surface) {
    return AgcDriver::Graphics::StorageTexture::FindPending(reinterpret_cast<std::uintptr_t>(surface.Data()), SurfaceBytes) != nullptr;
}

std::unique_lock<AgcDriver::GuestMemory::GpuMutexType> ProduceHeld(GuestSurface& surface, GuestSurface& unrelated) {
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (!Pending(unrelated)) Produce(unrelated.Data());
        Produce(surface.Data());
        std::unique_lock hold(AgcDriver::GuestMemory::GpuMutex());
        if (Pending(surface) && Pending(unrelated)) return hold;
    }
    throw std::runtime_error("file write: the queue-idle store kept taking the pending images");
}

enum class Api { Write, PosixWrite, CrtWrite, Pwrite, PosixPwrite, Writev, PosixWritev, Pwritev, PosixPwritev, Aio, AioMultiple, Fwrite };

bool CheckWrite(Api api, const char* name, GuestSurface& surface, GuestSurface& unrelated) {
    const auto hold = ProduceHeld(surface, unrelated);
    OutputFile file;
    auto* source = surface.Data() + SourceOffset;
    const std::array<KernelIovec, 4> vectors{{{source, 111}, {nullptr, 0}, {source + 111, 65537}, {source + 65648, TransferBytes - 65648}}};
    const bool positioned = api == Api::Pwrite || api == Api::PosixPwrite || api == Api::Pwritev || api == Api::PosixPwritev || api == Api::Aio || api == Api::AioMultiple;
    if (api == Api::Fwrite) {
        auto* stream = fopen_nid_postfix(file.Name().c_str(), "wb");
        Require(stream != nullptr, "file write: cannot open the stdio output");
        Require(fwrite_nid_postfix(source, 0, 7, stream) == 0, "file write: zero-sized fwrite failed");
        Require(fwrite_nid_postfix(source, 5, 0, stream) == 0, "file write: zero-count fwrite failed");
        bool overflow = false;
        try { fwrite_nid_postfix(source, std::numeric_limits<std::size_t>::max(), 2, stream); }
        catch (const std::overflow_error&) { overflow = true; }
        Require(overflow, "file write: fwrite byte count overflow was accepted");
        Require(AgcDriver::Graphics::StorageTexture::FindPending(reinterpret_cast<std::uintptr_t>(surface.Data()), SurfaceBytes) != nullptr, "file write: an empty or overflowing fwrite flushed its source");
        const auto written = fwrite_nid_postfix(source, 5, TransferBytes / 5, stream);
        Require(fclose_nid_postfix(stream) == 0, "file write: cannot close the stdio output");
        Require(written == TransferBytes / 5, "file write: fwrite returned a short element count");
    } else {
        const auto descriptor = file.Open();
        if (api == Api::Write) {
            Require(sceKernelWrite(descriptor, source, 0) == 0, "file write: zero length failed");
            Require(sceKernelPwrite(descriptor, nullptr, 0, 0) == 0, "file write: zero-length pwrite failed");
            Require(sceKernelWritev(descriptor, nullptr, 0) == 0, "file write: empty writev failed");
            Require(sceKernelPwrite(descriptor, source, 1, -1) == static_cast<std::int64_t>(static_cast<int>(0x80020016u)), "file write: negative offset was accepted");
            Require(AgcDriver::Graphics::StorageTexture::FindPending(reinterpret_cast<std::uintptr_t>(surface.Data()), SurfaceBytes) != nullptr, "file write: an empty or invalid write flushed its source");
            const char cpu = 'x';
            Require(sceKernelWrite(descriptor, &cpu, 1) == 1, "file write: unrelated CPU write failed");
            Require(AgcDriver::Graphics::StorageTexture::FindPending(reinterpret_cast<std::uintptr_t>(surface.Data()), SurfaceBytes) != nullptr, "file write: an unrelated source flushed the GPU image");
            Require(sceKernelClose(descriptor) == 0, "file write: cannot close the scope-control output");
        }
        const auto output = api == Api::Write ? file.Open() : descriptor;
        std::int64_t written = -1;
        switch (api) {
        case Api::Write: written = sceKernelWrite(output, source, TransferBytes); break;
        case Api::PosixWrite: written = write_nid_postfix(output, reinterpret_cast<const char*>(source), TransferBytes); break;
        case Api::CrtWrite: written = _write_nid_postfix(output, source, TransferBytes); break;
        case Api::Pwrite: written = sceKernelPwrite(output, source, TransferBytes, FileOffset); break;
        case Api::PosixPwrite: written = pwrite_nid_disambig1_nid_postfix(output, source, TransferBytes, FileOffset); break;
        case Api::Writev: written = sceKernelWritev(output, vectors.data(), vectors.size()); break;
        case Api::PosixWritev: written = writev_nid_postfix(output, vectors.data(), vectors.size()); break;
        case Api::Pwritev: written = sceKernelPwritev(output, vectors.data(), vectors.size(), FileOffset); break;
        case Api::PosixPwritev: written = pwritev_nid_postfix(output, vectors.data(), vectors.size(), FileOffset); break;
        case Api::Aio:
        case Api::AioMultiple: {
            KernelAioResult result{};
            KernelAioRwRequest request{FileOffset, TransferBytes, source, &result, output};
            std::int32_t id = 0;
            Require((api == Api::Aio ? sceKernelAioSubmitWriteCommands(&request, 1, 1, &id) : sceKernelAioSubmitWriteCommandsMultiple(&request, 1, 1, &id)) == 0, "file write: AIO submission failed");
            std::int32_t removed = -1;
            Require(sceKernelAioDeleteRequest(id, &removed) == 0 && removed == 0, "file write: AIO deletion failed");
            Require(result.state == 3, "file write: AIO did not complete");
            written = result.return_value;
            break;
        }
        case Api::Fwrite: break;
        }
        Require(sceKernelClose(output) == 0, "file write: cannot close the output");
        Require(written == TransferBytes, "file write: the transfer returned a short byte count");
    }
    const auto stored = file.Read(positioned ? FileOffset : 0);
    std::vector<std::byte> expected(TransferBytes);
    AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(source), expected);
    Require(std::any_of(expected.begin(), expected.end(), [](std::byte value) { return value != std::byte{}; }), "file write: the GPU producer wrote only zeros");
    std::size_t differences = 0;
    for (std::size_t index = 0; index < stored.size(); ++index) differences += stored[index] != expected[index];
    std::printf("%s: %zu incorrect file bytes out of %zu\n", name, differences, stored.size());
    Require(Pending(unrelated), "file write: an unrelated pending GPU image was flushed");
    return differences == 0;
}

void CheckConcurrentFlushWait(GuestSurface& surface) {
    Produce(surface.Data());
    const auto address = reinterpret_cast<std::uintptr_t>(surface.Data());
    Require(AgcDriver::Graphics::StorageTexture::FindPending(address, SurfaceBytes) != nullptr, "file write: concurrent flush setup has no pending image");
    std::mutex mutex;
    std::condition_variable changed;
    bool gpuLocked = false;
    bool releaseGpu = false;
    std::thread holder([&] {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        {
            std::lock_guard lock(mutex);
            gpuLocked = true;
        }
        changed.notify_all();
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return releaseGpu; });
    });
    {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return gpuLocked; });
    }
    std::exception_ptr flushFailure;
    std::thread flusher([&] {
        try { AgcDriver::Graphics::StorageTexture::FlushPending(address, SurfaceBytes, nullptr, "concurrent flush test"); }
        catch (...) { flushFailure = std::current_exception(); }
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (AgcDriver::Graphics::StorageTexture::FindPending(address, SurfaceBytes) != nullptr && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    const bool taken = AgcDriver::Graphics::StorageTexture::FindPending(address, SurfaceBytes) == nullptr;
    std::vector<std::byte> seen(SurfaceBytes);
    std::atomic<bool> readDone = false;
    std::exception_ptr readFailure;
    std::thread reader;
    if (taken) {
        reader = std::thread([&] {
            try {
                AgcDriver::Graphics::StorageTexture::FlushPending(address, SurfaceBytes, nullptr, "concurrent read test");
                AgcDriver::GuestMemory::Read(address, seen);
            } catch (...) {
                readFailure = std::current_exception();
            }
            readDone.store(true, std::memory_order_release);
            changed.notify_all();
        });
        std::unique_lock lock(mutex);
        changed.wait_for(lock, std::chrono::milliseconds(50), [&] { return readDone.load(std::memory_order_acquire); });
        releaseGpu = true;
    } else {
        std::lock_guard lock(mutex);
        releaseGpu = true;
    }
    changed.notify_all();
    holder.join();
    flusher.join();
    if (reader.joinable()) reader.join();
    Require(taken, "file write: concurrent flush did not take the pending image");
    if (flushFailure != nullptr) std::rethrow_exception(flushFailure);
    if (readFailure != nullptr) std::rethrow_exception(readFailure);
    Require(std::any_of(seen.begin(), seen.end(), [](std::byte value) { return value != std::byte{}; }), "file write: concurrent guest read returned before an overlapping image was stored");
}

void CheckShutdownAndGuard() {
    OutputFile file;
    std::vector<std::byte> source(TransferBytes, std::byte{0x5a});
    AgcDriver::GuestMemory::SetFlushHook(nullptr);
    const auto descriptor = file.Open();
    AgcDriver::GuestMemory::SetReadGuard([](std::uint64_t, std::size_t) { throw std::runtime_error("file source read guard"); });
    bool refused = false;
    try { sceKernelWrite(descriptor, source.data(), source.size()); }
    catch (const std::runtime_error& error) { refused = std::string(error.what()) == "file source read guard"; }
    AgcDriver::GuestMemory::SetReadGuard(nullptr);
    Require(refused && std::filesystem::file_size(file.Name()) == 0, "file write: source read guard was bypassed before the file changed");
    Require(sceKernelWrite(descriptor, source.data(), source.size()) == static_cast<std::int64_t>(source.size()), "file write: ordinary source failed after shutdown and hook reset");
    Require(sceKernelClose(descriptor) == 0, "file write: cannot close the shutdown output");
    Require(file.Read(0) == source, "file write: ordinary output changed after shutdown and hook reset");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        GuestSurface surface;
        GuestSurface unrelated;
        CheckConcurrentFlushWait(surface);
        Produce(unrelated.Data());
        bool correct = true;
        const std::array<std::pair<Api, const char*>, 12> cases{{
            {Api::Write, "sceKernelWrite"}, {Api::PosixWrite, "write"}, {Api::CrtWrite, "_write"},
            {Api::Pwrite, "sceKernelPwrite"}, {Api::PosixPwrite, "pwrite"},
            {Api::Writev, "sceKernelWritev"}, {Api::PosixWritev, "writev"},
            {Api::Pwritev, "sceKernelPwritev"}, {Api::PosixPwritev, "pwritev"},
            {Api::Aio, "AIO"}, {Api::AioMultiple, "AIO multiple"}, {Api::Fwrite, "fwrite"},
        }};
        for (const auto& [api, name] : cases) {
            correct = CheckWrite(api, name, surface, unrelated) && correct;
        }
        Require(correct, "file write: pending GPU results were not stored before the host read");
        std::vector<std::byte> unused(SurfaceBytes);
        AgcDriver::GuestMemory::Read(reinterpret_cast<std::uintptr_t>(unrelated.Data()), unused);
        AgcDriverShutdown_nid_postfix();
        CheckShutdownAndGuard();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
