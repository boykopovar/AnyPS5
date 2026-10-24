#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <optional>
#include <span>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void Check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool PositionIsInvariant(std::span<const std::uint32_t> words) {
    Check(words.size() >= 5 && words[0] == spv::MagicNumber, "rect-list invariant: missing SPIR-V module");
    std::vector<std::pair<std::uint32_t, std::uint32_t>> positionMembers;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> invariantMembers;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        Check(count != 0 && count <= words.size() - offset, "rect-list invariant: malformed SPIR-V instruction");
        const auto opcode = words[offset] & spv::OpCodeMask;
        if (opcode == spv::OpMemberDecorate && count == 5 && words[offset + 3] == spv::DecorationBuiltIn && words[offset + 4] == spv::BuiltInPosition) {
            positionMembers.emplace_back(words[offset + 1], words[offset + 2]);
        }
        if (opcode == spv::OpMemberDecorate && count == 4 && words[offset + 3] == spv::DecorationInvariant) {
            invariantMembers.emplace_back(words[offset + 1], words[offset + 2]);
        }
        offset += count;
    }
    Check(!positionMembers.empty(), "rect-list invariant: shader has no gl_Position");
    for (const auto& member : positionMembers) {
        if (std::find(invariantMembers.begin(), invariantMembers.end(), member) == invariantMembers.end()) return false;
    }
    return true;
}

bool InstructionHasType(spv::Op opcode) {
    switch (opcode) {
    case spv::OpConstant:
    case spv::OpConstantComposite:
    case spv::OpVariable:
    case spv::OpLoad:
    case spv::OpAccessChain:
    case spv::OpFAdd:
    case spv::OpIAdd:
    case spv::OpIMul:
    case spv::OpConvertFToS:
    case spv::OpCompositeExtract:
    case spv::OpCopyObject:
    case spv::OpFunction:
        return true;
    default:
        return false;
    }
}

struct CornerValue {
    enum class Kind { Float, Int, Vector };
    Kind kind;
    float x = 0;
    float y = 0;
    int index = 0;
};

int CornerIndex(std::span<const std::uint32_t> words, float x, float y) {
    Check(words.size() >= 5 && words[0] == spv::MagicNumber, "rect-list corner: missing SPIR-V module");
    std::unordered_map<std::uint32_t, std::size_t> definitions;
    std::unordered_map<std::uint32_t, bool> floatTypes;
    std::unordered_map<std::uint32_t, bool> intTypes;
    std::uint32_t tessCoord = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        Check(count != 0 && count <= words.size() - offset, "rect-list corner: malformed SPIR-V instruction");
        const auto opcode = static_cast<spv::Op>(words[offset] & spv::OpCodeMask);
        if (opcode == spv::OpTypeFloat && count >= 2) floatTypes[words[offset + 1]] = true;
        if (opcode == spv::OpTypeInt && count >= 2) intTypes[words[offset + 1]] = true;
        if (opcode == spv::OpDecorate && count == 4 && words[offset + 2] == spv::DecorationBuiltIn && words[offset + 3] == spv::BuiltInTessCoord) tessCoord = words[offset + 1];
        if (InstructionHasType(opcode) && count >= 3) definitions[words[offset + 2]] = offset;
        offset += count;
    }
    Check(tessCoord != 0, "rect-list evaluation shader has no gl_TessCoord");

    std::unordered_map<std::uint32_t, CornerValue> known;
    std::unordered_map<std::uint32_t, bool> pending;
    const auto evaluate = [&](auto&& self, std::uint32_t id) -> CornerValue {
        if (const auto found = known.find(id); found != known.end()) return found->second;
        Check(!pending[id], "rect-list corner index depends on itself");
        const auto defined = definitions.find(id);
        Check(defined != definitions.end(), "rect-list corner index depends on an undefined value");
        pending[id] = true;
        const auto offset = defined->second;
        const auto count = words[offset] >> spv::WordCountShift;
        const auto opcode = static_cast<spv::Op>(words[offset] & spv::OpCodeMask);
        const auto operand = [&](std::uint32_t index) {
            Check(3u + index < count, "rect-list corner instruction is missing an operand");
            return words[offset + 3u + index];
        };
        CornerValue value{CornerValue::Kind::Int};
        switch (opcode) {
        case spv::OpConstant:
            if (floatTypes[words[offset + 1]]) value = {CornerValue::Kind::Float, std::bit_cast<float>(words[offset + 3])};
            else if (intTypes[words[offset + 1]]) value = {CornerValue::Kind::Int, 0, 0, static_cast<int>(words[offset + 3])};
            else Check(false, "rect-list corner constant is neither float nor int");
            break;
        case spv::OpCopyObject:
            value = self(self, operand(0));
            break;
        case spv::OpLoad:
            if (operand(0) == tessCoord) {
                value = {CornerValue::Kind::Vector, x, y};
                break;
            }
            if (const auto pointer = definitions.find(operand(0)); pointer != definitions.end() && static_cast<spv::Op>(words[pointer->second] & spv::OpCodeMask) == spv::OpAccessChain && (words[pointer->second] >> spv::WordCountShift) >= 5 && words[pointer->second + 3] == tessCoord) {
                const auto component = self(self, words[pointer->second + 4]);
                Check(component.kind == CornerValue::Kind::Int && (component.index == 0 || component.index == 1), "rect-list gl_TessCoord component is not x or y");
                value = {CornerValue::Kind::Float, component.index == 0 ? x : y};
                break;
            }
            Check(false, "rect-list corner index loads something other than gl_TessCoord");
            break;
        case spv::OpCompositeExtract: {
            const auto composite = self(self, operand(0));
            Check(composite.kind == CornerValue::Kind::Vector && count == 5, "rect-list corner extract is not a gl_TessCoord component");
            const auto component = words[offset + 4];
            Check(component <= 1u, "rect-list corner extract uses gl_TessCoord.z");
            value = {CornerValue::Kind::Float, component == 0u ? composite.x : composite.y};
            break;
        }
        case spv::OpFAdd: {
            const auto left = self(self, operand(0));
            const auto right = self(self, operand(1));
            Check(left.kind == CornerValue::Kind::Float && right.kind == CornerValue::Kind::Float, "rect-list corner addition is not float");
            value = {CornerValue::Kind::Float, left.x + right.x};
            break;
        }
        case spv::OpConvertFToS: {
            const auto source = self(self, operand(0));
            Check(source.kind == CornerValue::Kind::Float, "rect-list corner conversion is not from float");
            value = {CornerValue::Kind::Int, 0, 0, static_cast<int>(source.x)};
            break;
        }
        case spv::OpIAdd:
        case spv::OpIMul: {
            const auto left = self(self, operand(0));
            const auto right = self(self, operand(1));
            Check(left.kind == CornerValue::Kind::Int && right.kind == CornerValue::Kind::Int, "rect-list corner arithmetic is not integral");
            value = {CornerValue::Kind::Int, 0, 0, opcode == spv::OpIAdd ? left.index + right.index : left.index * right.index};
            break;
        }
        default:
            Check(false, "rect-list corner index uses opcode " + std::to_string(static_cast<unsigned>(opcode)));
            break;
        }
        pending[id] = false;
        known[id] = value;
        return value;
    };

    std::optional<int> selected;
    for (const auto& [id, offset] : definitions) {
        const auto count = words[offset] >> spv::WordCountShift;
        if (static_cast<spv::Op>(words[offset] & spv::OpCodeMask) != spv::OpAccessChain || words[offset + 3] == tessCoord) continue;
        for (std::uint32_t operand = 4; operand < count; ++operand) {
            const auto indexId = words[offset + operand];
            const auto defined = definitions.find(indexId);
            if (defined != definitions.end() && static_cast<spv::Op>(words[defined->second] & spv::OpCodeMask) == spv::OpConstant) continue;
            const auto value = evaluate(evaluate, indexId);
            Check(value.kind == CornerValue::Kind::Int, "rect-list vertex index is not an integer");
            Check(!selected.has_value() || *selected == value.index, "rect-list evaluation indexes different vertices from one coordinate");
            selected = value.index;
        }
    }
    Check(selected.has_value(), "rect-list evaluation does not index a tessellation corner");
    return *selected;
}

}

int main() {
    try {
        static constexpr std::array<std::uint32_t, 2> capabilities{spv::CapabilityShader, spv::CapabilityTessellation};
        SpirvTarget target{};
        target.vulkanVersion = 0x00401000u;
        target.spirvVersion = 0x00010400u;
        target.supportedCapabilities = capabilities;
        target.tessellation = TessellationTargetLimits{32, 128, 128, 120, 4096, 128, 128};
        RecompileResult vertex;
        RecompileResult fragment;
        const auto shaders = BuildRectListShaders(vertex, fragment, target);
        Check(PositionIsInvariant(shaders.control.spirv.Words()), "rect-list control gl_Position is not Invariant");
        Check(PositionIsInvariant(shaders.evaluation.spirv.Words()), "rect-list evaluation gl_Position is not Invariant");
        const auto belowOne = std::nextafter(1.0f, 0.0f);
        const std::array<std::array<float, 3>, 7> corners{{
            {0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 2.0f},
            {1.0f, 1.0f, 3.0f},
            {belowOne, 0.0f, 1.0f},
            {0.0f, belowOne, 2.0f},
            {belowOne, belowOne, 3.0f},
        }};
        for (const auto& corner : corners) {
            const auto selected = CornerIndex(shaders.evaluation.spirv.Words(), corner[0], corner[1]);
            char message[160];
            std::snprintf(message, sizeof(message), "rect-list corner (%.9g, %.9g) selected vertex %d, expected %.0f", static_cast<double>(corner[0]), static_cast<double>(corner[1]), selected, static_cast<double>(corner[2]));
            Check(selected == static_cast<int>(corner[2]), message);
        }
        std::printf("rect-list position invariant tests passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
