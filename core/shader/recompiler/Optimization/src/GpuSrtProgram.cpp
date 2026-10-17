#include "Optimization/GpuSrtProgram.hpp"
#include "Optimization/SrtWalker/SrtAddressArithmetic.hpp"
#include "Optimization/SrtWalker/SrtInstructionPredicates.hpp"
#include "IntermediateRepresentation/IrValue.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <unordered_set>
#include <unordered_map>

namespace ShaderRecompiler {

namespace {

class Encoder {
public:
    explicit Encoder(const IrResourcePlan& plan) : plan(plan) {}

    bool Run(GpuSrtProgram& out) {
        if (!plan.srtPlanComplete) return fail("SRT plan is incomplete");
        if (!plan.controlFlow.empty()) return fail("control flow");
        for (const auto clean : plan.cleanFlatSlots) {
            if (clean != 0u) return fail("clean flat slots");
        }
        for (const auto sourceIndex : plan.materializationSources) {
            if (sourceIndex >= plan.descriptorSources.size()) return fail("unknown descriptor source");
            const auto& source = plan.descriptorSources[sourceIndex];
            if (source.dwordCount > 8u) return fail("descriptor wider than 8 dwords");
            out.sourceDwordCounts.push_back(source.dwordCount);
            for (std::uint32_t dword = 0; dword < source.dwordCount; ++dword) {
                std::uint32_t slot = 0;
                if (!slotOf(source.dwords[dword], slot)) return false;
                out.sourceDwords.push_back(slot);
            }
        }
        out.flatSize = static_cast<std::uint32_t>(plan.srtReads.size());
        for (const auto& read : plan.srtReads) {
            if (read.flatOffset >= out.flatSize) return fail("flat offset past the flattened SRT");
            const bool pure = read.flatOffset < plan.pureFlatSlots.size() && plan.pureFlatSlots[read.flatOffset] != 0u;
            const auto before = code.size();
            std::uint32_t slot = 0;
            if (!slotOf(read.value, slot)) return false;
            if (pure) {
                const IrValue* leaf = read.value->Resolve();
                if (leaf->Opcode() != IrOpcode::LoadAddressU32 && leaf->Opcode() != IrOpcode::ReadConstBuffer) return fail("pure flat slot without a raw read");
                if (code.size() == before || code.back().dst != slot) return fail("pure flat slot read shared with a root");
                code.back().leaf = read.flatOffset + 1u;
            }
            out.flat.emplace_back(read.flatOffset, slot);
        }
        const auto base = static_cast<std::uint32_t>(constants.size());
        const auto remap = [&](std::uint32_t slot) { return (slot & ConstantBit) != 0u ? slot & ~ConstantBit : slot + base; };
        for (auto& instruction : code) {
            instruction.dst = remap(instruction.dst);
            for (std::uint32_t index = 0; index < argumentCount(instruction.op); ++index) instruction.args[index] = remap(instruction.args[index]);
        }
        for (auto& [offset, slot] : out.flat) slot = remap(slot);
        for (auto& slot : out.sourceDwords) slot = remap(slot);
        out.constants = std::move(constants);
        out.slotCount = base + nextSlot;
        out.code = std::move(code);
        return true;
    }

    std::string reason;

private:
    static constexpr std::uint32_t ConstantBit = 0x80000000u;

public:
    static std::uint32_t argumentCount(GpuSrtOp op) {
        switch (op) {
            case GpuSrtOp::UserData: case GpuSrtOp::ShaderBase: return 0;
            case GpuSrtOp::Copy: case GpuSrtOp::Extract64: case GpuSrtOp::Not32: case GpuSrtOp::LNot: return 1;
            case GpuSrtOp::ReadAddress: case GpuSrtOp::Select: case GpuSrtOp::BfeU: case GpuSrtOp::BfeS: return 3;
            case GpuSrtOp::Bfi: return 4;
            case GpuSrtOp::ReadBuffer: return 5;
            default: return 2;
        }
    }

private:
    bool fail(std::string why) {
        if (reason.empty()) reason = std::move(why);
        return false;
    }

    std::uint32_t constant(std::uint64_t value) {
        const auto [it, inserted] = constantSlots.emplace(value, static_cast<std::uint32_t>(constants.size()));
        if (inserted) constants.push_back(value);
        return it->second | ConstantBit;
    }

    std::uint32_t emit(GpuSrtOp op, std::initializer_list<std::uint32_t> args, std::uint32_t immediate = 0, std::uint32_t flags = 0) {
        GpuSrtInstruction instruction;
        instruction.op = op;
        instruction.dst = nextSlot++;
        std::size_t index = 0;
        for (const auto arg : args) instruction.args[index++] = arg;
        instruction.immediate = immediate;
        instruction.flags = flags;
        code.push_back(instruction);
        return instruction.dst;
    }

    bool slotOf(const IrValue* value, std::uint32_t& slot) {
        if (value == nullptr) return fail("null value");
        value = value->Resolve();
        if (value->HasImmediate()) {
            const auto bits = value->ImmediateU64();
            switch (value->Type()) {
                case IrType::Bool: slot = constant(bits != 0u ? 1u : 0u); return true;
                case IrType::U8: slot = constant(static_cast<std::uint8_t>(bits)); return true;
                case IrType::U16: slot = constant(static_cast<std::uint16_t>(bits)); return true;
                case IrType::U32: case IrType::F32: slot = constant(static_cast<std::uint32_t>(bits)); return true;
                case IrType::U64: slot = constant(bits); return true;
                default: return fail("immediate of an unsupported type");
            }
        }
        if (value->Opcode() == IrOpcode::Void) return fail("void value");
        if (const auto found = slots.find(value); found != slots.end()) {
            slot = found->second;
            return true;
        }
        if (visiting.contains(value)) return fail("cycle");
        visiting.insert(value);
        const bool ok = encode(*value, slot);
        visiting.erase(value);
        if (!ok) return false;
        slots.emplace(value, slot);
        return true;
    }

    bool args(const IrValue& inst, std::initializer_list<std::size_t> indices, std::array<std::uint32_t, 5>& out) {
        std::size_t position = 0;
        for (const auto index : indices) {
            if (index >= inst.ArgumentCount()) return fail("missing argument");
            if (!slotOf(inst.Argument(index), out[position++])) return false;
        }
        return true;
    }

    bool binary(const IrValue& inst, GpuSrtOp op, std::uint32_t& slot) {
        std::array<std::uint32_t, 5> a{};
        if (!args(inst, {0, 1}, a)) return false;
        slot = emit(op, {a[0], a[1]});
        return true;
    }

    bool unary(const IrValue& inst, GpuSrtOp op, std::uint32_t& slot) {
        std::array<std::uint32_t, 5> a{};
        if (!args(inst, {0}, a)) return false;
        slot = emit(op, {a[0]});
        return true;
    }

    bool encode(const IrValue& inst, std::uint32_t& slot) {
        std::array<std::uint32_t, 5> a{};
        switch (inst.Opcode()) {
            case IrOpcode::GetUserData: {
                const auto reg = RegIndex(static_cast<ScalarReg>(inst.Argument(0)->Resolve()->Register().index));
                if (reg < plan.userDataBase) return fail("user data register below the base");
                slot = emit(GpuSrtOp::UserData, {}, reg - plan.userDataBase);
                return true;
            }
            case IrOpcode::GetShaderBase: slot = emit(GpuSrtOp::ShaderBase, {}); return true;
            case IrOpcode::BitCastU32F32:
            case IrOpcode::BitCastF32U32: return args(inst, {0}, a) && (slot = a[0], true);
            case IrOpcode::CompositeConstructU64: return binary(inst, GpuSrtOp::Construct64, slot);
            case IrOpcode::CompositeExtractU64:
            case IrOpcode::CompositeExtractU32x2: return encodeExtract(inst, slot);
            case IrOpcode::ReadConst: {
                const IrValue* index = inst.Argument(1)->Resolve();
                if (!index->HasImmediate() || index->Type() != IrType::U32) return fail("ReadConst at a runtime slot");
                const auto srtSlot = static_cast<std::uint32_t>(index->ImmediateU64());
                if (srtSlot >= plan.srtReads.size()) return fail("ReadConst past the SRT reads");
                return slotOf(plan.srtReads[srtSlot].value, slot);
            }
            case IrOpcode::LoadAddressU32:
            case IrOpcode::ReadConstBuffer: return encodeRead(inst, slot);
            case IrOpcode::IAdd32: return binary(inst, GpuSrtOp::IAdd32, slot);
            case IrOpcode::IAdd64: return binary(inst, GpuSrtOp::IAdd64, slot);
            case IrOpcode::ISub32: return binary(inst, GpuSrtOp::ISub32, slot);
            case IrOpcode::ISub64: return binary(inst, GpuSrtOp::ISub64, slot);
            case IrOpcode::IMul32: return binary(inst, GpuSrtOp::IMul32, slot);
            case IrOpcode::IMul64: return binary(inst, GpuSrtOp::IMul64, slot);
            case IrOpcode::SMin32: return binary(inst, GpuSrtOp::SMin32, slot);
            case IrOpcode::UMin32: return binary(inst, GpuSrtOp::UMin32, slot);
            case IrOpcode::BitwiseAnd32: return binary(inst, GpuSrtOp::And32, slot);
            case IrOpcode::BitwiseAnd64: return binary(inst, GpuSrtOp::And64, slot);
            case IrOpcode::BitwiseOr32: return binary(inst, GpuSrtOp::Or32, slot);
            case IrOpcode::BitwiseXor32: return binary(inst, GpuSrtOp::Xor32, slot);
            case IrOpcode::BitwiseNot32: return unary(inst, GpuSrtOp::Not32, slot);
            case IrOpcode::ShiftLeftLogical32: return binary(inst, GpuSrtOp::Shl32, slot);
            case IrOpcode::ShiftLeftLogical64: return binary(inst, GpuSrtOp::Shl64, slot);
            case IrOpcode::ShiftRightLogical32: return binary(inst, GpuSrtOp::Shr32, slot);
            case IrOpcode::ShiftRightLogical64: return binary(inst, GpuSrtOp::Shr64, slot);
            case IrOpcode::ShiftRightArithmetic32: return binary(inst, GpuSrtOp::Sar32, slot);
            case IrOpcode::ShiftRightArithmetic64: return binary(inst, GpuSrtOp::Sar64, slot);
            case IrOpcode::BitFieldUExtract:
            case IrOpcode::BitFieldSExtract:
                if (!args(inst, {0, 1, 2}, a)) return false;
                slot = emit(inst.Opcode() == IrOpcode::BitFieldUExtract ? GpuSrtOp::BfeU : GpuSrtOp::BfeS, {a[0], a[1], a[2]});
                return true;
            case IrOpcode::BitFieldInsert:
                if (!args(inst, {0, 1, 2, 3}, a)) return false;
                slot = emit(GpuSrtOp::Bfi, {a[0], a[1], a[2], a[3]});
                return true;
            case IrOpcode::SelectU32:
            case IrOpcode::SelectU1:
            case IrOpcode::SelectF32:
                if (!args(inst, {0, 1, 2}, a)) return false;
                slot = emit(GpuSrtOp::Select, {a[0], a[1], a[2]});
                return true;
            case IrOpcode::IEqual32: return binary(inst, GpuSrtOp::IEq, slot);
            case IrOpcode::INotEqual32: return binary(inst, GpuSrtOp::INe, slot);
            case IrOpcode::SLessThan32: return binary(inst, GpuSrtOp::SLt, slot);
            case IrOpcode::ULessThan32: return binary(inst, GpuSrtOp::ULt, slot);
            case IrOpcode::UGreaterThan32: return binary(inst, GpuSrtOp::UGt, slot);
            case IrOpcode::LogicalAnd: return binary(inst, GpuSrtOp::LAnd, slot);
            case IrOpcode::LogicalOr: return binary(inst, GpuSrtOp::LOr, slot);
            case IrOpcode::LogicalXor: return binary(inst, GpuSrtOp::LXor, slot);
            case IrOpcode::LogicalNot: return unary(inst, GpuSrtOp::LNot, slot);
            default: return fail(std::string("opcode ") + std::string(IrOpcodeName(inst.Opcode())));
        }
    }

    bool encodeExtract(const IrValue& inst, std::uint32_t& slot) {
        const IrValue* index = inst.Argument(1)->Resolve();
        if (!index->HasImmediate() || index->Type() != IrType::U32) return fail("extract at a runtime component");
        const auto component = static_cast<std::uint32_t>(index->ImmediateU64());
        if (component >= 2u) return fail("extract past component 1");
        std::array<std::uint32_t, 5> a{};
        if (inst.Opcode() == IrOpcode::CompositeExtractU64) {
            if (!args(inst, {0}, a)) return false;
            slot = emit(GpuSrtOp::Extract64, {a[0]}, component);
            return true;
        }
        const IrValue* source = inst.Argument(0)->Resolve();
        if (source->Opcode() == IrOpcode::Void) return fail("extract of a void value");
        if (source->Opcode() == IrOpcode::CompositeConstructU32x2) return slotOf(source->Argument(component), slot);
        if (source->Opcode() == IrOpcode::IAddCarry32) {
            if (!args(*source, {0, 1}, a)) return false;
            slot = emit(GpuSrtOp::AddCarry, {a[0], a[1]}, component);
            return true;
        }
        return fail("extract of an unsupported composite");
    }

    bool encodeRead(const IrValue& inst, std::uint32_t& slot) {
        const auto flags = inst.Flags<MemoryFlags>();
        if (!Detail::IsRawRead(plan, inst.Opcode(), flags.index)) return fail("read that is not a raw read");
        const auto& memory = plan.memoryInfo[flags.index];
        const IrValue* handle = inst.Argument(0)->Resolve();
        if (handle->Opcode() == IrOpcode::Void) return fail("read through a void handle");
        std::array<std::uint32_t, 5> a{};
        const bool buffer = inst.Opcode() == IrOpcode::ReadConstBuffer;
        if (buffer && handle->ArgumentCount() != 4u) return fail("buffer read handle without 4 words");
        if (!args(*handle, {0, 1}, a)) return false;
        std::array<std::uint32_t, 5> offset{};
        if (!args(inst, {1}, offset)) return false;
        a[2] = offset[0];
        if (buffer) {
            std::array<std::uint32_t, 5> words{};
            if (!args(*handle, {2, 3}, words)) return false;
            a[3] = words[0];
            a[4] = words[1];
            if (static_cast<std::int32_t>(memory.offset) < 0) return fail("buffer read at a negative offset");
        }
        const auto loaded = Detail::LoadedFromMemory(*handle) ? GpuSrtLoadedFromMemory : 0u;
        slot = buffer ? emit(GpuSrtOp::ReadBuffer, {a[0], a[1], a[2], a[3], a[4]}, memory.offset, loaded) : emit(GpuSrtOp::ReadAddress, {a[0], a[1], a[2]}, memory.offset, loaded);
        code.back().pc = flags.pc;
        return true;
    }

    const IrResourcePlan& plan;
    std::vector<std::uint64_t> constants;
    std::unordered_map<std::uint64_t, std::uint32_t> constantSlots;
    std::unordered_map<const IrValue*, std::uint32_t> slots;
    std::unordered_set<const IrValue*> visiting;
    std::vector<GpuSrtInstruction> code;
    std::uint32_t nextSlot = 0;
};

}

std::optional<GpuSrtProgram> EncodeGpuSrtProgram(const IrResourcePlan& plan, std::string* reason) {
    Encoder encoder(plan);
    GpuSrtProgram program;
    if (!encoder.Run(program)) {
        if (reason != nullptr) *reason = encoder.reason;
        return std::nullopt;
    }
    return program;
}

bool RunGpuSrtProgram(const GpuSrtProgram& program, const SrtRuntime& runtime, GpuSrtResult& result) {
    result.nullRoots = 0;
    result.poison.clear();
    std::vector<std::uint64_t> slots(program.slotCount);
    std::copy(program.constants.begin(), program.constants.end(), slots.begin());
    std::vector<std::uint32_t> poisonOf(program.slotCount, 0u);
    std::vector<std::pair<std::uint32_t, std::uint64_t>> origins{{0u, 0u}};
    const auto u32 = [](std::uint64_t value) { return static_cast<std::uint32_t>(value); };
    const auto s32 = [](std::uint64_t value) { return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)); };
    for (const auto& instruction : program.code) {
        std::uint32_t inherited = 0;
        for (std::uint32_t index = 0; index < Encoder::argumentCount(instruction.op) && inherited == 0u; ++index) inherited = poisonOf[instruction.args[index]];
        if (inherited != 0u) {
            poisonOf[instruction.dst] = inherited;
            slots[instruction.dst] = 0;
            continue;
        }
        const auto a = slots[instruction.args[0]];
        const auto b = slots[instruction.args[1]];
        const auto c = slots[instruction.args[2]];
        const auto d = slots[instruction.args[3]];
        auto& out = slots[instruction.dst];
        switch (instruction.op) {
            case GpuSrtOp::UserData:
                if (instruction.immediate >= runtime.userData.size()) return false;
                out = runtime.userData[instruction.immediate];
                if (runtime.userDataUse != nullptr) runtime.userDataUse->asValue |= instruction.immediate < SrtUserDataUse::TrackedWords ? std::uint64_t{1} << instruction.immediate : ~std::uint64_t{0};
                break;
            case GpuSrtOp::ShaderBase: out = runtime.shaderBase; break;
            case GpuSrtOp::Copy: out = a; break;
            case GpuSrtOp::Extract64: out = u32(a >> (instruction.immediate * 32u)); break;
            case GpuSrtOp::AddCarry: {
                const auto sum = static_cast<std::uint64_t>(u32(a)) + u32(b);
                out = instruction.immediate == 0u ? u32(sum) : u32(sum >> 32u);
                break;
            }
            case GpuSrtOp::Construct64: out = u32(a) | (static_cast<std::uint64_t>(u32(b)) << 32u); break;
            case GpuSrtOp::ReadAddress:
            case GpuSrtOp::ReadBuffer: {
                const auto base = ((b << 32u) | u32(a)) & Detail::AddressMask;
                const auto immediate = static_cast<std::int64_t>(static_cast<std::int32_t>(instruction.immediate));
                std::uint64_t address = 0;
                if (instruction.op == GpuSrtOp::ReadBuffer) {
                    const auto byteOffset = static_cast<std::uint64_t>(immediate) + u32(c);
                    const auto aligned = byteOffset & ~std::uint64_t{3};
                    const auto stride = (u32(b) >> 16u) & 0x3fffu;
                    const auto size = stride == 0u ? static_cast<std::uint64_t>(u32(d)) : static_cast<std::uint64_t>(stride) * u32(d);
                    if (aligned > size || size - aligned < sizeof(std::uint32_t)) {
                        out = 0;
                        break;
                    }
                    address = ((base & ~std::uint64_t{3}) + byteOffset) & ~std::uint64_t{3};
                } else {
                    const auto relative = (immediate & ~std::int64_t{3}) + static_cast<std::int64_t>(u32(c) & ~3u);
                    if (!Detail::AddSignedAddress(base & ~std::uint64_t{3}, relative, address)) return false;
                }
                if (base == 0 && (instruction.flags & GpuSrtLoadedFromMemory) == 0u) {
                    ++result.nullRoots;
                    out = 0;
                    break;
                }
                if (runtime.accessible != nullptr && !runtime.accessible(runtime.userContext, address, sizeof(std::uint32_t))) {
                    if ((instruction.flags & GpuSrtLoadedFromMemory) == 0u) return false;
                    origins.emplace_back(instruction.pc, address);
                    poisonOf[instruction.dst] = static_cast<std::uint32_t>(origins.size() - 1u);
                    out = 0;
                    break;
                }
                if (auto* trace = runtime.readTrace; trace != nullptr) {
                    if (instruction.leaf != 0u) trace->leaves.emplace_back(instruction.leaf - 1u, address);
                    else trace->otherReads.push_back(address);
                }
                std::uint32_t word = 0;
                if (runtime.readMemory == nullptr || !runtime.readMemory(runtime.userContext, address, &word)) return false;
                out = word;
                break;
            }
            case GpuSrtOp::IAdd32: out = u32(a + b); break;
            case GpuSrtOp::IAdd64: out = a + b; break;
            case GpuSrtOp::ISub32: out = u32(a - b); break;
            case GpuSrtOp::ISub64: out = a - b; break;
            case GpuSrtOp::IMul32: out = u32(a * b); break;
            case GpuSrtOp::IMul64: out = a * b; break;
            case GpuSrtOp::SMin32: out = static_cast<std::uint32_t>(std::min(s32(a), s32(b))); break;
            case GpuSrtOp::UMin32: out = std::min(u32(a), u32(b)); break;
            case GpuSrtOp::And32: out = u32(a & b); break;
            case GpuSrtOp::And64: out = a & b; break;
            case GpuSrtOp::Or32: out = u32(a | b); break;
            case GpuSrtOp::Xor32: out = u32(a ^ b); break;
            case GpuSrtOp::Not32: out = ~u32(a); break;
            case GpuSrtOp::Shl32: out = u32(u32(a) << (b & 31u)); break;
            case GpuSrtOp::Shl64: out = a << (b & 63u); break;
            case GpuSrtOp::Shr32: out = u32(a) >> (b & 31u); break;
            case GpuSrtOp::Shr64: out = a >> (b & 63u); break;
            case GpuSrtOp::Sar32: out = static_cast<std::uint32_t>(s32(a) >> (b & 31u)); break;
            case GpuSrtOp::Sar64: out = static_cast<std::uint64_t>(std::bit_cast<std::int64_t>(a) >> (b & 63u)); break;
            case GpuSrtOp::BfeU:
            case GpuSrtOp::BfeS: {
                const auto offset = u32(b);
                const auto width = u32(c);
                if (offset > 32u || width > 32u - offset) return false;
                if (width == 0u) {
                    out = 0;
                    break;
                }
                const auto mask = width == 32u ? 0xffffffffu : (std::uint32_t{1} << width) - 1u;
                auto bits = (u32(a) >> offset) & mask;
                if (instruction.op == GpuSrtOp::BfeS && width < 32u && (bits & (std::uint32_t{1} << (width - 1u))) != 0u) bits |= ~mask;
                out = bits;
                break;
            }
            case GpuSrtOp::Bfi: {
                const auto offset = u32(c);
                const auto width = u32(d);
                if (offset > 32u || width > 32u - offset) return false;
                if (width == 0u) {
                    out = u32(a);
                    break;
                }
                const auto mask = width == 32u ? 0xffffffffu : ((std::uint32_t{1} << width) - 1u) << offset;
                out = (u32(a) & ~mask) | ((u32(b) << offset) & mask);
                break;
            }
            case GpuSrtOp::Select: out = a != 0u ? b : c; break;
            case GpuSrtOp::IEq: out = u32(a) == u32(b) ? 1u : 0u; break;
            case GpuSrtOp::INe: out = u32(a) != u32(b) ? 1u : 0u; break;
            case GpuSrtOp::SLt: out = s32(a) < s32(b) ? 1u : 0u; break;
            case GpuSrtOp::ULt: out = u32(a) < u32(b) ? 1u : 0u; break;
            case GpuSrtOp::UGt: out = u32(a) > u32(b) ? 1u : 0u; break;
            case GpuSrtOp::LAnd: out = (a != 0u) && (b != 0u) ? 1u : 0u; break;
            case GpuSrtOp::LOr: out = (a != 0u) || (b != 0u) ? 1u : 0u; break;
            case GpuSrtOp::LXor: out = (a != 0u) != (b != 0u) ? 1u : 0u; break;
            case GpuSrtOp::LNot: out = a == 0u ? 1u : 0u; break;
        }
    }
    result.flat.assign(program.flatSize, 0u);
    for (const auto& [offset, slot] : program.flat) {
        if (const auto origin = poisonOf[slot]; origin != 0u) {
            result.flat[offset] = 0u;
            result.poison.push_back({offset, origins[origin].first, origins[origin].second});
            continue;
        }
        result.flat[offset] = u32(slots[slot]);
    }
    result.sources.clear();
    std::size_t cursor = 0;
    for (const auto count : program.sourceDwordCounts) {
        DescriptorValue value;
        value.dwordCount = count;
        std::uint32_t zeroedBy = 0;
        for (std::uint32_t dword = 0; dword < count; ++dword) {
            const auto slot = program.sourceDwords[cursor + dword];
            if (poisonOf[slot] != 0u) {
                zeroedBy = poisonOf[slot];
                break;
            }
            value.dwords[dword] = u32(slots[slot]);
        }
        cursor += count;
        if (zeroedBy != 0u) {
            const auto [pc, address] = origins[zeroedBy];
            if (std::none_of(result.poison.begin(), result.poison.end(), [&](const SrtReadPoison& entry) { return entry.pc == pc && entry.address == address; })) return false;
            value.dwords.fill(0u);
        }
        result.sources.push_back(value);
    }
    return true;
}

}
