#define SPV_ENABLE_UTILITY_CODE
#include <spirv/unified1/spirv.hpp>
#undef SPV_ENABLE_UTILITY_CODE
#include "Recompiler.hpp"
#include "SubvectorLoopFixture.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint32_t> recompile(bool robustBufferAccess2) {
    const std::array<std::uint32_t, 8> userData{0x10000000u, 0u, 0x100u, 0x31016facu, 0x10001000u, 0u, 0x900u, 0x31016facu};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    const std::array<std::uint32_t, 3> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, SubvectorLoopFixture::ExecKeptCode, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 64;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.target.robustBufferAccess2 = robustBufferAccess2;
    request.layout = {0, 0, 0, 128};
    request.useCache = false;
    auto result = Recompile(request);
    require(!result.spirv.empty(), "the shader did not recompile");
    return result.spirv.Copy();
}

enum class EmptyExecTest {
    Other,
    OneCompare,
    TwoCompares
};

class BranchConditions {
public:
    explicit BranchConditions(std::vector<std::uint32_t> module) : words(std::move(module)) {
        require(words.size() >= 5, "the SPIR-V header is missing");
        for (std::size_t offset = 5; offset < words.size();) {
            const auto count = words[offset] >> spv::WordCountShift;
            require(count != 0 && count <= words.size() - offset, "a SPIR-V instruction is truncated");
            const auto opcode = static_cast<spv::Op>(words[offset] & spv::OpCodeMask);
            bool result = false;
            bool type = false;
            spv::HasResultAndType(opcode, &result, &type);
            if (result) definitions.emplace(words[offset + (type ? 2 : 1)], offset);
            if (opcode == spv::OpBranchConditional) conditions.push_back(words[offset + 1]);
            offset += count;
        }
    }

    std::vector<EmptyExecTest> Tests() const {
        std::vector<EmptyExecTest> tests;
        for (const auto condition : conditions) tests.push_back(classify(condition));
        return tests;
    }

private:
    std::vector<std::uint32_t> words;
    std::vector<std::uint32_t> conditions;
    std::map<std::uint32_t, std::size_t> definitions;

    std::span<const std::uint32_t> instruction(std::uint32_t id, spv::Op opcode, std::size_t count) const {
        const auto found = definitions.find(id);
        if (found == definitions.end()) return {};
        const auto offset = found->second;
        if ((words[offset] & spv::OpCodeMask) != static_cast<std::uint32_t>(opcode) || (words[offset] >> spv::WordCountShift) != count) return {};
        return std::span<const std::uint32_t>(words).subspan(offset, count);
    }

    bool zero(std::uint32_t id) const {
        const auto constant = instruction(id, spv::OpConstant, 4);
        return !constant.empty() && constant[3] == 0u;
    }

    std::optional<std::uint32_t> comparedWithZero(std::uint32_t id) const {
        const auto compare = instruction(id, spv::OpIEqual, 5);
        if (compare.empty()) return std::nullopt;
        if (zero(compare[4])) return compare[3];
        if (zero(compare[3])) return compare[4];
        return std::nullopt;
    }

    std::optional<std::pair<std::uint32_t, std::uint32_t>> execWord(std::uint32_t id) const {
        const auto extract = instruction(id, spv::OpCompositeExtract, 5);
        if (extract.empty() || instruction(extract[3], spv::OpGroupNonUniformBallot, 5).empty()) return std::nullopt;
        return std::pair{extract[3], extract[4]};
    }

    bool execWords(std::uint32_t first, std::uint32_t second) const {
        const auto low = execWord(first);
        const auto high = execWord(second);
        return low && high && low->first == high->first && low->second <= 1u && high->second <= 1u && low->second != high->second;
    }

    EmptyExecTest classify(std::uint32_t condition) const {
        if (const auto negation = instruction(condition, spv::OpLogicalNot, 4); !negation.empty()) return classify(negation[3]);
        if (const auto operand = comparedWithZero(condition)) {
            const auto combined = instruction(*operand, spv::OpBitwiseOr, 5);
            if (!combined.empty() && execWords(combined[3], combined[4])) return EmptyExecTest::OneCompare;
        }
        const auto both = instruction(condition, spv::OpLogicalAnd, 5);
        if (!both.empty()) {
            const auto first = comparedWithZero(both[3]);
            const auto second = comparedWithZero(both[4]);
            if (first && second && execWords(*first, *second)) return EmptyExecTest::TwoCompares;
        }
        return EmptyExecTest::Other;
    }
};

int failures = 0;

void fail(const std::string& message) {
    std::fprintf(stderr, "SpirvSubvectorLoopBegin: %s\n", message.c_str());
    ++failures;
}

void check(bool robustBufferAccess2) {
    const std::string mode = robustBufferAccess2 ? "robustBufferAccess2 on" : "robustBufferAccess2 off";
    std::uint32_t oneCompare = 0;
    std::uint32_t twoCompares = 0;
    for (const auto test : BranchConditions(recompile(robustBufferAccess2)).Tests()) {
        if (test == EmptyExecTest::OneCompare) ++oneCompare;
        if (test == EmptyExecTest::TwoCompares) ++twoCompares;
    }
    if (twoCompares != 0u) {
        fail(mode + ": branches that compare exec_lo and exec_hi with 0 separately: " + std::to_string(twoCompares) + ", expected 0");
    }
    if (oneCompare != 1u) {
        fail(mode + ": branches that compare (exec_lo | exec_hi) with 0: " + std::to_string(oneCompare) + ", expected 1 (s_subvector_loop_begin)");
    }
}

}

int main() {
    try {
        check(false);
        check(true);
    } catch (const std::exception& error) {
        fail(error.what());
    }
    if (failures != 0) return 1;
    std::puts("SpirvSubvectorLoopBegin: s_subvector_loop_begin tests an empty exec mask as (exec_lo | exec_hi) == 0 in both robustBufferAccess2 modes");
    return 0;
}
