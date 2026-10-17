#include "WgslBackend/WgslEmitter.hpp"
#include "IntermediateRepresentation/IrMetadata.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::uint32_t NoComponent = 0xffffffffu;
constexpr const char* ReturnMarker = "aps5_return_marker;";

constexpr const char* HalfTowardZero = R"(fn aps5_f16_rtz(value: f32) -> u32 {
    let bits = bitcast<u32>(value);
    let sign = (bits >> 16u) & 0x8000u;
    let exponent = (bits >> 23u) & 0xffu;
    let mantissa = bits & 0x007fffffu;
    let normal = sign | (((exponent - 112u) << 10u) | (mantissa >> 13u));
    let subShift = select(select(126u - exponent, 31u, exponent < 103u), 14u, exponent > 112u);
    let subnormal = sign | ((mantissa | 0x00800000u) >> subShift);
    let nan = sign | (0x7c00u | ((mantissa >> 13u) | 0x0200u));
    let special = select(nan, sign | 0x7c00u, mantissa == 0u);
    let finite0 = select(normal, subnormal, exponent <= 112u);
    let finite1 = select(finite0, sign, exponent < 103u);
    let finite2 = select(finite1, sign | 0x7bffu, exponent >= 143u);
    return select(finite2, special, exponent == 255u) & 0xffffu;
}
)";

constexpr const char* IsNan = R"(fn aps5_is_nan(value: f32) -> bool {
    return (bitcast<u32>(value) & 0x7fffffffu) > 0x7f800000u;
}
)";

constexpr const char* Saturate = R"(fn aps5_saturate(value: f32) -> f32 {
    let bits = bitcast<u32>(value);
    return bitcast<f32>(select(0u, min(bits, 0x3f800000u), (bits - 1u) < 0x7f800000u));
}
)";

constexpr const char* TurnSeries = R"(fn aps5_sin_series(turns: f32) -> f32 {
    let square = turns * turns;
    var polynomial = bitcast<f32>(0xc17183a8u);
    polynomial = fma(polynomial, square, bitcast<f32>(0x42283c1au));
    polynomial = fma(polynomial, square, bitcast<f32>(0xc2996966u));
    polynomial = fma(polynomial, square, bitcast<f32>(0x42a335e3u));
    polynomial = fma(polynomial, square, bitcast<f32>(0xc2255de7u));
    polynomial = fma(polynomial, square, bitcast<f32>(0x40c90fdbu));
    return turns * polynomial;
}

fn aps5_cos_series(turns: f32) -> f32 {
    let square = turns * turns;
    var polynomial = bitcast<f32>(0x40fce9c5u);
    polynomial = fma(polynomial, square, bitcast<f32>(0xc1d368f9u));
    polynomial = fma(polynomial, square, bitcast<f32>(0x4270fa83u));
    polynomial = fma(polynomial, square, bitcast<f32>(0xc2aae9e4u));
    polynomial = fma(polynomial, square, bitcast<f32>(0x4281e0f8u));
    polynomial = fma(polynomial, square, bitcast<f32>(0xc19de9e6u));
    polynomial = fma(polynomial, square, bitcast<f32>(0x3f800000u));
    return polynomial;
}

fn aps5_series_when_far(value: f32, series: f32) -> f32 {
    let valueBits = bitcast<u32>(value);
    let seriesBits = bitcast<u32>(series);
    return select(value, series, min(valueBits - seriesBits, seriesBits - valueBits) > 4u);
}

fn aps5_linear_near_zero(value: f32, distance: f32, factor: f32) -> f32 {
    return select(value, distance * factor, (bitcast<u32>(distance) & 0x7fffffffu) < 0x39800000u);
}
)";

constexpr const char* Sine = R"(fn aps5_sin(value: f32) -> f32 {
    let reduced = value - round(value);
    let cycle = select(reduced, value, (bitcast<u32>(value) & 0x7fffffffu) == 0u);
    let absolute = abs(cycle);
    let signBits = bitcast<u32>(cycle) & 0x80000000u;
    let nearHalf = absolute > bitcast<f32>(0x3e800000u);
    let distance = select(absolute, bitcast<f32>(0x3f000000u) - absolute, nearHalf);
    let series = select(aps5_sin_series(distance), aps5_cos_series(bitcast<f32>(0x3e800000u) - distance), distance > bitcast<f32>(0x3e000000u));
    let approximation = aps5_series_when_far(sin(absolute * bitcast<f32>(0x40c90fdbu)), series);
    let result = aps5_linear_near_zero(approximation, distance, select(bitcast<f32>(0x40c90fd5u), bitcast<f32>(0x40c90fdbu), nearHalf));
    return bitcast<f32>(bitcast<u32>(result) | signBits);
}
)";

constexpr const char* Cosine = R"(fn aps5_cos(value: f32) -> f32 {
    let absolute = abs(value - round(value));
    let distance = bitcast<f32>(0x3e800000u) - absolute;
    let low = aps5_cos_series(absolute);
    let middle = aps5_sin_series(distance);
    let high = -aps5_cos_series(bitcast<f32>(0x3f000000u) - absolute);
    let series = select(select(low, middle, absolute > bitcast<f32>(0x3e000000u)), high, absolute > bitcast<f32>(0x3ec00000u));
    let approximation = aps5_series_when_far(cos(absolute * bitcast<f32>(0x40c90fdbu)), series);
    return aps5_linear_near_zero(approximation, distance, bitcast<f32>(0x40c90fdbu));
}
)";

constexpr const char* BufferFits = R"(fn aps5_buffer_fits(stride: u32, records: u32, offset: u32, bytes: u32) -> bool {
    if (stride == 0u) {
        return records >= bytes && offset <= records - bytes;
    }
    let low = stride * (records & 0xffffu);
    let high = stride * (records >> 16u);
    let sizeLow = low + (high << 16u);
    let sizeHigh = (high >> 16u) + select(0u, 1u, sizeLow < low);
    let end = offset + bytes;
    let endHigh = select(0u, 1u, end < offset);
    return endHigh < sizeHigh || (endHigh == sizeHigh && end <= sizeLow);
}
)";

struct DimensionInfo {
    std::uint32_t coordinateComponents;
    std::uint32_t spatialComponents;
    bool arrayed;
    bool multisampled;
};

DimensionInfo DimensionInfoFor(RdnaImageDimension dimension) {
    switch (dimension) {
    case RdnaImageDimension::Dim1D: return {1, 1, false, false};
    case RdnaImageDimension::Dim2D: return {2, 2, false, false};
    case RdnaImageDimension::Dim3D: return {3, 3, false, false};
    case RdnaImageDimension::Dim1DArray: return {2, 1, true, false};
    case RdnaImageDimension::Dim2DArray: return {3, 2, true, false};
    case RdnaImageDimension::Dim2DMsaa: return {2, 2, false, true};
    case RdnaImageDimension::Dim2DMsaaArray: return {3, 2, true, true};
    case RdnaImageDimension::Unknown: break;
    }
    throw std::runtime_error("WGSL emission: image dimension " + std::to_string(static_cast<std::uint32_t>(dimension)) + " is invalid");
}

struct SampleLayout {
    std::uint32_t offset = NoComponent;
    std::uint32_t bias = NoComponent;
    std::uint32_t dref = NoComponent;
    std::uint32_t gradX = NoComponent;
    std::uint32_t gradY = NoComponent;
    std::uint32_t coord = 0;
    std::uint32_t lod = NoComponent;
    std::uint32_t clamp = NoComponent;
};

SampleLayout LayoutOf(const MemoryInfo& memory) {
    SampleLayout layout;
    std::uint32_t cursor = 0;
    const auto info = DimensionInfoFor(memory.imageDimension);
    const auto has = [&](std::uint32_t flag) { return (memory.imageSampleFlags & flag) != 0u; };
    if (has(RdnaImageSampleFlagOffset)) layout.offset = cursor++;
    if (has(RdnaImageSampleFlagBias)) layout.bias = cursor++;
    if (has(RdnaImageSampleFlagCompare)) layout.dref = cursor++;
    if (has(RdnaImageSampleFlagDerivative)) {
        layout.gradX = cursor;
        cursor += info.spatialComponents;
        layout.gradY = cursor;
        cursor += info.spatialComponents;
    }
    layout.coord = cursor;
    cursor += info.coordinateComponents;
    if (has(RdnaImageSampleFlagLod)) layout.lod = cursor++;
    if (has(RdnaImageSampleFlagLodClamp)) layout.clamp = cursor++;
    return layout;
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%xu", value);
    return text;
}

std::string FloatBits(std::uint32_t bits) {
    return "bitcast<f32>(" + Hex(bits) + ")";
}

std::string FloatLiteral(float value) {
    return FloatBits(std::bit_cast<std::uint32_t>(value));
}

std::string Component(std::uint32_t index) {
    static constexpr const char* names[] = {"x", "y", "z", "w"};
    return names[index & 3u];
}

std::string ScalarType(IrTextureNumericClass numericClass) {
    switch (numericClass) {
    case IrTextureNumericClass::Float: return "f32";
    case IrTextureNumericClass::Uint: return "u32";
    case IrTextureNumericClass::Sint: return "i32";
    case IrTextureNumericClass::Unsupported: break;
    }
    throw std::runtime_error("WGSL emission: unsupported texture numeric class");
}

struct EntryInput {
    std::string attributes;
    std::string name;
    std::string type;
};

struct Declaration {
    std::uint32_t binding;
    std::uint32_t element;
    std::string name;
    std::string text;
};

class Emitter {
public:
    Emitter(const IrProgram& program, const ShaderStageInputInfo& inputInfo, std::span<const PipelineSpecializationConstant> constants, std::span<const WgslPlanBinding> planBindings)
        : program(program), inputInfo(inputInfo), layout(program.Metadata().bindings), stage(program.Resources().stage), planBindings(planBindings.begin(), planBindings.end()) {
        for (const auto& constant : constants) this->constants[constant.id] = constant.value;
    }

    WgslModule Run() {
        if (stage != IrShaderStage::Vertex && stage != IrShaderStage::Pixel) fail("only vertex and pixel programs are implemented");
        if (layout.UsesPushData()) fail("push data is not available in WebGPU");
        if (program.Metadata().barycentricEmulation) fail("barycentric emulation is not implemented");
        prepareValidMask();
        emitBlocks();
        return finish();
    }

private:
    [[noreturn]] void fail(const std::string& reason) const {
        char hash[32];
        std::snprintf(hash, sizeof(hash), "%llx", static_cast<unsigned long long>(program.Resources().shaderHash));
        throw std::runtime_error(std::string("WGSL emission failed: hash=0x") + hash + " stage=" + std::to_string(static_cast<unsigned>(stage)) + " reason=" + reason);
    }

    [[noreturn]] void fail(const IrValue& inst, const std::string& reason) const {
        fail("opcode=" + std::string(IrOpcodeName(inst.Opcode())) + " " + reason);
    }

    std::uint32_t spec(std::uint32_t id, std::uint32_t defaultValue) const {
        const auto found = constants.find(id);
        return found == constants.end() ? defaultValue : found->second;
    }

    void line(const std::string& text) {
        body += "    " + indent + text + "\n";
    }

    std::string temporary() {
        return "t" + std::to_string(temporaries++);
    }

    std::string bind(const std::string& expression) {
        const auto name = temporary();
        line("let " + name + " = " + expression + ";");
        return name;
    }

    void define(const IrValue& inst, const std::string& expression) {
        const auto name = "v" + std::to_string(inst.Id());
        line("let " + name + " = " + expression + ";");
        names[&inst] = name;
    }

    std::string value(const IrValue* argument) const {
        const auto* resolved = argument->Resolve();
        if (resolved->HasImmediate()) {
            switch (resolved->Type()) {
            case IrType::Bool: return resolved->ImmediateBool() ? "true" : "false";
            case IrType::U8: return Hex(resolved->ImmediateU8());
            case IrType::U16: return Hex(resolved->ImmediateU16());
            case IrType::U32: return Hex(resolved->ImmediateU32());
            case IrType::F32: return FloatLiteral(resolved->ImmediateF32());
            default: fail(*resolved, "has an immediate of an unsupported type");
            }
        }
        const auto found = names.find(resolved);
        if (found == names.end()) fail(*resolved, "is used before its definition");
        return found->second;
    }

    std::string argument(const IrValue& inst, std::size_t index) const {
        return value(inst.Argument(index));
    }

    const MemoryInfo& memory(const IrValue& inst) const {
        return program.Resources().memoryInfo.at(inst.Flags<MemoryFlags>().index);
    }

    const ExportInfo& exportOf(const IrValue& inst) const {
        return program.Metadata().exportInfo.at(inst.Flags<ExportFlags>().index);
    }

    void helper(const char* text) {
        if (std::find(helpers.begin(), helpers.end(), text) == helpers.end()) helpers.push_back(text);
    }

    std::uint32_t element(DescriptorBindingKind kind, std::uint32_t resource) const {
        for (const auto& descriptor : layout.descriptors) {
            if (descriptor.kind != kind) continue;
            const auto found = std::find(descriptor.resources.begin(), descriptor.resources.end(), resource);
            if (found == descriptor.resources.end()) fail("resource " + std::to_string(resource) + " is absent from descriptor kind " + std::to_string(static_cast<std::uint32_t>(kind)));
            return static_cast<std::uint32_t>(found - descriptor.resources.begin());
        }
        fail("descriptor kind " + std::to_string(static_cast<std::uint32_t>(kind)) + " was not allocated");
    }

    std::string resource(DescriptorBindingKind kind, std::uint32_t element, const std::string& type, const std::string& space) {
        const auto binding = NativeBinding(stage, kind);
        const auto name = "aps5_b" + std::to_string(binding) + "_" + std::to_string(element);
        if (!declared.contains(name)) {
            declared.insert(name);
            declarations.push_back({binding, element, name, "var" + space + " " + name + ": " + type + ";"});
        }
        return name;
    }

    std::string shaderData() {
        return resource(DescriptorBindingKind::ShaderData, 0, "array<u32>", "<storage, read>");
    }

    std::string shaderDataDword(std::uint32_t index) {
        return shaderData() + "[" + Hex(index) + "]";
    }

    std::string userData(const IrValue& inst) {
        const auto registerIndex = static_cast<std::uint32_t>(inst.Argument(0)->Register().index);
        const auto& registers = layout.userDataRegisters;
        const auto found = std::lower_bound(registers.begin(), registers.end(), registerIndex);
        if (found == registers.end() || *found != registerIndex) return Hex(0);
        return shaderDataDword(static_cast<std::uint32_t>(found - registers.begin()));
    }

    std::string bufferByteOffset(std::uint32_t elementIndex) {
        if (elementIndex >= layout.memoryOffsetCount) fail("buffer element has no memory offset");
        if (const auto found = byteOffsets.find(elementIndex); found != byteOffsets.end()) return found->second;
        const auto name = "aps5_offset" + std::to_string(elementIndex);
        prologue += "    let " + name + " = (" + shaderDataDword(layout.memoryOffsetDword + elementIndex / 4u) + " >> " + Hex((elementIndex % 4u) * 8u) + ") & 0xffu;\n";
        byteOffsets[elementIndex] = name;
        return name;
    }

    void prepareValidMask() {
        if (stage != IrShaderStage::Pixel) return;
        for (const auto& info : program.Metadata().exportInfo) validMask |= info.vm;
    }

    void emitBlocks() {
        const auto& blocks = program.BlockOrder();
        const auto& blockInfo = program.Metadata().blockInfo;
        if (blocks.size() != blockInfo.size()) fail("control flow block metadata is inconsistent");
        if (blocks.empty()) fail("the program has no blocks");
        for (std::size_t index = 0; index < blocks.size(); ++index) blockIndex[blockInfo[index].id] = index;
        emitRegion(blockInfo.front().id, InvalidControlFlowId);
    }

    void emitRegion(std::uint32_t block, std::uint32_t stop) {
        const auto& blocks = program.BlockOrder();
        const auto& blockInfo = program.Metadata().blockInfo;
        while (block != stop) {
            if (block == InvalidControlFlowId) {
                line(ReturnMarker);
                return;
            }
            const auto found = blockIndex.find(block);
            if (found == blockIndex.end()) fail("branches to unknown block " + std::to_string(block));
            if (!emitted.insert(block).second) fail("control flow other than nested if/else is not implemented");
            for (const auto* inst : blocks[found->second]->Instructions()) emit(*inst);
            const auto& info = blockInfo[found->second];
            const auto& terminator = info.terminator;
            if (terminator.loopHeader || terminator.continueBlock != InvalidControlFlowId) fail("loops are not implemented");
            if (terminator.kind == TerminatorKind::Return) {
                line(ReturnMarker);
                return;
            }
            if (terminator.kind == TerminatorKind::Branch) {
                block = terminator.trueBlock;
                continue;
            }
            if (terminator.kind != TerminatorKind::ConditionalBranch) fail("indirect branches are not implemented");
            if (terminator.condition == BranchCondition::GotoVariable || terminator.condition == BranchCondition::IndirectTarget || terminator.condition == BranchCondition::Unknown) fail("goto and indirect branch conditions are not implemented");
            if (terminator.trueBlock == terminator.falseBlock) {
                block = terminator.trueBlock;
                continue;
            }
            if (info.condition == nullptr) fail("a conditional branch has no condition");
            const auto merge = terminator.mergeBlock;
            line("if (" + value(info.condition) + ") {");
            indent += "    ";
            emitRegion(terminator.trueBlock, merge);
            indent.resize(indent.size() - 4);
            line("} else {");
            indent += "    ";
            emitRegion(terminator.falseBlock, merge);
            indent.resize(indent.size() - 4);
            line("}");
            if (merge == InvalidControlFlowId) return;
            block = merge;
        }
    }

    std::string withReturns(const std::vector<std::string>& epilogue) const {
        std::string text;
        std::size_t start = 0;
        while (start < body.size()) {
            const auto end = body.find('\n', start);
            const auto current = body.substr(start, end == std::string::npos ? std::string::npos : end - start);
            const auto content = current.find_first_not_of(' ');
            if (content != std::string::npos && current.compare(content, std::string::npos, ReturnMarker) == 0) {
                for (const auto& statement : epilogue) text += current.substr(0, content) + statement + "\n";
            } else {
                text += current + "\n";
            }
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return text;
    }

    void emit(const IrValue& inst) {
        const auto binary = [&](const char* operation) { define(inst, "(" + argument(inst, 0) + " " + operation + " " + argument(inst, 1) + ")"); };
        const auto ordered = [&](const char* operation) {
            helper(IsNan);
            define(inst, "(!aps5_is_nan(" + argument(inst, 0) + ") && !aps5_is_nan(" + argument(inst, 1) + ") && (" + argument(inst, 0) + " " + operation + " " + argument(inst, 1) + "))");
        };
        const auto unordered = [&](const char* operation) {
            helper(IsNan);
            define(inst, "(aps5_is_nan(" + argument(inst, 0) + ") || aps5_is_nan(" + argument(inst, 1) + ") || (" + argument(inst, 0) + " " + operation + " " + argument(inst, 1) + "))");
        };
        switch (inst.Opcode()) {
        case IrOpcode::Identity:
            names[&inst] = argument(inst, 0);
            return;
        case IrOpcode::Waitcnt:
        case IrOpcode::InstPrefetch:
        case IrOpcode::Sendmsg:
        case IrOpcode::GetBufferResource:
        case IrOpcode::GetImageResource:
        case IrOpcode::GetSamplerResource:
        case IrOpcode::MakeImageAddress:
        case IrOpcode::Reference:
        case IrOpcode::ReferenceU32:
            return;
        case IrOpcode::BitwiseAnd32: return binary("&");
        case IrOpcode::BitwiseOr32: return binary("|");
        case IrOpcode::BitwiseXor32: return binary("^");
        case IrOpcode::ShiftLeftLogical32: return binary("<<");
        case IrOpcode::ShiftRightLogical32: return binary(">>");
        case IrOpcode::IAdd32: return binary("+");
        case IrOpcode::ISub32: return binary("-");
        case IrOpcode::IEqual32: return binary("==");
        case IrOpcode::INotEqual32: return binary("!=");
        case IrOpcode::UGreaterThan32: return binary(">");
        case IrOpcode::ULessThan32: return binary("<");
        case IrOpcode::LogicalOr: return binary("||");
        case IrOpcode::LogicalAnd: return binary("&&");
        case IrOpcode::LogicalXor: return binary("!=");
        case IrOpcode::LogicalNot:
            define(inst, "(!" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPAdd32: return binary("+");
        case IrOpcode::FPSub32: return binary("-");
        case IrOpcode::FPMul32: return binary("*");
        case IrOpcode::FPOrdEqual32: return ordered("==");
        case IrOpcode::FPOrdNotEqual32: return ordered("!=");
        case IrOpcode::FPOrdLessThan32: return ordered("<");
        case IrOpcode::FPOrdGreaterThan32: return ordered(">");
        case IrOpcode::FPOrdLessThanEqual32: return ordered("<=");
        case IrOpcode::FPOrdGreaterThanEqual32: return ordered(">=");
        case IrOpcode::FPUnordEqual32: return unordered("==");
        case IrOpcode::FPUnordNotEqual32: return unordered("!=");
        case IrOpcode::FPUnordLessThan32: return unordered("<");
        case IrOpcode::FPUnordGreaterThan32: return unordered(">");
        case IrOpcode::FPUnordLessThanEqual32: return unordered("<=");
        case IrOpcode::FPUnordGreaterThanEqual32: return unordered(">=");
        case IrOpcode::FPIsNan32:
            helper(IsNan);
            define(inst, "aps5_is_nan(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPMad32:
            define(inst, "((" + argument(inst, 0) + " * " + argument(inst, 1) + ") + " + argument(inst, 2) + ")");
            return;
        case IrOpcode::FPAbs32:
            define(inst, "bitcast<f32>(bitcast<u32>(" + argument(inst, 0) + ") & 0x7fffffffu)");
            return;
        case IrOpcode::FPSaturate32:
            helper(Saturate);
            define(inst, "aps5_saturate(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPSqrt:
            define(inst, "sqrt(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPTrunc32:
            define(inst, "trunc(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPRecip32:
            define(inst, "(" + FloatBits(0x3f800000u) + " / " + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPSin:
            helper(TurnSeries);
            helper(Sine);
            define(inst, "aps5_sin(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPCos:
            helper(TurnSeries);
            helper(Cosine);
            define(inst, "aps5_cos(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::PackFloat2x16Rtz:
            helper(HalfTowardZero);
            define(inst, "(aps5_f16_rtz(" + argument(inst, 0) + ") | (aps5_f16_rtz(" + argument(inst, 1) + ") << 16u))");
            return;
        case IrOpcode::ConvertF32S32:
            define(inst, "f32(bitcast<i32>(" + argument(inst, 0) + "))");
            return;
        case IrOpcode::SelectU32:
        case IrOpcode::SelectF32:
            define(inst, "select(" + argument(inst, 2) + ", " + argument(inst, 1) + ", " + argument(inst, 0) + ")");
            return;
        case IrOpcode::BitCastF32U32:
            define(inst, "bitcast<f32>(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::BitCastU32F32:
            define(inst, "bitcast<u32>(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::CompositeExtractU32x4:
            define(inst, argument(inst, 0) + "." + Component(inst.Argument(1)->Resolve()->ImmediateU32()));
            return;
        case IrOpcode::CompositeConstructU32x4:
            define(inst, "vec4<u32>(" + argument(inst, 0) + ", " + argument(inst, 1) + ", " + argument(inst, 2) + ", " + argument(inst, 3) + ")");
            return;
        case IrOpcode::Ballot:
            define(inst, "vec4<u32>(select(0u, 1u, " + argument(inst, 0) + "), 0u, 0u, 0u)");
            return;
        case IrOpcode::LaneId:
            define(inst, Hex(0));
            return;
        case IrOpcode::GetUserData:
            define(inst, userData(inst));
            return;
        case IrOpcode::GetBuiltin:
            define(inst, builtin(inst));
            return;
        case IrOpcode::GetAttribute:
            define(inst, attribute(inst));
            return;
        case IrOpcode::SetAttribute:
            exportAttribute(inst);
            return;
        case IrOpcode::ReadConstBuffer:
            readConstBuffer(inst);
            return;
        case IrOpcode::ImageSampleRaw:
            sample(inst);
            return;
        default:
            fail(inst, "is not implemented in WGSL");
        }
    }

    std::string builtin(const IrValue& inst) {
        const auto kind = static_cast<StageInputKind>(inst.Argument(0)->Resolve()->ImmediateU32());
        const auto component = inst.Argument(1)->Resolve()->ImmediateU32();
        switch (kind) {
        case StageInputKind::VertexIndex:
            if (stage != IrShaderStage::Vertex) break;
            entryInputs.insert({0x100u, {"@builtin(vertex_index)", "aps5_vertex_index", "u32"}});
            return "aps5_vertex_index";
        case StageInputKind::InstanceIndex:
            if (stage != IrShaderStage::Vertex) break;
            entryInputs.insert({0x101u, {"@builtin(instance_index)", "aps5_instance_index", "u32"}});
            return "aps5_instance_index";
        case StageInputKind::FragCoord:
            if (stage != IrShaderStage::Pixel) break;
            entryInputs.insert({0x100u, {"@builtin(position)", "aps5_frag_coord", "vec4<f32>"}});
            return "bitcast<u32>(aps5_frag_coord." + Component(component) + ")";
        case StageInputKind::FrontFacing:
            if (stage != IrShaderStage::Pixel) break;
            entryInputs.insert({0x101u, {"@builtin(front_facing)", "aps5_front_facing", "bool"}});
            return "select(0u, 1u, aps5_front_facing)";
        case StageInputKind::HelperInvocation:
            if (stage != IrShaderStage::Pixel) break;
            return Hex(0);
        default:
            break;
        }
        fail(inst, "reads builtin " + std::to_string(static_cast<std::uint32_t>(kind)) + ", which is not implemented");
    }

    const StageInput* parameterInput(std::uint32_t location) const {
        for (const auto& input : program.Info().inputs) {
            if (input.kind == StageInputKind::Parameter && input.location == location) return &input;
        }
        return nullptr;
    }

    std::string vertexAttribute(std::uint32_t location, std::uint32_t component) {
        const auto* input = parameterInput(location);
        if (input == nullptr) return Hex(0);
        const auto count = std::clamp(input->componentCount, 1u, 4u);
        const auto numericClass = spec(PipelineSpecialization::VertexBase + location * PipelineSpecialization::VertexWords + 5u, 0u);
        static constexpr const char* scalars[] = {"f32", "i32", "u32"};
        if (numericClass >= 3u) fail("vertex attribute " + std::to_string(location) + " has an invalid numeric class");
        const auto type = count == 1u ? std::string(scalars[numericClass]) : "vec" + std::to_string(count) + "<" + scalars[numericClass] + ">";
        const auto name = "aps5_attribute" + std::to_string(location);
        entryInputs.insert({location, {"@location(" + std::to_string(location) + ")", name, type}});
        if (count != 1u && component >= count) return Hex(0);
        const auto scalar = count == 1u ? name : name + "." + Component(component);
        return numericClass == 2u ? scalar : "bitcast<u32>(" + scalar + ")";
    }

    std::string pixelAttribute(std::uint32_t attribute, std::uint32_t component) {
        const auto& pixel = *inputInfo.pixel;
        if (pixel.InputIsDefault(attribute)) return Hex(pixel.InputDefaultBits(attribute, component & 3u));
        const auto* input = parameterInput(attribute);
        if (input == nullptr) return Hex(0);
        if (input->perVertex) fail("per-vertex pixel attributes are not implemented");
        if (attribute >= 32u || attribute >= pixel.inputNum) fail("pixel interpolator index is out of range");
        if (pixel.InputIsFp16(attribute)) fail("16-bit pixel attributes are not implemented");
        const auto slot = pixel.InputSlot(attribute);
        const bool flat = pixel.InputIsFlat(attribute);
        const auto& metadata = program.Metadata();
        const bool linear = !flat && pixel.InputIsLinear(attribute, metadata.pixelLinearInputs, metadata.pixelPerspectiveInputs);
        const auto name = "aps5_parameter" + std::to_string(slot);
        const auto interpolation = flat ? "@interpolate(flat)" : linear ? "@interpolate(linear, center)" : "@interpolate(perspective, center)";
        const EntryInput declaration{"@location(" + std::to_string(slot) + ") " + interpolation, name, "vec4<f32>"};
        const auto found = entryInputs.find(slot);
        if (found != entryInputs.end() && found->second.attributes != declaration.attributes) fail("pixel input slot " + std::to_string(slot) + " is read with two interpolations");
        entryInputs.insert({slot, declaration});
        return "bitcast<u32>(" + name + "." + Component(component) + ")";
    }

    std::string attribute(const IrValue& inst) {
        const auto location = inst.Argument(0)->Resolve()->ImmediateU32();
        const auto component = inst.Argument(1)->Resolve()->ImmediateU32();
        if (stage == IrShaderStage::Pixel) {
            if (inputInfo.pixel == nullptr) fail("pixel input info is missing");
            return pixelAttribute(location, component);
        }
        if (inst.Flags<std::uint32_t>() != 1u) return vertexAttribute(location, component & 3u);
        if (inputInfo.vertex == nullptr || !inputInfo.vertex->fetchEmbedded || component >= 4u) fail(inst, "is an invalid prepared vertex attribute");
        const auto first = PipelineSpecialization::VertexBase + location * PipelineSpecialization::VertexWords;
        const auto selector = spec(first + component, component + 4u);
        if (selector == 1u) return Hex(spec(first + 4u, 0x3f800000u));
        if (selector >= 4u && selector < 8u) return vertexAttribute(location, selector - 4u);
        return Hex(0);
    }

    std::string exportVector(const std::string& data, const ExportInfo& info, std::uint32_t mode, bool uintOutput) {
        if (info.compr && !uintOutput) {
            const auto unpack = mode == 5u ? "unpack2x16unorm" : mode == 6u ? "unpack2x16snorm" : "unpack2x16float";
            std::array<std::string, 4> components{FloatBits(0), FloatBits(0), FloatBits(0), FloatBits(0x3f800000u)};
            for (std::uint32_t pair = 0; pair < 2u; ++pair) {
                if ((info.en & (3u << (pair * 2u))) == 0u) continue;
                const auto unpacked = bind(std::string(unpack) + "(" + data + "." + Component(pair) + ")");
                for (std::uint32_t lane = 0; lane < 2u; ++lane) {
                    const auto component = pair * 2u + lane;
                    if (((info.en >> component) & 1u) != 0u) components[component] = unpacked + "." + Component(lane);
                }
            }
            return bind("vec4<f32>(" + components[0] + ", " + components[1] + ", " + components[2] + ", " + components[3] + ")");
        }
        std::array<std::string, 4> raw{Hex(0), Hex(0), Hex(0), Hex(uintOutput ? 1u : 0x3f800000u)};
        const auto exported = info.en & (mode == 1u ? 0x1u : mode == 2u || mode == 3u ? 0x3u : 0xfu);
        if (info.compr) {
            for (std::uint32_t pair = 0; pair < 2u; ++pair) {
                if ((info.en & (3u << (pair * 2u))) == 0u) continue;
                for (std::uint32_t lane = 0; lane < 2u; ++lane) {
                    const auto component = pair * 2u + lane;
                    if (((info.en >> component) & 1u) != 0u) raw[component] = "extractBits(" + data + "." + Component(pair) + ", " + Hex(lane * 16u) + ", 16u)";
                }
            }
        } else {
            for (std::uint32_t component = 0; component < 4u; ++component) {
                if (((exported >> component) & 1u) != 0u) raw[mode == 3u && component == 1u ? 3u : component] = data + "." + Component(component);
            }
        }
        const auto vector = "vec4<u32>(" + raw[0] + ", " + raw[1] + ", " + raw[2] + ", " + raw[3] + ")";
        return bind(uintOutput ? vector : "bitcast<vec4<f32>>(" + vector + ")");
    }

    void exportAttribute(const IrValue& inst) {
        const auto& info = exportOf(inst);
        const auto exec = argument(inst, 1);
        if (stage == IrShaderStage::Pixel && info.vm && validMask) line("aps5_valid = select(0u, 1u, " + exec + ");");
        if (info.kind == ExportTargetKind::Null || info.en == 0u || info.kind == ExportTargetKind::Primitive) return;
        if (info.kind == ExportTargetKind::Position && info.index != 0u) fail(inst, "exports an auxiliary position, which is not implemented");
        if (info.kind == ExportTargetKind::MrtZ) fail(inst, "exports depth, which is not implemented");
        if (info.kind != ExportTargetKind::Position && info.kind != ExportTargetKind::Parameter && info.kind != ExportTargetKind::Mrt) fail(inst, "has an unsupported export target");
        const auto mode = stage == IrShaderStage::Pixel && info.kind == ExportTargetKind::Mrt && inputInfo.pixel != nullptr && info.index < std::size(inputInfo.pixel->targetOutputMode) ? inputInfo.pixel->targetOutputMode[info.index] : 0u;
        const bool uintOutput = mode == 7u;
        line("if (" + exec + ") {");
        indent += "    ";
        auto vector = exportVector(argument(inst, 0), info, mode, uintOutput);
        if (info.kind == ExportTargetKind::Mrt) {
            if (stage != IrShaderStage::Pixel || info.index >= 8u) fail(inst, "exports an invalid render target");
            std::array<std::string, 4> mapped;
            for (std::uint32_t component = 0; component < 4u; ++component) mapped[component] = vector + "." + Component(spec(PipelineSpecialization::ExportBase + info.index * 4u + component, component));
            vector = bind(std::string(uintOutput ? "vec4<u32>" : "vec4<f32>") + "(" + mapped[0] + ", " + mapped[1] + ", " + mapped[2] + ", " + mapped[3] + ")");
            if (info.index == 0u && !uintOutput && inputInfo.pixel != nullptr && inputInfo.pixel->psAlphaToCoverage) fail(inst, "uses alpha to coverage, which is not implemented");
            if (!uintOutput && mode != 5u && mode != 6u && spec(PipelineSpecialization::ExportPackingBase + info.index, static_cast<std::uint32_t>(ColorExportPacking::None)) != static_cast<std::uint32_t>(ColorExportPacking::None)) fail(inst, "exports a packed 10:11:11 target, which is not implemented");
            renderTargets[info.index] = uintOutput;
            line("aps5_out.target" + std::to_string(info.index) + " = " + vector + ";");
        } else if (info.kind == ExportTargetKind::Position) {
            if (inputInfo.vertex == nullptr) fail(inst, "has no vertex input info for a position export");
            const auto& transform = inputInfo.vertex->clipSpace;
            if (transform.enabled) {
                const auto convert = [&](std::uint32_t axis) { return "(((" + vector + "." + Component(axis) + " * " + FloatLiteral(transform.scale[axis]) + ") + " + FloatLiteral(transform.offset[axis]) + ") / " + FloatLiteral(transform.halfExtent[axis]) + " - " + FloatLiteral(1.0f) + ")"; };
                vector = bind("vec4<f32>(" + convert(0) + ", " + convert(1) + ", " + vector + ".z, " + vector + ".w)");
            }
            line("aps5_out.position = " + vector + ";");
        } else {
            parameters.insert(info.index);
            line("aps5_out.parameter" + std::to_string(info.index) + " = " + vector + ";");
        }
        indent.resize(indent.size() - 4);
        line("}");
    }

    struct BufferDescriptor {
        std::uint32_t word1;
        std::uint32_t word3;
        bool present;
        std::string records;
    };

    BufferDescriptor bufferDescriptor(const IrValue& inst, const MemoryInfo& info) {
        const auto* handle = inst.Argument(0)->Resolve();
        if (handle->Opcode() != IrOpcode::GetBufferResource || handle->ArgumentCount() != 4u) fail(inst, "requires a runtime four-dword V#");
        if (info.gpuDescriptor) fail(inst, "reads through a GPU-written descriptor, which is not implemented");
        const auto first = PipelineSpecialization::BufferBase + info.resource * PipelineSpecialization::BufferWords;
        return {spec(first, 0u), spec(first + 1u, 0u), spec(first + 2u, 0u) != 0u, value(handle->Argument(2))};
    }

    void readConstBuffer(const IrValue& inst) {
        const auto& info = memory(inst);
        if (info.planningOnly) return;
        if (info.kind != ResourceKind::ScalarBuffer) fail(inst, "must read a scalar buffer resource");
        const auto descriptor = bufferDescriptor(inst, info);
        if (!descriptor.present) {
            define(inst, Hex(0));
            return;
        }
        helper(BufferFits);
        const auto stride = (descriptor.word1 >> 16u) & 0x3fffu;
        const auto offset = argument(inst, 1);
        const auto address = bind("(" + offset + " + " + Hex(info.offset) + ")");
        const auto byte = bind("(" + address + " & 0xfffffffcu)");
        const auto inBounds = bind("((" + address + " >= " + offset + ") && aps5_buffer_fits(" + Hex(stride) + ", " + descriptor.records + ", " + byte + ", 4u))");
        const auto elementIndex = element(DescriptorBindingKind::Buffers, info.resource);
        const auto buffer = resource(DescriptorBindingKind::Buffers, elementIndex, "array<u32>", "<storage, read>");
        const auto index = bind("((" + byte + " >> 2u) + (" + bufferByteOffset(elementIndex) + " >> 2u))");
        define(inst, "select(0u, " + buffer + "[" + index + "], " + inBounds + " && " + index + " < arrayLength(&" + buffer + "))");
    }

    std::string addressComponent(const IrValue& address, const MemoryInfo& info, std::uint32_t component) {
        const auto layout = GetRdnaImageAddressComponentLayout(info.imageSampleFlags, component);
        const auto packed = layout.bitOffset / 32u;
        if (packed >= address.ArgumentCount()) fail(address, "has an image address component outside of the address operands");
        auto result = value(address.Argument(packed));
        if (layout.bitWidth == 16u) {
            if ((layout.bitOffset & 31u) != 0u) result = "(" + result + " >> 16u)";
            result = "(" + result + " & 0xffffu)";
            return "unpack2x16float(" + result + ").x";
        }
        return "bitcast<f32>(" + result + ")";
    }

    std::string imageType(const ImageResource& image) const {
        if (image.resourceClass != ImageResourceClass::Sampled) fail("storage images are not implemented");
        if (image.depthCompare) fail("depth comparison images are not implemented");
        const auto scalar = ScalarType(image.numericClass);
        switch (image.dimension) {
        case RdnaImageDimension::Dim1D: return "texture_1d<" + scalar + ">";
        case RdnaImageDimension::Dim2D: return std::string(image.cube ? "texture_cube<" : "texture_2d<") + scalar + ">";
        case RdnaImageDimension::Dim2DArray: return std::string(image.cube ? "texture_cube_array<" : "texture_2d_array<") + scalar + ">";
        case RdnaImageDimension::Dim3D: return "texture_3d<" + scalar + ">";
        case RdnaImageDimension::Dim2DMsaa: return "texture_multisampled_2d<" + scalar + ">";
        default: break;
        }
        fail("image dimension " + std::to_string(static_cast<std::uint32_t>(image.dimension)) + " has no WGSL texture type");
    }

    void sample(const IrValue& inst) {
        const auto& info = memory(inst);
        const auto& images = program.Info().images;
        const auto& samplers = program.Info().samplers;
        if (info.resource >= images.size() || info.sampler >= samplers.size()) fail(inst, "names a missing image or sampler");
        const auto& base = images[info.resource];
        const auto& samplerInfo = samplers[info.sampler];
        if (base.table != NoTable || samplerInfo.table != NoTable) fail(inst, "samples through an image table, which is not implemented");
        const auto modes = ResourceMaterializer::RuntimeImageModes(base);
        const auto modeIndex = modes.size() == 1u ? 0u : spec(PipelineSpecialization::ImageBase + info.resource * PipelineSpecialization::ImageWords, 0u);
        if (modeIndex >= modes.size()) fail(inst, "selects a missing runtime image mode");
        const auto& image = modes[modeIndex];
        if (image.constantSwizzle) fail(inst, "samples a constant-swizzle image, which is not implemented");
        if (image.srgbDecode) fail(inst, "samples an sRGB image the device cannot sample");
        if (info.imagePacked) fail(inst, "is a packed horizontal gather, which is not implemented");
        if (image.conversionFormat != IrBufferFormat::Invalid || image.depthBits || image.depthUnorm16) fail(inst, "samples a converted image, which is not implemented");
        if (image.numericClass != IrTextureNumericClass::Float) fail(inst, "samples an integer image, which WGSL cannot sample");
        if ((info.imageSampleFlags & RdnaImageSampleFlagCompare) != 0u) fail(inst, "samples with depth comparison, which is not implemented");
        if (SamplesBaseLevelIfUnnormalized(samplerInfo) && spec(PipelineSpecialization::UnnormalizedBaseLevelBase + info.sampler, 0u) != 0u) fail(inst, "samples with unnormalized coordinates, which WebGPU does not support");
        const auto* address = inst.Argument(2)->Resolve();
        if (address->Opcode() != IrOpcode::MakeImageAddress) fail(inst, "has an image address not made by MakeImageAddress");
        const auto dimension = DimensionInfoFor(image.dimension);
        const auto addressDimension = DimensionInfoFor(info.imageDimension);
        const auto layoutInfo = LayoutOf(info);
        const auto present = std::min(addressDimension.coordinateComponents, dimension.coordinateComponents);
        if ((addressDimension.coordinateComponents < dimension.coordinateComponents && (image.cube || addressDimension.coordinateComponents == 0u)) || info.imageAddressComponents < layoutInfo.coord + present) fail(inst, "has an image address with too few coordinate components");
        if (image.cube) fail(inst, "samples a cube image, which is not implemented");
        const auto coordinate = [&](std::uint32_t index) { return index < present ? addressComponent(*address, info, layoutInfo.coord + index) : FloatBits(0); };
        std::string coords;
        std::string arrayIndex;
        if (dimension.spatialComponents == 1u) coords = coordinate(0);
        else if (dimension.spatialComponents == 2u) coords = "vec2<f32>(" + coordinate(0) + ", " + coordinate(1) + ")";
        else coords = "vec3<f32>(" + coordinate(0) + ", " + coordinate(1) + ", " + coordinate(2) + ")";
        if (dimension.arrayed) arrayIndex = "i32(round(" + coordinate(dimension.spatialComponents) + "))";
        if (dimension.multisampled) fail(inst, "samples a multisampled image");
        const auto imageKind = DescriptorBindingForImage(image);
        const auto imageElement = element(imageKind, info.resource);
        const auto texture = resource(imageKind, spec(PipelineSpecialization::DescriptorIndex(static_cast<std::uint32_t>(imageKind), imageElement), imageElement), imageType(image), "");
        const bool point = image.numericClass == IrTextureNumericClass::Sint || image.conversionFormat != IrBufferFormat::Invalid || image.depthBits;
        const auto samplerElement = element(DescriptorBindingKind::Samplers, info.sampler) + (point ? 1u : 0u);
        const auto sampler = resource(DescriptorBindingKind::Samplers, spec(PipelineSpecialization::DescriptorIndex(static_cast<std::uint32_t>(DescriptorBindingKind::Samplers), samplerElement), samplerElement), "sampler", "");
        std::string offset;
        if (layoutInfo.offset != NoComponent) {
            const auto component = GetRdnaImageAddressComponentLayout(info.imageSampleFlags, layoutInfo.offset);
            const auto* bits = component.bitWidth == 32u && component.bitOffset / 32u < address->ArgumentCount() ? address->Argument(component.bitOffset / 32u)->Resolve() : nullptr;
            if (bits == nullptr || !bits->HasImmediate()) fail(inst, "has a texel offset that is not a constant");
            std::array<std::string, 3> values;
            for (std::uint32_t index = 0; index < dimension.spatialComponents; ++index) values[index] = std::to_string(static_cast<std::int32_t>(((bits->ImmediateU32() >> (index * 8u)) & 0x3fu) ^ 0x20u) - 0x20) + "i";
            offset = dimension.spatialComponents == 1u ? values[0] : dimension.spatialComponents == 2u ? "vec2<i32>(" + values[0] + ", " + values[1] + ")" : "vec3<i32>(" + values[0] + ", " + values[1] + ", " + values[2] + ")";
            if (dimension.spatialComponents == 1u) fail(inst, "uses a texel offset on a 1D image, which WGSL does not support");
        }
        if (layoutInfo.clamp != NoComponent) fail(inst, "clamps its LOD, which is not implemented");
        const bool explicitLod = ImageSampleExplicitLod(info.imageSampleFlags, stage);
        std::string arguments = texture + ", " + sampler + ", " + coords + (arrayIndex.empty() ? "" : ", " + arrayIndex);
        std::string call;
        if ((info.imageSampleFlags & RdnaImageSampleFlagDerivative) != 0u) {
            const auto gradient = [&](std::uint32_t first) {
                std::array<std::string, 3> values;
                for (std::uint32_t index = 0; index < dimension.spatialComponents; ++index) values[index] = index < addressDimension.spatialComponents ? addressComponent(*address, info, first + index) : FloatBits(0);
                return dimension.spatialComponents == 1u ? values[0] : dimension.spatialComponents == 2u ? "vec2<f32>(" + values[0] + ", " + values[1] + ")" : "vec3<f32>(" + values[0] + ", " + values[1] + ", " + values[2] + ")";
            };
            call = "textureSampleGrad(" + arguments + ", " + gradient(layoutInfo.gradX) + ", " + gradient(layoutInfo.gradY);
        } else if (explicitLod) {
            call = "textureSampleLevel(" + arguments + ", " + ((info.imageSampleFlags & RdnaImageSampleFlagLod) != 0u ? addressComponent(*address, info, layoutInfo.lod) : FloatBits(0));
        } else if (layoutInfo.bias != NoComponent) {
            call = "textureSampleBias(" + arguments + ", " + addressComponent(*address, info, layoutInfo.bias);
        } else {
            call = "textureSample(" + arguments;
        }
        if (!offset.empty()) call += ", " + offset;
        call += ")";
        const auto texel = bind("bitcast<vec4<u32>>(" + call + ")");
        std::array<std::string, 4> swizzled;
        for (std::uint32_t component = 0; component < 4u; ++component) {
            const auto selector = spec(PipelineSpecialization::ImageBase + info.resource * PipelineSpecialization::ImageWords + 1u + component, component + 4u);
            swizzled[component] = selector == 1u ? Hex(0x3f800000u) : selector >= 4u && selector < 8u ? texel + "." + Component(selector - 4u) : Hex(0);
        }
        if (info.dataBits == 16u) {
            if (info.dataDwords > 4u) fail(inst, "has more than four packed image result dwords");
            const auto dmask = info.dmask != 0u ? info.dmask : 1u;
            std::vector<std::uint32_t> enabled;
            for (std::uint32_t component = 0; component < 4u; ++component) {
                if (((dmask >> component) & 1u) != 0u) enabled.push_back(component);
            }
            std::array<std::string, 4> packed{Hex(0), Hex(0), Hex(0), Hex(0)};
            for (std::uint32_t word = 0; word < info.dataDwords; ++word) {
                const auto low = word * 2u < enabled.size() ? "bitcast<f32>(" + swizzled[enabled[word * 2u]] + ")" : FloatBits(0);
                const auto high = word * 2u + 1u < info.componentCount && word * 2u + 1u < enabled.size() ? "bitcast<f32>(" + swizzled[enabled[word * 2u + 1u]] + ")" : FloatBits(0);
                packed[word] = "pack2x16float(vec2<f32>(" + low + ", " + high + "))";
            }
            define(inst, "vec4<u32>(" + packed[0] + ", " + packed[1] + ", " + packed[2] + ", " + packed[3] + ")");
            return;
        }
        define(inst, "vec4<u32>(" + swizzled[0] + ", " + swizzled[1] + ", " + swizzled[2] + ", " + swizzled[3] + ")");
    }

    std::string bindingDeclarations() {
        std::map<std::pair<std::uint32_t, std::uint32_t>, const Declaration*> byElement;
        std::set<std::uint32_t> referenced;
        for (const auto& declaration : declarations) {
            byElement[{declaration.binding, declaration.element}] = &declaration;
            referenced.insert(declaration.binding);
        }
        const auto group = stage == IrShaderStage::Pixel ? 1u : 0u;
        std::string text;
        std::uint32_t number = 0;
        std::set<std::pair<std::uint32_t, std::uint32_t>> placed;
        for (const auto& binding : planBindings) {
            if (!referenced.contains(binding.binding)) continue;
            for (std::uint32_t index = 0; index < binding.count; ++index, ++number) {
                const auto found = byElement.find({binding.binding, index});
                if (found == byElement.end()) continue;
                text += "@group(" + std::to_string(group) + ") @binding(" + std::to_string(number) + ") " + found->second->text + "\n";
                placed.insert(found->first);
            }
        }
        for (const auto& [key, declaration] : byElement) {
            if (!placed.contains(key)) fail("binding " + std::to_string(key.first) + " element " + std::to_string(key.second) + " is not in the prepared binding plan");
        }
        bindingsUsed.assign(referenced.begin(), referenced.end());
        return text;
    }

    std::string vertexFunctions(const std::string& arguments) const {
        std::string text = "fn aps5_adjust(output: Aps5Output) -> Aps5Output {\n    var adjusted = output;\n";
        text += "    if (aps5_flip_y) {\n        adjusted.position.y = -adjusted.position.y;\n    }\n";
        text += "    if (aps5_depth_half) {\n        adjusted.position.z = (adjusted.position.z + adjusted.position.w) * 0.5;\n    }\n    return adjusted;\n}\n\n";
        std::string decorated;
        for (const auto& [key, input] : entryInputs) decorated += (decorated.empty() ? "" : ", ") + input.attributes + " " + input.name + ": " + input.type;
        text += "@vertex\nfn main(" + decorated + ") -> Aps5Output {\n    return aps5_adjust(aps5_vertex(" + arguments + "));\n}\n";
        if (std::any_of(entryInputs.begin(), entryInputs.end(), [](const auto& entry) { return entry.first < 0x100u; })) return text;
        text += "\nfn aps5_rect_fourth(a: Aps5Output, b: Aps5Output, corner: Aps5Output) -> Aps5Output {\n    var result: Aps5Output;\n";
        text += "    result.position = a.position + b.position - corner.position;\n";
        for (const auto index : parameters) text += "    result.parameter" + std::to_string(index) + " = a.parameter" + std::to_string(index) + " + b.parameter" + std::to_string(index) + " - corner.parameter" + std::to_string(index) + ";\n";
        text += "    return result;\n}\n\n@vertex\nfn aps5_rect(@builtin(vertex_index) aps5_rect_vertex: u32";
        if (entryInputs.contains(0x101u)) text += ", @builtin(instance_index) aps5_instance_index: u32";
        std::string rectArguments;
        for (const auto& [key, input] : entryInputs) rectArguments += (rectArguments.empty() ? "" : ", ") + std::string(key == 0x100u ? "first + index" : input.name);
        text += ") -> Aps5Output {\n    let first = (aps5_rect_vertex / 6u) * 3u;\n    var corners: array<Aps5Output, 3>;\n";
        text += "    for (var index = 0u; index < 3u; index++) {\n        corners[index] = aps5_vertex(" + rectArguments + ");\n    }\n";
        text += "    var edge: array<bool, 3>;\n    for (var index = 0u; index < 3u; index++) {\n";
        text += "        let here = corners[index].position.xy;\n        let nextEqual = here == corners[(index + 1u) % 3u].position.xy;\n        let previousEqual = here == corners[(index + 2u) % 3u].position.xy;\n";
        text += "        edge[index] = (nextEqual.x && previousEqual.y) || (nextEqual.y && previousEqual.x);\n    }\n";
        text += "    let corner = select(select(0u, 2u, edge[2]), 1u, edge[1]);\n    var quads = array<u32, 6>(0u, 1u, 2u, 1u, 3u, 2u);\n    let quad = quads[aps5_rect_vertex % 6u];\n";
        text += "    if (quad == 3u) {\n        return aps5_adjust(aps5_rect_fourth(corners[(corner + 1u) % 3u], corners[(corner + 2u) % 3u], corners[corner]));\n    }\n";
        text += "    return aps5_adjust(corners[(corner + quad) % 3u]);\n}\n";
        return text;
    }

    WgslModule finish() {
        std::string text;
        for (const auto* helperText : helpers) text += std::string(helperText) + "\n";
        text += bindingDeclarations();
        if (!text.empty()) text += "\n";
        if (stage == IrShaderStage::Vertex) {
            std::string parameterList;
            std::string argumentList;
            for (const auto& [key, input] : entryInputs) {
                parameterList += (parameterList.empty() ? "" : ", ") + input.name + ": " + input.type;
                argumentList += (argumentList.empty() ? "" : ", ") + input.name;
            }
            text += "override aps5_flip_y: bool = false;\noverride aps5_depth_half: bool = false;\n\n";
            text += "struct Aps5Output {\n    @builtin(position) position: vec4<f32>,\n";
            for (const auto index : parameters) text += "    @location(" + std::to_string(index) + ") @interpolate(perspective, center) parameter" + std::to_string(index) + ": vec4<f32>,\n";
            text += "}\n\nfn aps5_vertex(" + parameterList + ") -> Aps5Output {\n    var aps5_out: Aps5Output;\n";
            text += prologue + withReturns({"return aps5_out;"}) + "}\n\n" + vertexFunctions(argumentList);
            return {std::move(text), std::move(bindingsUsed)};
        }
        std::string inputs;
        for (const auto& [key, input] : entryInputs) inputs += (inputs.empty() ? "" : ", ") + input.attributes + " " + input.name + ": " + input.type;
        const bool outputs = !renderTargets.empty();
        if (outputs) {
            text += "struct Aps5Output {\n";
            for (const auto& [index, uintOutput] : renderTargets) text += "    @location(" + std::to_string(index) + ") target" + std::to_string(index) + ": " + (uintOutput ? "vec4<u32>" : "vec4<f32>") + ",\n";
            text += "}\n\n";
        }
        text += "@fragment\nfn main(" + inputs + ")" + (outputs ? " -> Aps5Output" : "") + " {\n";
        if (outputs) text += "    var aps5_out: Aps5Output;\n";
        std::vector<std::string> epilogue;
        if (validMask) {
            text += "    var aps5_valid = 1u;\n";
            epilogue = {"if (aps5_valid == 0u) {", "    discard;", "}"};
        }
        epilogue.push_back(outputs ? "return aps5_out;" : "return;");
        text += prologue + withReturns(epilogue) + "}\n";
        return {std::move(text), std::move(bindingsUsed)};
    }

    const IrProgram& program;
    const ShaderStageInputInfo& inputInfo;
    const IrBindingLayout& layout;
    IrShaderStage stage;
    std::vector<WgslPlanBinding> planBindings;
    std::map<std::uint32_t, std::uint32_t> constants;
    std::map<const IrValue*, std::string> names;
    std::vector<const char*> helpers;
    std::vector<Declaration> declarations;
    std::set<std::string> declared;
    std::map<std::uint32_t, std::string> byteOffsets;
    std::map<std::uint32_t, EntryInput> entryInputs;
    std::set<std::uint32_t> parameters;
    std::map<std::uint32_t, bool> renderTargets;
    std::vector<std::uint32_t> bindingsUsed;
    std::string prologue;
    std::string body;
    std::string indent;
    std::map<std::uint32_t, std::size_t> blockIndex;
    std::set<std::uint32_t> emitted;
    std::uint32_t temporaries = 0;
    bool validMask = false;
};

}  // namespace

WgslModule WgslEmitter::Emit(const IrProgram& program, const ShaderStageInputInfo& inputInfo, std::span<const PipelineSpecializationConstant> constants, std::span<const WgslPlanBinding> planBindings) const {
    return Emitter(program, inputInfo, constants, planBindings).Run();
}

WgslSource::WgslSource(IrProgram program) : program(std::move(program)) {}

std::shared_ptr<const WgslModule> WgslSource::Module(const ShaderStageInputInfo& inputInfo, std::span<const PipelineSpecializationConstant> constants, std::span<const WgslPlanBinding> planBindings) const {
    std::vector<std::uint32_t> key;
    key.reserve(constants.size() * 2u + planBindings.size() * 2u + 1u);
    for (const auto& constant : constants) key.insert(key.end(), {constant.id, constant.value});
    key.push_back(0xffffffffu);
    for (const auto& binding : planBindings) key.insert(key.end(), {binding.binding, binding.count});
    std::lock_guard lock(mutex);
    if (const auto found = modules.find(key); found != modules.end()) return found->second;
    auto module = std::make_shared<const WgslModule>(WgslEmitter{}.Emit(program, inputInfo, constants, planBindings));
    modules.emplace(std::move(key), module);
    return module;
}

std::string SpecializeWgslVertexOutput(std::string source, std::uint32_t location, bool flat, bool linear) {
    const auto name = "@location(" + std::to_string(location) + ") @interpolate(perspective, center) parameter" + std::to_string(location) + ":";
    const auto found = source.find(name);
    if (found == std::string::npos) return source;
    const auto replacement = "@location(" + std::to_string(location) + ") " + (flat ? "@interpolate(flat)" : linear ? "@interpolate(linear, center)" : "@interpolate(perspective, center)") + " parameter" + std::to_string(location) + ":";
    source.replace(found, name.size(), replacement);
    return source;
}

}  // namespace ShaderRecompiler
