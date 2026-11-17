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
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t BufferBytes = Lanes * 4u;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t TableOffset = 0x300;
constexpr std::size_t NewTableOffset = 0x400;
constexpr std::size_t MovedOffset = 0x800;
constexpr std::size_t ShiftOffset = 0x900;
constexpr std::size_t StillOffset = 0xa00;
constexpr std::size_t ColorBytes = 65536;
constexpr std::uint32_t RenderTargetTileMode = 0x1b;

alignas(256) constexpr std::array<std::uint32_t, 11> TableCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x34020082u, 0xe0301000u, 0x80020201u, 0xbf8c3f70u, 0x4a060481u, 0xe0701000u, 0x80010301u, 0xbf810000u};

alignas(256) std::array<std::uint32_t, 20> ComputeStoreCode{};
alignas(256) std::array<std::uint32_t, 20> DrawStoreCode{};

alignas(256) constexpr std::array<std::uint32_t, 40> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u,
    0xf4080104u, 0xfa000000u, 0xf4080204u, 0xfa000010u, 0xf4080304u, 0xfa000020u,
    0xbf8cc07fu,
    0xe00c2000u, 0x80010a05u, 0xe00c2000u, 0x80020e05u, 0xe00c2000u, 0x80031205u,
    0xbf8c3f70u,
    0x0602030au, 0x0604050bu, 0x0606070cu, 0x0608090du,
    0x0602030eu, 0x0604050fu, 0x06060710u, 0x06080911u,
    0x06020312u, 0x06040513u, 0x06060714u, 0x06080915u,
    0xf80008cfu, 0x04030201u,
    0xbf810000u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 11> PixelRead{0x7e1a0280u, 0xdc308000u, 0x0c02000du, 0xbf8c3f70u, 0x7e10030cu, 0x7e120280u, 0x7e140280u, 0x7e160280u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header ComputeHeader{};
alignas(8) Header ComputeStoreHeader{};
alignas(8) Header DrawStoreHeader{};
alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

void Register(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type, bool userData = true) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = userData ? &header.userData : nullptr;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void BuildStoreCode(std::array<std::uint32_t, 20>& code, const std::array<std::uint32_t, 4>& words) {
    std::size_t at = 0;
    code[at++] = 0x7e1a0280u;
    for (std::uint32_t word = 0; word < 4; ++word) {
        code[at++] = 0x7e1802ffu;
        code[at++] = words[word];
        code[at++] = 0xdc708000u | (word * 4u);
        code[at++] = 0x00020c0du;
    }
    code[at++] = 0xbf810000u;
}

class GuestBlock {
public:
    explicit GuestBlock(bool writable = true) {
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
        if (block == nullptr) throw std::runtime_error("raw pointer store pending: cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, writable, true);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint64_t At(std::size_t offset) const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block + offset)); }
    std::uint8_t* Bytes(std::size_t offset) const { return block + offset; }
    std::uint32_t* Words(std::size_t offset) const { return reinterpret_cast<std::uint32_t*>(block + offset); }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), BufferBytes, 0x31016facu};
}

std::array<std::uint32_t, 4> VertexDescriptor(std::uint64_t address) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), 3u, 0x0104dfacu};
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}
void ComputeProgram(std::vector<std::uint32_t>& commands, const std::uint32_t* code, std::uint32_t userWords, std::uint32_t threads) {
    const auto address = reinterpret_cast<std::uintptr_t>(code);
    Append(commands, 0x76, {0x20c, static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)});
    Append(commands, 0x76, {0x207, threads, 1u, 1u});
    Append(commands, 0x76, {0x213, userWords << 1u});
}

void Dispatch(std::vector<std::uint32_t>& commands) {
    Append(commands, 0x15, {1u, 1u, 1u, 0x8041u});
}

void Enqueue(std::vector<std::uint32_t>& commands, const std::string& what) {
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    if (sceAgcDriverSubmitDcb(&packet) != 0) throw std::runtime_error("raw pointer store pending: " + what + ": the submission failed");
}

void Submit(std::vector<std::uint32_t>& commands, const std::string& what) {
    Enqueue(commands, what);
    AgcDriverWaitIdle_nid_postfix();
}

class SubmitHold {
public:
    SubmitHold() {
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            recorder = AgcDriver::Graphics::Recorder::Active();
        }
        if (recorder == nullptr) throw std::runtime_error("raw pointer store pending: no active recorder to hold a batch on the GPU");
        device = recorder->Device();
#ifdef _WIN32
        constexpr const char* libraryName = "vulkan-1.dll";
#elif defined(__APPLE__)
        constexpr const char* libraryName = "libvulkan.1.dylib";
#else
        constexpr const char* libraryName = "libvulkan.so.1";
#endif
        library.reset(SDL_LoadObject(libraryName));
        if (library == nullptr) throw std::runtime_error("raw pointer store pending: cannot load Vulkan to hold a batch");
        resolve = reinterpret_cast<PFN_vkGetDeviceProcAddr>(SDL_LoadFunction(library.get(), "vkGetDeviceProcAddr"));
        if (resolve == nullptr) throw std::runtime_error("raw pointer store pending: cannot resolve vkGetDeviceProcAddr");
        const auto createSemaphore = function<PFN_vkCreateSemaphore>("vkCreateSemaphore");
        destroySemaphore = function<PFN_vkDestroySemaphore>("vkDestroySemaphore");
        signalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(resolve(device, "vkSignalSemaphoreKHR"));
        if (signalSemaphore == nullptr) signalSemaphore = function<PFN_vkSignalSemaphore>("vkSignalSemaphore");
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type};
        if (createSemaphore(device, &info, nullptr, &semaphore) != VK_SUCCESS) throw std::runtime_error("raw pointer store pending: cannot create the batch hold");
    }

    SubmitHold(const SubmitHold&) = delete;
    SubmitHold& operator=(const SubmitHold&) = delete;

    ~SubmitHold() {
        if (watcher.joinable()) {
            watcher.request_stop();
            watcher.join();
        }
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

    std::uint64_t AwaitHeldSubmission() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
                if (recorder->Submissions() >= first && !recorder->Recording()) return recorder->Submissions();
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        throw std::runtime_error("raw pointer store pending: the storing submission was not submitted to the GPU");
    }

    void ReleaseAfterLaterSubmission(std::uint64_t held) {
        watcher = std::jthread([this, held](std::stop_token stop) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (!stop.stop_requested() && recorder->NewestSubmitted() <= held && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::microseconds(100));
            Release();
        });
    }

    void Finish() {
        if (watcher.joinable()) watcher.join();
        Release();
        if (releaseResult != VK_SUCCESS) throw std::runtime_error("raw pointer store pending: cannot release the held batch");
    }

private:
    template <typename TFunction>
    TFunction function(const char* name) const {
        const auto found = reinterpret_cast<TFunction>(resolve(device, name));
        if (found == nullptr) throw std::runtime_error(std::string("raw pointer store pending: cannot resolve ") + name);
        return found;
    }

    void Release() {
        if (released.exchange(true)) return;
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
    std::jthread watcher;
    std::atomic<bool> released{false};
    VkResult releaseResult = VK_NOT_READY;
};

void SettleOutput(const GuestBlock& block, std::size_t offset, std::size_t bytes) {
    AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(block.At(offset)), bytes);
}

std::vector<std::string> Failures;
std::vector<std::string> ControlFailures;

void Report(const std::string& what, bool good, const std::string& detail, bool control = false) {
    std::printf("%s: %s (%s)\n", what.c_str(), good ? "correct" : "WRONG", detail.c_str());
    if (!good) (control ? ControlFailures : Failures).push_back(what + ": " + detail);
}

std::string OutputText(const GuestBlock& output) {
    char text[96];
    std::snprintf(text, sizeof(text), "lane 0 holds 0x%x, lane %u holds 0x%x", *output.Words(0), Lanes - 1u, *output.Words((Lanes - 1u) * 4u));
    return text;
}

bool OutputIs(const GuestBlock& output, std::uint32_t base) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        if (*output.Words(lane * 4u) != base + lane + 1u) return false;
    }
    return true;
}
void FillInputs(const GuestBlock& first, const GuestBlock& moved) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        *first.Words(lane * 4u) = 0x1000u + lane;
        *moved.Words(lane * 4u) = 0x2000u + lane;
    }
}
void ComputeTableWrittenByDispatch(bool sameSubmission) {
    GuestBlock table, first, moved, output;
    FillInputs(first, moved);
    const auto input = BufferDescriptor(first.At(0));
    std::memcpy(table.Bytes(0), input.data(), sizeof(input));
    const auto stored = BufferDescriptor(output.At(0));
    BuildStoreCode(ComputeStoreCode, BufferDescriptor(moved.At(0)));
    Register(ComputeStoreHeader, ComputeStoreCode.data(), sizeof(ComputeStoreCode), 0, false);
    std::vector<std::uint32_t> commands;
    ComputeProgram(commands, ComputeStoreCode.data(), 4u, 1u);
    Append(commands, 0x76, {0x240, 0u, 0u, static_cast<std::uint32_t>(table.At(0)), static_cast<std::uint32_t>(table.At(0) >> 32u)});
    Dispatch(commands);
    if (!sameSubmission) {
        Submit(commands, "compute table written by a dispatch, first submission");
        SettleOutput(table, 0, 16);
        commands.clear();
    }
    ComputeProgram(commands, TableCode.data(), 8u, Lanes);
    Append(commands, 0x76, {0x240, static_cast<std::uint32_t>(table.At(0)), static_cast<std::uint32_t>(table.At(0) >> 32u), 0u, 0u, stored[0], stored[1], stored[2], stored[3]});
    Dispatch(commands);
    Submit(commands, "compute table written by a dispatch");
    SettleOutput(output, 0, BufferBytes);
    const bool right = OutputIs(output, 0x2000u);
    Report(sameSubmission ? "a compute dispatch whose V# table a dispatch just before it rewrote through a raw pointer, in the same submission" : "control: a compute dispatch whose V# table an earlier submission's dispatch rewrote through a raw pointer", right, OutputText(output), !sameSubmission);
}

void ComputeTableWrittenWhileHeld() {
    GuestBlock table, first, moved, output;
    FillInputs(first, moved);
    const auto input = BufferDescriptor(first.At(0));
    std::memcpy(table.Bytes(0), input.data(), sizeof(input));
    const auto stored = BufferDescriptor(output.At(0));
    BuildStoreCode(ComputeStoreCode, BufferDescriptor(moved.At(0)));
    Register(ComputeStoreHeader, ComputeStoreCode.data(), sizeof(ComputeStoreCode), 0, false);
    std::vector<std::uint32_t> store;
    ComputeProgram(store, ComputeStoreCode.data(), 4u, 1u);
    Append(store, 0x76, {0x240, 0u, 0u, static_cast<std::uint32_t>(table.At(0)), static_cast<std::uint32_t>(table.At(0) >> 32u)});
    Dispatch(store);
    std::vector<std::uint32_t> reader;
    ComputeProgram(reader, TableCode.data(), 8u, Lanes);
    Append(reader, 0x76, {0x240, static_cast<std::uint32_t>(table.At(0)), static_cast<std::uint32_t>(table.At(0) >> 32u), 0u, 0u, stored[0], stored[1], stored[2], stored[3]});
    Dispatch(reader);
    SubmitHold hold;
    hold.Arm();
    Enqueue(store, "compute table written by a dispatch held on the GPU, first submission");
    const auto held = hold.AwaitHeldSubmission();
    hold.ReleaseAfterLaterSubmission(held);
    Enqueue(reader, "compute table written by a dispatch held on the GPU");
    AgcDriverWaitIdle_nid_postfix();
    hold.Finish();
    SettleOutput(output, 0, BufferBytes);
    Report("a compute dispatch recorded while an earlier submission's dispatch, which rewrites its V# table through a raw pointer, is still held on the GPU", OutputIs(output, 0x2000u), OutputText(output));
}

void ReadOnlyRangeNotPending() {
    GuestBlock table, moved;
    GuestBlock readOnly(false);
    const auto input = BufferDescriptor(moved.At(0));
    std::memcpy(table.Bytes(0), input.data(), sizeof(input));
    BuildStoreCode(ComputeStoreCode, BufferDescriptor(moved.At(0)));
    Register(ComputeStoreHeader, ComputeStoreCode.data(), sizeof(ComputeStoreCode), 0, false);
    std::vector<std::uint32_t> store;
    ComputeProgram(store, ComputeStoreCode.data(), 4u, 1u);
    Append(store, 0x76, {0x240, 0u, 0u, static_cast<std::uint32_t>(table.At(0)), static_cast<std::uint32_t>(table.At(0) >> 32u)});
    Dispatch(store);
    SubmitHold hold;
    hold.Arm();
    Enqueue(store, "a raw-pointer store held on the GPU beside a read-only range");
    static_cast<void>(hold.AwaitHeldSubmission());
    bool tableNoted = false;
    bool readOnlyNoted = false;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const auto* recorder = AgcDriver::Graphics::Recorder::Active();
        tableNoted = recorder != nullptr && recorder->PendingWriteOverlaps(table.At(0), 16);
        readOnlyNoted = recorder != nullptr && recorder->PendingWriteOverlaps(readOnly.At(0), BlockBytes);
    }
    hold.Finish();
    AgcDriverWaitIdle_nid_postfix();
    Report("a held dispatch that stores through a raw pointer leaves the writable range it can store into pending", tableNoted, tableNoted ? "the table's range is pending" : "the table's range is not pending");
    Report("a held dispatch that stores through a raw pointer leaves no pending write over a range its address table maps read-only", !readOnlyNoted, readOnlyNoted ? "the read-only range is pending" : "the read-only range is not pending");
}

void DrawRegisters(std::vector<std::uint32_t>& commands, const GuestBlock& target, std::uint64_t pixelData, const std::function<void(std::vector<std::uint32_t>&)>& tableRegisters) {
    const auto color = target.At(0);
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(PixelRead.data());
    const float scale = 32.0f;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000u | (RenderTargetTileMode << 14u)}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(scale)}, {0x110, std::bit_cast<std::uint32_t>(scale)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
        {0x2fe, 0}, {0x302, 0}, {0x306, 0}, {0x30a, 0},
    };
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : std::initializer_list<std::pair<std::uint32_t, std::uint32_t>>{{0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 2u << 1u}, {0x00b, 8u << 1u}, {0x00c, 0}, {0x00d, 0}}) Append(commands, 0x76, {offset, value});
    tableRegisters(commands);
    Append(commands, 0x76, {0x008, static_cast<std::uint32_t>(pixel >> 8u), static_cast<std::uint32_t>(pixel >> 40u)});
    Append(commands, 0x76, {0x00e, static_cast<std::uint32_t>(pixelData), static_cast<std::uint32_t>(pixelData >> 32u)});
    Append(commands, 0x2d, {3, 2});
}

unsigned Brightest(const GuestBlock& target) {
    AgcDriver::GuestMemory::FlushGpuWrites(static_cast<std::uintptr_t>(target.At(0)), ColorBytes);
    unsigned brightest = 0;
    for (std::size_t i = 0; i < ColorBytes; ++i) brightest = std::max(brightest, static_cast<unsigned>(target.Bytes(0)[i]));
    return brightest;
}

void FillTables(const GuestBlock& vertices) {
    const auto write = [&](std::size_t base, std::size_t slot, std::array<std::uint32_t, 4> descriptor) { std::memcpy(vertices.Bytes(base + slot * 16u), descriptor.data(), sizeof(descriptor)); };
    const auto writeVertices = [&](std::size_t offset, float x) {
        for (std::size_t vertex = 0; vertex < 3; ++vertex) {
            const std::array<float, 4> value{x, 0.0f, 0.0f, 0.0f};
            std::memcpy(vertices.Bytes(offset + vertex * 16u), value.data(), sizeof(value));
        }
    };
    writeVertices(MovedOffset, -8.0f);
    writeVertices(ShiftOffset, 8.0f);
    writeVertices(StillOffset, 0.0f);
    write(TableOffset, 0, {});
    write(TableOffset, 1, VertexDescriptor(vertices.At(ShiftOffset)));
    write(TableOffset, 2, VertexDescriptor(vertices.At(StillOffset)));
    write(NewTableOffset, 0, VertexDescriptor(vertices.At(MovedOffset)));
    write(NewTableOffset, 1, VertexDescriptor(vertices.At(ShiftOffset)));
    write(NewTableOffset, 2, VertexDescriptor(vertices.At(StillOffset)));
}
void DrawTableWrittenByDispatch(bool sameSubmission) {
    GuestBlock vertices, data, target;
    FillTables(vertices);
    const float one = 1.0f;
    std::memcpy(data.Bytes(0), &one, sizeof(one));
    const auto table = vertices.At(TableOffset);
    BuildStoreCode(DrawStoreCode, VertexDescriptor(vertices.At(MovedOffset)));
    Register(DrawStoreHeader, DrawStoreCode.data(), sizeof(DrawStoreCode), 0, false);
    std::vector<std::uint32_t> commands;
    ComputeProgram(commands, DrawStoreCode.data(), 4u, 1u);
    Append(commands, 0x76, {0x240, 0u, 0u, static_cast<std::uint32_t>(table), static_cast<std::uint32_t>(table >> 32u)});
    Dispatch(commands);
    if (!sameSubmission) {
        Submit(commands, "draw table written by a dispatch, first submission");
        SettleOutput(vertices, TableOffset, 16);
        commands.clear();
    }
    DrawRegisters(commands, target, data.At(0), [&](std::vector<std::uint32_t>& out) { Append(out, 0x76, {0x8c, static_cast<std::uint32_t>(table), static_cast<std::uint32_t>(table >> 32u)}); });
    Submit(commands, "draw table written by a dispatch");
    const auto brightest = Brightest(target);
    char text[96];
    std::snprintf(text, sizeof(text), "brightest color byte %u, 255 when the V# the dispatch stored was bound", brightest);
    Report(sameSubmission ? "a draw whose V# table a dispatch just before it rewrote through a raw pointer, in the same submission" : "control: a draw whose V# table an earlier submission's dispatch rewrote through a raw pointer", brightest == 255u, text, !sameSubmission);
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Register(ComputeHeader, TableCode.data(), sizeof(TableCode), 0, false);
        Register(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
        Register(PixelHeader, PixelRead.data(), sizeof(PixelRead), 1);
        ComputeTableWrittenByDispatch(false);
        DrawTableWrittenByDispatch(false);
        if (!ControlFailures.empty()) {
            std::string all;
            for (const auto& failure : ControlFailures) all += "\n  " + failure;
            throw std::runtime_error("raw pointer store pending: the controls, which write before the submission or in an earlier one, are wrong, so the test itself is broken:" + all);
        }
        ComputeTableWrittenByDispatch(true);
        DrawTableWrittenByDispatch(true);
        ComputeTableWrittenWhileHeld();
        ReadOnlyRangeNotPending();
        AgcDriverShutdown_nid_postfix();
        if (!Failures.empty()) {
            std::string all;
            for (const auto& failure : Failures) all += "\n  " + failure;
            throw std::runtime_error("raw pointer store pending: memory a packet wrote earlier was not seen by the packet that read it:" + all);
        }
        std::puts("raw pointer store pending: a descriptor table a dispatch stored through a raw pointer is the one the next packet reads");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
