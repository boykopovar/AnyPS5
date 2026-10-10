#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Default = 0x11111111u;
constexpr std::uint32_t Payload = 0x3f800000u;

alignas(256) std::array<std::uint32_t, Lanes> Output{};
alignas(256) std::array<std::uint32_t, 16> Root{};
alignas(256) std::array<std::uint32_t, 4> Data{Payload, 0u, 0u, 0u};
alignas(256) std::array<std::uint64_t, 1> Indirect{};
alignas(256) std::array<std::uint32_t, 8> Table{};

alignas(256) constexpr std::array<std::uint32_t, 19> BranchCode{
    0xf4080100u, 0xfa000000u, 0xf4000200u, 0xfa000010u, 0xf4040280u, 0xfa000018u, 0xbe8c03ffu, 0x11111111u,
    0xbf8cc07fu, 0xbf068008u, 0xbf850003u, 0xf4000305u, 0xfa000000u, 0xbf8cc07fu, 0x7e02020cu, 0x34040082u,
    0xe0701000u, 0x80010102u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 21> ExecCode{
    0xf4080100u, 0xfa000000u, 0xf4000200u, 0xfa000010u, 0xf4040280u, 0xfa000018u, 0xbe8c03ffu, 0x11111111u,
    0xbf8cc07fu, 0x7e02020cu, 0x34040082u, 0x7da80008u, 0xbf880004u, 0xf4000305u, 0xfa000000u, 0xbf8cc07fu,
    0x7e02020cu, 0xbefe04c1u, 0xe0701000u, 0x80010102u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 21> EmptyExecCode{
    0xf4080100u, 0xfa000000u, 0xf4000200u, 0xfa000010u, 0xf4040280u, 0xfa000018u, 0xbe8c03ffu, 0x11111111u,
    0xbf8cc07fu, 0x7e02020cu, 0x34040082u, 0x7da80008u, 0xbf800000u, 0xf4000305u, 0xfa000000u, 0xbf8cc07fu,
    0x7e02020cu, 0xbefe04c1u, 0xe0701000u, 0x80010102u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 22> ChainCode{
    0xf4080100u, 0xfa000000u, 0xf4000200u, 0xfa000010u, 0xf4040280u, 0xfa000018u, 0xbe8c03ffu, 0x11111111u,
    0xbf8cc07fu, 0xf4040385u, 0xfa000000u, 0xbf8cc07fu, 0xbf068008u, 0xbf850003u, 0xf4000307u, 0xfa000000u,
    0xbf8cc07fu, 0x7e02020cu, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 21> DescriptorCode{
    0xf4080100u, 0xfa000000u, 0xf4000200u, 0xfa000010u, 0xf4040280u, 0xfa000018u, 0x7e0202ffu, 0x11111111u,
    0xbf8cc07fu, 0xbf068008u, 0xbf850006u, 0xf4080405u, 0xfa000010u, 0xbf8cc07fu, 0xe0300000u, 0x80040100u,
    0xbf8c3f70u, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u,
};

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }
    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

struct Outcome {
    std::vector<std::uint32_t> words;
    std::string log;
    std::uint64_t variantId = 0;
    std::uint64_t pipelineVariantId = 0;
    std::uint32_t poisoned = 0;
    bool cacheable = false;
};

template <std::size_t CodeWords>
Outcome Run(AgcDriver::VulkanDevice& device, const std::array<std::uint32_t, CodeWords>& code, std::uint64_t pointer, std::uint32_t guard) {
    const auto output = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Output.data()));
    Root = {};
    Root[0] = static_cast<std::uint32_t>(output);
    Root[1] = static_cast<std::uint32_t>((output >> 32u) & 0xffffu);
    Root[2] = static_cast<std::uint32_t>(sizeof(Output));
    Root[3] = 0x30027facu;
    Root[4] = guard;
    Root[6] = static_cast<std::uint32_t>(pointer);
    Root[7] = static_cast<std::uint32_t>(pointer >> 32u);
    std::fill(Output.begin(), Output.end(), 0xdeadbeefu);
    const auto root = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Root.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(root), static_cast<std::uint32_t>(root >> 32u)};
    const std::span<const std::uint32_t> span(code);
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(span.data()), span, 0, {}},
        {64, 0, userData, compute, std::nullopt, std::nullopt, {}},
        device.Target(),
        {0, 0, 0, 128}
    };
    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    const auto regions = memory.Regions();
    request.context.memory = regions;
    const auto result = ShaderRecompiler::Recompile(request, *capture);
    Outcome outcome;
    outcome.variantId = result->variantId;
    outcome.pipelineVariantId = result->PipelineVariantId();
    outcome.poisoned = result->poisonedSrtReads;
    outcome.cacheable = AgcDriver::DriverDetail::CacheableResult(*result);
    StderrCapture log;
    try {
        device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(span.data()));
        device.WaitIdle();
    } catch (...) {
        static_cast<void>(log.Finish());
        throw;
    }
    outcome.log = log.Finish();
    outcome.words.assign(Output.begin(), Output.end());
    return outcome;
}

bool Faulted(const Outcome& outcome) {
    return outcome.log.find("BDA access failed") != std::string::npos;
}

std::string Fault(std::uint64_t address, std::uint32_t pc) {
    char text[96];
    std::snprintf(text, sizeof(text), "address=0x%llx instruction=0x%x bytes=4", static_cast<unsigned long long>(address), pc);
    return text;
}

void RequireWords(const Outcome& outcome, std::uint32_t lanes, std::uint32_t inside, const std::string& what) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto expected = lane < lanes ? inside : Default;
        Require(outcome.words[lane] == expected, what + ": lane " + std::to_string(lane) + " wrote " + std::to_string(outcome.words[lane]) + ", expected " + std::to_string(expected));
    }
}

class InaccessibleRange {
public:
    InaccessibleRange() {
#ifdef _WIN32
        reserved = VirtualAlloc(nullptr, 65536, MEM_RESERVE, PAGE_NOACCESS);
#else
        reserved = mmap(nullptr, 65536, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (reserved == MAP_FAILED) reserved = nullptr;
#endif
        Require(reserved != nullptr, "cannot reserve an inaccessible range");
    }

    ~InaccessibleRange() {
#ifdef _WIN32
        VirtualFree(reserved, 0, MEM_RELEASE);
#else
        munmap(reserved, 65536);
#endif
    }

    InaccessibleRange(const InaccessibleRange&) = delete;
    InaccessibleRange& operator=(const InaccessibleRange&) = delete;

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(reserved)); }

private:
    void* reserved = nullptr;
};

std::uint64_t ValidPointer() {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Data.data()));
}

const Testing::Case branchGuard{"GuardedNullPointer_ReadBehindBranch_FaultsOnlyWhenTheReadRuns", [] {
    auto& device = SharedVulkanTestDevice();
    const auto valid = ValidPointer();
    const InaccessibleRange range;
    const auto unmapped = range.Address();
    const auto taken = Run(device, BranchCode, valid, 1u);
    Require(!Faulted(taken) && taken.poisoned == 0u && taken.cacheable, "guarded pointer: a mapped pointer faulted, was poisoned or left the caches:\n" + taken.log);
    RequireWords(taken, Lanes, Payload, "guarded pointer: a mapped pointer under a taken branch");
    const auto skippedNull = Run(device, BranchCode, 0u, 0u);
    Require(!Faulted(skippedNull) && skippedNull.poisoned == 1u && !skippedNull.cacheable, "guarded pointer: a null pointer behind a branch that skips its read faulted or stayed cacheable:\n" + skippedNull.log);
    RequireWords(skippedNull, 0u, 0u, "guarded pointer: a skipped read through a null pointer");
    const auto takenNull = Run(device, BranchCode, 0u, 1u);
    Require(takenNull.pipelineVariantId == skippedNull.pipelineVariantId && takenNull.pipelineVariantId == taken.pipelineVariantId && takenNull.variantId == taken.variantId, "guarded pointer: the poison or the branch condition changed the pipeline variant");
    Require(Faulted(takenNull) && takenNull.log.find(Fault(0u, 0x2cu)) != std::string::npos && takenNull.log.find("reason=1") != std::string::npos, "guarded pointer: a read through a null pointer that runs did not fault at its pc and address:\n" + takenNull.log);
    Require(std::all_of(takenNull.words.begin(), takenNull.words.end(), [](std::uint32_t word) { return word == 0u || word == 0xdeadbeefu; }), "guarded pointer: a faulting read did not return zero");
    const auto takenUnmapped = Run(device, BranchCode, unmapped, 1u);
    Require(Faulted(takenUnmapped) && takenUnmapped.log.find(Fault(unmapped, 0x2cu)) != std::string::npos, "guarded pointer: a read through an unmapped pointer that runs did not fault at its address:\n" + takenUnmapped.log);
    Require(takenUnmapped.pipelineVariantId == taken.pipelineVariantId, "guarded pointer: an unmapped pointer changed the pipeline variant");
}};

const Testing::Case execGuard{"GuardedNullPointer_ReadUnderExec_FaultsOnlyWhenTheReadRuns", [] {
    auto& device = SharedVulkanTestDevice();
    const auto valid = ValidPointer();
    const auto someLanes = Run(device, ExecCode, valid, 5u);
    Require(!Faulted(someLanes), "guarded pointer: a mapped pointer under EXEC faulted:\n" + someLanes.log);
    RequireWords(someLanes, 5u, Payload, "guarded pointer: a mapped pointer read under EXEC");
    const auto noLanes = Run(device, ExecCode, 0u, 0u);
    Require(!Faulted(noLanes) && noLanes.poisoned == 1u, "guarded pointer: a null pointer read skipped by s_cbranch_execz faulted:\n" + noLanes.log);
    RequireWords(noLanes, 0u, 0u, "guarded pointer: a read skipped by s_cbranch_execz");
    const auto oneLane = Run(device, ExecCode, 0u, 1u);
    Require(Faulted(oneLane) && oneLane.log.find(Fault(0u, 0x34u)) != std::string::npos, "guarded pointer: a null pointer read with one EXEC lane did not fault:\n" + oneLane.log);
    const auto emptyMapped = Run(device, EmptyExecCode, valid, 0u);
    Require(!Faulted(emptyMapped), "guarded pointer: a mapped pointer read with an empty EXEC faulted:\n" + emptyMapped.log);
    RequireWords(emptyMapped, 0u, 0u, "guarded pointer: a mapped pointer read with an empty EXEC");
    const auto emptyNull = Run(device, EmptyExecCode, 0u, 0u);
    Require(Faulted(emptyNull) && emptyNull.log.find(Fault(0u, 0x34u)) != std::string::npos, "guarded pointer: a null pointer read that runs with an empty EXEC did not fault:\n" + emptyNull.log);
    Require(noLanes.pipelineVariantId == someLanes.pipelineVariantId && oneLane.pipelineVariantId == someLanes.pipelineVariantId && emptyNull.pipelineVariantId == emptyMapped.pipelineVariantId, "guarded pointer: the poison changed an EXEC program's pipeline variant");
}};

const Testing::Case pointerChain{"GuardedNullPointer_PointerChain_FaultsAtTheFirstNullRead", [] {
    auto& device = SharedVulkanTestDevice();
    const auto valid = ValidPointer();
    const auto chainSkipped = Run(device, ChainCode, 0u, 0u);
    Require(chainSkipped.poisoned == 3u && Faulted(chainSkipped) && chainSkipped.log.find(Fault(0u, 0x24u)) != std::string::npos, "guarded pointer: a pointer read through a null pointer, used only by a skipped read, did not fault at its own pc:\n" + chainSkipped.log);
    const auto chainTaken = Run(device, ChainCode, 0u, 1u);
    Require(Faulted(chainTaken) && chainTaken.log.find(Fault(0u, 0x24u)) != std::string::npos, "guarded pointer: a read through a pointer read through a null pointer did not fault at the first read:\n" + chainTaken.log);
    Indirect[0] = valid;
    const auto chainMapped = Run(device, ChainCode, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Indirect.data())), 1u);
    Require(!Faulted(chainMapped) && chainMapped.poisoned == 0u && chainMapped.cacheable && chainMapped.pipelineVariantId == chainTaken.pipelineVariantId && chainSkipped.pipelineVariantId == chainTaken.pipelineVariantId, "guarded pointer: a chain's poison changed its pipeline variant, or a mapped chain faulted:\n" + chainMapped.log);
    RequireWords(chainMapped, Lanes, Payload, "guarded pointer: a read through a mapped pointer chain");
}};

const Testing::Case descriptorGuard{"GuardedNullPointer_DescriptorBehindPointer_FaultsOnlyWhenTheLoadRuns", [] {
    auto& device = SharedVulkanTestDevice();
    const auto valid = ValidPointer();
    const InaccessibleRange range;
    const auto unmapped = range.Address();
    Table = {0u, 0u, 0u, 0u, static_cast<std::uint32_t>(valid), static_cast<std::uint32_t>((valid >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(Data)), 0x30027facu};
    const auto table = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Table.data()));
    const auto descriptorTaken = Run(device, DescriptorCode, table, 1u);
    Require(!Faulted(descriptorTaken) && descriptorTaken.poisoned == 0u && descriptorTaken.cacheable, "guarded pointer: a V# behind a mapped pointer faulted, was poisoned or left the caches:\n" + descriptorTaken.log);
    RequireWords(descriptorTaken, Lanes, Payload, "guarded pointer: a buffer read through a V# behind a mapped pointer");
    const auto descriptorSkipped = Run(device, DescriptorCode, 0u, 0u);
    Require(!Faulted(descriptorSkipped) && descriptorSkipped.poisoned == 4u && !descriptorSkipped.cacheable, "guarded pointer: a V# behind a branch that skips its null pointer faulted or stayed cacheable:\n" + descriptorSkipped.log);
    RequireWords(descriptorSkipped, 0u, 0u, "guarded pointer: a skipped V# load through a null pointer");
    const auto descriptorNull = Run(device, DescriptorCode, 0u, 1u);
    Require(descriptorNull.pipelineVariantId == descriptorSkipped.pipelineVariantId && Faulted(descriptorNull) && descriptorNull.log.find(Fault(0x10u, 0x2cu)) != std::string::npos, "guarded pointer: a V# load through a null pointer that runs did not fault at its pc and address:\n" + descriptorNull.log);
    const auto descriptorUnmapped = Run(device, DescriptorCode, unmapped, 1u);
    Require(descriptorUnmapped.pipelineVariantId == descriptorNull.pipelineVariantId && Faulted(descriptorUnmapped) && descriptorUnmapped.log.find(Fault(unmapped + 0x10u, 0x2cu)) != std::string::npos, "guarded pointer: a V# load through an unmapped pointer that runs did not fault at its address:\n" + descriptorUnmapped.log);
    Table = {};
    const auto descriptorZero = Run(device, DescriptorCode, table, 1u);
    Require(!Faulted(descriptorZero) && descriptorZero.poisoned == 0u && descriptorZero.cacheable && descriptorZero.pipelineVariantId == descriptorNull.pipelineVariantId, "guarded pointer: a zero V# read from mapped memory faulted, or its pipeline variant differs from a V# zeroed by poison:\n" + descriptorZero.log);
    RequireWords(descriptorZero, Lanes, 0u, "guarded pointer: a buffer read through a zero V#");
}};

} // namespace
