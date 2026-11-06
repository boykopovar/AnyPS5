#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::DriverDetail::DispatchVariant;
using AgcDriver::DriverDetail::DrawProgram;
using AgcDriver::DriverDetail::Driver;
using AgcDriver::DriverDetail::RelocatableUserWord;
using ShaderRecompiler::RecompileResult;

alignas(256) constexpr std::array<std::uint32_t, 2> SwappcCode{0xbe822100u, 0xbf810000u};
alignas(256) std::array<std::uint32_t, 1024> FirstCallee{};
alignas(256) std::array<std::uint32_t, 1024> SecondCallee{};
alignas(256) std::array<std::uint32_t, 1024> OtherCallee{};
alignas(256) std::array<std::uint32_t, 64> Buffer{};

void Fill(std::array<std::uint32_t, 1024>& callee, std::uint32_t first) {
    callee.fill(0xbf810000u);
    callee[0] = first;
    callee[1] = 0xbe802002u;
}

std::uint32_t Low(const void* address) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address));
}

std::uint32_t High(const void* address) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(address)) >> 32u);
}

DrawProgram VertexProgram(const void* callee) {
    DrawProgram program{};
    program.binary.stage = ShaderRecompiler::ShaderStage::Vertex;
    program.binary.code = SwappcCode;
    program.firstUserSgpr = 0;
    program.userData = {Low(callee), High(callee), 0u, 0u};
    return program;
}

std::shared_ptr<DispatchVariant> Variant(std::shared_ptr<const RecompileResult> compiled) {
    auto variant = std::make_shared<DispatchVariant>();
    variant->compiled = std::move(compiled);
    return variant;
}

ShaderRecompiler::DescriptorBinding GuestBuffer(std::vector<std::uint32_t> descriptor, bool written) {
    ShaderRecompiler::DescriptorBinding binding{ShaderRecompiler::DescriptorKind::StorageBuffer, ShaderRecompiler::DescriptorRole::GuestBuffers, 0, 0, 1, std::move(descriptor)};
    binding.bufferWritten = {written};
    binding.bufferAtomic = {false};
    binding.bufferRead = {true};
    return binding;
}

void CalleeWordsAreKeyedByTheirCode() {
    ShaderRecompiler::SetGcnGuestShaders(true);
    Fill(FirstCallee, 0x7e000280u);
    Fill(SecondCallee, 0x7e000280u);
    Fill(OtherCallee, 0x7e000281u);
    const auto program = VertexProgram(FirstCallee.data());
    const auto variant = Variant(std::make_shared<RecompileResult>());
    Driver::tagUserValueWords(*variant, program);
    Require(variant->calleeWords == 1u && variant->calleeReads.size() == 1u && variant->calleeReads[0].first == 0u, "the fetch subroutine pointer in s[0:1] was not tagged as a callee word (mask " + std::to_string(variant->calleeWords) + ")");
    Require(variant->calleeReads[0].second == 2u, "the callee was measured as " + std::to_string(variant->calleeReads[0].second) + " words, expected 2");
    const RelocatableUserWord word{0u, {{0, variant->calleeReads[0].second}}, false, High(FirstCallee.data())};
    bool readable = true;
    const auto first = Driver::relocatedContentKey(word, Low(FirstCallee.data()), readable);
    const auto second = Driver::relocatedContentKey(word, Low(SecondCallee.data()), readable);
    const auto other = Driver::relocatedContentKey(word, Low(OtherCallee.data()), readable);
    Require(readable, "a callee in mapped memory was reported unreadable");
    Require(first == second, "two copies of the same callee code at different addresses keyed differently");
    Require(first != other, "callees with different code keyed the same");

    auto bound = std::make_shared<RecompileResult>();
    bound->bindings.push_back(GuestBuffer({Low(FirstCallee.data()), 0u, 16u, 0u}, false));
    const auto boundVariant = Variant(bound);
    Driver::tagUserValueWords(*boundVariant, program);
    Require(boundVariant->calleeWords == 0u, "a call target that also feeds a bound descriptor was tagged as a callee word");

    ShaderRecompiler::SetGcnGuestShaders(false);
    const auto rdna = Variant(std::make_shared<RecompileResult>());
    Driver::tagUserValueWords(*rdna, program);
    Require(rdna->calleeWords == 0u, "a vertex program was tagged for GCN user-data callees outside GCN guest shaders");
}

void BufferValueWordsArePatchedInTheirBinding() {
    ShaderRecompiler::SetGcnGuestShaders(false);
    const std::vector<std::uint32_t> descriptor{Low(Buffer.data()), High(Buffer.data()) | (16u << 16u), 4u, 0x20077facu};
    DrawProgram program{};
    program.binary.stage = ShaderRecompiler::ShaderStage::Fragment;
    program.firstUserSgpr = 0;
    program.userData = {0u, 0u, 0u, 0u, descriptor[0], descriptor[1], descriptor[2], descriptor[3]};
    const auto result = [&](bool written) {
        auto compiled = std::make_shared<RecompileResult>();
        compiled->bindings.push_back(GuestBuffer(descriptor, written));
        return compiled;
    };

    const auto variant = Variant(result(false));
    Driver::tagUserValueWords(*variant, program);
    Require(variant->valueWords == ((1u << 4u) | (1u << 6u)), "the base and record words of a read-only V# in user data were not tagged (mask " + std::to_string(variant->valueWords) + ")");
    Require(variant->valueSlots.size() == 2u && variant->valueSlots[0] == std::array<std::uint32_t, 3>{4u, 0u, 0u} && variant->valueSlots[1] == std::array<std::uint32_t, 3>{6u, 0u, 2u}, "the tagged V# words do not name their binding words");
    variant->descriptorPatches = {{0u, 0u, descriptor[0] + 0x100u}, {0u, 2u, 7u}};
    Driver::applyShaderDataPatches(*variant);
    const auto& patched = variant->compiled->bindings[0].guestDescriptor;
    Require(patched[0] == descriptor[0] + 0x100u && patched[1] == descriptor[1] && patched[2] == 7u && patched[3] == descriptor[3], "the descriptor patches did not move the bound V# base and record count");
    Require(variant->descriptorPatches.empty(), "the descriptor patches were kept after they were applied");

    const auto written = Variant(result(true));
    Driver::tagUserValueWords(*written, program);
    Require(written->valueWords == 0u, "a V# the shader writes through was tagged as relocatable");

    auto pushedResult = result(false);
    pushedResult->pushConstants.resize(sizeof(std::uint32_t));
    std::memcpy(pushedResult->pushConstants.data(), &descriptor[0], sizeof(std::uint32_t));
    const auto pushed = Variant(pushedResult);
    Driver::tagUserValueWords(*pushed, program);
    Require((pushed->valueWords >> 4u & 1u) == 0u, "a V# base that is also a push constant word not forwarded from it was tagged");
    auto forwardedResult = std::make_shared<RecompileResult>(*pushedResult);
    forwardedResult->shaderDataUserWords = {4u};
    const auto forwarded = Variant(forwardedResult);
    Driver::tagUserValueWords(*forwarded, program);
    Require((forwarded->valueWords >> 4u & 1u) != 0u, "a V# base forwarded from its own user word as push data was not tagged");

    const auto captured = Variant(result(false));
    captured->runs = {{reinterpret_cast<std::uintptr_t>(Buffer.data()) + 16u, reinterpret_cast<std::uintptr_t>(Buffer.data()) + 32u}};
    Driver::tagUserValueWords(*captured, program);
    Require(captured->valueWords == 0u, "a V# whose memory the stage captured was tagged as relocatable");

    const auto pointer = Variant(result(false));
    pointer->relocatableWords = 1u << 4u;
    Driver::tagUserValueWords(*pointer, program);
    Require((pointer->valueWords >> 4u & 1u) == 0u, "a user word relocated as a pointer was also tagged as a V# value");

    const auto shared = [&](std::uint32_t otherRecords) {
        auto compiled = result(false);
        compiled->bindings.push_back(GuestBuffer({Low(Buffer.data()) + 0x100u, descriptor[1], otherRecords, descriptor[3]}, false));
        compiled->bindings.back().binding = 1;
        const auto variant = Variant(compiled);
        Driver::tagUserValueWords(*variant, program);
        return variant;
    };
    const auto sharedCount = shared(descriptor[2]);
    Require((sharedCount->valueWords >> 4u & 1u) != 0u && (sharedCount->valueWords >> 6u & 1u) == 0u, "a record count that also builds a second descriptor was tagged (mask " + std::to_string(sharedCount->valueWords) + ")");
    Require(sharedCount->valueSlots.size() == 1u && sharedCount->valueSlots[0] == std::array<std::uint32_t, 3>{4u, 0u, 0u}, "a V# whose record count builds a second descriptor recorded a record-count patch");
    const auto ownCount = shared(descriptor[2] + 1u);
    Require((ownCount->valueWords >> 6u & 1u) != 0u, "a record count used by one descriptor only was not tagged next to an independent second descriptor");
}

alignas(256) constexpr std::array<std::uint32_t, 16> VertexCode{
    0x7e000d05u, 0x080200f4u, 0x10020300u, 0x100202f4u, 0x060202f3u, 0x060400f3u, 0x10040500u, 0x060404f3u,
    0x7e060280u, 0x7e0802f2u, 0xf80008cfu, 0x04030201u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 16> SharedCountPixelCode{
    0xbe880304u, 0xbe890305u, 0xbe8a0302u, 0xbe8b0307u, 0xf4200300u, 0xfa000000u, 0xf4200344u, 0xfa000004u,
    0xbf8cc07fu, 0x7e00020cu, 0x7e02020du, 0x7e040280u, 0x7e0602f2u, 0xf800180fu, 0x03020100u, 0xbf810000u,
};
alignas(256) constexpr std::array<std::uint32_t, 16> OwnCountPixelCode{
    0xbe880304u, 0xbe890305u, 0xbe8a0306u, 0xbe8b0307u, 0xf4200300u, 0xfa000000u, 0xf4200344u, 0xfa000004u,
    0xbf8cc07fu, 0x7e00020cu, 0x7e02020du, 0x7e040280u, 0x7e0602f2u, 0xf800180fu, 0x03020100u, 0xbf810000u,
};
alignas(256) std::array<std::byte, 1024> ColorMemory{};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header SharedCountHeader{};
alignas(8) Header OwnCountHeader{};

void Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::array<std::uint8_t, 4> DrawWithUserData(const std::uint32_t* pixelCode, const std::array<std::uint32_t, 8>& userData) {
    constexpr std::byte Kept{0x40};
    ColorMemory.fill(Kept);
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    const auto vertex = reinterpret_cast<std::uintptr_t>(VertexCode.data());
    const auto pixel = reinterpret_cast<std::uintptr_t>(pixelCode);
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
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : std::initializer_list<std::pair<std::uint32_t, std::uint32_t>>{
             {0xc8, static_cast<std::uint32_t>(vertex >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertex >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
             {0x008, static_cast<std::uint32_t>(pixel >> 8u)}, {0x009, static_cast<std::uint32_t>(pixel >> 40u)}, {0x00b, 8u << 1u}}) {
        Append(commands, 0x76, {offset, value});
    }
    for (std::uint32_t k = 0; k < userData.size(); ++k) Append(commands, 0x76, {0x00c + k, userData[k]});
    Append(commands, 0x2d, {3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(color, ColorMemory.size());
    std::array<std::uint8_t, 4> drawn{0x40, 0x40, 0x40, 0x40};
    for (std::size_t at = 0; at < ColorMemory.size(); at += 4) {
        const std::array<std::uint8_t, 4> pixel{static_cast<std::uint8_t>(ColorMemory[at]), static_cast<std::uint8_t>(ColorMemory[at + 1]), static_cast<std::uint8_t>(ColorMemory[at + 2]), static_cast<std::uint8_t>(ColorMemory[at + 3])};
        if (pixel != std::array<std::uint8_t, 4>{0x40, 0x40, 0x40, 0x40}) drawn = pixel;
    }
    return drawn;
}

void MovedRecordCountsReachEveryDescriptor(bool sharedCount) {
    const auto* pixelCode = sharedCount ? SharedCountPixelCode.data() : OwnCountPixelCode.data();
    Registered(VertexHeader, VertexCode.data(), sizeof(VertexCode), 2);
    Registered(sharedCount ? SharedCountHeader : OwnCountHeader, pixelCode, sizeof(SharedCountPixelCode), 1);
    const std::string mode = sharedCount ? "one count SGPR for both descriptors" : "a count SGPR for each descriptor";
    alignas(16) static std::array<std::array<float, 4>, 8> first{};
    alignas(16) static std::array<std::array<float, 4>, 8> second{};
    for (auto& entry : first) entry = {1.0f, 0.0f, 0.0f, 0.0f};
    for (auto& entry : second) entry = {0.0f, 1.0f, 0.0f, 0.0f};
    const auto userData = [&](std::size_t i, std::uint32_t records) {
        return std::array<std::uint32_t, 8>{Low(first[i].data()), High(first[i].data()), sharedCount ? records : 12u, 0x20077facu, Low(second[i].data()), High(second[i].data()), sharedCount ? 4u : records, 0x20077facu};
    };
    const auto start = Driver::Get().DrawCacheCounters();
    for (std::size_t i = 0; i < 5; ++i) {
        const auto pixel = DrawWithUserData(pixelCode, userData(i, 4u));
        Require(pixel == std::array<std::uint8_t, 4>{255, 0, 0, 255}, mode + ": a draw whose second descriptor holds one dword drew " + std::to_string(pixel[0]) + "," + std::to_string(pixel[1]) + "," + std::to_string(pixel[2]) + "," + std::to_string(pixel[3]));
    }
    const auto taught = Driver::Get().DrawCacheCounters();
    Require(taught.hits > start.hits, mode + ": draws whose descriptors moved to equal contents never reused the cached draw (" + std::to_string(taught.lookups - start.lookups) + " lookups)");
    const auto pixel = DrawWithUserData(pixelCode, userData(5, 8u));
    Require(pixel == std::array<std::uint8_t, 4>{255, 255, 0, 255}, mode + ": after the record count grew to two dwords, the second descriptor's second dword drew " + std::to_string(pixel[1]) + " (a reused draw kept the earlier count)");
    if (!sharedCount) {
        const auto after = Driver::Get().DrawCacheCounters();
        Require(after.hits > taught.hits, mode + ": a draw that only moved its own record counts did not reuse the cached draw");
    }
}

void BufferValueWordsKeyByNothing() {
    const RelocatableUserWord word{4u, {}, true, 0u};
    bool readable = true;
    const auto first = Driver::relocatedContentKey(word, 0x1000u, readable);
    const auto second = Driver::relocatedContentKey(word, 0x2002u, readable);
    Require(readable && first == second, "a V# value word changed the relocated content key");
}

}

int main() {
    try {
        CalleeWordsAreKeyedByTheirCode();
        BufferValueWordsArePatchedInTheirBinding();
        BufferValueWordsKeyByNothing();
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        MovedRecordCountsReachEveryDescriptor(true);
        MovedRecordCountsReachEveryDescriptor(false);
        AgcDriverShutdown_nid_postfix();
        std::cout << "User-data V# values and fetch subroutine pointers relocate\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
