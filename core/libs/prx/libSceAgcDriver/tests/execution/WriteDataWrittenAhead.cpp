#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/BufferCopy.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver::DriverDetail {
struct DriverTestAccess {
    static void PrepareSamplingForRejectedAhead() {
        auto& driver = Driver::Get();
        driver.evidenceValidations.store(1u, std::memory_order_relaxed);
        driver.evidenceReads.store(15u, std::memory_order_relaxed);
    }
};
}

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::size_t BlockBytes = 65536;

struct GuestBlock {
    explicit GuestBlock(std::size_t bytes) : bytes(bytes), watched(AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        if (watched) {
            data = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockBytes));
            GuestArena::GuestArenaCommit_nid_postfix(data, bytes, PAGE_READWRITE, bytes);
        } else {
            data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        if (watched) {
            void* raw = mmap(nullptr, bytes + BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(raw != MAP_FAILED, "cannot map a guest block");
            const auto begin = reinterpret_cast<std::uintptr_t>(raw);
            const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
            if (aligned != begin) munmap(raw, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockBytes) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockBytes - aligned);
            data = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(data, bytes);
        } else {
            data = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, bytes));
        }
#endif
        Require(data != nullptr, "cannot allocate a guest block");
        GuestAllocations::Mutation mutation;
        mutation.Add(data, bytes, true, true, true);
    }
    ~GuestBlock() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(data);
        }
#ifdef _WIN32
        if (watched) {
            GuestArena::GuestArenaReset_nid_postfix(data, bytes);
            GuestArena::GuestArenaRelease_nid_postfix(data, bytes);
        } else {
            VirtualFree(data, 0, MEM_RELEASE);
        }
#else
        if (watched) {
            munmap(data, bytes);
            GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(data, bytes);
        } else {
            std::free(data);
        }
#endif
    }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint32_t* Dwords(std::size_t byteOffset) const { return reinterpret_cast<std::uint32_t*>(data + byteOffset); }
    std::uint8_t* data = nullptr;
    std::size_t bytes = 0;
    bool watched = false;
};

alignas(256) std::array<std::uint32_t, 7> CounterCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x7e000281u, 0xe0c80000u, 0x80020000u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 9> WriterCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x7e000202u, 0x7e020203u, 0x7e040204u, 0xe07c0000u, 0x80020000u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 9> ImageWriterCode{0x7e000280u, 0x7e020280u, 0x7e040208u, 0x7e060209u, 0x7e08020au, 0x7e0a020bu, 0xf0201f08u, 0x00000200u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 4> GdsWriterCode{0x7e000280u, 0xd8360000u, 0x00000000u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 16> StreamCopyCode{0xd7460008u, 0x04010e08u, 0x8f6a8708u, 0xd76d0009u, 0x0401806au, 0xe00c2000u, 0x80000008u, 0xe00c2000u, 0x80000409u, 0xbf8c3f71u, 0xe01c2000u, 0x80010008u, 0xbf8c3f70u, 0xe01c2000u, 0x80010409u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 9> FillKernelCode{0xd7460004u, 0x04010c08u, 0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0xe01c2000u, 0x80000004u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 69> DwordPatternFillCode{
    0xbfa00003u, 0xd7460002u, 0x04010c0au, 0x7da80408u, 0xbf88003fu, 0x7e000c09u, 0xbf070980u, 0x858a807eu,
    0x7e005700u, 0x100000ffu, 0x4f800000u, 0x7e060f00u, 0xd5766a00u, 0x02020609u, 0x7d8a0280u, 0x4c020080u,
    0x02000101u, 0xd56a0001u, 0x00020700u, 0x4c000303u, 0x4a020303u, 0x02000101u, 0xd56a0000u, 0x00020500u,
    0xd5690001u, 0x00020009u, 0x4c060302u, 0x7d860609u, 0x7d8c02f9u, 0x06068c02u, 0x87ea6a0cu, 0x50000080u,
    0xd5286a00u, 0x003200c1u, 0xd5010000u, 0x002a00c1u, 0xd5690000u, 0x00020009u, 0x4c000102u, 0x7d0a0080u,
    0xbe88246au, 0xbf880015u, 0x7d0a0081u, 0xbe8a246au, 0xbf88000cu, 0x7d0a0082u, 0xbeea246au, 0xbf880003u,
    0x7e000207u, 0xe0102000u, 0x80000002u, 0x8afe7e6au, 0xbf880003u, 0x7e000206u, 0xe0102000u, 0x80000002u,
    0xbefe046au, 0x8afe7e0au, 0xbf880003u, 0x7e000205u, 0xe0102000u, 0x80000002u, 0xbefe040au, 0x8afe7e08u,
    0xbf880003u, 0x7e000204u, 0xe0102000u, 0x80000002u, 0xbf810000u};

alignas(256) std::array<std::uint32_t, 4> First{};
alignas(256) std::array<std::uint32_t, 4> Stale{};
alignas(256) std::array<std::uint32_t, 4> Fresh{};
alignas(256) std::array<std::uint32_t, 4> Arguments{1, 1, 1, 0};
alignas(256) std::array<std::uint32_t, 4> FirstDescriptor{};
alignas(256) std::array<std::uint32_t, 4> SecondDescriptor{};
alignas(256) std::array<std::uint32_t, 4> CopySource{};
alignas(256) std::array<std::uint32_t, 4> ArgumentsDescriptor{};
alignas(256) volatile std::uint32_t Gate = 1;
alignas(256) volatile std::uint32_t Done = 0;

std::uint32_t Low(const volatile void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const volatile void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

std::array<std::uint32_t, 4> Descriptor(const void* pointer, std::uint32_t bytes) {
    return {Low(pointer), High(pointer) & 0xffffu, bytes, 0x31016facu};
}

void SetShaderRegister(std::vector<std::uint32_t>& words, std::uint32_t offset, std::uint32_t value) {
    words.insert(words.end(), {0xc0017600u, offset, value});
}

void Program(std::vector<std::uint32_t>& words, const void* code, std::initializer_list<std::uint32_t> userData, std::uint32_t threadsX = 1, bool groupIdX = false) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(code));
    SetShaderRegister(words, 0x20c, static_cast<std::uint32_t>(address >> 8u));
    SetShaderRegister(words, 0x20d, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    SetShaderRegister(words, 0x207, threadsX);
    SetShaderRegister(words, 0x208, 1);
    SetShaderRegister(words, 0x209, 1);
    SetShaderRegister(words, 0x212, 0);
    SetShaderRegister(words, 0x213, (static_cast<std::uint32_t>(userData.size()) << 1u) | (groupIdX ? (1u << 7u) : 0u));
    std::uint32_t offset = 0x240;
    for (const auto value : userData) SetShaderRegister(words, offset++, value);
}

void Count(std::vector<std::uint32_t>& words, const void* descriptor, std::uint32_t threadsX = 1) {
    Program(words, CounterCode.data(), {Low(descriptor), High(descriptor)}, threadsX);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void CountIndirect(std::vector<std::uint32_t>& words, const void* descriptor, const void* arguments) {
    Program(words, CounterCode.data(), {Low(descriptor), High(descriptor)});
    words.insert(words.end(), {0xc0021600u, Low(arguments), High(arguments), 0x8041u});
}

void WriteThreeDwords(std::vector<std::uint32_t>& words, const void* targetDescriptor, std::uint32_t d0, std::uint32_t d1, std::uint32_t d2) {
    Program(words, WriterCode.data(), {Low(targetDescriptor), High(targetDescriptor), d0, d1, d2});
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void ImageWriteDescriptor(std::vector<std::uint32_t>& words, const void* destination, const std::array<std::uint32_t, 4>& pattern) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(destination));
    constexpr std::uint32_t format = 75u;
    constexpr std::uint32_t width = 16u;
    constexpr std::uint32_t height = 1u;
    constexpr std::uint32_t type2d = 9u;
    Program(words, ImageWriterCode.data(), {
        static_cast<std::uint32_t>(address >> 8u),
        (static_cast<std::uint32_t>(address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u),
        0xfacu | (type2d << 28u),
        0u, 0u, 0u, 0u,
        pattern[0], pattern[1], pattern[2], pattern[3]
    });
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void GdsWrite(std::vector<std::uint32_t>& words) {
    Program(words, GdsWriterCode.data(), {});
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void StreamCopyDescriptor(std::vector<std::uint32_t>& words, const void* source, const void* destination) {
    constexpr std::uint32_t format = 0x0004bfacu;
    Program(words, StreamCopyCode.data(), {
        Low(source), (High(source) & 0xffffu) | (16u << 16u), 1u, format,
        Low(destination), (High(destination) & 0xffffu) | (16u << 16u), 1u, format
    }, 64, true);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void FillKernelDescriptor(std::vector<std::uint32_t>& words, const void* destination, const std::array<std::uint32_t, 4>& pattern) {
    constexpr std::uint32_t format = 0x0004bfacu;
    Program(words, FillKernelCode.data(), {
        Low(destination), (High(destination) & 0xffffu) | (16u << 16u), 1u, format,
        pattern[0], pattern[1], pattern[2], pattern[3]
    }, 64, true);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void DwordPatternFillDescriptor(std::vector<std::uint32_t>& words, const void* destination, const std::array<std::uint32_t, 4>& pattern) {
    constexpr std::uint32_t format = 0x00014facu;
    Program(words, DwordPatternFillCode.data(), {
        Low(destination), (High(destination) & 0xffffu) | (4u << 16u), 4u, format,
        pattern[0], pattern[1], pattern[2], pattern[3], 4u, 4u
    }, 64, true);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

template <typename EmitUpdate>
void RunUpdateAheadCase(const std::string& label, std::uint32_t* targetDescriptor, const std::array<std::uint32_t, 4>& stale, EmitUpdate&& emitUpdate, std::uint64_t expectedAdopted = 4, bool exactAdopted = false) {
    First[0] = 0;
    Stale[0] = 0;
    Fresh[0] = 0;
    Done = 0;
    std::memcpy(targetDescriptor, stale.data(), sizeof(stale));
    const auto adoptedBefore = AgcDriverDispatchesAdoptedAhead_nid_postfix();
    std::vector<std::uint32_t> words;
    words.insert(words.end(), {0xc0053c00u, 0x13u, Low(&Gate), High(&Gate), 1u, 0xffffffffu, 0x19u});
    Count(words, FirstDescriptor.data());
    WriteThreeDwords(words, ArgumentsDescriptor.data(), 1u, 1u, 1u);
    emitUpdate(words);
    Count(words, targetDescriptor);
    CountIndirect(words, targetDescriptor, Arguments.data());
    words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(&Done), High(&Done), 1u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitAcb(0x20, &packet) == 0, label + ": compute submit failed");
    AgcDriverWaitIdle_nid_postfix();
    Require(Done == 1, label + ": submission did not finish");
    Require(First[0] == 1, label + ": first dispatch did not run");
    Require(Fresh[0] == 2 && Stale[0] == 0, label + ": dispatches after descriptor update used stale descriptor (stale " + std::to_string(Stale[0]) + ", fresh " + std::to_string(Fresh[0]) + ")");
    const auto adopted = AgcDriverDispatchesAdoptedAhead_nid_postfix() - adoptedBefore;
    if (exactAdopted) {
        Require(adopted == expectedAdopted, label + ": expected exactly " + std::to_string(expectedAdopted) + " dispatches adopted ahead, got " + std::to_string(adopted));
    } else {
        Require(adopted >= expectedAdopted, label + ": expected at least " + std::to_string(expectedAdopted) + " dispatches adopted ahead across pre-update and reopened post-update gates, got " + std::to_string(adopted));
    }
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        GuestBlock imported(BlockBytes);
        std::uint32_t* importedSecondDescriptor = imported.Dwords(0);
        std::uint32_t* importedCopySource = imported.Dwords(256);
        std::uint32_t* importedCopySourceDescriptor = imported.Dwords(512);
        std::uint32_t* importedPolluteDescriptor = imported.Dwords(768);
        std::uint32_t* importedPolluteMetaDescriptor = imported.Dwords(1024);
        const auto stale = Descriptor(Stale.data(), sizeof(Stale));
        const auto fresh = Descriptor(Fresh.data(), sizeof(Fresh));
        FirstDescriptor = Descriptor(First.data(), sizeof(First));
        SecondDescriptor = stale;
        std::memcpy(importedSecondDescriptor, stale.data(), sizeof(stale));
        const auto copySrcDesc = Descriptor(importedCopySource, 16u);
        std::memcpy(importedCopySourceDescriptor, copySrcDesc.data(), sizeof(copySrcDesc));
        std::memcpy(importedPolluteDescriptor, stale.data(), sizeof(stale));
        const auto polluteMetaDesc = Descriptor(importedPolluteDescriptor, 16u);
        std::memcpy(importedPolluteMetaDescriptor, polluteMetaDesc.data(), sizeof(polluteMetaDesc));
        ArgumentsDescriptor = Descriptor(Arguments.data(), sizeof(Arguments));
        std::vector<std::uint32_t> warmUp;
        Count(warmUp, FirstDescriptor.data());
        Count(warmUp, SecondDescriptor.data());
        CountIndirect(warmUp, SecondDescriptor.data(), Arguments.data());
        Count(warmUp, importedSecondDescriptor);
        CountIndirect(warmUp, importedSecondDescriptor, Arguments.data());
        WriteThreeDwords(warmUp, ArgumentsDescriptor.data(), 1u, 1u, 1u);
        WriteThreeDwords(warmUp, importedCopySourceDescriptor, fresh[0], fresh[1], fresh[2]);
        Count(warmUp, importedPolluteDescriptor, 2);
        for (int i = 0; i < 4; ++i) {
            WriteThreeDwords(warmUp, importedPolluteMetaDescriptor, stale[0], stale[1], stale[2]);
            Count(warmUp, importedPolluteDescriptor, 2);
        }
        WriteThreeDwords(warmUp, importedPolluteMetaDescriptor, fresh[0], fresh[1], fresh[2]);
        ImageWriteDescriptor(warmUp, importedSecondDescriptor, stale);
        GdsWrite(warmUp);
        warmUp.insert(warmUp.end(), {0xc0033700u, 0x00100200u, Low(&Done), High(&Done), 2u});
        Packet first{warmUp.data(), static_cast<std::uint32_t>(warmUp.size()), 0, {}};
        Require(sceAgcDriverSubmitAcb(0x20, &first) == 0, "warm-up submit failed");
        AgcDriverWaitIdle_nid_postfix();
        Require(Done == 2 && First[0] == 1 && Stale[0] == 14, "the warm-up dispatches did not run");
        First[0] = 0;
        Stale[0] = 0;
        Fresh[0] = 0;
        {
            Done = 0;
            std::memcpy(importedPolluteDescriptor, stale.data(), sizeof(stale));
            AgcDriver::DriverDetail::DriverTestAccess::PrepareSamplingForRejectedAhead();
            const auto countersBefore = AgcDriver::DriverDetail::Driver::Get().DispatchCacheCounters();
            const auto adoptedBefore = AgcDriverDispatchesAdoptedAhead_nid_postfix();
            std::vector<std::uint32_t> polluteWords;
            polluteWords.insert(polluteWords.end(), {0xc0053c00u, 0x13u, Low(&Gate), High(&Gate), 1u, 0xffffffffu, 0x19u});
            Count(polluteWords, FirstDescriptor.data());
            WriteThreeDwords(polluteWords, importedPolluteMetaDescriptor, fresh[0], fresh[1], fresh[2]);
            Count(polluteWords, importedPolluteDescriptor, 3);
            polluteWords.insert(polluteWords.end(), {0xc0033700u, 0x00100200u, Low(&Done), High(&Done), 1u});
            Packet pollutePacket{polluteWords.data(), static_cast<std::uint32_t>(polluteWords.size()), 0, {}};
            Require(sceAgcDriverSubmitAcb(0x20, &pollutePacket) == 0, "rejected ahead cache pollution submit failed");
            AgcDriverWaitIdle_nid_postfix();
            const auto adopted = AgcDriverDispatchesAdoptedAhead_nid_postfix() - adoptedBefore;
            const auto countersAfter = AgcDriver::DriverDetail::Driver::Get().DispatchCacheCounters();
            Require(Done == 1 && First[0] == 1, "rejected ahead cache pollution submission did not finish");
            Require(adopted == 2, "rejected ahead capture was unexpectedly adopted (adopted " + std::to_string(adopted) + ")");
            Require(Fresh[0] == 3 && Stale[0] == 0, "normal dispatch after rejected ahead capture received the rejected stale variant from global dispatchCache (stale " + std::to_string(Stale[0]) + ", fresh " + std::to_string(Fresh[0]) + ")");
            Require(countersAfter.inserts - countersBefore.inserts == 1, "rejected ahead capture polluted global dispatchCache with an extra insert (inserts delta " + std::to_string(countersAfter.inserts - countersBefore.inserts) + ")");
        }
        First[0] = 0;
        Stale[0] = 0;
        Fresh[0] = 0;
        std::vector<std::uint32_t> words;
        words.insert(words.end(), {0xc0053c00u, 0x13u, Low(&Gate), High(&Gate), 1u, 0xffffffffu, 0x19u});
        Count(words, FirstDescriptor.data());
        words.insert(words.end(), {0xc0063700u, 0x00100200u, Low(SecondDescriptor.data()), High(SecondDescriptor.data()), fresh[0], fresh[1], fresh[2], fresh[3]});
        Count(words, SecondDescriptor.data());
        words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(&Done), High(&Done), 1u});
        Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
        Require(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "compute submit failed");
        AgcDriverWaitIdle_nid_postfix();
        Require(Done == 1, "the submission did not finish");
        Require(SecondDescriptor[0] == fresh[0] && SecondDescriptor[1] == fresh[1], "WRITE_DATA did not replace the descriptor");
        Require(First[0] == 1, "the first dispatch did not run");
        Require(Fresh[0] == 1 && Stale[0] == 0, "a dispatch whose descriptor a WRITE_DATA before it replaces used the old descriptor (stale " + std::to_string(Stale[0]) + ", fresh " + std::to_string(Fresh[0]) + ")");

        CopySource = fresh;
        RunUpdateAheadCase("HLE StreamCopy (CPU-backed)", SecondDescriptor.data(), stale, [&](std::vector<std::uint32_t>& cmd) {
            StreamCopyDescriptor(cmd, CopySource.data(), SecondDescriptor.data());
        });

        const auto gpuCopiesBefore = AgcDriver::DriverDetail::copyPathCounts[AgcDriver::DriverDetail::CopyGpu].load();
        RunUpdateAheadCase("HLE StreamCopy (GPU-backed SourcePending)", importedSecondDescriptor, stale, [&](std::vector<std::uint32_t>& cmd) {
            const std::array<std::uint32_t, 4> initSrc{0u, 0u, fresh[2], fresh[3]};
            std::memcpy(importedCopySource, initSrc.data(), sizeof(initSrc));
            WriteThreeDwords(cmd, importedCopySourceDescriptor, fresh[0], fresh[1], fresh[2]);
            StreamCopyDescriptor(cmd, importedCopySource, importedSecondDescriptor);
        }, 4);
        Require(AgcDriver::DriverDetail::copyPathCounts[AgcDriver::DriverDetail::CopyGpu].load() > gpuCopiesBefore, "GPU-backed StreamCopy did not take CopyGpu path");

        RunUpdateAheadCase("HLE FillKernel (CPU-backed)", SecondDescriptor.data(), stale, [&](std::vector<std::uint32_t>& cmd) {
            FillKernelDescriptor(cmd, SecondDescriptor.data(), fresh);
        });

        RunUpdateAheadCase("HLE FillKernel (GPU-backed)", importedSecondDescriptor, stale, [&](std::vector<std::uint32_t>& cmd) {
            FillKernelDescriptor(cmd, importedSecondDescriptor, fresh);
        });

        RunUpdateAheadCase("HLE DwordPatternFill (CPU-backed)", SecondDescriptor.data(), stale, [&](std::vector<std::uint32_t>& cmd) {
            DwordPatternFillDescriptor(cmd, SecondDescriptor.data(), fresh);
        });

        RunUpdateAheadCase("HLE DwordPatternFill (GPU-backed)", importedSecondDescriptor, stale, [&](std::vector<std::uint32_t>& cmd) {
            DwordPatternFillDescriptor(cmd, importedSecondDescriptor, fresh);
        });

        RunUpdateAheadCase("StorageImage write overlap", importedSecondDescriptor, stale, [&](std::vector<std::uint32_t>& cmd) {
            ImageWriteDescriptor(cmd, importedSecondDescriptor, fresh);
        }, 3, true);

        {
            First[0] = 0;
            Stale[0] = 0;
            Fresh[0] = 0;
            Done = 0;
            SecondDescriptor = fresh;
            const auto adoptedBefore = AgcDriverDispatchesAdoptedAhead_nid_postfix();
            std::vector<std::uint32_t> gdsWords;
            gdsWords.insert(gdsWords.end(), {0xc0053c00u, 0x13u, Low(&Gate), High(&Gate), 1u, 0xffffffffu, 0x19u});
            Count(gdsWords, FirstDescriptor.data());
            GdsWrite(gdsWords);
            Count(gdsWords, SecondDescriptor.data());
            gdsWords.insert(gdsWords.end(), {0xc0033700u, 0x00100200u, Low(&Done), High(&Done), 1u});
            Packet gdsPacket{gdsWords.data(), static_cast<std::uint32_t>(gdsWords.size()), 0, {}};
            Require(sceAgcDriverSubmitAcb(0x20, &gdsPacket) == 0, "GDS unbounded write submit failed");
            AgcDriverWaitIdle_nid_postfix();
            const auto adopted = AgcDriverDispatchesAdoptedAhead_nid_postfix() - adoptedBefore;
            Require(Done == 1 && First[0] == 1 && Fresh[0] == 1, "GDS unbounded write submission did not run");
            Require(adopted == 2, "unbounded GDS write did not stop resolveGroupAhead (expected 2 adopted ahead, got " + std::to_string(adopted) + ")");
        }

        std::cout << "write data written ahead test passed\n";
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
