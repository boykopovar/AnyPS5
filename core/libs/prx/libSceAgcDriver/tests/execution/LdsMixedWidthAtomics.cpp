#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
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
#include <unistd.h>
#endif

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

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

constexpr std::uint32_t Threads = 256;
constexpr std::uint32_t Iterations = 16;
constexpr std::uint32_t Repeats = 32;
constexpr std::uint32_t LdsDwords = 4;
alignas(256) std::array<std::uint32_t, Threads * 2> Output{};
alignas(256) std::array<std::uint32_t, Threads * 4> Input = [] {
    std::array<std::uint32_t, Threads * 4> rows{};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        rows[tid * 4u] = 0xffffffffu;
        rows[tid * 4u + 2u] = 1u;
    }
    return rows;
}();

template<std::uint32_t Count, std::size_t Head, std::size_t Body, std::size_t Tail>
constexpr auto Unrolled(const std::array<std::uint32_t, Head>& head, const std::array<std::uint32_t, Body>& body, const std::array<std::uint32_t, Tail>& tail) {
    std::array<std::uint32_t, Head + Body * Count + Tail> code{};
    std::size_t next = 0;
    for (const auto word : head) code[next++] = word;
    for (std::uint32_t iteration = 0; iteration < Count; ++iteration) {
        for (const auto word : body) code[next++] = word;
    }
    for (const auto word : tail) code[next++] = word;
    return code;
}

alignas(256) constexpr auto UniformCode = Unrolled<Iterations>(
    std::to_array<std::uint32_t>({
        0x34100083,
        0x7e020280,
        0x7e0402c1,
        0x7e060280,
        0x7e080281,
        0x7e0c0280,
        0x7e0e0280,
        0xd9340000, 0x00000601,
        0xbf8cc07f,
        0xbf8a0000,
    }),
    std::to_array<std::uint32_t>({
        0xd9000000, 0x00000201,
        0xd8000004, 0x00000401,
        0xbf8cc07f,
    }),
    std::to_array<std::uint32_t>({
        0xbf8cc07f,
        0xbf8a0000,
        0xd9d80000, 0x0a000001,
        0xbf8cc07f,
        0xe0741000, 0x80010a08,
        0xbf810000,
    }));

alignas(256) constexpr auto DivergentCode = Unrolled<Iterations>(
    std::to_array<std::uint32_t>({
        0x34100083,
        0x7e020280,
        0x7e0402c1,
        0x7e060280,
        0x7e080281,
        0x7e0c0280,
        0x7e0e0280,
        0xd9340000, 0x00000601,
        0xbf8cc07f,
        0xbf8a0000,
        0xbe89037e,
    }),
    std::to_array<std::uint32_t>({
        0xbefe03ff, 0x55555555,
        0xd9000000, 0x00000201,
        0xbefe03ff, 0xaaaaaaaa,
        0xd8000004, 0x00000401,
        0xbf8cc07f,
    }),
    std::to_array<std::uint32_t>({
        0xbefe0309,
        0xbf8cc07f,
        0xbf8a0000,
        0xd9d80000, 0x0a000001,
        0xbf8cc07f,
        0xe0741000, 0x80010a08,
        0xbf810000,
    }));

alignas(256) constexpr auto ParityCode = Unrolled<1>(
    std::to_array<std::uint32_t>({
        0x34100083,
        0x7e020280,
        0x7e0402c1,
        0x7e060280,
        0x7e080281,
        0x7e0c0280,
        0x7e0e0280,
        0xd9340000, 0x00000601,
        0xbf8cc07f,
        0xbf8a0000,
        0xbe89037e,
    }),
    std::to_array<std::uint32_t>({
        0xbefe03ff, 0x55555555,
        0xd9000000, 0x00000201,
        0xbf8cc07f,
        0xbefe03ff, 0xaaaaaaaa,
        0xd8000004, 0x00000401,
        0xbf8cc07f,
    }),
    std::to_array<std::uint32_t>({
        0xbefe0309,
        0xbf8cc07f,
        0xbf8a0000,
        0xd9d80000, 0x0a000001,
        0xbf8cc07f,
        0xe0741000, 0x80010a08,
        0xbf810000,
    }));

alignas(256) constexpr auto LoadedParityCode = Unrolled<1>(
    std::to_array<std::uint32_t>({
        0x34100083,
        0x340a0084,
        0x7e020280,
        0xe0381000, 0x80000c05,
        0x7e0c0280,
        0x7e0e0280,
        0xd9340000, 0x00000601,
        0xbf8c0070,
        0xbf8a0000,
        0xbe89037e,
    }),
    std::to_array<std::uint32_t>({
        0xbefe03ff, 0x55555555,
        0xd9000000, 0x00000c01,
        0xbf8cc07f,
        0xbefe03ff, 0xaaaaaaaa,
        0xd8000004, 0x00000e01,
        0xbf8cc07f,
    }),
    std::to_array<std::uint32_t>({
        0xbefe0309,
        0xbf8cc07f,
        0xbf8a0000,
        0xd9d80000, 0x0a000001,
        0xbf8cc07f,
        0xe0741000, 0x80010a08,
        0xbf810000,
    }));

alignas(256) constexpr auto MisalignedCode = Unrolled<1>(
    std::to_array<std::uint32_t>({
        0x34100083,
        0x7e020280,
        0x7e0402c1,
        0x7e060280,
        0x7e080281,
        0x7e0c0280,
        0x7e0e0280,
        0xd9340000, 0x00000601,
        0xbf8cc07f,
        0xbf8a0000,
    }),
    std::to_array<std::uint32_t>({
        0xd9000004, 0x00000201,
        0xbf8cc07f,
    }),
    std::to_array<std::uint32_t>({
        0xbf8cc07f,
        0xbf8a0000,
        0xd9d80000, 0x0a000001,
        0xbf8cc07f,
        0xe0741000, 0x80010a08,
        0xbf810000,
    }));

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%016llx", static_cast<unsigned long long>(value));
    return text;
}

void Check(const char* name, std::uint64_t operations, std::uint32_t run) {
    const std::uint64_t expected = operations * 0xffffffffull + (operations << 32u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint64_t actual = Output[tid * 2u] | (static_cast<std::uint64_t>(Output[tid * 2u + 1u]) << 32u);
        Require(actual == expected, std::string("lds mixed width atomics ") + name + " run " + std::to_string(run) + ": lane " + std::to_string(tid) + " read " + Hex(actual) + ", expected " + Hex(expected) + " after " + std::to_string(operations) + " ds_add_u64 and " + std::to_string(operations) + " ds_add_u32 on one slot");
    }
}

std::uint64_t WorkgroupBytes(std::span<const std::uint32_t> words) {
    std::unordered_map<std::uint32_t, std::span<const std::uint32_t>> types;
    std::unordered_map<std::uint32_t, std::uint32_t> constants;
    std::vector<std::uint32_t> pointers;
    for (std::size_t at = 5; at < words.size();) {
        const auto count = words[at] >> 16u;
        Require(count != 0u && count <= words.size() - at, "lds lock budget: malformed SPIR-V");
        const auto instruction = words.subspan(at, count);
        const auto opcode = instruction[0] & 0xffffu;
        if (opcode == spv::OpTypeInt || opcode == spv::OpTypeArray || opcode == spv::OpTypePointer) types[instruction[1]] = instruction;
        if (opcode == spv::OpConstant) constants[instruction[2]] = instruction[3];
        if (opcode == spv::OpVariable && instruction[3] == spv::StorageClassWorkgroup) pointers.push_back(instruction[1]);
        at += count;
    }
    std::uint64_t total = 0;
    for (const auto pointer : pointers) {
        const auto definition = types.at(pointer);
        Require((definition[0] & 0xffffu) == spv::OpTypePointer, "lds lock budget: Workgroup storage has no pointer type");
        const auto array = types.at(definition[3]);
        Require((array[0] & 0xffffu) == spv::OpTypeArray, "lds lock budget: unexpected Workgroup allocation shape");
        const auto element = types.at(array[2]);
        Require((element[0] & 0xffffu) == spv::OpTypeInt && element[2] == 32u, "lds lock budget: Workgroup array is not dwords");
        total += static_cast<std::uint64_t>(constants.at(array[3])) * sizeof(std::uint32_t);
    }
    return total;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t ldsDwords) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, ldsDwords, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const char* name, std::uint64_t operations, std::uint32_t repeats, std::uint32_t ldsDwords = LdsDwords, std::optional<std::uint32_t> expectedDeviceDwords = std::nullopt) {
    const auto result = Compile(device, code, ldsDwords);
    if (expectedDeviceDwords) {
        const auto bytes = WorkgroupBytes(result.spirv.Words());
        const auto limit = device.Target().maxWorkgroupSharedMemoryBytes;
        Require(bytes <= limit, "lds lock budget: guest LDS and lock exceed the device limit before dispatch");
        Require(result.workgroupMemoryDwords == *expectedDeviceDwords, "lds lock budget: unexpected device-backed LDS allocation before dispatch");
        if (*expectedDeviceDwords == 0u) Require(bytes == limit, "lds lock budget: exact-fit case did not retain its lock");
        std::printf("lds lock budget: %s declarations verified; guest=%u; workgroup=%llu; device=%u\n", name, ldsDwords * 4u, static_cast<unsigned long long>(bytes), result.workgroupMemoryDwords);
    }
    for (std::uint32_t run = 0; run < repeats; ++run) {
        Output.fill(0xdeadbeefu);
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
        device.WaitIdle();
        Check(name, operations, run);
    }
    if (expectedDeviceDwords) std::printf("lds lock budget: %s GPU readback verified across 256 lanes\n", name);
}

void Misaligned(AgcDriver::VulkanDevice& device, std::uint32_t ldsDwords, const char* lds) {
    const auto result = Compile(device, MisalignedCode, ldsDwords);
    Output.fill(0xdeadbeefu);
    StderrCapture capture;
    try {
        device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(MisalignedCode.data()));
        device.WaitIdle();
    } catch (...) {
        static_cast<void>(capture.Finish());
        throw;
    }
    const auto log = capture.Finish();
    Require(log.find("BDA access failed: address=0x4 instruction=0x2c bytes=8 ") != std::string::npos && log.find("reason=7") != std::string::npos, std::string("lds lock budget: misaligned ds_add_u64 on ") + lds + " LDS did not fault");
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Require(Output[tid * 2u] == 0u && Output[tid * 2u + 1u] == 0u, std::string("lds lock budget: misaligned ds_add_u64 on ") + lds + " LDS changed the slot: lane " + std::to_string(tid) + " read " + Hex(Output[tid * 2u] | (static_cast<std::uint64_t>(Output[tid * 2u + 1u]) << 32u)));
    std::printf("lds lock budget: misaligned ds_add_u64 on %s LDS faulted\n", lds);
}

void LockBudget(AgcDriver::VulkanDevice& device) {
    const auto target = device.Target();
    const auto limit = target.maxWorkgroupSharedMemoryBytes;
    std::printf("lds lock budget: robustBufferAccess2=%u; maxWorkgroupSharedMemoryBytes=%u\n", static_cast<unsigned>(target.robustBufferAccess2), limit);
    if (std::getenv("ANYPS5_REQUIRE_ROBUST2") != nullptr) Require(target.robustBufferAccess2, "lds lock budget: robustBufferAccess2 was not enabled");
    Require(limit >= 32u && limit % 4u == 0u, "lds lock budget: unexpected device shared-memory limit");
    const auto operations = static_cast<std::uint64_t>(Threads) * Iterations;
    Run(device, UniformCode, "limit", operations, 1, limit / 4u, (limit / 4u + 2u) & ~1u);
    const auto& capabilities = target.supportedCapabilities;
    if (std::find(capabilities.begin(), capabilities.end(), static_cast<std::uint32_t>(spv::CapabilityInt64Atomics)) != capabilities.end()) Misaligned(device, limit / 4u, "device-backed");
    Misaligned(device, LdsDwords, "workgroup");
    Run(device, UniformCode, "limit-4", operations, 1, (limit - 4u) / 4u, 0u);
    std::puts("lds lock budget tests passed");
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (argc == 2 && std::string_view(argv[1]) == "--lock-budget") {
            LockBudget(*device);
            return 0;
        }
        Require(argc == 1, "lds mixed width atomics: unexpected argument");
        Run(*device, UniformCode, "uniform", static_cast<std::uint64_t>(Threads) * Iterations, 1);
        Run(*device, DivergentCode, "divergent", static_cast<std::uint64_t>(Threads) / 2u * Iterations, 1);
        Run(*device, ParityCode, "split lanes", Threads / 2u, Repeats);
        Run(*device, LoadedParityCode, "split lanes with per-lane operands", Threads / 2u, Repeats);
        std::puts("lds mixed width atomics tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
