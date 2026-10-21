#include "WgslBackend/WgslEmitter.hpp"
#include "IntermediateRepresentation/IrMetadata.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include "SpirvBackend/SpirvBufferFormat.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::uint32_t NoComponent = 0xffffffffu;
constexpr const char* ReturnMarker = "aps5_return_marker;";
constexpr const char* RawLoad = "aps5_raw_load";
constexpr const char* StorageFormat = "aps5_storage_format";

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

constexpr const char* NumericMinMax = R"(fn aps5_numeric_min(lhs: u32, rhs: u32) -> u32 {
    return select(select(rhs, lhs, bitcast<f32>(lhs) < bitcast<f32>(rhs)), lhs | rhs, ((lhs | rhs) & 0x7fffffffu) == 0u);
}

fn aps5_numeric_max(lhs: u32, rhs: u32) -> u32 {
    return select(select(rhs, lhs, bitcast<f32>(lhs) >= bitcast<f32>(rhs)), lhs & rhs, ((lhs | rhs) & 0x7fffffffu) == 0u);
}
)";

constexpr const char* MinMax = R"(fn aps5_min(lhs: f32, rhs: f32) -> f32 {
    let r = bitcast<u32>(rhs);
    return bitcast<f32>(select(aps5_numeric_min(bitcast<u32>(lhs), r), bitcast<u32>(lhs), (r & 0x7fffffffu) > 0x7f800000u));
}

fn aps5_max(lhs: f32, rhs: f32) -> f32 {
    let r = bitcast<u32>(rhs);
    return bitcast<f32>(select(aps5_numeric_max(bitcast<u32>(lhs), r), bitcast<u32>(lhs), (r & 0x7fffffffu) > 0x7f800000u));
}
)";

constexpr const char* Fraction = R"(fn aps5_fract(value: f32) -> f32 {
    let fraction = fract(abs(value));
    return select(fraction, 1.0 - fraction, value < 0.0 && fraction != 0.0);
}
)";

constexpr const char* MultiplyHigh = R"(fn aps5_umulhi(a: u32, b: u32) -> u32 {
    let low = (a & 0xffffu) * (b & 0xffffu);
    let cross0 = (a & 0xffffu) * (b >> 16u);
    let cross1 = (a >> 16u) * (b & 0xffffu);
    let middle = (low >> 16u) + (cross0 & 0xffffu) + (cross1 & 0xffffu);
    return (a >> 16u) * (b >> 16u) + (cross0 >> 16u) + (cross1 >> 16u) + (middle >> 16u);
}
)";

constexpr const char* Logarithm = R"(fn aps5_log2(x: f32) -> f32 {
    let value = log2(x);
    let difference = x - 1.0;
    var polynomial = bitcast<f32>(0xbe000000u);
    polynomial = fma(polynomial, difference, bitcast<f32>(0x3e124925u));
    polynomial = fma(polynomial, difference, bitcast<f32>(0xbe2aaaabu));
    polynomial = fma(polynomial, difference, bitcast<f32>(0x3e4ccccdu));
    polynomial = fma(polynomial, difference, bitcast<f32>(0xbe800000u));
    polynomial = fma(polynomial, difference, bitcast<f32>(0x3eaaaaabu));
    polynomial = fma(polynomial, difference, bitcast<f32>(0xbf000000u));
    polynomial = fma(polynomial, difference, bitcast<f32>(0x3f800000u));
    let series = (difference * polynomial) * bitcast<f32>(0x3fb8aa3bu);
    let valueBits = bitcast<u32>(value);
    let seriesBits = bitcast<u32>(series);
    let distance = min(valueBits - seriesBits, seriesBits - valueBits);
    return select(value, series, abs(difference) <= bitcast<f32>(0x3e000000u) && distance > 4u);
}
)";

constexpr const char* SignedMultiplyHigh = R"(fn aps5_smulhi(a: u32, b: u32) -> u32 {
    return aps5_umulhi(a, b) - (select(0u, b, bitcast<i32>(a) < 0) + select(0u, a, bitcast<i32>(b) < 0));
}
)";

constexpr const char* FloatClass = R"(fn aps5_float_class(value: f32, mask: u32) -> bool {
    let bits = bitcast<u32>(value);
    let sign = (bits & 0x80000000u) != 0u;
    let magnitude = bits & 0x7fffffffu;
    let exponent = magnitude & 0x7f800000u;
    let mantissa = magnitude & 0x007fffffu;
    let nan = exponent == 0x7f800000u && mantissa != 0u;
    let quiet = (mantissa & 0x00400000u) != 0u;
    let infinite = exponent == 0x7f800000u && mantissa == 0u;
    let normal = exponent != 0u && exponent != 0x7f800000u;
    let denormal = exponent == 0u && mantissa != 0u;
    let zero = magnitude == 0u;
    var classes = select(0u, 1u, nan && !quiet) | select(0u, 2u, nan && quiet);
    classes |= select(select(0u, 0x200u, infinite), select(0u, 4u, infinite), sign);
    classes |= select(select(0u, 0x100u, normal), select(0u, 8u, normal), sign);
    classes |= select(select(0u, 0x80u, denormal), select(0u, 0x10u, denormal), sign);
    classes |= select(select(0u, 0x40u, zero), select(0u, 0x20u, zero), sign);
    return (classes & mask) != 0u;
}
)";

constexpr const char* Shift64 = R"(fn aps5_shl64(v: vec2<u32>, shift: u32) -> vec2<u32> {
    let amount = shift & 63u;
    let word = amount & 31u;
    let low = v.x << word;
    let carry = select(0u, v.x >> ((32u - word) & 31u), amount != 0u);
    return select(vec2<u32>(low, (v.y << word) | carry), vec2<u32>(0u, low), amount >= 32u);
}

fn aps5_shr64(v: vec2<u32>, shift: u32, arithmetic: bool) -> vec2<u32> {
    let amount = shift & 63u;
    let word = amount & 31u;
    let high = select(v.y >> word, bitcast<u32>(bitcast<i32>(v.y) >> word), arithmetic);
    let carry = select(0u, v.y << ((32u - word) & 31u), amount != 0u);
    let low = (v.x >> word) | carry;
    let fill = select(0u, bitcast<u32>(bitcast<i32>(v.y) >> 31u), arithmetic);
    return select(vec2<u32>(low, high), vec2<u32>(high, fill), amount >= 32u);
}
)";

constexpr const char* MedianOfThree = R"(fn aps5_med3(x: f32, y: f32, z: f32) -> f32 {
    let a = bitcast<u32>(x);
    let b = bitcast<u32>(y);
    let c = bitcast<u32>(z);
    let nanA = (a & 0x7fffffffu) > 0x7f800000u;
    let nanB = (b & 0x7fffffffu) > 0x7f800000u;
    let nanC = (c & 0x7fffffffu) > 0x7f800000u;
    let minAb = select(aps5_numeric_min(a, b), a, nanB);
    let min3 = select(aps5_numeric_min(minAb, c), minAb, nanC);
    let median = aps5_numeric_max(minAb, aps5_numeric_min(aps5_numeric_max(a, b), c));
    return bitcast<f32>(select(median, min3, nanA || nanB || nanC));
}
)";

constexpr const char* BufferAddress = R"(fn aps5_buffer_address(index: u32, offset: u32, soffset: u32, records: u32, stride: u32, word3: u32, swizzle: bool, bytes: u32) -> vec2<u32> {
    let indexShift = ((word3 >> 21u) & 3u) + 3u;
    let indices = 1u << indexShift;
    let swizzled = (((index >> indexShift) * stride + (offset & ~3u)) * indices) + (((index & (indices - 1u)) << 2u) + (offset & 3u));
    let byte = select(index * stride + offset, swizzled, swizzle) + soffset;
    let mode = (word3 >> 28u) & 3u;
    let indexInBounds = index < records;
    let rawRecords = records - soffset;
    let strideFits = stride >= bytes && offset <= stride - bytes;
    let recordsFit = rawRecords >= bytes && offset <= rawRecords - bytes;
    let raw = soffset <= records && select(recordsFit, index < rawRecords && strideFits, swizzle);
    var inBounds = select(indexInBounds, indexInBounds && offset < stride, mode == 0u);
    inBounds = select(select(raw, records != 0u, mode == 2u), inBounds, mode < 2u);
    inBounds = inBounds && ((word3 >> 12u) & 0x7fu) != 0u;
    return vec2<u32>(byte, select(0u, 1u, inBounds));
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
        if (stage != IrShaderStage::Vertex && stage != IrShaderStage::Pixel && stage != IrShaderStage::Compute) fail("only vertex, pixel and compute programs are implemented");
        if (layout.UsesPushData()) fail("push data is not available in WebGPU");
        if (program.Metadata().barycentricEmulation) fail("barycentric emulation is not implemented");
        if (stage == IrShaderStage::Compute && inputInfo.compute == nullptr) fail("compute input info is missing");
        prepareValidMask();
        prepareWrittenBuffers();
        prepareWaveLds();
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
        line((hoisted.contains(&inst) ? "" : "let ") + name + " = " + expression + ";");
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
            case IrType::U64: return "vec2<u32>(" + Hex(static_cast<std::uint32_t>(resolved->ImmediateU64())) + ", " + Hex(static_cast<std::uint32_t>(resolved->ImmediateU64() >> 32u)) + ")";
            case IrType::F32: return FloatLiteral(resolved->ImmediateF32());
            default: fail(*resolved, "has an immediate of an unsupported type");
            }
        }
        const auto found = names.find(resolved);
        if (found == names.end()) fail(*resolved, "is used before its definition");
        if (found->second == RawLoad) fail(*resolved, "loads through a raw address and its value is used, which is not implemented");
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
        const auto declaration = "var" + space + " " + name + ": " + type + ";";
        if (!declared.contains(name)) {
            declared.insert(name);
            declarations.push_back({binding, element, name, declaration});
        } else if (const auto found = std::find_if(declarations.begin(), declarations.end(), [&](const auto& entry) { return entry.name == name; }); found != declarations.end() && found->text != declaration) {
            fail("binding " + name + " is used both as `" + found->text + "` and as `" + declaration + "`");
        }
        return name;
    }

    std::string buffer(std::uint32_t elementIndex) {
        return resource(DescriptorBindingKind::Buffers, elementIndex, "array<u32>", writtenBuffers.contains(elementIndex) ? "<storage, read_write>" : "<storage, read>");
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

    void prepareWrittenBuffers() {
        for (const auto* block : program.BlockOrder()) {
            for (const auto* inst : block->Instructions()) {
                switch (inst->Opcode()) {
                case IrOpcode::StoreBufferU8:
                case IrOpcode::StoreBufferU16:
                case IrOpcode::StoreBufferU32:
                case IrOpcode::StoreBufferU32x2:
                case IrOpcode::StoreBufferU32x3:
                case IrOpcode::StoreBufferU32x4:
                    if (stage != IrShaderStage::Compute) fail(*inst, "stores to a buffer outside a compute program, which is not implemented");
                    writtenBuffers.insert(element(DescriptorBindingKind::Buffers, memory(*inst).resource));
                    break;
                case IrOpcode::ImageWrite:
                    if (stage != IrShaderStage::Compute) fail(*inst, "writes an image outside a compute program, which is not implemented");
                    writtenImages.insert(memory(*inst).resource);
                    break;
                default:
                    break;
                }
            }
        }
    }

    void prepareWaveLds() {
        if (stage != IrShaderStage::Compute) return;
        bool writes = false;
        for (const auto* block : program.BlockOrder()) {
            for (const auto* inst : block->Instructions()) {
                const auto access = SharedAccessOf(inst->Opcode());
                writes |= access != SharedAccess::None && access != SharedAccess::Read;
            }
        }
        const auto& threads = inputInfo.compute->threadsNum;
        waveLds = writes && std::max(threads[0], 1u) * std::max(threads[1], 1u) * std::max(threads[2], 1u) <= program.WaveSize();
        bool barriers = false;
        for (const auto* block : program.BlockOrder()) {
            for (const auto* inst : block->Instructions()) barriers |= inst->Opcode() == IrOpcode::Barrier;
        }
        flatten = waveLds || barriers;
    }

    std::string guarded(const std::string& active) const {
        return predicate == "true" ? active : "(" + predicate + " && " + active + ")";
    }

    void orderWaveLds(const IrValue& inst) {
        if (!waveLds) return;
        if (inst.Opcode() == IrOpcode::Barrier) {
            ldsWritten = false;
            ldsRead = false;
            return;
        }
        const auto access = SharedAccessOf(inst.Opcode());
        if (access == SharedAccess::None) return;
        const bool writes = access != SharedAccess::Read;
        if (ldsWritten || (writes && ldsRead)) {
            line("workgroupBarrier();");
            ldsWritten = false;
            ldsRead = false;
        }
        ldsWritten |= writes;
        ldsRead |= access != SharedAccess::Write;
    }

    std::string lds() {
        const auto dwords = stage == IrShaderStage::Compute ? inputInfo.compute->ldsSizeDwords : 0u;
        if (dwords == 0u) fail("accesses LDS without an LDS allocation");
        if (!ldsDeclared) {
            ldsDeclared = true;
            moduleVariables += "var<workgroup> aps5_lds: array<u32, " + std::to_string(dwords) + ">;\n";
        }
        return "aps5_lds";
    }

    std::string ldsIndex(const IrValue& inst, std::uint32_t component) {
        const auto& info = memory(inst);
        if (info.kind != ResourceKind::Lds) fail(inst, "must access LDS");
        return bind("((" + argument(inst, 0) + " + " + Hex(info.offset + component * 4u) + ") >> 2u)");
    }

    void loadShared(const IrValue& inst, std::uint32_t components) {
        const auto storage = lds();
        const auto last = Hex(inputInfo.compute->ldsSizeDwords - 1u);
        const auto active = argument(inst, inst.ArgumentCount() - 1u);
        std::array<std::string, 4> values;
        for (std::uint32_t component = 0; component < components; ++component) {
            const auto index = ldsIndex(inst, component);
            values[component] = bind("select(0u, " + storage + "[min(" + index + ", " + last + ")], " + active + " && " + index + " <= " + last + ")");
        }
        if (components == 1u) {
            define(inst, values[0]);
            return;
        }
        std::string list;
        for (std::uint32_t component = 0; component < components; ++component) list += (component == 0u ? "" : ", ") + values[component];
        define(inst, "vec" + std::to_string(components) + "<u32>(" + list + ")");
    }

    void writeShared(const IrValue& inst, std::uint32_t components) {
        const auto storage = lds();
        const auto last = Hex(inputInfo.compute->ldsSizeDwords - 1u);
        const auto data = argument(inst, inst.ArgumentCount() - 2u);
        line("if (" + guarded(argument(inst, inst.ArgumentCount() - 1u)) + ") {");
        indent += "    ";
        for (std::uint32_t component = 0; component < components; ++component) {
            const auto index = ldsIndex(inst, component);
            line("if " + std::string("(") + index + " <= " + last + ") {");
            line("    " + storage + "[" + index + "] = " + (components == 1u ? data : data + "." + Component(component)) + ";");
            line("}");
        }
        indent.resize(indent.size() - 4);
        line("}");
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
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            blockIndex[blockInfo[index].id] = index;
            blockIds[blocks[index]] = blockInfo[index].id;
        }
        emitRegion(blockInfo.front().id, InvalidControlFlowId, InvalidControlFlowId);
    }

    static std::string phiName(const IrValue& phi) {
        return "aps5_phi" + std::to_string(phi.Id());
    }

    std::string valueType(const IrValue& inst) const {
        switch (inst.Type()) {
        case IrType::Bool: return "bool";
        case IrType::U32: return "u32";
        case IrType::U64: return "vec2<u32>";
        case IrType::F32: return "f32";
        case IrType::Vec2U32: return "vec2<u32>";
        case IrType::Vec3U32: return "vec3<u32>";
        case IrType::Vec4U32: return "vec4<u32>";
        case IrType::Vec2F32: return "vec2<f32>";
        case IrType::Vec3F32: return "vec3<f32>";
        case IrType::Vec4F32: return "vec4<f32>";
        default: break;
        }
        fail(inst, "has a type WGSL values do not hold");
    }

    std::vector<const IrValue*> phisOf(std::uint32_t block) const {
        std::vector<const IrValue*> result;
        const auto found = blockIndex.find(block);
        if (found == blockIndex.end()) return result;
        for (const auto* inst : program.BlockOrder()[found->second]->Instructions()) {
            if (inst->Opcode() == IrOpcode::Phi) result.push_back(inst);
        }
        return result;
    }

    struct LoopFrame {
        std::uint32_t header;
        std::uint32_t merge;
        std::uint32_t continueBlock;
    };

    std::vector<std::uint32_t> successors(std::uint32_t block) const {
        const auto found = blockIndex.find(block);
        if (found == blockIndex.end()) return {};
        const auto& terminator = program.Metadata().blockInfo[found->second].terminator;
        if (terminator.kind == TerminatorKind::Branch) return {terminator.trueBlock};
        if (terminator.kind == TerminatorKind::ConditionalBranch) return {terminator.trueBlock, terminator.falseBlock};
        return {};
    }

    std::set<std::uint32_t> loopBlocks(std::uint32_t header, std::uint32_t merge) const {
        std::set<std::uint32_t> result{header};
        std::vector<std::uint32_t> pending{header};
        while (!pending.empty()) {
            const auto block = pending.back();
            pending.pop_back();
            for (const auto next : successors(block)) {
                if (next == merge || next == InvalidControlFlowId || !result.insert(next).second) continue;
                pending.push_back(next);
            }
        }
        return result;
    }

    std::uint32_t blockOf(const IrValue* value) const {
        const auto* parent = value->Parent();
        if (parent == nullptr) return InvalidControlFlowId;
        const auto found = blockIds.find(parent);
        return found == blockIds.end() ? InvalidControlFlowId : found->second;
    }

    void hoistLoopValues(const LoopFrame& frame) {
        const auto inside = loopBlocks(frame.header, frame.merge);
        const auto& blocks = program.BlockOrder();
        std::vector<const IrValue*> declared;
        const auto hoist = [&](const IrValue* value) {
            const auto* resolved = value->Resolve();
            if (resolved->HasImmediate() || resolved->Type() == IrType::Void || hoisted.contains(resolved)) return;
            const auto block = blockOf(resolved);
            if (!inside.contains(block)) return;
            hoisted.insert(resolved);
            declared.push_back(resolved);
        };
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            const auto user = program.Metadata().blockInfo[index].id;
            for (const auto* inst : blocks[index]->Instructions()) {
                for (std::size_t argument = 0; argument < inst->ArgumentCount(); ++argument) {
                    const auto* defined = inst->Argument(argument)->Resolve();
                    if (defined->HasImmediate()) continue;
                    const auto from = blockOf(defined);
                    if (!inside.contains(from)) continue;
                    if (inst->Opcode() == IrOpcode::Phi) {
                        const auto edge = blockIds.find(inst->PhiBlock(argument));
                        const auto source = edge == blockIds.end() ? InvalidControlFlowId : edge->second;
                        if (user == frame.header || !inside.contains(user) || source == frame.continueBlock) {
                            if (from != frame.continueBlock || source != frame.continueBlock) hoist(defined);
                        }
                        continue;
                    }
                    if (!inside.contains(user) || (user == frame.continueBlock && from != frame.continueBlock)) hoist(defined);
                }
            }
            const auto& info = program.Metadata().blockInfo[index];
            if (info.id == frame.continueBlock && info.condition != nullptr && blockOf(info.condition->Resolve()) != frame.continueBlock) hoist(info.condition);
        }
        for (const auto* value : declared) line("var v" + std::to_string(value->Id()) + ": " + valueType(*value) + ";");
    }

    void declarePhis(std::uint32_t block) {
        for (const auto* phi : phisOf(block)) {
            if (declaredPhis.insert(phi).second) line("var " + phiName(*phi) + ": " + valueType(*phi) + " = " + valueType(*phi) + "();");
        }
    }

    std::uint32_t emitLoop(std::uint32_t header, std::uint32_t previous) {
        if (flatten) fail("loops in a program whose barriers need uniform control flow are not implemented");
        const auto found = blockIndex.find(header);
        const auto& info = program.Metadata().blockInfo[found->second];
        const auto& terminator = info.terminator;
        const LoopFrame frame{header, terminator.mergeBlock, terminator.continueBlock == InvalidControlFlowId ? header : terminator.continueBlock};
        if (frame.merge == InvalidControlFlowId) fail("a loop without a merge block is not implemented");
        declarePhis(header);
        declarePhis(frame.merge);
        if (frame.continueBlock != header) declarePhis(frame.continueBlock);
        assignPhis(previous, header);
        hoistLoopValues(frame);
        loops.push_back(frame);
        line("loop {");
        indent += "    ";
        if (!emitted.insert(header).second) fail("a loop header is emitted twice");
        for (const auto* inst : program.BlockOrder()[found->second]->Instructions()) {
            orderWaveLds(*inst);
            emit(*inst);
        }
        std::uint32_t body = InvalidControlFlowId;
        if (terminator.kind == TerminatorKind::Branch) {
            body = terminator.trueBlock;
        } else if (terminator.kind == TerminatorKind::ConditionalBranch && terminator.trueBlock == terminator.falseBlock) {
            body = terminator.trueBlock;
        } else if (terminator.kind == TerminatorKind::ConditionalBranch) {
            if (info.condition == nullptr) fail("a loop header branches without a condition");
            if (terminator.condition == BranchCondition::GotoVariable || terminator.condition == BranchCondition::IndirectTarget || terminator.condition == BranchCondition::Unknown) fail("goto and indirect branch conditions are not implemented");
            const bool exitOnTrue = terminator.trueBlock == frame.merge || terminator.trueBlock == frame.continueBlock;
            const bool exitOnFalse = terminator.falseBlock == frame.merge || terminator.falseBlock == frame.continueBlock;
            if (exitOnTrue == exitOnFalse) fail("a loop header that branches to two paths inside its loop is not implemented");
            const auto leave = exitOnTrue ? terminator.trueBlock : terminator.falseBlock;
            body = exitOnTrue ? terminator.falseBlock : terminator.trueBlock;
            line("if (" + std::string(exitOnTrue ? "" : "!") + value(info.condition) + ") {");
            indent += "    ";
            emitRegion(leave, InvalidControlFlowId, header);
            indent.resize(indent.size() - 4);
            line("}");
        } else {
            fail("a loop header ends without a branch");
        }
        if (body == frame.continueBlock && frame.continueBlock != header) {
            assignPhis(header, frame.continueBlock);
        } else if (body != frame.continueBlock) {
            emitRegion(body, frame.continueBlock, header);
        }
        line("continuing {");
        indent += "    ";
        if (frame.continueBlock != header) {
            const auto continued = blockIndex.find(frame.continueBlock);
            if (continued == blockIndex.end()) fail("a loop continues at an unknown block");
            if (!emitted.insert(frame.continueBlock).second) fail("a loop continue block is emitted twice");
            for (const auto* inst : program.BlockOrder()[continued->second]->Instructions()) {
                orderWaveLds(*inst);
                emit(*inst);
            }
            const auto& latch = program.Metadata().blockInfo[continued->second];
            const auto& back = latch.terminator;
            if (back.kind == TerminatorKind::Branch || (back.kind == TerminatorKind::ConditionalBranch && back.trueBlock == back.falseBlock)) {
                if (back.trueBlock != header) fail("a loop continue block does not branch back to its header");
                assignPhis(frame.continueBlock, header);
            } else if (back.kind == TerminatorKind::ConditionalBranch) {
                if (latch.condition == nullptr) fail("a loop continue block branches without a condition");
                const bool backOnTrue = back.trueBlock == header && back.falseBlock == frame.merge;
                const bool backOnFalse = back.falseBlock == header && back.trueBlock == frame.merge;
                if (!backOnTrue && !backOnFalse) fail("a loop continue block branches somewhere other than its header and merge");
                assignPhis(frame.continueBlock, header);
                assignPhis(frame.continueBlock, frame.merge);
                line("break if " + std::string(backOnTrue ? "!" : "") + value(latch.condition) + ";");
            } else {
                fail("a loop continue block ends without a branch");
            }
        }
        indent.resize(indent.size() - 4);
        line("}");
        indent.resize(indent.size() - 4);
        line("}");
        loops.pop_back();
        return frame.merge;
    }

    void assignPhis(std::uint32_t from, std::uint32_t to) {
        if (from == InvalidControlFlowId || to == InvalidControlFlowId) return;
        for (const auto* phi : phisOf(to)) {
            if (!declaredPhis.contains(phi)) fail(*phi, "merges values outside an if/else, which is not implemented");
            bool assigned = false;
            for (std::size_t index = 0; index < phi->PhiBlockCount() && !assigned; ++index) {
                const auto found = blockIds.find(phi->PhiBlock(index));
                if (found == blockIds.end() || found->second != from) continue;
                line(phiName(*phi) + " = " + (predicate == "true" ? value(phi->Argument(index)) : "select(" + phiName(*phi) + ", " + value(phi->Argument(index)) + ", " + predicate + ")") + ";");
                assigned = true;
            }
            if (!assigned) fail(*phi, "has no value for an edge into its block");
        }
    }

    void emitRegion(std::uint32_t block, std::uint32_t stop, std::uint32_t previous) {
        const auto& blocks = program.BlockOrder();
        const auto& blockInfo = program.Metadata().blockInfo;
        while (block != stop) {
            if (block == InvalidControlFlowId) {
                line(ReturnMarker);
                return;
            }
            if (!loops.empty() && block == loops.back().merge) {
                assignPhis(previous, block);
                line("break;");
                return;
            }
            if (!loops.empty() && block == loops.back().continueBlock) {
                assignPhis(previous, block);
                line("continue;");
                return;
            }
            for (std::size_t outer = 0; outer + 1 < loops.size(); ++outer) {
                if (block == loops[outer].merge || block == loops[outer].continueBlock) fail("branches out of a nested loop, which is not implemented");
            }
            const auto found = blockIndex.find(block);
            if (found == blockIndex.end()) fail("branches to unknown block " + std::to_string(block));
            if (program.Metadata().blockInfo[found->second].terminator.loopHeader) {
                block = emitLoop(block, previous);
                previous = InvalidControlFlowId;
                continue;
            }
            if (!emitted.insert(block).second) fail("control flow other than nested if/else is not implemented");
            ldsWritten = true;
            ldsRead = false;
            for (const auto* inst : blocks[found->second]->Instructions()) {
                orderWaveLds(*inst);
                emit(*inst);
            }
            const auto& info = blockInfo[found->second];
            const auto& terminator = info.terminator;
            if (terminator.kind == TerminatorKind::Return) {
                if (predicate != "true") fail("returns from inside a branch of a program whose barriers need uniform control flow, which is not implemented");
                line(ReturnMarker);
                return;
            }
            if (terminator.kind == TerminatorKind::Branch) {
                previous = block;
                block = terminator.trueBlock;
                continue;
            }
            if (terminator.kind != TerminatorKind::ConditionalBranch) fail("indirect branches are not implemented");
            if (terminator.condition == BranchCondition::GotoVariable || terminator.condition == BranchCondition::IndirectTarget || terminator.condition == BranchCondition::Unknown) fail("goto and indirect branch conditions are not implemented");
            if (terminator.trueBlock == terminator.falseBlock) {
                previous = block;
                block = terminator.trueBlock;
                continue;
            }
            if (info.condition == nullptr) fail("a conditional branch has no condition");
            const auto merge = terminator.mergeBlock;
            for (const auto* phi : phisOf(merge)) {
                if (declaredPhis.insert(phi).second) line("var " + phiName(*phi) + ": " + valueType(*phi) + " = " + valueType(*phi) + "();");
            }
            if (flatten) {
                if (merge == InvalidControlFlowId) fail("branches without a merge in a program whose barriers need uniform control flow, which is not implemented");
                const auto condition = value(info.condition);
                const auto outer = predicate;
                const auto taken = bind("(" + outer + " && " + condition + ")");
                const auto skipped = bind("(" + outer + " && !" + condition + ")");
                predicate = taken;
                emitRegion(terminator.trueBlock, merge, block);
                predicate = skipped;
                emitRegion(terminator.falseBlock, merge, block);
                predicate = outer;
                previous = InvalidControlFlowId;
                block = merge;
                continue;
            }
            line("if (" + value(info.condition) + ") {");
            indent += "    ";
            emitRegion(terminator.trueBlock, merge, block);
            indent.resize(indent.size() - 4);
            line("} else {");
            indent += "    ";
            emitRegion(terminator.falseBlock, merge, block);
            indent.resize(indent.size() - 4);
            line("}");
            if (merge == InvalidControlFlowId) return;
            previous = InvalidControlFlowId;
            block = merge;
        }
        assignPhis(previous, stop);
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
        case IrOpcode::GetSrtResource:
        case IrOpcode::GetAddressResource:
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
        case IrOpcode::IMul32: return binary("*");
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
        case IrOpcode::FPRoundEven32:
            define(inst, "round(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPRecip32:
            define(inst, "(" + FloatBits(0x3f800000u) + " / " + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPMin32:
            helper(NumericMinMax);
            helper(MinMax);
            define(inst, "aps5_min(" + argument(inst, 0) + ", " + argument(inst, 1) + ")");
            return;
        case IrOpcode::FPMax32:
            helper(NumericMinMax);
            helper(MinMax);
            define(inst, "aps5_max(" + argument(inst, 0) + ", " + argument(inst, 1) + ")");
            return;
        case IrOpcode::FPFma32:
            define(inst, "fma(" + argument(inst, 0) + ", " + argument(inst, 1) + ", " + argument(inst, 2) + ")");
            return;
        case IrOpcode::FPRecipSqrt32:
            define(inst, "inverseSqrt(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPFract32:
            helper(Fraction);
            define(inst, "aps5_fract(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPFloor32:
            define(inst, "floor(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::UMin32:
            define(inst, "select(" + argument(inst, 1) + ", " + argument(inst, 0) + ", " + argument(inst, 0) + " < " + argument(inst, 1) + ")");
            return;
        case IrOpcode::UMax32:
            define(inst, "select(" + argument(inst, 1) + ", " + argument(inst, 0) + ", " + argument(inst, 0) + " > " + argument(inst, 1) + ")");
            return;
        case IrOpcode::SMin32:
            define(inst, "select(" + argument(inst, 1) + ", " + argument(inst, 0) + ", bitcast<i32>(" + argument(inst, 0) + ") < bitcast<i32>(" + argument(inst, 1) + "))");
            return;
        case IrOpcode::SMax32:
            define(inst, "select(" + argument(inst, 1) + ", " + argument(inst, 0) + ", bitcast<i32>(" + argument(inst, 0) + ") > bitcast<i32>(" + argument(inst, 1) + "))");
            return;
        case IrOpcode::UMinTri32:
        case IrOpcode::UMaxTri32:
        case IrOpcode::SMinTri32:
        case IrOpcode::SMaxTri32:
        case IrOpcode::UMedTri32:
        case IrOpcode::SMedTri32:
            integerThree(inst);
            return;
        case IrOpcode::FPMinTri32:
        case IrOpcode::FPMaxTri32: {
            helper(NumericMinMax);
            helper(MinMax);
            const auto function = inst.Opcode() == IrOpcode::FPMinTri32 ? "aps5_min(" : "aps5_max(";
            define(inst, std::string(function) + function + argument(inst, 0) + ", " + argument(inst, 1) + "), " + argument(inst, 2) + ")");
            return;
        }
        case IrOpcode::ShiftRightArithmetic32:
            define(inst, "bitcast<u32>(bitcast<i32>(" + argument(inst, 0) + ") >> " + argument(inst, 1) + ")");
            return;
        case IrOpcode::BitwiseNot32:
            define(inst, "(~" + argument(inst, 0) + ")");
            return;
        case IrOpcode::ULessThanEqual32: return binary("<=");
        case IrOpcode::UGreaterThanEqual32: return binary(">=");
        case IrOpcode::SLessThan32:
        case IrOpcode::SLessThanEqual32:
        case IrOpcode::SGreaterThan32:
        case IrOpcode::SGreaterThanEqual32: {
            const auto operation = inst.Opcode() == IrOpcode::SLessThan32 ? " < " : inst.Opcode() == IrOpcode::SLessThanEqual32 ? " <= " : inst.Opcode() == IrOpcode::SGreaterThan32 ? " > " : " >= ";
            define(inst, "(bitcast<i32>(" + argument(inst, 0) + ")" + operation + "bitcast<i32>(" + argument(inst, 1) + "))");
            return;
        }
        case IrOpcode::BitFieldUExtract:
            define(inst, "extractBits(" + argument(inst, 0) + ", " + argument(inst, 1) + ", " + argument(inst, 2) + ")");
            return;
        case IrOpcode::BitFieldSExtract:
            define(inst, "bitcast<u32>(extractBits(bitcast<i32>(" + argument(inst, 0) + "), " + argument(inst, 1) + ", " + argument(inst, 2) + "))");
            return;
        case IrOpcode::BitFieldInsert:
            define(inst, "insertBits(" + argument(inst, 0) + ", " + argument(inst, 1) + ", " + argument(inst, 2) + ", " + argument(inst, 3) + ")");
            return;
        case IrOpcode::BitCount32:
            define(inst, "countOneBits(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::BitReverse32:
            define(inst, "reverseBits(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FindUMsb32:
            define(inst, "firstLeadingBit(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FindILsb32:
            define(inst, "firstTrailingBit(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::IAbs32:
            define(inst, "bitcast<u32>(abs(bitcast<i32>(" + argument(inst, 0) + ")))");
            return;
        case IrOpcode::SMulHi:
            helper(MultiplyHigh);
            helper(SignedMultiplyHigh);
            define(inst, "aps5_smulhi(" + argument(inst, 0) + ", " + argument(inst, 1) + ")");
            return;
        case IrOpcode::UDiv32: return binary("/");
        case IrOpcode::IAddCarry32:
            define(inst, "vec2<u32>(" + argument(inst, 0) + " + " + argument(inst, 1) + ", select(0u, 1u, " + argument(inst, 0) + " + " + argument(inst, 1) + " < " + argument(inst, 0) + "))");
            return;
        case IrOpcode::SelectU1:
            define(inst, "select(" + argument(inst, 2) + ", " + argument(inst, 1) + ", " + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPCeil32:
            define(inst, "ceil(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPNeg32:
            define(inst, "(-" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPRecipIFlag32:
            define(inst, "(" + FloatBits(0x3f800000u) + " / " + argument(inst, 0) + ")");
            return;
        case IrOpcode::ConvertS32F32:
            define(inst, "bitcast<u32>(i32(" + argument(inst, 0) + "))");
            return;
        case IrOpcode::ConvertU32F32:
            define(inst, "u32(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPCmpClass32:
            helper(FloatClass);
            define(inst, "aps5_float_class(" + argument(inst, 0) + ", " + argument(inst, 1) + ")");
            return;
        case IrOpcode::CompositeConstructU64:
            define(inst, "vec2<u32>(" + argument(inst, 0) + ", " + argument(inst, 1) + ")");
            return;
        case IrOpcode::CompositeExtractU64:
            define(inst, argument(inst, 0) + "." + Component(inst.Argument(1)->Resolve()->ImmediateU32()));
            return;
        case IrOpcode::WqmU64: {
            const auto mask = argument(inst, 0);
            const auto merged = bind("(" + mask + " | (" + mask + " >> vec2<u32>(1u)))");
            define(inst, "(((" + merged + " | (" + merged + " >> vec2<u32>(2u))) & vec2<u32>(0x11111111u)) * vec2<u32>(0xfu))");
            return;
        }
        case IrOpcode::BitwiseAnd64: return binary("&");
        case IrOpcode::IEqual64:
            define(inst, "all(" + argument(inst, 0) + " == " + argument(inst, 1) + ")");
            return;
        case IrOpcode::INotEqual64:
            define(inst, "any(" + argument(inst, 0) + " != " + argument(inst, 1) + ")");
            return;
        case IrOpcode::ULessThan64:
        case IrOpcode::UGreaterThan64:
        case IrOpcode::SLessThan64: {
            const auto a = argument(inst, 0);
            const auto b = argument(inst, 1);
            const auto opcode = inst.Opcode();
            const auto high = opcode == IrOpcode::SLessThan64 ? "bitcast<i32>(" + a + ".y) < bitcast<i32>(" + b + ".y)" : a + ".y" + (opcode == IrOpcode::UGreaterThan64 ? " > " : " < ") + b + ".y";
            const auto low = a + ".x" + (opcode == IrOpcode::UGreaterThan64 ? " > " : " < ") + b + ".x";
            define(inst, "((" + high + ") || (" + a + ".y == " + b + ".y && " + low + "))");
            return;
        }
        case IrOpcode::IAdd64: {
            const auto a = argument(inst, 0);
            const auto b = argument(inst, 1);
            const auto low = bind("(" + a + ".x + " + b + ".x)");
            define(inst, "vec2<u32>(" + low + ", " + a + ".y + " + b + ".y + select(0u, 1u, " + low + " < " + a + ".x))");
            return;
        }
        case IrOpcode::ISub64: {
            const auto a = argument(inst, 0);
            const auto b = argument(inst, 1);
            define(inst, "vec2<u32>(" + a + ".x - " + b + ".x, " + a + ".y - " + b + ".y - select(0u, 1u, " + a + ".x < " + b + ".x))");
            return;
        }
        case IrOpcode::ShiftLeftLogical64:
            helper(Shift64);
            define(inst, "aps5_shl64(" + argument(inst, 0) + ", " + argument(inst, 1) + ")");
            return;
        case IrOpcode::ShiftRightLogical64:
        case IrOpcode::ShiftRightArithmetic64:
            helper(Shift64);
            define(inst, "aps5_shr64(" + argument(inst, 0) + ", " + argument(inst, 1) + ", " + (inst.Opcode() == IrOpcode::ShiftRightArithmetic64 ? "true" : "false") + ")");
            return;
        case IrOpcode::BitCount64:
            define(inst, "(countOneBits(" + argument(inst, 0) + ".x) + countOneBits(" + argument(inst, 0) + ".y))");
            return;
        case IrOpcode::FindUMsb64: {
            const auto v = argument(inst, 0);
            define(inst, "select(firstLeadingBit(" + v + ".x), firstLeadingBit(" + v + ".y) + 32u, " + v + ".y != 0u)");
            return;
        }
        case IrOpcode::FPExp2:
            define(inst, "exp2(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::FPLog2:
            helper(Logarithm);
            define(inst, "aps5_log2(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::LoadSharedU32:
            loadShared(inst, 1u);
            return;
        case IrOpcode::LoadSharedU32x2:
            loadShared(inst, 2u);
            return;
        case IrOpcode::LoadSharedU32x3:
            loadShared(inst, 3u);
            return;
        case IrOpcode::LoadSharedU32x4:
            loadShared(inst, 4u);
            return;
        case IrOpcode::WriteSharedU32:
            writeShared(inst, 1u);
            return;
        case IrOpcode::WriteSharedU32x2:
            writeShared(inst, 2u);
            return;
        case IrOpcode::WriteSharedU32x3:
            writeShared(inst, 3u);
            return;
        case IrOpcode::WriteSharedU32x4:
            writeShared(inst, 4u);
            return;
        case IrOpcode::Barrier:
            if (stage != IrShaderStage::Compute) fail(inst, "is a barrier outside a compute program, which is not implemented");
            line("workgroupBarrier();");
            return;
        case IrOpcode::DppMoveU32:
            dppMove(inst);
            return;
        case IrOpcode::DppUpdateU32:
            dppUpdate(inst);
            return;
        case IrOpcode::ConvertF32U32:
            define(inst, "f32(" + argument(inst, 0) + ")");
            return;
        case IrOpcode::UMulHi:
            helper(MultiplyHigh);
            define(inst, "aps5_umulhi(" + argument(inst, 0) + ", " + argument(inst, 1) + ")");
            return;
        case IrOpcode::ReadConst:
            readConst(inst);
            return;
        case IrOpcode::LoadAddressU8:
        case IrOpcode::LoadAddressU16:
        case IrOpcode::LoadAddressU32:
        case IrOpcode::LoadAddressU32x2:
        case IrOpcode::LoadAddressU32x3:
        case IrOpcode::LoadAddressU32x4:
            names[&inst] = RawLoad;
            return;
        case IrOpcode::ImageRead:
            imageRead(inst);
            return;
        case IrOpcode::ImageWrite:
            imageWrite(inst);
            return;
        case IrOpcode::FPMedTri32:
            helper(NumericMinMax);
            helper(MedianOfThree);
            define(inst, "aps5_med3(" + argument(inst, 0) + ", " + argument(inst, 1) + ", " + argument(inst, 2) + ")");
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
        case IrOpcode::CompositeExtractU32x2:
        case IrOpcode::CompositeExtractU32x3:
        case IrOpcode::CompositeExtractU32x4:
            define(inst, argument(inst, 0) + "." + Component(inst.Argument(1)->Resolve()->ImmediateU32()));
            return;
        case IrOpcode::CompositeConstructU32x2:
            define(inst, "vec2<u32>(" + argument(inst, 0) + ", " + argument(inst, 1) + ")");
            return;
        case IrOpcode::CompositeConstructU32x3:
            define(inst, "vec3<u32>(" + argument(inst, 0) + ", " + argument(inst, 1) + ", " + argument(inst, 2) + ")");
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
        case IrOpcode::ReadFirstLane:
            define(inst, argument(inst, 0));
            return;
        case IrOpcode::GetShaderBase:
            define(inst, "vec2<u32>(0u, 0u)");
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
        case IrOpcode::ImageGatherRaw:
            gather(inst);
            return;
        case IrOpcode::Phi:
            if (!declaredPhis.contains(&inst)) fail(inst, "merges values outside an if/else, which is not implemented");
            define(inst, phiName(inst));
            return;
        case IrOpcode::LoadBufferU8:
        case IrOpcode::LoadBufferU16:
        case IrOpcode::LoadBufferU32:
            loadBuffer(inst, 1u);
            return;
        case IrOpcode::LoadBufferU32x2:
            loadBuffer(inst, 2u);
            return;
        case IrOpcode::LoadBufferU32x3:
            loadBuffer(inst, 3u);
            return;
        case IrOpcode::LoadBufferU32x4:
            loadBuffer(inst, 4u);
            return;
        case IrOpcode::StoreBufferU32:
            storeBuffer(inst, 1u);
            return;
        case IrOpcode::StoreBufferU32x2:
            storeBuffer(inst, 2u);
            return;
        case IrOpcode::StoreBufferU32x3:
            storeBuffer(inst, 3u);
            return;
        case IrOpcode::StoreBufferU32x4:
            storeBuffer(inst, 4u);
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
        case StageInputKind::WorkgroupId:
            if (stage != IrShaderStage::Compute) break;
            entryInputs.insert({0x110u, {"@builtin(workgroup_id)", "aps5_workgroup_id", "vec3<u32>"}});
            return "aps5_workgroup_id." + Component(component);
        case StageInputKind::LocalInvocationId:
            if (stage != IrShaderStage::Compute) break;
            entryInputs.insert({0x111u, {"@builtin(local_invocation_id)", "aps5_local_invocation_id", "vec3<u32>"}});
            return "aps5_local_invocation_id." + Component(component);
        case StageInputKind::LocalInvocationIndex:
            if (stage != IrShaderStage::Compute) break;
            return localInvocationIndex();
        case StageInputKind::GlobalInvocationId:
            if (stage != IrShaderStage::Compute) break;
            entryInputs.insert({0x113u, {"@builtin(global_invocation_id)", "aps5_global_invocation_id", "vec3<u32>"}});
            return "aps5_global_invocation_id." + Component(component);
        case StageInputKind::DispatchThreadLimit:
            if (stage != IrShaderStage::Compute) break;
            return shaderDataDword(layout.DispatchThreadLimitDword() + component);
        default:
            break;
        }
        fail(inst, "reads builtin " + std::to_string(static_cast<std::uint32_t>(kind)) + ", which is not implemented");
    }

    std::string quadSource(const IrValue& inst, const DppMoveFlags& flags) {
        if (stage != IrShaderStage::Pixel || (flags.control & DppMoveFlags::Lanes8) != 0u || flags.control > 0xffu) fail(inst, "uses a DPP pattern other than a quad permutation in a pixel program, which is not implemented");
        subgroups = true;
        entryInputs.insert({0x100u, {"@builtin(position)", "aps5_frag_coord", "vec4<f32>"}});
        return bind("((" + Hex(flags.control) + " >> (((u32(aps5_frag_coord.x) & 1u) | ((u32(aps5_frag_coord.y) & 1u) << 1u)) * 2u)) & 3u)");
    }

    std::string quadPick(const std::string& value, const std::string& source) {
        return bind("select(select(select(quadBroadcast(" + value + ", 0u), quadBroadcast(" + value + ", 1u), " + source + " == 1u), quadBroadcast(" + value + ", 2u), " + source + " == 2u), quadBroadcast(" + value + ", 3u), " + source + " == 3u)");
    }

    std::string quadSourceActive(const IrValue* active, const std::string& source) {
        const auto* resolved = active->Resolve();
        if (resolved->HasImmediate() && resolved->ImmediateBool()) return "true";
        return "(" + quadPick("select(0u, 1u, " + value(active) + ")", source) + " != 0u)";
    }

    void dppMove(const IrValue& inst) {
        const auto flags = inst.Flags<DppMoveFlags>();
        const auto source = quadSource(inst, flags);
        auto moved = quadPick(argument(inst, 0), source);
        if (!flags.fetchInactive) moved = "select(0u, " + moved + ", " + quadSourceActive(inst.Argument(1), source) + ")";
        define(inst, moved);
    }

    void dppUpdate(const IrValue& inst) {
        const auto flags = inst.Flags<DppMoveFlags>();
        if (flags.rowMask != 0xfu || flags.bankMask != 0xfu) fail(inst, "uses DPP row or bank masks, which are not implemented");
        std::string writable = "true";
        if (!flags.boundControl) {
            const auto source = quadSource(inst, flags);
            if (!flags.fetchInactive) writable = quadSourceActive(inst.Argument(2), source);
        }
        define(inst, writable == "true" ? argument(inst, 0) : "select(" + argument(inst, 1) + ", " + argument(inst, 0) + ", " + writable + ")");
    }

    void integerThree(const IrValue& inst) {
        const auto opcode = inst.Opcode();
        const bool signedValue = opcode == IrOpcode::SMinTri32 || opcode == IrOpcode::SMaxTri32 || opcode == IrOpcode::SMedTri32;
        const auto pick = [&](const std::string& left, const std::string& right, bool maxValue) {
            const auto compare = signedValue ? "bitcast<i32>(" + left + ")" + (maxValue ? " > " : " < ") + "bitcast<i32>(" + right + ")" : left + (maxValue ? " > " : " < ") + right;
            return bind("select(" + right + ", " + left + ", " + compare + ")");
        };
        const auto a = argument(inst, 0);
        const auto b = argument(inst, 1);
        const auto c = argument(inst, 2);
        if (opcode == IrOpcode::UMinTri32 || opcode == IrOpcode::SMinTri32) {
            define(inst, pick(pick(a, b, false), c, false));
        } else if (opcode == IrOpcode::UMaxTri32 || opcode == IrOpcode::SMaxTri32) {
            define(inst, pick(pick(a, b, true), c, true));
        } else {
            const auto minimum = pick(pick(a, b, false), c, false);
            const auto maximum = pick(pick(a, b, true), c, true);
            define(inst, "(((" + a + " + " + b + ") + " + c + ") - " + minimum + ") - " + maximum);
        }
    }

    std::string localInvocationIndex() {
        entryInputs.insert({0x112u, {"@builtin(local_invocation_index)", "aps5_local_invocation_index", "u32"}});
        return "aps5_local_invocation_index";
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
            if (spec(PipelineSpecialization::ReversedBlend, 0u) != 0u) fail(inst, "blends a target with a reversed component order, which is not implemented");
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
        const auto storage = buffer(elementIndex);
        const auto index = bind("((" + byte + " >> 2u) + (" + bufferByteOffset(elementIndex) + " >> 2u))");
        define(inst, "select(0u, " + storage + "[" + index + "], " + inBounds + " && " + index + " < arrayLength(&" + storage + "))");
    }

    struct BufferAccess {
        std::string storage;
        std::string index;
        std::string valid;
        std::string address;
    };

    BufferAccess bufferAccess(const IrValue& inst, const MemoryInfo& info, const BufferDescriptor& descriptor, std::uint32_t componentOffset, std::uint32_t bytes) {
        if (spec(PipelineSpecialization::BufferBase + info.resource * PipelineSpecialization::BufferWords + 3u, 0u) != 0u) fail(inst, "accesses a buffer whose base is not dword aligned, which is not implemented");
        const auto stride = (descriptor.word1 >> 16u) & 0x3fffu;
        const bool swizzle = stride != 0u && ((descriptor.word1 >> 31u) & 1u) != 0u;
        const bool addTid = ((descriptor.word3 >> 23u) & 1u) != 0u;
        auto index = argument(inst, 1);
        if (addTid && stage == IrShaderStage::Compute) index = "(" + index + " + (" + localInvocationIndex() + " & " + Hex(program.WaveSize() - 1u) + "))";
        const auto offset = "(" + argument(inst, 2) + " + " + Hex(info.offset + componentOffset) + ")";
        helper(BufferAddress);
        const auto address = bind("aps5_buffer_address(" + index + ", " + offset + ", " + argument(inst, 3) + ", " + descriptor.records + ", " + Hex(stride) + ", " + Hex(descriptor.word3) + ", " + (swizzle ? "true" : "false") + ", " + Hex(bytes) + ")");
        const auto elementIndex = element(DescriptorBindingKind::Buffers, info.resource);
        const auto storage = buffer(elementIndex);
        const auto word = bind("((" + address + ".x >> 2u) + (" + bufferByteOffset(elementIndex) + " >> 2u))");
        const auto inBounds = addTid && stage != IrShaderStage::Compute ? std::string("false") : "(" + address + ".y != 0u)";
        return {storage, word, "(" + inBounds + " && " + word + " < arrayLength(&" + storage + "))", address};
    }

    SpirvBufferFormatInfo bufferFormat(const IrValue& inst, const MemoryInfo& info, const BufferDescriptor& descriptor) const {
        if (info.d16) fail(inst, "is a 16-bit formatted buffer access, which is not implemented");
        const auto format = GetFormatInfo(info.typed ? DecodeTBufferFormat(info.dataFormat, info.numberFormat) : static_cast<IrBufferFormat>((descriptor.word3 >> 12u) & 0x7fu));
        for (std::uint32_t component = 0; component < format.componentCount; ++component) {
            const bool plain = format.type == SpirvFormatComponentType::Uint || format.type == SpirvFormatComponentType::Sint || format.type == SpirvFormatComponentType::Float;
            if (!plain || format.packedBitfield || format.componentBits[component] != 32u) fail(inst, "uses buffer format " + std::to_string(static_cast<std::uint32_t>(format.format)) + ", whose conversion is not implemented");
        }
        return format;
    }

    std::array<std::string, 4> formattedLoad(const IrValue& inst, const MemoryInfo& info, const BufferDescriptor& descriptor, std::uint32_t components, const std::string& active) {
        std::array<std::string, 4> outputs{Hex(0), Hex(0), Hex(0), Hex(0)};
        const auto format = bufferFormat(inst, info, descriptor);
        const auto count = format.componentCount;
        if (count == 0u) return outputs;
        std::array<std::uint32_t, 4> selectors{};
        std::array<std::uint32_t, 4> selected{};
        std::array<bool, 4> required{};
        for (std::uint32_t output = 0; output < components; ++output) {
            selectors[output] = info.typed ? (output < count ? 4u + output : 0u) : (descriptor.word3 >> (output * 3u)) & 7u;
            selected[output] = (selectors[output] - 4u) % count;
            if (selectors[output] >= 4u) required[selected[output]] = true;
        }
        std::array<BufferAccess, 4> accesses;
        std::string inBounds = active;
        for (std::uint32_t component = 0; component < count; ++component) {
            if (!required[component]) continue;
            accesses[component] = bufferAccess(inst, info, descriptor, GetFormatComponentByteOffset(format, component), 4u);
            inBounds += " && " + accesses[component].valid;
        }
        const auto loaded = bind("(" + inBounds + ")");
        std::array<std::string, 4> values{Hex(0), Hex(0), Hex(0), Hex(0)};
        for (std::uint32_t component = 0; component < count; ++component) {
            if (required[component]) values[component] = bind("select(0u, " + accesses[component].storage + "[" + accesses[component].index + "], " + loaded + ")");
        }
        const bool integer = format.type == SpirvFormatComponentType::Uint || format.type == SpirvFormatComponentType::Sint;
        for (std::uint32_t output = 0; output < components; ++output) outputs[output] = selectors[output] == 0u ? Hex(0) : selectors[output] == 1u ? Hex(integer ? 1u : 0x3f800000u) : values[selected[output]];
        return outputs;
    }

    void loadBuffer(const IrValue& inst, std::uint32_t components) {
        const auto& info = memory(inst);
        if (info.kind != ResourceKind::Buffer) fail(inst, "must read a buffer resource");
        const auto descriptor = bufferDescriptor(inst, info);
        const auto type = components == 1u ? std::string("u32") : "vec" + std::to_string(components) + "<u32>";
        if (!descriptor.present) {
            define(inst, type + "()");
            return;
        }
        const auto active = argument(inst, inst.ArgumentCount() - 1u);
        std::array<std::string, 4> values;
        if (info.formatted) {
            values = formattedLoad(inst, info, descriptor, components, active);
        } else {
            for (std::uint32_t component = 0; component < components; ++component) {
                const auto access = bufferAccess(inst, info, descriptor, component * 4u, info.dataBits / 8u);
                auto word = access.storage + "[" + access.index + "]";
                if (info.dataBits != 32u) word = "((" + word + " >> ((" + access.address + ".x & 3u) * 8u)) & " + Hex((1u << info.dataBits) - 1u) + ")";
                values[component] = bind("select(0u, " + word + ", " + active + " && " + access.valid + ")");
            }
        }
        if (components == 1u) {
            define(inst, values[0]);
            return;
        }
        std::string list;
        for (std::uint32_t component = 0; component < components; ++component) list += (component == 0u ? "" : ", ") + values[component];
        define(inst, type + "(" + list + ")");
    }

    void storeBuffer(const IrValue& inst, std::uint32_t components) {
        const auto& info = memory(inst);
        if (info.kind != ResourceKind::Buffer) fail(inst, "must write a buffer resource");
        if (!info.formatted && info.dataBits != 32u) fail(inst, "stores part of a dword, which is not implemented");
        const auto descriptor = bufferDescriptor(inst, info);
        if (!descriptor.present) return;
        const auto data = argument(inst, inst.ArgumentCount() - 2u);
        const auto component = [&](std::uint32_t index) { return index >= components ? Hex(0) : components == 1u ? data : data + "." + Component(index); };
        line("if (" + guarded(argument(inst, inst.ArgumentCount() - 1u)) + ") {");
        indent += "    ";
        if (info.formatted) {
            const auto format = bufferFormat(inst, info, descriptor);
            std::vector<BufferAccess> accesses;
            std::string inBounds = "true";
            for (std::uint32_t index = 0; index < format.componentCount; ++index) {
                accesses.push_back(bufferAccess(inst, info, descriptor, GetFormatComponentByteOffset(format, index), 4u));
                inBounds += " && " + accesses.back().valid;
            }
            if (!accesses.empty()) {
                line("if (" + inBounds + ") {");
                for (std::uint32_t index = 0; index < accesses.size(); ++index) line("    " + accesses[index].storage + "[" + accesses[index].index + "] = " + component(index) + ";");
                line("}");
            }
        } else {
            for (std::uint32_t index = 0; index < components; ++index) {
                const auto access = bufferAccess(inst, info, descriptor, index * 4u, 4u);
                line("if " + access.valid + " {");
                line("    " + access.storage + "[" + access.index + "] = " + component(index) + ";");
                line("}");
            }
        }
        indent.resize(indent.size() - 4);
        line("}");
    }

    std::string addressU32(const IrValue& address, const MemoryInfo& info, std::uint32_t component) {
        const auto layout = GetRdnaImageAddressComponentLayout(info.imageSampleFlags, component);
        const auto packed = layout.bitOffset / 32u;
        if (packed >= address.ArgumentCount()) fail(address, "has an image address component outside of the address operands");
        auto result = value(address.Argument(packed));
        if (layout.bitWidth == 16u) {
            if ((layout.bitOffset & 31u) != 0u) result = "(" + result + " >> 16u)";
            result = "(" + result + " & 0xffffu)";
        }
        return result;
    }

    std::string addressComponent(const IrValue& address, const MemoryInfo& info, std::uint32_t component) {
        const auto bits = addressU32(address, info, component);
        if (GetRdnaImageAddressComponentLayout(info.imageSampleFlags, component).bitWidth == 16u) return "unpack2x16float(" + bits + ").x";
        return "bitcast<f32>(" + bits + ")";
    }

    void readConst(const IrValue& inst) {
        const auto* slot = inst.Argument(1)->Resolve();
        const auto& guarded = program.Resources().guardedSrtSlots;
        if (!guarded.empty() && (!slot->HasImmediate() || std::binary_search(guarded.begin(), guarded.end(), slot->ImmediateU32()))) fail(inst, "reads a flattened SRT slot whose memory was unmapped, which is not implemented");
        const auto srt = resource(DescriptorBindingKind::FlattenedSrt, 0, "array<u32>", "<storage, read>");
        define(inst, srt + "[" + value(slot) + "]");
    }

    const ImageResource& imageMode(const IrValue& inst, const MemoryInfo& info) const {
        const auto& images = program.Info().images;
        if (info.resource >= images.size()) fail(inst, "names a missing image");
        const auto& base = images[info.resource];
        if (base.table != NoTable) fail(inst, "accesses an image table, which is not implemented");
        const auto& modes = imageModes.emplace(info.resource, ResourceMaterializer::RuntimeImageModes(base)).first->second;
        const auto modeIndex = modes.size() == 1u ? 0u : spec(PipelineSpecialization::ImageBase + info.resource * PipelineSpecialization::ImageWords, 0u);
        if (modeIndex >= modes.size()) fail(inst, "selects a missing runtime image mode");
        return modes[modeIndex];
    }

    struct ImageCoordinates {
        std::string coordinates;
        std::string arrayIndex;
        std::string level;
    };

    ImageCoordinates imageCoordinates(const IrValue& inst, const MemoryInfo& info, const ImageResource& image) {
        const auto* address = inst.Argument(1)->Resolve();
        if (address->Opcode() != IrOpcode::MakeImageAddress) fail(inst, "has an image address not made by MakeImageAddress");
        const auto dimension = DimensionInfoFor(image.dimension);
        const auto addressDimension = DimensionInfoFor(info.imageDimension);
        if (dimension.multisampled || image.dimension == RdnaImageDimension::Dim3D || image.dimension == RdnaImageDimension::Dim1D || image.dimension == RdnaImageDimension::Dim1DArray) fail(inst, "accesses an image that is not 2D or a 2D array, which is not implemented");
        if (info.imageDimension == RdnaImageDimension::Dim3D) fail(inst, "addresses a 2D image as a volume, which is not implemented");
        const auto encoded = std::min(dimension.coordinateComponents, addressDimension.coordinateComponents);
        if (info.imageAddressComponents < encoded) fail(inst, "has an image address with too few coordinate components");
        const auto component = [&](std::uint32_t index) { return index < encoded ? addressU32(*address, info, index) : Hex(0); };
        ImageCoordinates result;
        result.coordinates = "vec2<i32>(i32(" + component(0) + "), i32(" + component(1) + "))";
        if (dimension.arrayed) result.arrayIndex = "i32(" + component(2) + ")";
        if (info.imageHasMip) {
            if (info.imageAddressComponents <= addressDimension.coordinateComponents) fail(inst, "has an image address without the mip level component");
            result.level = "i32(" + addressU32(*address, info, addressDimension.coordinateComponents) + ")";
        }
        return result;
    }

    std::string imageResult(const IrValue& inst, const MemoryInfo& info, const std::string& texel) {
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
            return "vec4<u32>(" + packed[0] + ", " + packed[1] + ", " + packed[2] + ", " + packed[3] + ")";
        }
        return "vec4<u32>(" + swizzled[0] + ", " + swizzled[1] + ", " + swizzled[2] + ", " + swizzled[3] + ")";
    }

    void requirePlainImage(const IrValue& inst, const MemoryInfo& info, const ImageResource& image) const {
        if (info.imageByElements != 0u || info.imagePacked) fail(inst, "accesses packed image elements, which is not implemented");
        if (image.constantSwizzle) fail(inst, "accesses a constant-swizzle image, which is not implemented");
        if (image.srgbDecode) fail(inst, "accesses an sRGB image the device cannot decode");
        if (image.conversionFormat != IrBufferFormat::Invalid || image.depthBits || image.depthUnorm16) fail(inst, "accesses a converted image, which is not implemented");
        if (image.numericClass == IrTextureNumericClass::Unsupported) fail(inst, "accesses an image of an unsupported numeric class");
    }

    void imageRead(const IrValue& inst) {
        const auto& info = memory(inst);
        const auto& image = imageMode(inst, info);
        requirePlainImage(inst, info, image);
        if (writtenImages.contains(info.resource)) fail(inst, "reads an image the program also writes, which is not implemented");
        const auto at = imageCoordinates(inst, info, image);
        const auto kind = DescriptorBindingForImage(image);
        const auto imageElement = element(kind, info.resource);
        const auto type = std::string(DimensionInfoFor(image.dimension).arrayed ? "texture_2d_array<" : "texture_2d<") + ScalarType(image.numericClass) + ">";
        const auto texture = resource(kind, spec(PipelineSpecialization::DescriptorIndex(static_cast<std::uint32_t>(kind), imageElement), imageElement), type, "");
        const auto load = "textureLoad(" + texture + ", " + at.coordinates + (at.arrayIndex.empty() ? "" : ", " + at.arrayIndex) + ", " + (at.level.empty() ? "0" : at.level) + ")";
        const auto texel = bind("select(vec4<u32>(), bitcast<vec4<u32>>(" + load + "), " + argument(inst, 2) + ")");
        define(inst, imageResult(inst, info, texel));
    }

    void imageWrite(const IrValue& inst) {
        const auto& info = memory(inst);
        const auto& image = imageMode(inst, info);
        requirePlainImage(inst, info, image);
        if (image.resourceClass != ImageResourceClass::Storage) fail(inst, "writes an image that is not a storage image");
        if (info.dataBits != 32u) fail(inst, "writes 16-bit image data, which is not implemented");
        if (image.mipMode == ImageMipMode::DynamicStorage) fail(inst, "writes an image level chosen at run time, which is not implemented");
        const auto at = imageCoordinates(inst, info, image);
        const auto kind = DescriptorBindingForImage(image);
        const auto imageElement = element(kind, info.resource);
        const auto type = std::string(DimensionInfoFor(image.dimension).arrayed ? "texture_storage_2d_array<" : "texture_storage_2d<") + StorageFormat + ", write>";
        const auto texture = resource(kind, spec(PipelineSpecialization::DescriptorIndex(static_cast<std::uint32_t>(kind), imageElement), imageElement), type, "");
        const auto data = argument(inst, 2);
        const auto dmask = info.dmask != 0u ? info.dmask : 1u;
        std::array<std::string, 4> values{Hex(0), Hex(0), Hex(0), Hex(0)};
        for (std::uint32_t source = 4u; source != 0u; --source) {
            const auto selector = spec(PipelineSpecialization::ImageBase + info.resource * PipelineSpecialization::ImageWords + source, source + 3u);
            if (selector < 4u || selector >= 8u) continue;
            std::uint32_t packedIndex = 0;
            for (std::uint32_t index = 0; index + 1u < source; ++index) packedIndex += (dmask >> index) & 1u;
            values[selector - 4u] = ((dmask >> (source - 1u)) & 1u) != 0u ? data + "." + Component(packedIndex) : Hex(0);
        }
        const auto raw = "vec4<u32>(" + values[0] + ", " + values[1] + ", " + values[2] + ", " + values[3] + ")";
        const auto texel = image.numericClass == IrTextureNumericClass::Uint ? raw : image.numericClass == IrTextureNumericClass::Sint ? "bitcast<vec4<i32>>(" + raw + ")" : "bitcast<vec4<f32>>(" + raw + ")";
        line("if (" + guarded(argument(inst, 3)) + ") {");
        line("    textureStore(" + texture + ", " + at.coordinates + (at.arrayIndex.empty() ? "" : ", " + at.arrayIndex) + ", " + texel + ");");
        line("}");
    }

    std::string imageType(const ImageResource& image) const {
        if (image.resourceClass != ImageResourceClass::Sampled) fail("storage images are not implemented");
        if (image.depthCompare) {
            if (image.dimension == RdnaImageDimension::Dim2D && !image.cube) return "texture_depth_2d";
            if (image.dimension == RdnaImageDimension::Dim2DArray) return "texture_depth_2d_array";
            fail("depth comparison images that are not 2D or 2D arrays are not implemented");
        }
        const auto scalar = ScalarType(image.numericClass);
        switch (image.dimension) {
        case RdnaImageDimension::Dim1D: return "texture_1d<" + scalar + ">";
        case RdnaImageDimension::Dim2D:
            if (image.cube) fail("cube images that are not arrayed have no WGSL texture type");
            return "texture_2d<" + scalar + ">";
        case RdnaImageDimension::Dim2DArray: return "texture_2d_array<" + scalar + ">";
        case RdnaImageDimension::Dim3D: return "texture_3d<" + scalar + ">";
        case RdnaImageDimension::Dim2DMsaa: return "texture_multisampled_2d<" + scalar + ">";
        default: break;
        }
        fail("image dimension " + std::to_string(static_cast<std::uint32_t>(image.dimension)) + " has no WGSL texture type");
    }

    bool explicitLodFor(const MemoryInfo& info) const {
        return ImageSampleExplicitLod(info.imageSampleFlags, stage);
    }

    void gather(const IrValue& inst) {
        sample(inst, true);
    }

    void sample(const IrValue& inst, bool gathering = false) {
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
        const bool compare = (info.imageSampleFlags & RdnaImageSampleFlagCompare) != 0u;
        if (compare && !image.depthCompare) fail(inst, "compares against a color image, which is not implemented");
        if (!compare && image.depthCompare) fail(inst, "samples a depth comparison image without comparing, which is not implemented");
        if (SamplesBaseLevelIfUnnormalized(samplerInfo) && spec(PipelineSpecialization::UnnormalizedBaseLevelBase + info.sampler, 0u) != 0u) fail(inst, "samples with unnormalized coordinates, which WebGPU does not support");
        const auto* address = inst.Argument(2)->Resolve();
        if (address->Opcode() != IrOpcode::MakeImageAddress) fail(inst, "has an image address not made by MakeImageAddress");
        const auto dimension = DimensionInfoFor(image.dimension);
        const auto addressDimension = DimensionInfoFor(info.imageDimension);
        const auto layoutInfo = LayoutOf(info);
        const auto present = std::min(addressDimension.coordinateComponents, dimension.coordinateComponents);
        const bool cubeFace = image.cube && info.imageDimension == RdnaImageDimension::Dim2D;
        if ((addressDimension.coordinateComponents < dimension.coordinateComponents && ((image.cube && !cubeFace) || addressDimension.coordinateComponents == 0u)) || info.imageAddressComponents < layoutInfo.coord + present) fail(inst, "has an image address with too few coordinate components");
        const auto coordinate = [&](std::uint32_t index) { return index < present ? addressComponent(*address, info, layoutInfo.coord + index) : FloatBits(0); };
        const auto spatial = [&](std::uint32_t index) { return image.cube ? "(" + coordinate(index) + " - " + FloatLiteral(1.0f) + ")" : coordinate(index); };
        std::string coords;
        std::string arrayIndex;
        if (dimension.spatialComponents == 1u) coords = coordinate(0);
        else if (dimension.spatialComponents == 2u) coords = "vec2<f32>(" + spatial(0) + ", " + spatial(1) + ")";
        else coords = "vec3<f32>(" + coordinate(0) + ", " + coordinate(1) + ", " + coordinate(2) + ")";
        if (image.cube) {
            const auto face = bind("u32(" + coordinate(dimension.spatialComponents) + ")");
            arrayIndex = "i32(" + face + " - ((" + face + " >> 3u) << 1u))";
        } else if (dimension.arrayed) {
            arrayIndex = "i32(round(" + coordinate(dimension.spatialComponents) + "))";
        }
        if (dimension.multisampled) fail(inst, "samples a multisampled image");
        const auto imageKind = DescriptorBindingForImage(image);
        const auto imageElement = element(imageKind, info.resource);
        const auto texture = resource(imageKind, spec(PipelineSpecialization::DescriptorIndex(static_cast<std::uint32_t>(imageKind), imageElement), imageElement), imageType(image), "");
        const bool point = image.numericClass == IrTextureNumericClass::Sint || image.conversionFormat != IrBufferFormat::Invalid || image.depthBits;
        const auto samplerElement = element(DescriptorBindingKind::Samplers, info.sampler) + (point ? 1u : 0u);
        const auto sampler = resource(DescriptorBindingKind::Samplers, spec(PipelineSpecialization::DescriptorIndex(static_cast<std::uint32_t>(DescriptorBindingKind::Samplers), samplerElement), samplerElement), compare ? "sampler_comparison" : "sampler", "");
        std::string offset;
        if (layoutInfo.offset != NoComponent) {
            const auto component = GetRdnaImageAddressComponentLayout(info.imageSampleFlags, layoutInfo.offset);
            const auto* bits = component.bitWidth == 32u && component.bitOffset / 32u < address->ArgumentCount() ? address->Argument(component.bitOffset / 32u)->Resolve() : nullptr;
            if (bits == nullptr) fail(inst, "has a texel offset outside of the address operands");
        }
        if (layoutInfo.offset != NoComponent && !address->Argument(GetRdnaImageAddressComponentLayout(info.imageSampleFlags, layoutInfo.offset).bitOffset / 32u)->Resolve()->HasImmediate()) {
            if (dimension.spatialComponents != 2u) fail(inst, "has a texel offset chosen at run time on an image that is not 2D, which is not implemented");
            const auto raw = addressU32(*address, info, layoutInfo.offset);
            const auto texels = bind("vec2<f32>(f32(bitcast<i32>(" + raw + " << 26u) >> 26u), f32(bitcast<i32>((" + raw + " >> 8u) << 26u) >> 26u))");
            const auto level = !gathering && explicitLodFor(info) && (info.imageSampleFlags & RdnaImageSampleFlagLod) != 0u ? "i32(" + addressComponent(*address, info, layoutInfo.lod) + ")" : std::string("0");
            coords = bind("(" + coords + " + " + texels + " / vec2<f32>(textureDimensions(" + texture + ", " + level + ")))");
        } else if (layoutInfo.offset != NoComponent) {
            const auto component = GetRdnaImageAddressComponentLayout(info.imageSampleFlags, layoutInfo.offset);
            const auto* bits = address->Argument(component.bitOffset / 32u)->Resolve();
            std::array<std::string, 3> values;
            for (std::uint32_t index = 0; index < dimension.spatialComponents; ++index) values[index] = std::to_string(static_cast<std::int32_t>(((bits->ImmediateU32() >> (index * 8u)) & 0x3fu) ^ 0x20u) - 0x20) + "i";
            offset = dimension.spatialComponents == 1u ? values[0] : dimension.spatialComponents == 2u ? "vec2<i32>(" + values[0] + ", " + values[1] + ")" : "vec3<i32>(" + values[0] + ", " + values[1] + ", " + values[2] + ")";
            if (dimension.spatialComponents == 1u) fail(inst, "uses a texel offset on a 1D image, which WGSL does not support");
        }
        if (layoutInfo.clamp != NoComponent) fail(inst, "clamps its LOD, which is not implemented");
        const bool explicitLod = ImageSampleExplicitLod(info.imageSampleFlags, stage);
        std::string arguments = texture + ", " + sampler + ", " + coords + (arrayIndex.empty() ? "" : ", " + arrayIndex);
        if (gathering) {
            if (compare) fail(inst, "gathers with depth comparison, which is not implemented");
            if (dimension.spatialComponents != 2u) fail(inst, "gathers from an image that is not 2D, which is not implemented");
            if ((info.imageSampleFlags & (RdnaImageSampleFlagDerivative | RdnaImageSampleFlagBias)) != 0u) fail(inst, "gathers with a bias or derivatives, which WGSL cannot express");
            if ((info.imageSampleFlags & RdnaImageSampleFlagLod) != 0u) {
                const auto component = GetRdnaImageAddressComponentLayout(info.imageSampleFlags, layoutInfo.lod);
                const auto* lod = component.bitOffset / 32u < address->ArgumentCount() ? address->Argument(component.bitOffset / 32u)->Resolve() : nullptr;
                if (lod == nullptr || !lod->HasImmediate() || lod->ImmediateU32() != 0u) fail(inst, "gathers from a level other than 0, which WGSL cannot express");
            }
            const auto dmask = info.dmask != 0u ? info.dmask : 1u;
            if (!std::has_single_bit(dmask)) fail(inst, "gathers more than one component");
            const auto gathered = static_cast<std::uint32_t>(std::countr_zero(dmask));
            const auto selector = spec(PipelineSpecialization::ImageBase + info.resource * PipelineSpecialization::ImageWords + 1u + gathered, gathered + 4u);
            std::string texel;
            if (selector >= 4u && selector < 8u) {
                texel = bind("bitcast<vec4<u32>>(textureGather(" + std::to_string(selector - 4u) + "u, " + arguments + (offset.empty() ? "" : ", " + offset) + "))");
            } else {
                const auto constant = Hex(selector == 1u ? 0x3f800000u : 0u);
                texel = "vec4<u32>(" + constant + ", " + constant + ", " + constant + ", " + constant + ")";
            }
            if (info.dataBits == 16u) {
                if (info.dataDwords > 2u) fail(inst, "has more than two packed gather result dwords");
                const auto pack = [&](const char* low, const char* high) { return "pack2x16float(vec2<f32>(bitcast<f32>(" + texel + "." + low + "), bitcast<f32>(" + texel + "." + high + ")))"; };
                define(inst, "vec4<u32>(" + pack("x", "y") + ", " + (info.dataDwords > 1u ? pack("z", "w") : Hex(0)) + ", 0u, 0u)");
                return;
            }
            define(inst, texel);
            return;
        }
        if (compare) {
            if ((info.imageSampleFlags & (RdnaImageSampleFlagDerivative | RdnaImageSampleFlagBias)) != 0u) fail(inst, "compares with a bias or derivatives, which WGSL cannot express");
            const auto reference = addressComponent(*address, info, layoutInfo.dref);
            std::string call;
            if (explicitLod) {
                if ((info.imageSampleFlags & RdnaImageSampleFlagLod) != 0u) {
                    const auto component = GetRdnaImageAddressComponentLayout(info.imageSampleFlags, layoutInfo.lod);
                    const auto* lod = component.bitOffset / 32u < address->ArgumentCount() ? address->Argument(component.bitOffset / 32u)->Resolve() : nullptr;
                    if (lod == nullptr || !lod->HasImmediate() || lod->ImmediateU32() != 0u) fail(inst, "compares at a level other than 0, which WGSL cannot express");
                }
                call = "textureSampleCompareLevel(" + arguments + ", " + reference;
            } else {
                call = "textureSampleCompare(" + arguments + ", " + reference;
            }
            if (!offset.empty()) call += ", " + offset;
            call += ")";
            const auto result = bind("bitcast<u32>(" + call + ")");
            if (info.dataBits == 16u) {
                if (info.dataDwords > 4u) fail(inst, "has more than four packed image result dwords");
                std::array<std::string, 4> packed{Hex(0), Hex(0), Hex(0), Hex(0)};
                for (std::uint32_t word = 0; word < info.dataDwords; ++word) {
                    const auto high = word * 2u + 1u < info.componentCount ? "bitcast<f32>(" + result + ")" : FloatBits(0);
                    packed[word] = "pack2x16float(vec2<f32>(bitcast<f32>(" + result + "), " + high + "))";
                }
                define(inst, "vec4<u32>(" + packed[0] + ", " + packed[1] + ", " + packed[2] + ", " + packed[3] + ")");
                return;
            }
            define(inst, "vec4<u32>(" + result + ", " + result + ", " + result + ", " + result + ")");
            return;
        }
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
        if (subgroups) text += "enable subgroups;\n";
        text += "diagnostic(off, derivative_uniformity);\n";
        if (subgroups) text += "diagnostic(off, subgroup_uniformity);\n";
        text += "\n";
        for (const auto* helperText : helpers) text += std::string(helperText) + "\n";
        text += bindingDeclarations();
        text += moduleVariables;
        if (!text.empty()) text += "\n";
        if (stage == IrShaderStage::Compute) {
            const auto& threads = inputInfo.compute->threadsNum;
            std::string inputs;
            for (const auto& [key, input] : entryInputs) inputs += (inputs.empty() ? "" : ", ") + input.attributes + " " + input.name + ": " + input.type;
            text += "@compute @workgroup_size(" + std::to_string(std::max(threads[0], 1u)) + ", " + std::to_string(std::max(threads[1], 1u)) + ", " + std::to_string(std::max(threads[2], 1u)) + ")\n";
            text += "fn main(" + inputs + ") {\n" + prologue + withReturns({"return;"}) + "}\n";
            return {std::move(text), std::move(bindingsUsed)};
        }
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
    std::set<std::uint32_t> writtenBuffers;
    std::set<std::uint32_t> writtenImages;
    mutable std::map<std::uint32_t, std::vector<ImageResource>> imageModes;
    std::map<std::uint32_t, EntryInput> entryInputs;
    std::set<std::uint32_t> parameters;
    std::map<std::uint32_t, bool> renderTargets;
    std::vector<std::uint32_t> bindingsUsed;
    std::string prologue;
    std::string body;
    std::string indent;
    std::map<std::uint32_t, std::size_t> blockIndex;
    std::map<const IrBlock*, std::uint32_t> blockIds;
    std::set<const IrValue*> declaredPhis;
    std::set<const IrValue*> hoisted;
    std::vector<LoopFrame> loops;
    std::set<std::uint32_t> emitted;
    std::uint32_t temporaries = 0;
    bool validMask = false;
    bool subgroups = false;
    bool waveLds = false;
    bool flatten = false;
    std::string predicate = "true";
    bool ldsWritten = true;
    bool ldsRead = false;
    bool ldsDeclared = false;
    std::string moduleVariables;
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

namespace {

struct StorageTexelPacking {
    std::uint32_t bytes;
    std::vector<std::string> words;
};

StorageTexelPacking PackStorageTexel(std::string_view format) {
    if (format == "r32float") return {4u, {"bitcast<u32>(texel.x)"}};
    if (format == "rgba32float") return {16u, {"bitcast<u32>(texel.x)", "bitcast<u32>(texel.y)", "bitcast<u32>(texel.z)", "bitcast<u32>(texel.w)"}};
    if (format == "rgba16float") return {8u, {"(HALF(texel.x) | (HALF(texel.y) << 16u))", "(HALF(texel.z) | (HALF(texel.w) << 16u))"}};
    if (format == "rg16float") return {4u, {"(HALF(texel.x) | (HALF(texel.y) << 16u))"}};
    if (format == "r16float") return {2u, {"HALF(texel.x)"}};
    if (format == "rgba8unorm") return {4u, {"pack4x8unorm(texel)"}};
    if (format == "bgra8unorm") return {4u, {"pack4x8unorm(texel.zyxw)"}};
    if (format == "rg8unorm") return {2u, {"(pack4x8unorm(vec4<f32>(texel.xy, 0.0, 0.0)) & 0xffffu)"}};
    if (format == "r8unorm") return {1u, {"(pack4x8unorm(vec4<f32>(texel.x, 0.0, 0.0, 0.0)) & 0xffu)"}};
    if (format == "rgb10a2unorm") return {4u, {"(u32(floor(fma(saturate(texel.x), 1023.0, 0.5))) | (u32(floor(fma(saturate(texel.y), 1023.0, 0.5))) << 10u) | (u32(floor(fma(saturate(texel.z), 1023.0, 0.5))) << 20u) | (u32(floor(fma(saturate(texel.w), 3.0, 0.5))) << 30u))"}};
    throw std::runtime_error("WGSL: storage images of format " + std::string(format) + " cannot be kept in a buffer");
}

bool IdentifierCharacter(char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '_';
}

std::size_t CountIdentifier(const std::string& source, const std::string& name) {
    std::size_t count = 0;
    for (auto found = source.find(name); found != std::string::npos; found = source.find(name, found + name.size())) {
        const bool before = found != 0 && IdentifierCharacter(source[found - 1]);
        const bool after = found + name.size() < source.size() && IdentifierCharacter(source[found + name.size()]);
        if (!before && !after) ++count;
    }
    return count;
}

}

std::uint32_t WgslStorageTexelBytes(std::string_view format) {
    return PackStorageTexel(format).bytes;
}

std::string BufferWgslStorageImage(std::string source, std::uint32_t binding, const WgslImageBuffer& layout) {
    const auto marker = "@binding(" + std::to_string(binding) + ") var ";
    const auto found = source.find(marker);
    if (found == std::string::npos) throw std::runtime_error("WGSL: storage image binding " + std::to_string(binding) + " is not declared");
    const auto newline = source.rfind('\n', found);
    const auto lineStart = newline == std::string::npos ? 0 : newline + 1;
    const auto lineEnd = std::min(source.find('\n', found), source.size());
    const auto nameStart = found + marker.size();
    const auto colon = source.find(':', nameStart);
    if (colon == std::string::npos || colon > lineEnd) throw std::runtime_error("WGSL: binding " + std::to_string(binding) + " has no type");
    const auto name = source.substr(nameStart, colon - nameStart);
    const auto type = source.substr(colon + 1, lineEnd - colon - 1);
    const bool arrayed = type.find("texture_storage_2d_array<") != std::string::npos;
    const auto open = type.find('<');
    const auto comma = type.find(',', open);
    if ((!arrayed && type.find("texture_storage_2d<") == std::string::npos) || open == std::string::npos || comma == std::string::npos) throw std::runtime_error("WGSL: binding " + std::to_string(binding) + " is not a 2D storage image");
    if (type.find(", write>") == std::string::npos) throw std::runtime_error("WGSL: storage image " + name + " is not write-only");
    const auto packing = PackStorageTexel(std::string_view(type).substr(open + 1, comma - open - 1));
    if (layout.width == 0 || layout.height == 0 || layout.layers == 0 || layout.bytesPerRow % 4u != 0 || layout.bytesPerRow < static_cast<std::uint64_t>(layout.width) * packing.bytes) throw std::runtime_error("WGSL: invalid buffer layout for storage image " + name);
    if (static_cast<std::uint64_t>(layout.bytesPerRow) * layout.height * layout.layers > 0xffffffffull) throw std::runtime_error("WGSL: storage image " + name + " is too large for a buffer");
    const auto call = "textureStore(" + name + ", ";
    std::size_t calls = 0;
    for (auto at = source.find(call); at != std::string::npos; at = source.find(call, at + call.size())) ++calls;
    if (CountIdentifier(source, name) != calls + 1u) throw std::runtime_error("WGSL: storage image " + name + " is used other than by stores");

    const auto unsignedValue = [](std::uint64_t value) { return std::to_string(value) + "u"; };
    const auto store = name + "_store";
    const auto half = name + "_half";
    std::string replacement = source.substr(lineStart, found - lineStart) + "@binding(" + std::to_string(binding) + ") var<storage, read_write> " + name + ": array<atomic<u32>>;\n";
    std::vector<std::string> words = packing.words;
    bool halves = false;
    for (auto& word : words) {
        for (auto at = word.find("HALF("); at != std::string::npos; at = word.find("HALF(", at + half.size())) {
            word.replace(at, 4, half);
            halves = true;
        }
    }
    if (halves) {
        replacement += "fn " + half + "(value: f32) -> u32 {\n";
        replacement += "    let bits = bitcast<u32>(value);\n";
        replacement += "    let sign = (bits >> 16u) & 0x8000u;\n";
        replacement += "    let biased = (bits >> 23u) & 0xffu;\n";
        replacement += "    let mantissa = bits & 0x7fffffu;\n";
        replacement += "    if (biased == 0xffu) {\n        return sign | 0x7c00u | select(0u, 0x200u | (mantissa >> 13u), mantissa != 0u);\n    }\n";
        replacement += "    let exponent = i32(biased) - 112;\n";
        replacement += "    if (exponent >= 31) {\n        return sign | 0x7bffu;\n    }\n";
        replacement += "    if (exponent > 0) {\n        return sign | (u32(exponent) << 10u) | (mantissa >> 13u);\n    }\n";
        replacement += "    if (exponent < -10) {\n        return sign;\n    }\n";
        replacement += "    return sign | ((mantissa | 0x800000u) >> u32(14 - exponent));\n";
        replacement += "}\n";
    }
    replacement += "fn " + store + "(coordinates: vec2<i32>, " + (arrayed ? "layer: i32, " : "") + "texel: vec4<f32>) {\n";
    replacement += "    if (coordinates.x < 0 || coordinates.y < 0 || coordinates.x >= " + std::to_string(layout.width) + " || coordinates.y >= " + std::to_string(layout.height);
    if (arrayed) replacement += " || layer < 0 || layer >= " + std::to_string(layout.layers);
    replacement += ") {\n        return;\n    }\n";
    replacement += "    let byte = " + (arrayed ? "u32(layer) * " + unsignedValue(static_cast<std::uint64_t>(layout.bytesPerRow) * layout.height) + " + " : std::string()) + "u32(coordinates.y) * " + unsignedValue(layout.bytesPerRow) + " + u32(coordinates.x) * " + unsignedValue(packing.bytes) + ";\n";
    if (packing.bytes >= 4u) {
        for (std::size_t word = 0; word < words.size(); ++word) replacement += "    atomicStore(&" + name + "[(byte >> 2u) + " + unsignedValue(word) + "], " + words[word] + ");\n";
    } else {
        replacement += "    let index = byte >> 2u;\n";
        replacement += "    let shift = (byte & 3u) * 8u;\n";
        replacement += "    let mask = " + unsignedValue(packing.bytes == 1u ? 0xffu : 0xffffu) + " << shift;\n";
        replacement += "    let bits = (" + words[0] + " << shift) & mask;\n";
        replacement += "    loop {\n";
        replacement += "        let old = atomicLoad(&" + name + "[index]);\n";
        replacement += "        if (atomicCompareExchangeWeak(&" + name + "[index], old, (old & ~mask) | bits).exchanged) {\n";
        replacement += "            break;\n";
        replacement += "        }\n";
        replacement += "    }\n";
    }
    replacement += "}";
    source.replace(lineStart, lineEnd - lineStart, replacement);
    for (auto at = source.find(call); at != std::string::npos; at = source.find(call, at + store.size() + 1)) source.replace(at, call.size(), store + "(");
    return source;
}

}  // namespace ShaderRecompiler
