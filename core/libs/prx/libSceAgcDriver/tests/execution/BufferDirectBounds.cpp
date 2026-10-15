#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "PoisonedBufferPool.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t DataWords = 1024;
constexpr std::uint32_t OutputWords = Lanes * 2;
constexpr std::uint32_t Fill = 0xdeadbeefu;
constexpr std::uint32_t AllLanes = 0xffffffffu;
constexpr std::uint32_t PartialLanes = 0x0ff0f0f5u;
enum class Access { Load, Store };
enum class Width { Dword, Byte, Short, Format };

struct Case {
    Access access;
    Width width;
    std::uint32_t dwords;
    bool idxen;
    bool offen;
    std::uint32_t immediate;
    std::uint32_t oobMode;
    std::uint32_t stride;
    std::uint32_t records;
    std::uint32_t soffset;
    std::uint32_t exec = AllLanes;
};

struct Memory {
    std::array<std::uint32_t, OutputWords> output;
    std::array<std::uint32_t, DataWords> data;
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

const char* WidthName(Width width) {
    switch (width) {
        case Width::Dword: return "dword";
        case Width::Byte: return "byte";
        case Width::Short: return "short";
        default: return "format";
    }
}

std::string Describe(const Case& test, bool robust) {
    return std::string("buffer direct bounds ") + (test.access == Access::Load ? "load " : "store ") + WidthName(test.width) + " x" + std::to_string(test.dwords) + (test.idxen ? " idxen" : "") + (test.offen ? " offen" : "") + " offset " + std::to_string(test.immediate) + " oob " + std::to_string(test.oobMode) + " stride " + std::to_string(test.stride) + " records " + std::to_string(test.records) + " soffset " + std::to_string(test.soffset) + " exec " + Hex(test.exec) + (robust ? " robustBufferAccess2" : "");
}

std::uint32_t AddressRegister(const Case& test) {
    if (test.idxen) return 0u;
    if (test.width == Width::Byte) return 0u;
    if (test.width == Width::Short) return 5u;
    return test.dwords == 2u ? 4u : 1u;
}

std::uint32_t LaneOffset(const Case& test, std::uint32_t lane) {
    if (!test.offen) return 0u;
    if (test.idxen) return lane * 4u;
    if (test.width == Width::Byte) return lane;
    if (test.width == Width::Short) return lane * 2u;
    return test.dwords == 1u ? lane * 4u : lane * 8u;
}

std::uint32_t Opcode(const Case& test) {
    const bool load = test.access == Access::Load;
    switch (test.width) {
        case Width::Byte: return load ? 0xe0200000u : 0xe0600000u;
        case Width::Short: return load ? 0xe0280000u : 0xe0680000u;
        case Width::Format: return load ? 0xe0000000u : 0xe0100000u;
        default: return test.dwords == 2u ? (load ? 0xe0340000u : 0xe0740000u) : (load ? 0xe0300000u : 0xe0700000u);
    }
}

std::span<const std::uint32_t> Assemble(const Case& test, std::array<std::uint32_t, 32>& code) {
    Require(test.width == Width::Dword || test.dwords == 1u, "buffer direct bounds: only dword cases access two dwords");
    const auto flags = (test.idxen ? 0x2000u : 0u) | (test.offen ? 0x1000u : 0u) | test.immediate;
    std::vector<std::uint32_t> words{0x34020082u, 0x34080083u, 0x340a0081u, 0x380400ffu, 0x5a000000u, 0x380600ffu, 0x5b000000u, 0xbefe0309u, Opcode(test) | flags, 0x08000200u | AddressRegister(test)};
    if (test.access == Access::Load) {
        words.insert(words.end(), {0xbefe03c1u, 0xbf8c3f70u});
        words.insert(words.end(), {test.dwords == 2u ? 0xe0741000u : 0xe0701000u, test.dwords == 2u ? 0x80010204u : 0x80010201u});
    }
    words.push_back(0xbf810000u);
    Require(words.size() <= code.size(), "buffer direct bounds: program exceeds its code buffer");
    code.fill(0xbf810000u);
    std::copy(words.begin(), words.end(), code.begin());
    return std::span<const std::uint32_t>(code.data(), words.size());
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t records, std::uint32_t stride, std::uint32_t oobMode) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, 0x01016facu | (oobMode << 28u)};
}

std::uint32_t AccessBytes(const Case& test) {
    if (test.width == Width::Byte) return 1u;
    return test.width == Width::Short ? 2u : 4u;
}

bool GuestInBounds(const Case& test, std::uint32_t index, std::uint32_t offset, std::uint32_t bytes) {
    if (test.records == 0u) return false;
    switch (test.oobMode) {
        case 0: return index < test.records && offset < test.stride;
        case 1: return index < test.records;
        case 2: return true;
        default: return test.soffset <= test.records && test.records - test.soffset >= bytes && offset <= test.records - test.soffset - bytes;
    }
}

std::uint64_t ViewBytes(const Case& test) {
    return test.stride == 0u ? test.records : static_cast<std::uint64_t>(test.stride) * test.records;
}

struct Location {
    bool reached;
    std::uint32_t word;
};

bool Modelled(const Case& test) {
    if (test.width != Width::Dword) return false;
    const auto view = ViewBytes(test);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        for (std::uint32_t component = 0; component < test.dwords; ++component) {
            const auto index = test.idxen ? lane : 0u;
            const auto offset = LaneOffset(test, lane) + test.immediate + component * 4u;
            if (!GuestInBounds(test, index, offset, 4u)) continue;
            const auto byte = static_cast<std::uint64_t>(index) * test.stride + offset + test.soffset;
            if (byte % 4u != 0u || (byte < view && byte + 4u > view)) return false;
        }
    }
    return true;
}

Location Locate(const Case& test, std::uint32_t lane, std::uint32_t component) {
    const auto index = test.idxen ? lane : 0u;
    const auto offset = LaneOffset(test, lane) + test.immediate + component * 4u;
    if (!GuestInBounds(test, index, offset, 4u)) return {false, 0u};
    const auto byte = static_cast<std::uint64_t>(index) * test.stride + offset + test.soffset;
    if (byte + 4u > ViewBytes(test)) return {false, 0u};
    Require(byte + 4u <= DataWords * 4u, "buffer direct bounds: an in-bounds case reaches past the test buffer");
    return {true, static_cast<std::uint32_t>(byte / 4u)};
}

std::uint32_t Pattern(std::uint32_t word) {
    return 0x10203040u + word * 0x01010101u;
}

std::uint32_t Register(std::uint32_t lane, std::uint32_t component) {
    return (component == 0u ? 0x5a000000u : 0x5b000000u) | lane;
}

std::array<std::uint32_t, DataWords> InitialData(const Case& test) {
    std::array<std::uint32_t, DataWords> data{};
    for (std::uint32_t word = 0; word < DataWords; ++word) data[word] = Pattern(word);
    for (auto byte = ViewBytes(test); byte % 4u != 0u; ++byte) data[byte / 4u] &= ~(0xffu << (byte % 4u * 8u));
    return data;
}

struct Job {
    Case test;
    alignas(256) std::array<std::uint32_t, DataWords> data{};
    alignas(256) std::array<std::uint32_t, OutputWords> output{};
    alignas(256) std::array<std::uint32_t, 32> code{};
    std::span<const std::uint32_t> program;
    std::vector<std::uint32_t> userData;
    ShaderRecompiler::RecompileResult reference;
    ShaderRecompiler::RecompileResult robust;
};

ShaderRecompiler::RecompileResult Compile(const Job& job, const AgcDriver::VulkanDevice& device, bool robust) {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(job.code.data()), std::as_bytes(job.program)}}};
    auto target = device.Target();
    target.robustBufferAccess2 = robust;
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(job.code.data()), job.program, 0, {}},
        {Lanes, 0, job.userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::unique_ptr<Job> Prepare(const Case& test) {
    Require(ViewBytes(test) <= DataWords * 4u, "buffer direct bounds: a case views past the test buffer");
    auto job = std::make_unique<Job>();
    job->test = test;
    job->program = Assemble(test, job->code);
    for (const auto& descriptor : {BufferDescriptor(job->data.data(), test.records, test.stride, test.oobMode), BufferDescriptor(job->output.data(), OutputWords * 4u, 0u, 3u)}) {
        job->userData.insert(job->userData.end(), descriptor.begin(), descriptor.end());
    }
    job->userData.push_back(test.soffset);
    job->userData.push_back(test.exec);
    return job;
}

void CompileAll(std::vector<std::unique_ptr<Job>>& jobs, const AgcDriver::VulkanDevice& device, bool robust) {
    std::atomic<std::size_t> next{0};
    std::mutex failureMutex;
    std::exception_ptr failure;
    const auto worker = [&] {
        try {
            for (auto index = next++; index < jobs.size(); index = next++) {
                auto& job = *jobs[index];
                (robust ? job.robust : job.reference) = Compile(job, device, robust);
            }
        } catch (...) {
            std::lock_guard lock(failureMutex);
            if (!failure) failure = std::current_exception();
            next = jobs.size();
        }
    };
    std::vector<std::thread> threads;
    const auto count = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned thread = 0; thread < count; ++thread) threads.emplace_back(worker);
    for (auto& thread : threads) thread.join();
    if (failure) std::rethrow_exception(failure);
}

Memory Run(AgcDriver::VulkanDevice& device, Job& job, const ShaderRecompiler::RecompileResult& result) {
    job.data = InitialData(job.test);
    job.output.fill(Fill);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(job.code.data()));
    device.WaitIdle();
    return {job.output, job.data};
}

void CheckModel(const Case& test, const Memory& memory) {
    const auto run = Describe(test, false);
    auto data = InitialData(test);
    std::array<std::uint32_t, OutputWords> output{};
    output.fill(Fill);
    std::array<bool, DataWords> stored{};
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const bool active = ((test.exec >> lane) & 1u) != 0u;
        for (std::uint32_t component = 0; component < test.dwords; ++component) {
            const auto location = Locate(test, lane, component);
            if (test.access == Access::Load) {
                output[lane * test.dwords + component] = !active ? Register(lane, component) : location.reached ? data[location.word] : 0u;
            } else if (active && location.reached) {
                Require(!stored[location.word], run + ": two lanes store the same dword");
                stored[location.word] = true;
                data[location.word] = Register(lane, component);
            }
        }
    }
    for (std::uint32_t word = 0; word < OutputWords; ++word) {
        Require(memory.output[word] == output[word], run + ": result dword " + std::to_string(word) + " is " + Hex(memory.output[word]) + ", expected " + Hex(output[word]));
    }
    for (std::uint32_t word = 0; word < DataWords; ++word) {
        Require(memory.data[word] == data[word], run + ": buffer dword " + std::to_string(word) + " is " + Hex(memory.data[word]) + ", expected " + Hex(data[word]));
    }
}

void CheckSame(const Case& test, const Memory& robust, const Memory& reference) {
    const auto run = Describe(test, true);
    for (std::uint32_t word = 0; word < OutputWords; ++word) {
        Require(robust.output[word] == reference.output[word], run + ": result dword " + std::to_string(word) + " is " + Hex(robust.output[word]) + ", the checked path gives " + Hex(reference.output[word]));
    }
    for (std::uint32_t word = 0; word < DataWords; ++word) {
        Require(robust.data[word] == reference.data[word], run + ": buffer dword " + std::to_string(word) + " is " + Hex(robust.data[word]) + ", the checked path gives " + Hex(reference.data[word]));
    }
}

std::vector<Case> Cases() {
    std::vector<Case> cases;
    for (const auto access : {Access::Load, Access::Store}) {
        constexpr auto dword = Width::Dword;
        for (const auto records : {0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 10u, 64u, 124u, 126u, 127u, 128u}) cases.push_back({access, dword, 1, false, true, 0, 3, 0, records, 0});
        for (const auto soffset : {4u, 8u, 125u, 128u, 132u}) {
            cases.push_back({access, dword, 1, false, true, 0, 3, 0, 128, soffset});
            cases.push_back({access, dword, 1, false, true, 8, 3, 0, 128, soffset});
        }
        for (const auto records : {124u, 128u, 132u, 252u, 254u, 256u}) cases.push_back({access, dword, 2, false, true, 0, 3, 0, records, 0});
        cases.push_back({access, dword, 1, false, true, 0, 3, 0, 64, 0, PartialLanes});
        cases.push_back({access, dword, 2, false, true, 0, 3, 0, 132, 0, PartialLanes});
        for (const auto records : {0u, 1u, 16u, 31u, 32u}) {
            for (const auto immediate : {0u, 4u, 8u}) cases.push_back({access, dword, 1, true, false, immediate, 0, 8, records, 0});
        }
        cases.push_back({access, dword, 2, true, false, 0, 0, 8, 32, 0});
        cases.push_back({access, dword, 2, true, false, 4, 0, 8, 32, 0});
        cases.push_back({access, dword, 1, true, false, 0, 0, 8, 32, 16});
        cases.push_back({access, dword, 1, true, false, 4, 0, 8, 32, 0, PartialLanes});
        cases.push_back({access, dword, 1, true, false, 4, 1, 8, 3, 0});
        cases.push_back({access, dword, 1, true, false, 4, 1, 8, 32, 0});
        cases.push_back({access, dword, 1, true, true, 0, 1, 4, 8, 0});
        cases.push_back({access, dword, 2, true, true, 0, 1, 16, 20, 0});
        cases.push_back({access, dword, 1, true, false, 0, 2, 4, 16, 0});
        cases.push_back({access, dword, 1, true, false, 0, 3, 16, 8, 0});
        cases.push_back({access, dword, 1, true, false, 0, 3, 16, 2, 0});
        for (const auto width : {Width::Dword, Width::Byte, Width::Short, Width::Format}) {
            for (const auto immediate : {0u, 2u, 3u, 4u}) {
                for (const auto records : {1u, 2u, 7u}) cases.push_back({access, width, 1, true, false, immediate, 0, 5, records, 0});
                cases.push_back({access, width, 1, true, false, immediate, 0, 6, 3, 0});
                cases.push_back({access, width, 1, true, false, immediate, 0, 5, 7, 0, PartialLanes});
            }
            for (const auto records : {2u, 3u, 5u}) {
                for (const auto immediate : {0u, 3u, 4u, 8u}) cases.push_back({access, width, 1, true, false, immediate, 1, 5, records, 0});
            }
            for (const auto immediate : {0u, 2u, 4u}) {
                cases.push_back({access, width, 1, true, false, immediate, 2, 5, 3, 0});
                cases.push_back({access, width, 1, true, false, immediate, 2, 7, 5, 0});
            }
            for (const auto records : {1u, 2u, 3u, 5u, 6u, 7u, 9u, 13u, 33u, 66u, 129u}) {
                for (const auto immediate : {0u, 1u, 3u, 4u}) cases.push_back({access, width, 1, false, true, immediate, 3, 0, records, 0});
            }
            for (const auto soffset : {1u, 2u, 3u, 5u}) cases.push_back({access, width, 1, false, true, 0, 3, 0, 13, soffset});
            cases.push_back({access, width, 1, false, true, 0, 3, 0, 66, 0, PartialLanes});
        }
        cases.push_back({access, dword, 2, true, false, 4, 0, 10, 1, 0});
        cases.push_back({access, dword, 2, true, false, 0, 0, 10, 3, 0});
        cases.push_back({access, dword, 2, true, false, 4, 1, 10, 3, 0});
        cases.push_back({access, dword, 2, false, true, 0, 3, 0, 129, 0});
        cases.push_back({access, dword, 2, false, true, 4, 3, 0, 130, 0});
    }
    return cases;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool robust = device->Target().robustBufferAccess2;
        const auto referenceDevice = robust ? OpenVulkanTestDeviceWithoutRobustBufferAccess2() : nullptr;
        if (robust) {
            if (!referenceDevice) return VulkanTestSkipped;
            Require(!referenceDevice->Target().robustBufferAccess2, "buffer direct bounds: the reference device enables robustBufferAccess2");
        }
        std::vector<std::unique_ptr<Job>> jobs;
        for (const auto& test : Cases()) jobs.push_back(Prepare(test));
        auto& referenceRunner = robust ? *referenceDevice : *device;
        CompileAll(jobs, referenceRunner, false);
        if (robust) CompileAll(jobs, *device, true);
        for (const auto& job : jobs) {
            const auto& test = job->test;
            PoisonedBufferPool::PoisonPool(referenceRunner);
            const auto reference = Run(referenceRunner, *job, job->reference);
            if (Modelled(test)) CheckModel(test, reference);
            if (robust) {
                PoisonedBufferPool::PoisonPool(*device);
                CheckSame(test, Run(*device, *job, job->robust), reference);
            }
        }
        std::puts(robust ? "buffer direct bounds tests passed" : "buffer direct bounds tests passed without robustBufferAccess2");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
