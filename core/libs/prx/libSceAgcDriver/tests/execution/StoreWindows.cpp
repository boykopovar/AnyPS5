#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#include <SDL_loadso.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t OutputBytes = Lanes * 4u;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t TableOffset = 48;
constexpr std::uint32_t TableRecords = 32;

alignas(256) constexpr std::array<std::uint32_t, 12> StoreCode{0x7e080500u, 0x9385ff04u, 0x00050010u, 0x9885b005u, 0xf4080200u, 0x0a000000u, 0x34020082u, 0xbf8cc07fu, 0xe0701000u, 0x80020001u, 0xbf810000u, 0xbf810000u};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header StoreHeader{};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        if (block == nullptr) throw std::runtime_error("store windows: cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block + offset)); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }
    std::uint32_t* Words(std::size_t offset) const { return reinterpret_cast<std::uint32_t*>(block + offset); }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> RawDescriptor(std::uint64_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

void Enqueue(std::vector<std::uint32_t>& commands) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    if (sceAgcDriverSubmitDcb(&packet) != 0) throw std::runtime_error("store windows: the submission failed");
}

class SubmitHold {
public:
    SubmitHold() {
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            recorder = AgcDriver::Graphics::Recorder::Active();
        }
        if (recorder == nullptr) throw std::runtime_error("store windows: no active recorder to hold a batch on the GPU");
        device = recorder->Device();
#ifdef _WIN32
        constexpr const char* libraryName = "vulkan-1.dll";
#elif defined(__APPLE__)
        constexpr const char* libraryName = "libvulkan.1.dylib";
#else
        constexpr const char* libraryName = "libvulkan.so.1";
#endif
        library.reset(SDL_LoadObject(libraryName));
        if (library == nullptr) throw std::runtime_error("store windows: cannot load Vulkan to hold a batch");
        resolve = reinterpret_cast<PFN_vkGetDeviceProcAddr>(SDL_LoadFunction(library.get(), "vkGetDeviceProcAddr"));
        if (resolve == nullptr) throw std::runtime_error("store windows: cannot resolve vkGetDeviceProcAddr");
        const auto createSemaphore = function<PFN_vkCreateSemaphore>("vkCreateSemaphore");
        destroySemaphore = function<PFN_vkDestroySemaphore>("vkDestroySemaphore");
        signalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(resolve(device, "vkSignalSemaphoreKHR"));
        if (signalSemaphore == nullptr) signalSemaphore = function<PFN_vkSignalSemaphore>("vkSignalSemaphore");
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type};
        if (createSemaphore(device, &info, nullptr, &semaphore) != VK_SUCCESS) throw std::runtime_error("store windows: cannot create the batch hold");
    }

    SubmitHold(const SubmitHold&) = delete;
    SubmitHold& operator=(const SubmitHold&) = delete;

    ~SubmitHold() {
        Release();
        try {
            AgcDriverWaitIdle_nid_postfix();
        } catch (...) {
        }
        destroySemaphore(device, semaphore, nullptr);
    }

    void Arm() {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        recorder->HoldNextSubmit(semaphore, 1);
        first = recorder->OpenSerial();
    }

    void AwaitHeldSubmission() const {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
                if (recorder->Submissions() >= first && !recorder->Recording()) return;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        throw std::runtime_error("store windows: the storing submission was not submitted to the GPU");
    }

    void Finish() {
        Release();
        if (releaseResult != VK_SUCCESS) throw std::runtime_error("store windows: cannot release the held batch");
    }

private:
    template <typename TFunction>
    TFunction function(const char* name) const {
        const auto found = reinterpret_cast<TFunction>(resolve(device, name));
        if (found == nullptr) throw std::runtime_error(std::string("store windows: cannot resolve ") + name);
        return found;
    }

    void Release() {
        if (released) return;
        released = true;
        VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
        info.semaphore = semaphore;
        info.value = 1;
        releaseResult = signalSemaphore(device, &info);
    }

    AgcDriver::Graphics::Recorder* recorder = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    std::unique_ptr<void, decltype(&SDL_UnloadObject)> library{nullptr, &SDL_UnloadObject};
    PFN_vkGetDeviceProcAddr resolve = nullptr;
    PFN_vkDestroySemaphore destroySemaphore = nullptr;
    PFN_vkSignalSemaphore signalSemaphore = nullptr;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    std::uint64_t first = 0;
    bool released = false;
    VkResult releaseResult = VK_NOT_READY;
};

struct Pending {
    bool output = false;
    bool table = false;
    bool beside = false;
};

Pending HeldStore(const GuestBlock& table, const GuestBlock& output, const GuestBlock& beside) {
    const auto stored = RawDescriptor(output.At(0), OutputBytes);
    for (std::uint32_t record = 0; record < TableRecords; ++record) std::memcpy(table.Bytes(TableOffset + record * 16u), stored.data(), sizeof(stored));
    std::vector<std::uint32_t> commands;
    const auto code = reinterpret_cast<std::uintptr_t>(StoreCode.data());
    Append(commands, 0x76, {0x20c, static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)});
    Append(commands, 0x76, {0x207, Lanes, 1u, 1u});
    Append(commands, 0x76, {0x213, 2u << 1u});
    Append(commands, 0x76, {0x240, static_cast<std::uint32_t>(table.At(0)), static_cast<std::uint32_t>(table.At(0) >> 32u)});
    Append(commands, 0x15, {1u, 1u, 1u, 0x8041u});
    SubmitHold hold;
    hold.Arm();
    Enqueue(commands);
    hold.AwaitHeldSubmission();
    Pending pending;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const auto* recorder = AgcDriver::Graphics::Recorder::Active();
        if (recorder == nullptr) throw std::runtime_error("store windows: the recorder went away while the store was held");
        pending.output = recorder->PendingWriteOverlaps(output.At(0), OutputBytes);
        pending.table = recorder->PendingWriteOverlaps(table.At(0), BlockBytes);
        pending.beside = recorder->PendingWriteOverlaps(beside.At(0), BlockBytes);
    }
    hold.Finish();
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(output.At(0)), OutputBytes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        if (*output.Words(lane * 4u) != lane) throw std::runtime_error("store windows: lane " + std::to_string(lane) + " stored 0x" + std::to_string(*output.Words(lane * 4u)) + " through the table's V#");
    }
    return pending;
}

}

int main(int argc, char** argv) {
    try {
        const bool whole = argc == 2 && std::string_view(argv[1]) == "--whole";
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        StoreHeader.shader.file_header = 0x34333231u;
        StoreHeader.shader.version = 0x18u;
        StoreHeader.shader.header_size = sizeof(Header);
        StoreHeader.shader.shader_size = static_cast<std::uint32_t>(sizeof(StoreCode));
        StoreHeader.shader.code = StoreCode.data();
        StoreHeader.shader.type = 0;
        StoreHeader.shader.user_data = nullptr;
        AgcDriverRegisterShader_nid_postfix(&StoreHeader.shader);
        GuestBlock table, output, beside;
        const auto pending = HeldStore(table, output, beside);
        AgcDriverShutdown_nid_postfix();
        if (!pending.output) throw std::runtime_error("store windows: the held store left its V#'s range unpending");
        if (whole && !pending.beside) throw std::runtime_error("store windows: with APS5_NO_STORE_WINDOWS the held store did not note the whole writable reach");
        if (!whole && (pending.beside || pending.table)) throw std::runtime_error(std::string("store windows: the held store noted memory its table's V#s cannot reach (") + (pending.table ? "the table" : "an unrelated block") + ")");
        std::puts(whole ? "store windows: the kill switch notes the whole writable reach" : "store windows: a store through a table's V# leaves only that V#'s range pending");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
