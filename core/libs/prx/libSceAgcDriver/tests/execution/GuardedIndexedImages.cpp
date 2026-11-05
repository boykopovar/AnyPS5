#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "ShaderDiskCache.hpp"
#include "VulkanTestDevice.hpp"
#include "tests/TemporaryFile.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t TotalBytes = 4u * BlockBytes;
constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t OutputOffset = 1024;
constexpr std::uint32_t FirstTexels = 4096;
constexpr std::uint32_t SecondTexels = 8192;
constexpr std::uint32_t Guard = 0xdeadbeefu;
constexpr std::uint32_t Skipped = 0x11111111u;

constexpr std::array<std::uint32_t, 33> Code{
    0xf4000200u, 0xfa000000u, 0xf4000240u, 0xfa000004u, 0xbf8cc07fu, 0x8f088608u,
    0xf40c0401u, 0x10000000u, 0xbf8cc07fu, 0x7e020d00u, 0x060202f0u, 0x100202ffu,
    0x3c800000u, 0x7e0402f0u, 0x7e0802ffu, 0x11111111u, 0xf09c0108u, 0x00640301u,
    0xbf8c3f70u, 0x7daa0680u, 0xbf880007u, 0x8f098609u, 0xf40c0601u, 0x12000000u,
    0xbf8cc07fu, 0xf09c0108u, 0x00660401u,
    0xbf8c3f70u, 0xbefe04c1u, 0x340a0082u, 0xe0701000u, 0x80010405u, 0xbf810000u};

constexpr auto DirectCode = [] {
    auto code = Code;
    code[2] = 0xbe89030au;
    code[3] = 0xbf800000u;
    return code;
}();

constexpr auto OverflowCode = [] {
    auto code = Code;
    code[22] = 0xf40c0605u;
    return code;
}();

constexpr auto UnderflowCode = [] {
    auto code = OverflowCode;
    code[23] = 0x121fe000u;
    return code;
}();

enum class Boundary { None, Above, Below };

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        allocation = VirtualAlloc(nullptr, TotalBytes, MEM_RESERVE, PAGE_NOACCESS);
        Require(allocation != nullptr, "indexed images: cannot reserve guest pages");
        data = static_cast<std::byte*>(allocation);
#else
        allocation = mmap(nullptr, TotalBytes + BlockBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED, "indexed images: cannot reserve guest pages");
        const auto aligned = (reinterpret_cast<std::uintptr_t>(allocation) + BlockBytes - 1u) & ~(BlockBytes - 1u);
        data = reinterpret_cast<std::byte*>(aligned);
#endif
        for (const auto block : {0u, 1u, 3u}) {
#ifdef _WIN32
            Require(VirtualAlloc(data + block * BlockBytes, BlockBytes, MEM_COMMIT, PAGE_READWRITE) == data + block * BlockBytes, "indexed images: cannot commit guest page");
#else
            Require(mprotect(data + block * BlockBytes, BlockBytes, PROT_READ | PROT_WRITE) == 0, "indexed images: cannot commit guest page");
#endif
            GuestAllocations::Mutation().Add(data + block * BlockBytes, BlockBytes, true, true, true);
        }
    }

    ~GuestBlock() {
        for (const auto block : {0u, 1u, 3u}) GuestAllocations::Mutation().Remove(data + block * BlockBytes);
#ifdef _WIN32
        VirtualFree(allocation, 0, MEM_RELEASE);
#else
        munmap(allocation, TotalBytes + BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint32_t* Words(std::size_t offset = 0) { return reinterpret_cast<std::uint32_t*>(data + offset); }
    std::uint64_t Address(std::size_t offset = 0) { return reinterpret_cast<std::uintptr_t>(data + offset); }

private:
    void* allocation = nullptr;
    std::byte* data = nullptr;
};

class StderrCapture {
public:
    StderrCapture() : file(TemporaryFile::Open()) {
        Require(file != nullptr, "indexed images: cannot capture diagnostics");
        std::fflush(stderr);
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        Require(saved >= 0 && _dup2(_fileno(file), _fileno(stderr)) == 0, "indexed images: cannot redirect stderr");
#else
        saved = dup(fileno(stderr));
        Require(saved >= 0 && dup2(fileno(file), fileno(stderr)) >= 0, "indexed images: cannot redirect stderr");
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

void Descriptor(std::uint32_t* words, std::uint64_t address) {
    std::fill_n(words, 8u, 0u);
    words[0] = static_cast<std::uint32_t>(address >> 8u);
    words[1] = static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (20u << 20u) | (3u << 30u);
    words[2] = 15u | (1u << 31u);
    words[3] = 0xfacu | (9u << 28u);
}

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, bool fragment, std::uint32_t wave, unsigned pattern, std::uint32_t index, bool direct = false, bool fault = false, Boundary boundary = Boundary::None) {
    for (const auto block : {0u, 1u, 3u}) std::fill_n(guest.Words(block * BlockBytes), BlockBytes / 4u, Guard);
    guest.Words()[0] = 0u;
    guest.Words()[1] = index;
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        guest.Words(FirstTexels)[lane] = pattern == 0u ? 0u : pattern == 1u ? 1u : lane & 1u;
        guest.Words(SecondTexels)[lane] = 1000u + lane;
    }
    Descriptor(guest.Words(BlockBytes), guest.Address(FirstTexels));
    Descriptor(guest.Words(3u * BlockBytes), guest.Address(SecondTexels));
    std::array<std::vector<std::uint32_t>, 3> expected;
    const std::array<unsigned, 3> blocks{0u, 1u, 3u};
    for (std::size_t i = 0; i < blocks.size(); ++i) expected[i].assign(guest.Words(blocks[i] * BlockBytes), guest.Words((blocks[i] + 1u) * BlockBytes));
    const auto table = guest.Address(BlockBytes);
    const auto root = guest.Address();
    const auto output = guest.Address(OutputOffset);
    const auto secondary = boundary == Boundary::Above ? 0xffffffffffe0ull : 0x1000ull;
    const std::array<std::uint32_t, 16> user{
        static_cast<std::uint32_t>(root), static_cast<std::uint32_t>(root >> 32u),
        static_cast<std::uint32_t>(table), static_cast<std::uint32_t>(table >> 32u),
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu),
        Lanes * 4u, 0x31016facu, 0u, 0u,
        boundary == Boundary::None ? index : static_cast<std::uint32_t>(secondary),
        boundary == Boundary::None ? 0u : static_cast<std::uint32_t>(secondary >> 32u),
        0u, (4u * 256u) << 12u, 1u << 24u, 0u};
    const auto code = std::span(boundary == Boundary::Above ? OverflowCode : boundary == Boundary::Below ? UnderflowCode : direct ? DirectCode : Code);
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {fragment ? ShaderStage::Fragment : ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}},
        {wave, 0u, user, compute, std::nullopt, std::nullopt, {}}, device.ComputeTarget(32), {0u, 0u, 0u, 128u}};
    if (fragment) {
        request.context.compute.reset();
        request.context.pixel = ShaderRecompiler::ShaderPixelStageInfo{};
    }
    request.useCache = false;
    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    const bool poisoned = index == 1024u || boundary != Boundary::None;
    Require(capture->plan->info.images.size() == 2u, "indexed images: the fixture lost a texture source");
    Require(!capture->snapshot.srtPoison.empty() == poisoned, "indexed images: the inaccessible descriptor was not recorded for deferred fault reporting");
    const auto regions = memory.Regions();
    request.context.memory = regions;
    std::vector<std::byte> encoded;
    Require(ShaderRecompiler::ShaderDiskCache::EncodePlan(*capture->plan, encoded), "indexed images: cannot encode resource plan");
    ShaderRecompiler::IrResourcePlan decoded;
    Require(ShaderRecompiler::ShaderDiskCache::DecodePlan(encoded, decoded), "indexed images: cannot decode resource plan");
    ShaderRecompiler::RequestMemoryView view(regions);
    ShaderRecompiler::ResourceSnapshot restored;
    ShaderRecompiler::ResourceMaterializer{}.Materialize(decoded, view.MakeRuntime(user, request.shader.codeAddress), restored);
    Require(restored.flattenedSrt == capture->snapshot.flattenedSrt && restored.srtPoison.size() == capture->snapshot.srtPoison.size(), "indexed images: cached plan changed flattened values or deferred faults");
    const auto compiled = ShaderRecompiler::Recompile(request, *capture);
    Require((compiled->poisonedSrtReads != 0u) == poisoned, "indexed images: the compiled shader lost deferred read faults");
    if (!fragment) {
        StderrCapture captureLog;
        std::string failure;
        try {
            device.Dispatch(*compiled, 1u, 1u, 1u, {}, request.shader.codeAddress);
            device.WaitIdle();
        } catch (const std::runtime_error& error) {
            failure = error.what();
        }
        const auto log = captureLog.Finish() + failure;
        if (fault) {
            char address[112];
            const auto badAddress = boundary == Boundary::Above ? 0x20ull : boundary == Boundary::Below ? 0xfffffffff000ull : guest.Address(2u * BlockBytes);
            std::snprintf(address, sizeof(address), "address=0x%llx instruction=0x58 bytes=4", static_cast<unsigned long long>(badAddress));
            Require(log.find("BDA access failed") != std::string::npos && log.find(address) != std::string::npos, "indexed images: an executed bad descriptor did not fault at its address and PC: " + log);
        } else {
            Require(failure.empty() && log.find("BDA access failed") == std::string::npos, "indexed images: a valid or skipped texture path faulted: " + log);
            for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
                const auto taken = pattern == 1u || (pattern == 2u && (lane & 1u) != 0u);
                expected[0][OutputOffset / 4u + lane] = taken ? index == 0u ? 1u : 1000u + lane : Skipped;
            }
        }
    }
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto* actual = guest.Words(blocks[i] * BlockBytes);
        for (std::size_t at = 0; at < expected[i].size(); ++at) {
            if (fault && i == 0u && at >= OutputOffset / 4u && at < OutputOffset / 4u + Lanes) {
                Require(actual[at] == Guard || actual[at] == 0u, "indexed images: a faulting read neither stopped nor returned zero");
                continue;
            }
            Require(actual[at] == expected[i][at], "indexed images: unexpected guest bytes at " + std::to_string(blocks[i] * BlockBytes + at * 4u));
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "";
        Require(argc <= 2 && (mode.empty() || mode == "--fragment" || mode == "--fault32" || mode == "--fault64" || mode == "--wrap" || mode == "--faultwrap32" || mode == "--faultwrap64"), "indexed images: unknown arguments");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        if (mode == "--fault32" || mode == "--fault64") {
            Run(*device, guest, false, mode == "--fault32" ? 32u : 64u, 1u, 1024u, false, true);
            std::puts("indexed texture descriptor: executed inaccessible read preserves fault address and PC");
            return 0;
        }
        if (mode == "--faultwrap32" || mode == "--faultwrap64") {
            const auto wave = mode == "--faultwrap32" ? 32u : 64u;
            Run(*device, guest, false, wave, 1u, 1u, false, true, Boundary::Above);
            Run(*device, guest, false, wave, 1u, 0u, false, true, Boundary::Below);
            std::puts("indexed texture descriptor address wrap preserves runtime faults");
            return 0;
        }
        if (mode == "--wrap") {
            for (const bool fragment : {false, true}) {
                for (const auto wave : {32u, 64u}) {
                    Run(*device, guest, fragment, wave, 0u, 1u, false, false, Boundary::Above);
                    Run(*device, guest, fragment, wave, 0u, 0u, false, false, Boundary::Below);
                }
            }
            std::puts("indexed texture descriptor address wrap in skipped compute and fragment paths passed");
            return 0;
        }
        const bool fragment = mode == "--fragment";
        for (const auto wave : {32u, 64u}) {
            Run(*device, guest, fragment, wave, 0u, 2048u);
            Run(*device, guest, fragment, wave, 1u, 2048u);
            Run(*device, guest, fragment, wave, 1u, 0u);
            Run(*device, guest, fragment, wave, 2u, 2048u);
            Run(*device, guest, fragment, wave, 0u, 1024u);
            bool refused = false;
            try {
                Run(*device, guest, fragment, wave, 0u, 1024u, true);
            } catch (const std::runtime_error& error) {
                refused = std::string(error.what()).find("guest memory is not readable") != std::string::npos;
            }
            Require(refused, "indexed images: an inaccessible direct user-data index no longer refuses capture");
        }
        std::puts(fragment ? "indexed texture descriptor fragment capture and cached-plan tests passed" : "indexed texture descriptor GPU branch, output guards and cached-plan tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
