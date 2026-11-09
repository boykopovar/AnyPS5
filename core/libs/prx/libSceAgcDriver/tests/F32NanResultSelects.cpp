#define SPV_ENABLE_UTILITY_CODE
#include <spirv/unified1/spirv.hpp>
#undef SPV_ENABLE_UTILITY_CODE
#include "Recompiler.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Instruction {
    std::uint32_t opcode = 0;
    std::vector<std::uint32_t> operands;
};

struct Module {
    std::map<std::uint32_t, Instruction> definitions;
    std::multimap<std::uint32_t, std::uint32_t> users;
    std::map<std::uint32_t, std::uint32_t> constants;
    std::vector<std::uint32_t> results[2];
};

constexpr std::uint32_t ResultOpcodes[2] = {spv::OpFAdd, spv::OpFSub};

Module Parse(std::span<const std::uint32_t> words) {
    if (words.size() < 5 || words[0] != spv::MagicNumber) throw std::runtime_error("the recompiler returned no SPIR-V module");
    Module module;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        if (count == 0 || count > words.size() - offset) throw std::runtime_error("the recompiler returned a malformed SPIR-V instruction");
        const auto opcode = words[offset] & spv::OpCodeMask;
        bool hasResult = false;
        bool hasType = false;
        spv::HasResultAndType(static_cast<spv::Op>(opcode), &hasResult, &hasType);
        if (hasResult && hasType && count >= 3) {
            const auto id = words[offset + 2];
            Instruction instruction{opcode, {words.begin() + static_cast<std::ptrdiff_t>(offset + 3), words.begin() + static_cast<std::ptrdiff_t>(offset + count)}};
            for (const auto operand : instruction.operands) module.users.emplace(operand, id);
            if (opcode == spv::OpConstant && instruction.operands.size() == 1) module.constants[id] = instruction.operands[0];
            for (std::size_t kind = 0; kind < 2; ++kind)
                if (opcode == ResultOpcodes[kind]) module.results[kind].push_back(id);
            module.definitions.emplace(id, std::move(instruction));
        }
        offset += count;
    }
    return module;
}

bool Constant(const Module& module, std::uint32_t id, std::uint32_t value) {
    const auto found = module.constants.find(id);
    return found != module.constants.end() && found->second == value;
}

bool NanTest(const Module& module, std::uint32_t id) {
    const auto compare = module.definitions.find(id);
    if (compare == module.definitions.end() || compare->second.opcode != spv::OpUGreaterThan || compare->second.operands.size() != 2) return false;
    if (!Constant(module, compare->second.operands[1], 0x7f800000u)) return false;
    const auto magnitude = module.definitions.find(compare->second.operands[0]);
    return magnitude != module.definitions.end() && magnitude->second.opcode == spv::OpBitwiseAnd && magnitude->second.operands.size() == 2 && Constant(module, magnitude->second.operands[1], 0x7fffffffu);
}

std::uint32_t NanSelectsOnResult(const Module& module, std::uint32_t result) {
    std::uint32_t value = result;
    std::uint32_t selects = 0;
    for (bool advanced = true; advanced;) {
        advanced = false;
        const auto [first, last] = module.users.equal_range(value);
        for (auto user = first; user != last; ++user) {
            const auto& instruction = module.definitions.at(user->second);
            if (instruction.opcode == spv::OpBitcast) {
                value = user->second;
                advanced = true;
                break;
            }
            if (instruction.opcode == spv::OpSelect && instruction.operands.size() == 3 && instruction.operands[2] == value && NanTest(module, instruction.operands[0])) {
                value = user->second;
                ++selects;
                advanced = true;
                break;
            }
        }
    }
    return selects;
}

std::vector<std::uint32_t> Recompiled(std::span<const std::uint32_t> code) {
    const std::array<std::uint32_t, 4> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
    const std::array<std::uint32_t, 2> capabilities{1u, 61u};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 32u;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    const auto result = Recompile(request);
    const auto& words = result.spirv.Words();
    return {words.begin(), words.end()};
}

alignas(256) constexpr std::array<std::uint32_t, 11> Chain{
    0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0x06100b04u, 0x08100d08u, 0x0a100f08u, 0x06100908u,
    0xe0701000u, 0x80000801u, 0xbf810000u,
};

}

int main() {
    try {
        const auto module = Parse(Recompiled(Chain));
        bool passed = true;
        for (std::size_t kind = 0; kind < 2; ++kind) {
            const char* name = kind == 0 ? "v_add_f32" : "v_sub_f32/v_subrev_f32";
            if (module.results[kind].empty()) {
                std::printf("FAIL: no %s result in the module\n", name);
                passed = false;
                continue;
            }
            for (const auto result : module.results[kind]) {
                const auto selects = NanSelectsOnResult(module, result);
                if (selects != 1u) {
                    std::printf("FAIL: the %s result %u passes %u NaN selects before the register holds it, expected 1\n", name, result, selects);
                    passed = false;
                }
            }
        }
        std::printf(passed ? "PASS: every f32 add and sub result passes one NaN select\n" : "f32 NaN result selects: FAILED\n");
        return passed ? 0 : 1;
    } catch (const std::exception& error) {
        std::printf("FAIL: %s\n", error.what());
        return 1;
    }
}
