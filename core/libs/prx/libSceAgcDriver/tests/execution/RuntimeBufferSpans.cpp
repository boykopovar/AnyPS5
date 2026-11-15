#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include "tests/TemporaryFile.hpp"
#include <spirv/unified1/spirv.hpp>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t TotalBytes = 3 * BlockBytes;
constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t OutputBase = 2 * BlockBytes;
constexpr std::uint32_t Guard = 0xdeadbeefu;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        data = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, TotalBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        data = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, TotalBytes));
#endif
        Require(data != nullptr, "runtime buffer spans: cannot allocate guest memory");
        for (std::size_t offset = 0; offset < TotalBytes; offset += BlockBytes) GuestAllocations::Mutation().Add(reinterpret_cast<std::byte*>(data) + offset, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        for (std::size_t offset = 0; offset < TotalBytes; offset += BlockBytes) GuestAllocations::Mutation().Remove(reinterpret_cast<std::byte*>(data) + offset);
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        std::free(data);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint32_t* Data() { return data; }

private:
    std::uint32_t* data = nullptr;
};

class StderrCapture {
public:
    StderrCapture() : file(TemporaryFile::Open()) {
        Require(file != nullptr, "runtime buffer spans: cannot capture diagnostics");
        std::fflush(stderr);
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        Require(saved >= 0 && _dup2(_fileno(file), _fileno(stderr)) == 0, "runtime buffer spans: cannot redirect stderr");
#else
        saved = dup(fileno(stderr));
        Require(saved >= 0 && dup2(fileno(file), fileno(stderr)) >= 0, "runtime buffer spans: cannot redirect stderr");
#endif
    }

    ~StderrCapture() {
        Restore();
        if (file != nullptr) std::fclose(file);
    }

    std::string Finish() {
        Restore();
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t count = 0; (count = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, count);
        std::fclose(file);
        file = nullptr;
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    void Restore() {
        if (saved < 0) return;
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        saved = -1;
    }

    std::FILE* file = nullptr;
    int saved = -1;
};

struct Case {
    const char* name;
    std::uint32_t base;
    std::uint32_t records;
    bool swizzle = false;
    bool wrap = false;
    std::uint32_t stride = 0;
    bool masked = false;
    bool fault = false;
    bool wrappedIndex = false;
};

constexpr Case Cases[]{
    {"aligned", 1024, 1024},
    {"unaligned +1", 1025, 1024},
    {"unaligned +2", 1026, 1024},
    {"unaligned +3", 1027, 1024},
    {"partial", 1024, 37},
    {"short", 1024, 3},
    {"empty", 1024, 0},
    {"split", BlockBytes - 4, 1024},
    {"swizzled", 1024, 1024, true},
    {"wrapped offset", 1024, 16, false, true},
    {"structured", 1024, 37, false, false, 16},
    {"structured partial", 1024, 10, false, false, 6},
    {"masked EXEC", 1024, 1024, false, false, 0, true},
    {"offset wrap canceled by index", 1024, 0xffffffffu, false, false, 0, false, false, true}};

std::vector<std::uint32_t> Code(std::uint32_t components, bool coherent, bool barrier, const Case& test) {
    const auto opcode = components == 4u ? 2u : components == 3u ? 3u : 1u;
    std::vector<std::uint32_t> code{0xe0340000u, 0x80010200u, 0xbf8c3f70u, 0x7e140502u, 0x7e160503u, 0x34280084u};
    if (test.wrappedIndex) code.insert(code.end(), {0x7e020281u, 0x7e0402ffu, 0xfffffff8u});
    else if (test.wrap) code.insert(code.end(), {0x7e0202ffu, 0xfffffffcu});
    else if (test.fault) code.push_back(0x7e020280u);
    else code.push_back(test.stride != 0u ? 0x34020080u : 0x34020084u);
    if (test.masked) code.insert(code.end(), {0xbe9203ffu, 0x55555555u, 0xbe9303ffu, 0x55555555u, 0xbefe0412u});
    code.insert(code.end(), {0xe0300000u + (test.wrappedIndex ? 0x3000u : test.stride != 0u ? 0x2000u : 0x1000u) + (opcode << 18u) + (coherent ? 0x4000u : 0u), 0x80020401u, 0xbf8c3f70u});
    if (barrier) code.push_back(0xbf8a0000u);
    code.insert(code.end(), {0xe0701000u + (opcode << 18u), 0x80030414u, 0xbf810000u});
    return code;
}

bool HasVectorSpan(const ShaderRecompiler::SharedSpirv& spirv, std::uint32_t mask) {
    std::map<std::uint32_t, std::string> names;
    std::map<std::uint32_t, std::uint32_t> constants;
    for (std::size_t at = 5; at < spirv.size();) {
        const auto count = spirv[at] >> 16u;
        const auto opcode = spirv[at] & 0xffffu;
        Require(count != 0u && count <= spirv.size() - at, "runtime buffer spans: malformed SPIR-V");
        if (opcode == spv::OpName && count >= 3u) names[spirv[at + 1u]] = reinterpret_cast<const char*>(spirv.data() + at + 2u);
        if (opcode == spv::OpConstant && count == 4u) constants[spirv[at + 2u]] = spirv[at + 3u];
        if (opcode == spv::OpFunctionCall && count == 8u && names[spirv[at + 3u]].starts_with("read_bda_span") && constants[spirv[at + 6u]] == mask) return true;
        at += count;
    }
    return false;
}

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t components, std::uint32_t wave, bool coherent, bool barrier, const Case& test, bool checkSpan) {
    std::vector<std::uint32_t> expected(TotalBytes / 4u, Guard);
    for (std::size_t index = 0; index < OutputBase / 4u; ++index) expected[index] = 0x52000000u + static_cast<std::uint32_t>(index) * 0x00010203u;
    expected[0] = test.records;
    expected[1] = test.stride != 0u ? 0x01016facu : 0x31016facu;
    std::memcpy(guest.Data(), expected.data(), TotalBytes);
    const auto descriptor = reinterpret_cast<std::uintptr_t>(guest.Data());
    const auto input = descriptor + test.base;
    const auto output = descriptor + OutputBase;
    const std::array<std::uint32_t, 16> userData{
        0u, 0u, 0u, 0u,
        static_cast<std::uint32_t>(descriptor), static_cast<std::uint32_t>((descriptor >> 32u) & 0xffffu), 16u, 0x31016facu,
        static_cast<std::uint32_t>(input), static_cast<std::uint32_t>((input >> 32u) & 0xffffu) | (test.wrappedIndex ? 0x00080000u : test.swizzle ? 0x84000000u : test.stride << 16u), 0u, 0u,
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), Threads * 16u, 0x31016facu};
    const auto code = Code(components, coherent, barrier, test);
    const auto words = std::span(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}},
        {wave, 0u, userData, compute, std::nullopt, std::nullopt, regions},
        device.ComputeTarget(32), {0u, 0u, 0u, 128u}};
    request.useCache = false;
    const auto shader = ShaderRecompiler::Recompile(request);
    if (checkSpan) Require(HasVectorSpan(shader.spirv, (1u << components) - 1u), "runtime buffer vector load was split into one span lookup per component");
    device.Dispatch(shader, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    if (test.fault) return;
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        if (test.masked && (lane & 1u) != 0u) continue;
        for (std::uint32_t component = 0; component < components; ++component) {
            const std::uint32_t offset = (test.wrappedIndex ? 0xfffffff8u : test.wrap ? 0xfffffffcu : test.stride != 0u ? 0u : lane * 16u) + component * 4u;
            std::uint32_t value = 0;
            const bool inBounds = test.stride != 0u ? lane < test.records && offset < test.stride : test.records >= 4u && offset <= test.records - 4u && (!test.swizzle || test.wrappedIndex || offset <= 1020u);
            if (inBounds) {
                const auto physical = test.base + (test.wrappedIndex ? offset + 8u : test.stride != 0u ? lane * test.stride + offset : offset * (test.swizzle ? 8u : 1u));
                std::memcpy(&value, reinterpret_cast<const std::byte*>(expected.data()) + physical, 4u);
            }
            expected[OutputBase / 4u + lane * 4u + component] = value;
        }
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (guest.Data()[index] == expected[index]) continue;
        std::fprintf(stderr, "runtime buffer spans: %s x%u wave%u glc=%d barrier=%d byte=%zu actual=%08x expected=%08x\n", test.name, components, wave, coherent, barrier, index * 4u, guest.Data()[index], expected[index]);
        Require(false, "runtime buffer span output or guard differs");
    }
}

}

int main(int argc, char** argv) {
    try {
        const auto components = argc > 1 ? static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10)) : 4u;
        Require(components >= 2u && components <= 4u, "runtime buffer spans: invalid component count");
        const bool checkSpan = argc > 2 && std::string_view(argv[2]) == "--check-spans";
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        if (argc > 2 && std::string_view(argv[2]) == "--fault") {
            const Case test{"unmapped", TotalBytes - 4, 16, false, false, 0, false, true};
            char address[48];
            std::snprintf(address, sizeof(address), "address=0x%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(guest.Data()) + TotalBytes));
            StderrCapture log;
            std::string message;
            try {
                Run(*device, guest, components, 64u, false, argc > 3, test, false);
            } catch (const std::runtime_error& error) {
                message = error.what();
            }
            message += log.Finish();
            Require(message.find("BDA access failed:") != std::string::npos && message.find(address) != std::string::npos && message.find("instruction=0x1c") != std::string::npos && message.find("bytes=1 ") != std::string::npos && message.find("reason=1") != std::string::npos, "runtime buffer span fault lost its address, PC, width or reason: " + message);
            std::puts("runtime buffer span fault preserves unmapped address, instruction and byte width");
            return 0;
        }
        unsigned checked = 0;
        for (const auto wave : {32u, 64u}) {
            for (const bool coherent : {false, true}) {
                for (const bool barrier : {false, true}) {
                    if (argc == 1 && barrier) continue;
                    for (const auto& test : Cases) {
                        Run(*device, guest, components, wave, coherent, barrier, test, checkSpan);
                        ++checked;
                    }
                }
            }
        }
        std::printf("runtime buffer x%u span tests passed: %u alignment/bounds/swizzle/wrap/split/coherent/barrier/EXEC/structured cases\n", components, checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
