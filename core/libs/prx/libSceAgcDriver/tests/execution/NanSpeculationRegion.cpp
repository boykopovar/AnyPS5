#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include "F16LaneModel.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>
#include <spirv/unified1/spirv.hpp>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;
using namespace F16LaneModel;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Words = 8;

constexpr std::array<std::uint16_t, 16> Halves{
    0x0000u, 0x8000u, 0x3e00u, 0xc000u, 0x3400u, 0x7bffu, 0x0001u, 0x7c00u,
    0xfc00u, 0x7e01u, 0x7c05u, 0xfe33u, 0x3c00u, 0x83ffu, 0x5640u, 0xbc00u,
};

alignas(256) constexpr std::array<std::uint32_t, 25> Code{
    0x34140084u, 0x34160085u, 0xe03c1000u, 0x8001010au, 0xbf8c3f70u, 0xcc0e4004u, 0x1c0e0501u, 0xcc104001u,
    0x18020901u, 0xcc0f4002u, 0x18020302u, 0xcc124005u, 0x18020704u, 0x7d920704u, 0xd5010006u, 0x01a90280u,
    0xcc0e4003u, 0x1c060703u, 0xd74b0007u, 0x04060504u, 0xe0781000u, 0x8000010bu, 0xe0781010u, 0x8000040bu,
    0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 54> SelectWebCode{
    0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf40c0200u, 0xfa000000u, 0xf40c0700u, 0xfa000060u, 0xbf8cc07fu, 0xd7650000u, 0x000100c1u, 0x7d880090u,
    0x06280100u, 0x7e14021cu, 0x02041408u, 0x7e16021du, 0x02061609u, 0x7e18021eu, 0x0208180au, 0x7e1a021fu, 0x020a1a0bu, 0xbf068010u, 0xbf85000bu, 0x7d880088u,
    0x06280100u, 0x7e1c0220u, 0x020c1c0cu, 0x7e1e0221u, 0x020e1e0du, 0x7e200222u, 0x0210200eu, 0x7e220223u, 0x0212220fu, 0xbf820004u, 0x7e0c020cu, 0x7e0e020du,
    0x7e10020eu, 0x7e12020fu, 0x7e100502u, 0x7e120503u, 0x7e140504u, 0x7e160505u, 0x7e180506u, 0x7e1a0507u, 0x7e1c0508u, 0x7e1e0509u, 0xf09c0f08u, 0x00a20000u,
    0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u,
};

constexpr std::uint32_t Format8888UNorm = 56;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint8_t FirstTexel = 0xffu;
constexpr std::uint8_t SecondTexel = 0x80u;

alignas(256) std::array<std::uint8_t, 256> FirstTexture{};
alignas(256) std::array<std::uint8_t, 256> SecondTexture{};
alignas(256) std::array<std::uint32_t, 32> Root{};
alignas(256) std::array<std::uint32_t, 4> WebOutput{};
alignas(256) std::array<std::uint32_t, Threads * 4> Inputs{};
alignas(256) std::array<std::uint32_t, Threads * Words> Outputs{};

void EnableSpeculation() {
#ifdef _WIN32
    _putenv_s("APS5_NAN_SPECULATION", "1");
    _putenv_s("APS5_NAN_SPECULATION_MIN", "1");
#else
    setenv("APS5_NAN_SPECULATION", "1", 1);
    setenv("APS5_NAN_SPECULATION_MIN", "1", 1);
#endif
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

double HalfValue(std::uint32_t half) {
    const auto scaled = Scaled(half);
    return static_cast<double>(scaled) / 16777216.0;
}

bool HalfLess(std::uint32_t lhs, std::uint32_t rhs) {
    if (Nan(lhs) || Nan(rhs)) return false;
    if (Infinite(lhs) || Infinite(rhs)) {
        const auto rank = [](std::uint32_t half) { return Infinite(half) ? ((half & 0x8000u) != 0u ? -1 : 1) : 0; };
        if (rank(lhs) != rank(rhs)) return rank(lhs) < rank(rhs);
        return false;
    }
    return HalfValue(lhs) < HalfValue(rhs);
}

std::uint32_t Packed(HalfOperation operation, std::uint32_t a, std::uint32_t b, std::uint32_t c, bool ieee) {
    const auto low = Arithmetic(operation, a & 0xffffu, b & 0xffffu, c & 0xffffu, ieee);
    const auto high = Arithmetic(operation, a >> 16u, b >> 16u, c >> 16u, ieee);
    return (low & 0xffffu) | (high << 16u);
}

std::uint32_t PackedMax(std::uint32_t a, std::uint32_t b, bool ieee) {
    return (MinMax(true, a & 0xffffu, b & 0xffffu, ieee) & 0xffffu) | (MinMax(true, a >> 16u, b >> 16u, ieee) << 16u);
}

void Fill() {
    std::uint32_t state = 0x9e3779b9u;
    const auto next = [&] {
        state = state * 1664525u + 1013904223u;
        return Halves[state >> 28u];
    };
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        for (std::uint32_t source = 0; source < 3u; ++source) {
            const std::uint32_t low = next();
            const std::uint32_t high = next();
            Inputs[lane * 4u + source] = low | (high << 16u);
        }
        Inputs[lane * 4u + 3u] = 0u;
    }
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::size_t SelectionMerges(std::span<const std::uint32_t> words) {
    std::size_t merges = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        Require(count != 0u && count <= words.size() - offset, "nan speculation region: the recompiler returned a malformed SPIR-V instruction");
        if ((words[offset] & spv::OpCodeMask) == spv::OpSelectionMerge) ++merges;
        offset += count;
    }
    return merges;
}

std::size_t Run(AgcDriver::VulkanDevice& device, bool ieee) {
    Outputs.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Outputs.data(), static_cast<std::uint32_t>(Outputs.size() * 4u));
    const auto input = BufferDescriptor(Inputs.data(), static_cast<std::uint32_t>(Inputs.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(input.begin(), input.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, ieee, false};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    Require(SelectionMerges(result.spirv.Words()) >= 2u, "nan speculation region: the recompiled shader has no speculated region");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::size_t failures = 0;
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto a = Inputs[lane * 4u];
        const auto b = Inputs[lane * 4u + 1u];
        const auto c = Inputs[lane * 4u + 2u];
        const auto v4 = Packed(HalfOperation::Fma, a, b, c, ieee);
        const auto v1 = Packed(HalfOperation::Mul, a, v4, 0u, ieee);
        const auto v2 = Packed(HalfOperation::Add, b, v1, 0u, ieee);
        const auto v5 = PackedMax(v4, c, ieee);
        const auto v6 = HalfLess(v4 & 0xffffu, c & 0xffffu) ? 1u : 0u;
        const auto v3 = Packed(HalfOperation::Fma, c, c, v1, ieee);
        const std::array<std::uint32_t, Words> expected{v1, v2, v3, v4, v4, v5, v6, 0u};
        const auto* actual = &Outputs[lane * Words];
        for (std::uint32_t word = 0; word < Words; ++word) {
            bool matches = actual[word] == expected[word];
            if (word == 7u) {
                const auto x = v4 & 0xffffu;
                const auto y = v2 & 0xffffu;
                const auto z = v1 & 0xffffu;
                const auto low = actual[word] & 0xffffu;
                matches = Nan(x) || Nan(y) || Nan(z) ? Nan(low) : low == (Arithmetic(HalfOperation::Fma, x, y, z, ieee) & 0xffffu);
            }
            if (matches) continue;
            if (failures++ < 12u) std::cerr << (ieee ? "IEEE 1" : "IEEE 0") << " lane " << lane << " sources " << Hex(a) << " " << Hex(b) << " " << Hex(c) << " word " << word << ": " << Hex(actual[word]) << ", expected " << Hex(expected[word]) << '\n';
        }
    }
    return failures;
}

void RequireGuardedDescriptorSkipped(AgcDriver::VulkanDevice& device) {
    alignas(256) static constexpr std::array<std::uint32_t, 18> GuardedCode{
        0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0x7da80010u, 0xbf880006u,
        0xf40c0200u, 0xfa000000u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u,
    };
    Root = {};
    const std::array<std::uint32_t, 8> invalid{0x43700000u, 0x43070000u, 0x3b888889u, 0x3bf2b9d6u, 0x3b088889u, 0x3b72b9d6u, 0x3f7f7777u, 0x3f7f0d46u};
    const auto output = BufferDescriptor(WebOutput.data(), static_cast<std::uint32_t>(sizeof(WebOutput)));
    std::copy(invalid.begin(), invalid.end(), Root.begin());
    std::copy(output.begin(), output.end(), Root.begin() + 16);
    const auto root = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Root.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(root), static_cast<std::uint32_t>(root >> 32u)};
    const std::span<const std::uint32_t> code(GuardedCode);
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32u, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, {}},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false};
    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    Require(capture->snapshot.images.size() == 1u && std::ranges::all_of(capture->snapshot.images[0].dwords, [](std::uint32_t word) { return word == 0u; }), "nan speculation region: a region holding only the EXEC compare hid that no lane takes the branch, so the T# behind it was read");
}

struct PruneCase {
    const char* name;
    std::vector<std::uint32_t> consumer;
    std::uint32_t (*apply)(std::uint32_t);
    std::uint32_t checked;
    std::uint32_t a;
    bool partialExec = false;
};

constexpr std::uint32_t PackedChecked = 0;
constexpr std::uint32_t LowChecked = 1;
constexpr std::uint32_t WordChecked = 2;
constexpr std::uint32_t QuietOne = 0x3c003c00u;
constexpr std::uint32_t Half = 0x38003800u;
constexpr std::uint32_t LowSignaling = 0x3c007c05u;
constexpr std::uint32_t HighSignaling = 0x7c053c00u;
constexpr std::uint32_t WordSignaling = 0x7f800005u;

std::uint32_t WordModel(std::uint32_t a, std::uint32_t b, bool ieee, bool add) {
    if ((a & 0x7fffffffu) > 0x7f800000u) return ieee ? a | 0x00400000u : a;
    const float lhs = std::bit_cast<float>(a);
    const float rhs = std::bit_cast<float>(b);
    return std::bit_cast<std::uint32_t>(add ? lhs + rhs : lhs * rhs);
}

std::vector<PruneCase> PruneCases() {
    return {
        {"copy", {0x7e0a0304u}, [](std::uint32_t v) { return v; }, PackedChecked, LowSignaling},
        {"shift left 16", {0x340a0890u}, [](std::uint32_t v) { return v << 16u; }, PackedChecked, HighSignaling},
        {"shift into an unchecked lane", {0x340a0890u}, [](std::uint32_t v) { return v << 16u; }, LowChecked, LowSignaling},
        {"next region", {0xbf800000u, 0x7e0a0304u}, [](std::uint32_t v) { return v; }, PackedChecked, LowSignaling},
        {"shift right 16", {0x2c0a0890u}, [](std::uint32_t v) { return v >> 16u; }, PackedChecked, LowSignaling},
        {"extract high", {0xd5480005u, 0x02412104u}, [](std::uint32_t v) { return v >> 16u; }, LowChecked, LowSignaling},
        {"mask low lane", {0x360a08ffu, 0xffff0000u}, [](std::uint32_t v) { return v & 0xffff0000u; }, PackedChecked, LowSignaling},
        {"flip magnitude", {0x3a0a08ffu, 0x7fff7fffu}, [](std::uint32_t v) { return v ^ 0x7fff7fffu; }, PackedChecked, LowSignaling},
        {"flip sign", {0x3a0a08ffu, 0x80008000u}, [](std::uint32_t v) { return v ^ 0x80008000u; }, PackedChecked, LowSignaling},
        {"set mantissa", {0x380a08ffu, 0x00010001u}, [](std::uint32_t v) { return v | 0x00010001u; }, PackedChecked, LowSignaling},
        {"select zero", {0x020a0f04u}, [](std::uint32_t) { return 0u; }, PackedChecked, LowSignaling},
        {"word mask", {0x360a08ffu, 0x00ffffffu}, [](std::uint32_t v) { return v & 0x00ffffffu; }, WordChecked, WordSignaling},
        {"word copy", {0x7e0a0304u}, [](std::uint32_t v) { return v; }, WordChecked, WordSignaling},
        {"copy under partial exec", {0x7e0a0304u}, [](std::uint32_t v) { return v; }, PackedChecked, LowSignaling, true},
    };
}

std::vector<std::uint32_t> PruneProgram(const PruneCase& test) {
    const bool word = test.checked == WordChecked;
    std::vector<std::uint32_t> code{0x34140084u, 0x34160084u, 0xe03c1000u, 0x8001010au, 0xbf8c3f70u};
    if (test.partialExec) code.insert(code.end(), {0x7d8800b0u, 0xbe88246au});
    code.insert(code.end(), {0x7e0e0280u, 0x7d8800c0u});
    if (word) code.push_back(0x10080501u);
    else code.insert(code.end(), {0xcc104004u, 0x18020501u});
    code.insert(code.end(), test.consumer.begin(), test.consumer.end());
    if (test.checked == PackedChecked) code.insert(code.end(), {0xcc0f4006u, 0x18020705u});
    else if (test.checked == LowChecked) code.insert(code.end(), {0xd74b0006u, 0x040e0705u});
    else code.push_back(0x060c0705u);
    code.insert(code.end(), {0xe0781000u, 0x8000040bu, 0xbf810000u});
    return code;
}

std::size_t PackedNanTests(std::span<const std::uint32_t> words) {
    std::uint32_t carry = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        if ((words[offset] & spv::OpCodeMask) == spv::OpConstant && count == 4u && words[offset + 3u] == 0x03ff03ffu) carry = words[offset + 2u];
        offset += count;
    }
    std::size_t tests = 0;
    for (std::size_t offset = 5; carry != 0u && offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        if ((words[offset] & spv::OpCodeMask) == spv::OpIAdd && count == 5u && (words[offset + 3u] == carry || words[offset + 4u] == carry)) ++tests;
        offset += count;
    }
    return tests;
}

std::size_t WordNanTests(std::span<const std::uint32_t> words) {
    std::size_t tests = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        if ((words[offset] & spv::OpCodeMask) == spv::OpFUnordNotEqual && count == 5u && words[offset + 3u] == words[offset + 4u]) ++tests;
        offset += count;
    }
    return tests;
}

std::size_t RunPruneCase(AgcDriver::VulkanDevice& device, const PruneCase& test, bool ieee) {
    const bool word = test.checked == WordChecked;
    const std::uint32_t b = word ? 0x3f800000u : QuietOne;
    const std::uint32_t c = word ? 0x3f000000u : Half;
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        Inputs[lane * 4u] = test.a;
        Inputs[lane * 4u + 1u] = b;
        Inputs[lane * 4u + 2u] = c;
        Inputs[lane * 4u + 3u] = 0u;
    }
    Outputs.fill(0xdeadbeefu);
    alignas(256) static std::array<std::uint32_t, 32> code{};
    const auto program = PruneProgram(test);
    std::copy(program.begin(), program.end(), code.begin());
    std::vector<std::uint32_t> userData(8, 0u);
    const auto output = BufferDescriptor(Outputs.data(), static_cast<std::uint32_t>(Outputs.size() * 4u));
    const auto input = BufferDescriptor(Inputs.data(), static_cast<std::uint32_t>(Inputs.size() * 4u));
    std::copy(output.begin(), output.end(), userData.begin());
    std::copy(input.begin(), input.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> words(code.data(), program.size());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(words.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(words.data()), words, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, ieee, false};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    std::size_t failures = 0;
    if ((std::string(test.name) == "copy" || test.partialExec) && PackedNanTests(result.spirv.Words()) != 1u) {
        std::cerr << "nan flag pruning: a packed product read only by a checked packed sum kept its own NaN test (" << PackedNanTests(result.spirv.Words()) << " tests)\n";
        ++failures;
    }
    if (std::string(test.name) == "word copy" && WordNanTests(result.spirv.Words()) != 1u) {
        std::cerr << "nan flag pruning: an f32 product read only by a checked f32 sum kept its own NaN test (" << WordNanTests(result.spirv.Words()) << " tests)\n";
        ++failures;
    }
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(words.data()));
    device.WaitIdle();
    const std::uint32_t v4 = word ? WordModel(test.a, b, ieee, false) : Packed(HalfOperation::Mul, test.a, b, 0u, ieee);
    const std::uint32_t v5 = test.apply(v4);
    std::uint32_t v6 = 0;
    if (test.checked == PackedChecked) v6 = Packed(HalfOperation::Add, v5, c, 0u, ieee);
    else if (test.checked == LowChecked) v6 = Arithmetic(HalfOperation::Fma, v5 & 0xffffu, c & 0xffffu, c & 0xffffu, ieee) & 0xffffu;
    else v6 = WordModel(v5, c, ieee, true);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const auto* actual = &Outputs[lane * 4u];
        const std::uint32_t w = test.checked == LowChecked ? actual[2] & 0xffffu : actual[2];
        if (test.partialExec && lane >= 48u) {
            if (actual[0] == 0xdeadbeefu && actual[1] == 0xdeadbeefu && actual[2] == 0xdeadbeefu) continue;
        } else if (actual[0] == v4 && actual[1] == v5 && w == v6) {
            continue;
        }
        if (failures++ < 4u) std::cerr << "nan flag pruning: " << test.name << (ieee ? " IEEE 1" : " IEEE 0") << " lane " << lane << ": " << Hex(actual[0]) << " " << Hex(actual[1]) << " " << Hex(w) << ", expected " << Hex(v4) << " " << Hex(v5) << " " << Hex(v6) << '\n';
    }
    return failures;
}

std::array<std::uint32_t, 8> ImageDescriptor(const std::array<std::uint8_t, 256>& texture) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.data()));
    return {static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
}

void RunSelectWeb(AgcDriver::VulkanDevice& device, std::uint32_t mode) {
    FirstTexture.fill(FirstTexel);
    SecondTexture.fill(SecondTexel);
    WebOutput.fill(0xdeadbeefu);
    Root = {};
    const auto first = ImageDescriptor(FirstTexture);
    const auto second = ImageDescriptor(SecondTexture);
    const auto output = BufferDescriptor(WebOutput.data(), static_cast<std::uint32_t>(sizeof(WebOutput)));
    std::copy(first.begin(), first.end(), Root.begin());
    std::copy(output.begin(), output.end(), Root.begin() + 16);
    Root[19] = 0x30027facu;
    Root[20] = mode;
    std::copy(second.begin(), second.end(), Root.begin() + 24);
    const auto root = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Root.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(root), static_cast<std::uint32_t>(root >> 32u)};
    const std::span<const std::uint32_t> code(SelectWebCode);
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32u, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, {}},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false};
    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    const auto regions = memory.Regions();
    request.context.memory = regions;
    const auto result = ShaderRecompiler::Recompile(request, *capture);
    Require(SelectionMerges(result->spirv.Words()) >= 2u, "nan speculation region: the descriptor select shader has no speculated region");
    device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    const float texel = std::bit_cast<float>(WebOutput[0]);
    Require(std::fabs(texel - static_cast<float>(SecondTexel) / 255.0f) < 0.001f, "nan speculation region: per-lane T# selects merged after a speculated region did not read the texture lane 0 selects (mode " + std::to_string(mode) + ", word " + Hex(WebOutput[0]) + ")");
}

}

int main() {
    try {
        EnableSpeculation();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Fill();
        std::size_t failures = 0;
        for (const bool ieee : {false, true}) failures += Run(*device, ieee);
        for (const std::uint32_t mode : {0u, 1u}) RunSelectWeb(*device, mode);
        RequireGuardedDescriptorSkipped(*device);
        std::size_t pruneFailures = 0;
        for (const auto& test : PruneCases()) {
            for (const bool ieee : {false, true}) pruneFailures += RunPruneCase(*device, test, ieee);
        }
        Require(pruneFailures == 0u, "nan speculation region: " + std::to_string(pruneFailures) + " NaN flag pruning results differ");
        Require(failures == 0u, "nan speculation region: " + std::to_string(failures) + " results differ from the lane model");
        std::puts("nan speculation region tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
