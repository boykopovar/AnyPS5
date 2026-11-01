#include "Optimization/SrtWalker/SrtTape.hpp"
#include "Optimization/SrtWalker/SrtAddressArithmetic.hpp"
#include "Optimization/SrtWalker/SrtInstructionPredicates.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>

namespace ShaderRecompiler::Detail {

namespace {

float float32(std::uint64_t bits) { return std::bit_cast<float>(static_cast<std::uint32_t>(bits)); }

std::uint64_t float32Bits(float value) { return std::bit_cast<std::uint32_t>(value); }

constexpr std::uint32_t MainContext = 0;
constexpr std::uint32_t CleanContext = 1;
constexpr std::uint32_t MaxDepth = 512;

}

class SrtTapeCompiler {
public:
    SrtTapeCompiler(const IrResourcePlan& plan, SrtTape& tape) : plan(plan), tape(tape) {
        contexts.push_back({nullptr, false, true});
        contexts.push_back({nullptr, true, false});
    }

    bool Build() {
        tape.sourceDwords.assign(plan.descriptorSources.size(), {});
        for (auto& dwords : tape.sourceDwords) dwords.fill(SrtTape::NoNode);
        for (const auto index : plan.materializationSources) {
            if (index >= plan.descriptorSources.size()) return false;
            const auto& source = plan.descriptorSources[index];
            if (source.dwordCount > source.dwords.size()) return false;
            for (std::uint32_t dword = 0; dword < source.dwordCount; ++dword) tape.sourceDwords[index][dword] = compile(source.dwords[dword], MainContext, 0);
        }
        tape.flatRoots.resize(plan.srtReads.size());
        for (std::size_t slot = 0; slot < plan.srtReads.size(); ++slot) {
            const auto& read = plan.srtReads[slot];
            const bool clean = read.flatOffset < plan.cleanFlatSlots.size() && plan.cleanFlatSlots[read.flatOffset] != 0u;
            tape.flatRoots[slot] = compile(read.value, clean ? CleanContext : MainContext, 0);
        }
        tape.conditionRoots.assign(plan.controlFlow.size(), SrtTape::NoNode);
        for (std::size_t block = 0; block < plan.controlFlow.size(); ++block) {
            if (plan.controlFlow[block].condition != nullptr) tape.conditionRoots[block] = compile(plan.controlFlow[block].condition, CleanContext, 0);
        }
        return !malformed;
    }

private:
    using Op = SrtTape::Op;
    using Node = SrtTape::Node;
    struct Context {
        const IrValue* mask;
        bool clean;
        bool cleanSlots;
    };
    static constexpr std::uint32_t Building = 0xfffffffeu;

    std::uint32_t emit(const Node& node) {
        tape.nodes.push_back(node);
        return static_cast<std::uint32_t>(tape.nodes.size() - 1);
    }

    std::uint32_t fail() {
        if (failNode == SrtTape::NoNode) failNode = emit({});
        return failNode;
    }

    std::uint32_t malformedFail() {
        malformed = true;
        return fail();
    }

    std::uint32_t constant(std::uint64_t value) {
        Node node;
        node.op = Op::Const;
        node.immediate = value;
        return emit(node);
    }

    std::uint32_t compile(IrValue* raw, std::uint32_t context, std::uint32_t depth) {
        if (raw == nullptr || depth > MaxDepth) return malformedFail();
        IrValue* value = raw->Resolve();
        if (value->HasImmediate()) {
            switch (value->Type()) {
                case IrType::Bool: return constant(value->ImmediateBool() ? 1u : 0u);
                case IrType::U8: return constant(value->ImmediateU8());
                case IrType::U16: return constant(value->ImmediateU16());
                case IrType::U32: return constant(value->ImmediateU32());
                case IrType::U64: return constant(value->ImmediateU64());
                case IrType::F32: return constant(float32Bits(value->ImmediateF32()));
                default: return fail();
            }
        }
        if (value->Opcode() == IrOpcode::Void) return fail();
        const auto mask = contexts[context].mask;
        if (mask != nullptr && IsRuntimeSelect(value->Opcode()) && value->ArgumentCount() == 3 && ActiveMaskImplies(mask, value->Argument(0))) return compile(value->Argument(1), context, depth + 1);
        const auto key = std::make_pair(static_cast<const IrValue*>(value), context);
        if (const auto found = built.find(key); found != built.end()) return found->second == Building ? fail() : found->second;
        built.emplace(key, Building);
        const auto index = build(value, context, depth);
        built[key] = index;
        return index;
    }

    std::uint32_t operation(Op op, IrValue* value, std::uint32_t context, std::uint32_t depth, std::uint8_t count) {
        if (value->ArgumentCount() < count) return malformedFail();
        Node node;
        node.op = op;
        node.argumentCount = count;
        for (std::uint8_t index = 0; index < count; ++index) node.arguments[index] = compile(value->Argument(index), context, depth + 1);
        return emit(node);
    }

    std::uint32_t build(IrValue* value, std::uint32_t context, std::uint32_t depth) {
        const auto current = contexts[context];
        switch (value->Opcode()) {
            case IrOpcode::GetUserData: {
                if (value->ArgumentCount() < 1 || value->Argument(0) == nullptr) return malformedFail();
                const auto reg = RegIndex(static_cast<ScalarReg>(value->Argument(0)->Register().index));
                if (reg < plan.userDataBase) return fail();
                Node node;
                node.op = Op::UserData;
                node.immediate = reg - plan.userDataBase;
                return emit(node);
            }
            case IrOpcode::GetShaderBase: {
                Node node;
                node.op = Op::ShaderBase;
                return emit(node);
            }
            case IrOpcode::Phi: {
                IrValue* resolved = ResolveInvariantPhi(plan, value);
                return resolved != nullptr ? compile(resolved, context, depth + 1) : fail();
            }
            case IrOpcode::ReadFirstLane: {
                if (value->ArgumentCount() < 2) return malformedFail();
                IrValue* laneMask = value->Argument(1);
                contexts.push_back({laneMask != nullptr ? laneMask->Resolve() : nullptr, current.clean, current.cleanSlots});
                return compile(value->Argument(0), static_cast<std::uint32_t>(contexts.size() - 1), depth + 1);
            }
            case IrOpcode::BitCastU32F32:
            case IrOpcode::BitCastF32U32:
                if (value->ArgumentCount() < 1) return malformedFail();
                return compile(value->Argument(0), context, depth + 1);
            case IrOpcode::CompositeExtractU64:
            case IrOpcode::CompositeExtractU32x2: {
                if (value->ArgumentCount() < 2) return malformedFail();
                IrValue* index = value->Argument(1)->Resolve();
                if (!index->HasImmediate() || index->Type() != IrType::U32) return fail();
                const auto component = index->ImmediateU32();
                if (component >= 2u) return fail();
                if (value->Opcode() == IrOpcode::CompositeExtractU64) {
                    Node node;
                    node.op = Op::ExtractHalf;
                    node.component = static_cast<std::uint8_t>(component);
                    node.argumentCount = 1;
                    node.arguments[0] = compile(value->Argument(0), context, depth + 1);
                    return emit(node);
                }
                IrValue* source = value->Argument(0)->Resolve();
                if (source->Opcode() == IrOpcode::Void) return fail();
                if (source->Opcode() == IrOpcode::CompositeConstructU32x2) {
                    if (source->ArgumentCount() <= component) return malformedFail();
                    return compile(source->Argument(component), context, depth + 1);
                }
                if (source->Opcode() == IrOpcode::IAddCarry32) {
                    if (source->ArgumentCount() < 2) return malformedFail();
                    Node node;
                    node.op = Op::AddCarry;
                    node.component = static_cast<std::uint8_t>(component);
                    node.argumentCount = 2;
                    node.arguments[0] = compile(source->Argument(0), context, depth + 1);
                    node.arguments[1] = compile(source->Argument(1), context, depth + 1);
                    return emit(node);
                }
                return fail();
            }
            case IrOpcode::CompositeConstructU64: return operation(Op::ConstructU64, value, context, depth, 2);
            case IrOpcode::ReadConst: {
                if (value->ArgumentCount() < 2) return malformedFail();
                IrValue* slot = value->Argument(1)->Resolve();
                if (!slot->HasImmediate() || slot->Type() != IrType::U32 || slot->ImmediateU32() >= plan.srtReads.size()) return fail();
                const auto index = slot->ImmediateU32();
                const bool clean = current.cleanSlots && index < plan.cleanFlatSlots.size() && plan.cleanFlatSlots[index] != 0u;
                return compile(plan.srtReads[index].value, clean ? CleanContext : context, depth + 1);
            }
            case IrOpcode::LoadAddressU32:
            case IrOpcode::ReadConstBuffer: {
                if (!IsRawRead(plan, *value)) return fail();
                if (value->ArgumentCount() < 2) return malformedFail();
                if (value->Flags<MemoryFlags>().index >= plan.memoryInfo.size()) return fail();
                const auto& memory = plan.memoryInfo[value->Flags<MemoryFlags>().index];
                IrValue* handle = value->Argument(0)->Resolve();
                if (handle->Opcode() == IrOpcode::Void) return fail();
                if (handle->ArgumentCount() < 2) return malformedFail();
                Node node;
                node.op = value->Opcode() == IrOpcode::ReadConstBuffer ? Op::ReadConstBuffer : Op::Read;
                node.clean = current.clean;
                node.loaded = LoadedFromMemory(*handle);
                node.source = value;
                if (IrValue* low = handle->Argument(0); low != nullptr) {
                    low = low->Resolve();
                    if (!low->HasImmediate() && low->Opcode() == IrOpcode::GetUserData && low->ArgumentCount() >= 1 && low->Argument(0) != nullptr) {
                        const auto reg = RegIndex(static_cast<ScalarReg>(low->Argument(0)->Register().index));
                        if (reg >= plan.userDataBase && reg - plan.userDataBase < SrtUserDataUse::TrackedWords) node.baseWord = static_cast<std::int32_t>(reg - plan.userDataBase);
                    }
                }
                const auto immediate = static_cast<std::int64_t>(static_cast<std::int32_t>(memory.offset));
                node.immediate = static_cast<std::uint64_t>(immediate);
                node.arguments[0] = compile(handle->Argument(0), context, depth + 1);
                node.arguments[1] = compile(handle->Argument(1), context, depth + 1);
                node.arguments[2] = compile(value->Argument(1), context, depth + 1);
                node.argumentCount = 3;
                if (node.op == Op::ReadConstBuffer) {
                    if (handle->ArgumentCount() != 4u || immediate < 0) return fail();
                    node.arguments[3] = compile(handle->Argument(2), context, depth + 1);
                    node.arguments[4] = compile(handle->Argument(3), context, depth + 1);
                    node.argumentCount = 5;
                }
                return emit(node);
            }
            case IrOpcode::IAdd32: return operation(Op::IAdd32, value, context, depth, 2);
            case IrOpcode::IAdd64: return operation(Op::IAdd64, value, context, depth, 2);
            case IrOpcode::ISub32: return operation(Op::ISub32, value, context, depth, 2);
            case IrOpcode::ISub64: return operation(Op::ISub64, value, context, depth, 2);
            case IrOpcode::IMul32: return operation(Op::IMul32, value, context, depth, 2);
            case IrOpcode::IMul64: return operation(Op::IMul64, value, context, depth, 2);
            case IrOpcode::SMin32: return operation(Op::SMin32, value, context, depth, 2);
            case IrOpcode::UMin32: return operation(Op::UMin32, value, context, depth, 2);
            case IrOpcode::ConvertF32U32: return operation(Op::ConvertF32U32, value, context, depth, 1);
            case IrOpcode::ConvertU32F32: return operation(Op::ConvertU32F32, value, context, depth, 1);
            case IrOpcode::FPMul32: return operation(Op::FPMul32, value, context, depth, 2);
            case IrOpcode::FPTrunc32: return operation(Op::FPTrunc32, value, context, depth, 1);
            case IrOpcode::FPIsNan32: return operation(Op::FPIsNan32, value, context, depth, 1);
            case IrOpcode::FPOrdLessThanEqual32: return operation(Op::FPOrdLessThanEqual32, value, context, depth, 2);
            case IrOpcode::FPOrdGreaterThanEqual32: return operation(Op::FPOrdGreaterThanEqual32, value, context, depth, 2);
            case IrOpcode::FPUnordLessThanEqual32: return operation(Op::FPUnordLessThanEqual32, value, context, depth, 2);
            case IrOpcode::FPUnordGreaterThanEqual32: return operation(Op::FPUnordGreaterThanEqual32, value, context, depth, 2);
            case IrOpcode::BitwiseAnd32: return operation(Op::BitwiseAnd32, value, context, depth, 2);
            case IrOpcode::BitwiseAnd64: return operation(Op::BitwiseAnd64, value, context, depth, 2);
            case IrOpcode::BitwiseOr32: return operation(Op::BitwiseOr32, value, context, depth, 2);
            case IrOpcode::BitwiseXor32: return operation(Op::BitwiseXor32, value, context, depth, 2);
            case IrOpcode::BitwiseNot32: return operation(Op::BitwiseNot32, value, context, depth, 1);
            case IrOpcode::ShiftLeftLogical32: return operation(Op::ShiftLeftLogical32, value, context, depth, 2);
            case IrOpcode::ShiftLeftLogical64: return operation(Op::ShiftLeftLogical64, value, context, depth, 2);
            case IrOpcode::ShiftRightLogical32: return operation(Op::ShiftRightLogical32, value, context, depth, 2);
            case IrOpcode::ShiftRightLogical64: return operation(Op::ShiftRightLogical64, value, context, depth, 2);
            case IrOpcode::ShiftRightArithmetic32: return operation(Op::ShiftRightArithmetic32, value, context, depth, 2);
            case IrOpcode::ShiftRightArithmetic64: return operation(Op::ShiftRightArithmetic64, value, context, depth, 2);
            case IrOpcode::BitFieldUExtract: return operation(Op::BitFieldUExtract, value, context, depth, 3);
            case IrOpcode::BitFieldSExtract: return operation(Op::BitFieldSExtract, value, context, depth, 3);
            case IrOpcode::BitFieldInsert: return operation(Op::BitFieldInsert, value, context, depth, 4);
            case IrOpcode::SelectU32:
            case IrOpcode::SelectU1:
            case IrOpcode::SelectF32: return operation(Op::Select, value, context, depth, 3);
            case IrOpcode::IEqual32: return operation(Op::IEqual32, value, context, depth, 2);
            case IrOpcode::INotEqual32: return operation(Op::INotEqual32, value, context, depth, 2);
            case IrOpcode::SLessThan32: return operation(Op::SLessThan32, value, context, depth, 2);
            case IrOpcode::ULessThan32: return operation(Op::ULessThan32, value, context, depth, 2);
            case IrOpcode::UGreaterThan32: return operation(Op::UGreaterThan32, value, context, depth, 2);
            case IrOpcode::LogicalAnd: return operation(Op::LogicalAnd, value, context, depth, 2);
            case IrOpcode::LogicalOr: return operation(Op::LogicalOr, value, context, depth, 2);
            case IrOpcode::LogicalXor: return operation(Op::LogicalXor, value, context, depth, 2);
            case IrOpcode::LogicalNot: return operation(Op::LogicalNot, value, context, depth, 1);
            default: return fail();
        }
    }

    const IrResourcePlan& plan;
    SrtTape& tape;
    std::vector<Context> contexts;
    std::map<std::pair<const IrValue*, std::uint32_t>, std::uint32_t> built;
    std::uint32_t failNode = SrtTape::NoNode;
    bool malformed = false;
};

bool SrtTape::Enabled() {
    static const bool enabled = std::getenv("APS5_NO_SRT_TAPE") == nullptr && std::getenv("APS5_SRT_DEBUG") == nullptr;
    return enabled;
}

bool SrtTape::Verified() {
    static const bool verified = std::getenv("APS5_VERIFY_SRT_TAPE") != nullptr;
    return verified;
}

std::shared_ptr<const SrtTape> SrtTape::Compile(const IrResourcePlan& plan) {
    if (!plan.srtPlanComplete) return nullptr;
    auto tape = std::make_shared<SrtTape>();
    SrtTapeCompiler compiler(plan, *tape);
    if (!compiler.Build()) return nullptr;
    return tape;
}

const SrtTape* SrtTape::For(const IrResourcePlan& plan) {
    auto* slot = plan.tapeSlot.get();
    if (slot == nullptr || !Enabled()) return nullptr;
    if (!slot->ready.load(std::memory_order_acquire)) {
        std::call_once(slot->once, [&] {
            try {
                slot->tape = Compile(plan);
            } catch (...) {
                slot->tape = nullptr;
            }
        });
        slot->ready.store(true, std::memory_order_release);
    }
    return static_cast<const SrtTape*>(slot->tape.get());
}

struct SrtTape::Frame {
    const SrtRuntime& runtime;
    std::uint64_t* values;
    std::uint8_t* states;
    bool nullRoot = false;
    std::uint32_t addressDepth = 0;
    std::size_t descriptorBytes = 0;
};

bool SrtTape::evaluate(Frame& frame, std::uint32_t index, std::uint64_t& result) const {
    const auto state = frame.states[index];
    if (state == 1) {
        result = frame.values[index];
        return true;
    }
    if (state == 2) return false;
    std::uint64_t value = 0;
    if (!compute(frame, nodes[index], value)) {
        frame.states[index] = 2;
        return false;
    }
    frame.states[index] = 1;
    frame.values[index] = value;
    result = value;
    return true;
}

bool SrtTape::compute(Frame& frame, const Node& node, std::uint64_t& result) const {
    if (node.op == Op::LogicalAnd || node.op == Op::LogicalOr) {
        const bool decisive = node.op == Op::LogicalOr;
        std::uint64_t first = 0;
        std::uint64_t second = 0;
        const bool firstEvaluated = evaluate(frame, node.arguments[0], first);
        if (firstEvaluated && (first != 0u) == decisive) {
            result = decisive ? 1u : 0u;
            return true;
        }
        const bool secondEvaluated = evaluate(frame, node.arguments[1], second);
        if (secondEvaluated && (second != 0u) == decisive) {
            result = decisive ? 1u : 0u;
            return true;
        }
        if (!firstEvaluated || !secondEvaluated) return false;
        result = decisive ? 0u : 1u;
        return true;
    }
    if (node.op == Op::ULessThan32 || node.op == Op::UGreaterThan32) {
        const bool greater = node.op == Op::UGreaterThan32;
        std::uint64_t first = 0;
        std::uint64_t second = 0;
        const bool firstEvaluated = evaluate(frame, node.arguments[0], first);
        if (firstEvaluated && static_cast<std::uint32_t>(first) == (greater ? 0u : 0xffffffffu)) {
            result = 0u;
            return true;
        }
        const bool secondEvaluated = evaluate(frame, node.arguments[1], second);
        if (secondEvaluated && static_cast<std::uint32_t>(second) == (greater ? 0xffffffffu : 0u)) {
            result = 0u;
            return true;
        }
        if (!firstEvaluated || !secondEvaluated) return false;
        const auto lhs = static_cast<std::uint32_t>(greater ? second : first);
        const auto rhs = static_cast<std::uint32_t>(greater ? first : second);
        result = lhs < rhs ? 1u : 0u;
        return true;
    }
    std::array<std::uint64_t, 5> operand{};
    std::int32_t origin = -1;
    const bool addressCone = node.op == Op::Read || node.op == Op::ReadConstBuffer;
    for (std::uint8_t index = 0; index < node.argumentCount; ++index) {
        if (index == 0 && node.baseWord >= 0 && frame.runtime.userDataUse != nullptr && static_cast<std::size_t>(node.baseWord) < frame.runtime.userData.size()) {
            origin = node.baseWord;
            operand[0] = frame.runtime.userData[static_cast<std::size_t>(origin)];
            frame.runtime.userDataUse->asBase |= std::uint64_t {1} << origin;
            continue;
        }
        const bool nestedAddress = addressCone || (node.op == Op::Select && index == 0u);
        if (nestedAddress) ++frame.addressDepth;
        const bool ok = evaluate(frame, node.arguments[index], operand[index]);
        if (nestedAddress) --frame.addressDepth;
        if (!ok) return false;
    }
    const auto a = operand[0];
    const auto b = operand[1];
    const auto c = operand[2];
    const auto d = operand[3];
    switch (node.op) {
        case Op::Fail: return false;
        case Op::Const: result = node.immediate; return true;
        case Op::UserData:
            if (node.immediate >= frame.runtime.userData.size()) return false;
            if (frame.runtime.userDataUse != nullptr) frame.runtime.userDataUse->asValue |= node.immediate < SrtUserDataUse::TrackedWords ? std::uint64_t {1} << node.immediate : ~std::uint64_t {0};
            result = frame.runtime.userData[static_cast<std::size_t>(node.immediate)];
            return true;
        case Op::ShaderBase: result = frame.runtime.shaderBase; return true;
        case Op::Read:
        case Op::ReadConstBuffer: {
            const auto base = ((b << 32u) | static_cast<std::uint32_t>(a)) & AddressMask;
            const auto immediate = static_cast<std::int64_t>(node.immediate);
            std::uint64_t address = 0;
            if (node.op == Op::ReadConstBuffer) {
                const auto records = d;
                const auto byteOffset = static_cast<std::uint64_t>(immediate) + static_cast<std::uint32_t>(c);
                const auto aligned = byteOffset & ~std::uint64_t {3};
                const auto stride = (static_cast<std::uint32_t>(b) >> 16u) & 0x3fffu;
                const auto size = stride == 0u ? static_cast<std::uint64_t>(static_cast<std::uint32_t>(records)) : static_cast<std::uint64_t>(stride) * static_cast<std::uint32_t>(records);
                if (aligned > size || size - aligned < sizeof(std::uint32_t)) {
                    result = 0;
                    return true;
                }
                address = ((base & ~std::uint64_t {3}) + byteOffset) & ~std::uint64_t {3};
            } else {
                const auto relative = (immediate & ~std::int64_t {3}) + static_cast<std::int64_t>(static_cast<std::uint32_t>(c) & ~3u);
                if (!AddSignedAddress(base & ~std::uint64_t {3}, relative, address)) return false;
            }
            if (base == 0u && !node.loaded) {
                frame.nullRoot = true;
                return false;
            }
            if (frame.runtime.userDataUse != nullptr) frame.runtime.userDataUse->reads.emplace_back(address, origin);
            if (node.loaded && frame.runtime.accessible != nullptr && !frame.runtime.accessible(frame.runtime.userContext, address, sizeof(std::uint32_t))) return false;
            if (auto* trace = frame.runtime.readTrace; trace != nullptr) {
                if (node.source == trace->leaf) trace->leaves.emplace_back(trace->leafSlot, address);
                else trace->otherReads.push_back(address);
            }
            const auto reader = node.clean ? frame.runtime.readSpecializationMemory : frame.runtime.readMemory;
            std::uint32_t word = 0;
            if (reader != nullptr) {
                const bool scoped = frame.addressDepth == 0u && frame.descriptorBytes != 0u && frame.runtime.descriptorScope != nullptr;
                if (scoped) static_cast<void>(frame.runtime.descriptorScope(frame.runtime.userContext, frame.descriptorBytes));
                const bool ok = reader(frame.runtime.userContext, address, &word);
                if (scoped) static_cast<void>(frame.runtime.descriptorScope(frame.runtime.userContext, 0u));
                if (!ok) return false;
            } else {
                std::memcpy(&word, reinterpret_cast<const void*>(address), sizeof(word));
            }
            result = word;
            return true;
        }
        case Op::ExtractHalf: result = static_cast<std::uint32_t>(a >> (node.component * 32u)); return true;
        case Op::AddCarry: {
            const auto sum = static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) + static_cast<std::uint32_t>(b);
            result = node.component == 0u ? static_cast<std::uint32_t>(sum) : static_cast<std::uint32_t>(sum >> 32u);
            return true;
        }
        case Op::ConstructU64: result = static_cast<std::uint32_t>(a) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(b)) << 32u); return true;
        case Op::ConvertF32U32: result = float32Bits(static_cast<float>(static_cast<std::uint32_t>(a))); return true;
        case Op::ConvertU32F32: {
            const auto value = float32(a);
            if (!std::isfinite(value) || value < 0.0f || static_cast<double>(value) > 4294967295.0) return false;
            result = static_cast<std::uint32_t>(value);
            return true;
        }
        case Op::FPMul32: result = float32Bits(float32(a) * float32(b)); return true;
        case Op::FPTrunc32: result = float32Bits(std::trunc(float32(a))); return true;
        case Op::FPIsNan32: result = std::isnan(float32(a)) ? 1u : 0u; return true;
        case Op::FPOrdLessThanEqual32: result = float32(a) <= float32(b) ? 1u : 0u; return true;
        case Op::FPOrdGreaterThanEqual32: result = float32(a) >= float32(b) ? 1u : 0u; return true;
        case Op::FPUnordLessThanEqual32: result = !(float32(a) > float32(b)) ? 1u : 0u; return true;
        case Op::FPUnordGreaterThanEqual32: result = !(float32(a) < float32(b)) ? 1u : 0u; return true;
        case Op::IAdd32: result = static_cast<std::uint32_t>(a + b); return true;
        case Op::IAdd64: result = a + b; return true;
        case Op::ISub32: result = static_cast<std::uint32_t>(a - b); return true;
        case Op::ISub64: result = a - b; return true;
        case Op::IMul32: result = static_cast<std::uint32_t>(a * b); return true;
        case Op::IMul64: result = a * b; return true;
        case Op::SMin32: result = static_cast<std::uint32_t>(std::min(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)), std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(b)))); return true;
        case Op::UMin32: result = std::min(static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(b)); return true;
        case Op::BitwiseAnd32: result = static_cast<std::uint32_t>(a & b); return true;
        case Op::BitwiseAnd64: result = a & b; return true;
        case Op::BitwiseOr32: result = static_cast<std::uint32_t>(a | b); return true;
        case Op::BitwiseXor32: result = static_cast<std::uint32_t>(a ^ b); return true;
        case Op::BitwiseNot32: result = ~static_cast<std::uint32_t>(a); return true;
        case Op::ShiftLeftLogical32: result = static_cast<std::uint32_t>(a) << (b & 31u); return true;
        case Op::ShiftLeftLogical64: result = a << (b & 63u); return true;
        case Op::ShiftRightLogical32: result = static_cast<std::uint32_t>(a) >> (b & 31u); return true;
        case Op::ShiftRightLogical64: result = a >> (b & 63u); return true;
        case Op::ShiftRightArithmetic32: result = static_cast<std::uint32_t>(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)) >> (b & 31u)); return true;
        case Op::ShiftRightArithmetic64: result = static_cast<std::uint64_t>(std::bit_cast<std::int64_t>(a) >> (b & 63u)); return true;
        case Op::BitFieldUExtract: {
            const auto offset = static_cast<std::uint32_t>(b);
            const auto width = static_cast<std::uint32_t>(c);
            if (offset > 32u || width > 32u - offset) return false;
            const auto mask = width == 32u ? 0xffffffffu : width == 0u ? 0u : (std::uint32_t {1} << width) - 1u;
            result = width == 0u ? 0u : (static_cast<std::uint32_t>(a) >> offset) & mask;
            return true;
        }
        case Op::BitFieldSExtract: {
            const auto offset = static_cast<std::uint32_t>(b);
            const auto width = static_cast<std::uint32_t>(c);
            if (offset > 32u || width > 32u - offset) return false;
            if (width == 0u) {
                result = 0;
                return true;
            }
            const auto mask = width == 32u ? 0xffffffffu : (std::uint32_t {1} << width) - 1u;
            auto bits = (static_cast<std::uint32_t>(a) >> offset) & mask;
            if (width < 32u && (bits & (std::uint32_t {1} << (width - 1u))) != 0u) bits |= ~mask;
            result = bits;
            return true;
        }
        case Op::BitFieldInsert: {
            const auto offset = static_cast<std::uint32_t>(c);
            const auto width = static_cast<std::uint32_t>(d);
            if (offset > 32u || width > 32u - offset) return false;
            if (width == 0u) {
                result = static_cast<std::uint32_t>(a);
                return true;
            }
            const auto mask = width == 32u ? 0xffffffffu : ((std::uint32_t {1} << width) - 1u) << offset;
            result = (static_cast<std::uint32_t>(a) & ~mask) | ((static_cast<std::uint32_t>(b) << offset) & mask);
            return true;
        }
        case Op::Select: result = a != 0u ? b : c; return true;
        case Op::IEqual32: result = static_cast<std::uint32_t>(a) == static_cast<std::uint32_t>(b) ? 1u : 0u; return true;
        case Op::INotEqual32: result = static_cast<std::uint32_t>(a) != static_cast<std::uint32_t>(b) ? 1u : 0u; return true;
        case Op::SLessThan32: result = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)) < std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(b)) ? 1u : 0u; return true;
        case Op::ULessThan32:
        case Op::UGreaterThan32: return false;
        case Op::LogicalAnd:
        case Op::LogicalOr: return false;
        case Op::LogicalXor: result = (a != 0u) != (b != 0u) ? 1u : 0u; return true;
        case Op::LogicalNot: result = a == 0u ? 1u : 0u; return true;
    }
    return false;
}

bool SrtTape::Run(const IrResourcePlan& plan, const SrtRuntime& runtime, std::vector<DescriptorValue>& results, std::vector<std::uint32_t>& flat, std::vector<std::uint8_t>& activeSources) const {
    if (!plan.srtPlanComplete) return false;
    if (std::any_of(plan.cleanFlatSlots.begin(), plan.cleanFlatSlots.end(), [](std::uint8_t clean) { return clean != 0u; }) && runtime.readSpecializationMemory == nullptr) return false;
    auto* trace = runtime.readTrace;
    const auto leavesBefore = trace != nullptr ? trace->leaves.size() : 0;
    const auto readsBefore = trace != nullptr ? trace->otherReads.size() : 0;
    auto* use = runtime.userDataUse;
    const auto baseBefore = use != nullptr ? use->asBase : 0;
    const auto valueBefore = use != nullptr ? use->asValue : 0;
    const auto useReadsBefore = use != nullptr ? use->reads.size() : 0;
    const auto refuse = [&] {
        if (trace != nullptr) {
            trace->leaves.resize(leavesBefore);
            trace->otherReads.resize(readsBefore);
            trace->leaf = nullptr;
        }
        if (use != nullptr) {
            use->asBase = baseBefore;
            use->asValue = valueBefore;
            use->reads.resize(useReadsBefore);
        }
        return false;
    };
    constexpr std::size_t InlineNodes = 512;
    std::array<std::uint64_t, InlineNodes> inlineValues;
    std::array<std::uint8_t, InlineNodes> inlineStates;
    std::vector<std::uint64_t> heapValues;
    std::vector<std::uint8_t> heapStates;
    std::uint64_t* values = inlineValues.data();
    std::uint8_t* states = inlineStates.data();
    if (nodes.size() > InlineNodes) {
        heapValues.resize(nodes.size());
        heapStates.assign(nodes.size(), 0u);
        values = heapValues.data();
        states = heapStates.data();
    } else {
        std::fill_n(states, nodes.size(), std::uint8_t {0});
    }
    Frame frame {runtime, values, states};

    struct Scratch {
        std::uint32_t attempt = 0;
        std::vector<std::uint8_t> active;
        std::vector<std::uint8_t> activeFlat;
        std::vector<std::uint8_t> visited;
        std::vector<std::uint32_t> pending;
        std::vector<DescriptorValue> evaluated;
        std::vector<std::uint32_t> flattened;
    };
    struct ScratchStorage {};
    auto& scratch = HostThreadLocal<Scratch, ScratchStorage>();
    struct AttemptScope {
        std::uint32_t& depth;
        ~AttemptScope() { --depth; }
    } attemptScope{scratch.attempt};
    const auto attempt = scratch.attempt++;
    const SrtForceSyncScope forceSync(runtime, attempt == 2u);
    const auto invalidationsBefore = runtime.descriptorScope != nullptr ? runtime.descriptorScope(runtime.userContext, 0u) : 0u;
    auto& active = scratch.active;
    const auto imageBase = plan.descriptorSources.size();
    active.assign(imageBase + plan.info.images.size(), 1u);
    auto& activeFlat = scratch.activeFlat;
    activeFlat.assign(plan.srtReads.size(), 1u);
    if (!plan.controlFlow.empty()) {
        for (const auto& block : plan.controlFlow) {
            for (const auto source : block.sources) {
                if (source >= active.size()) return refuse();
                active[source] = 0u;
            }
            for (const auto image : block.images) {
                if (image >= plan.info.images.size()) return refuse();
                active[imageBase + image] = 0u;
            }
            for (const auto slot : block.flatSlots) {
                if (slot >= activeFlat.size()) return refuse();
                activeFlat[slot] = 0u;
            }
        }
        auto& visited = scratch.visited;
        visited.assign(plan.controlFlow.size(), 0u);
        auto& pending = scratch.pending;
        pending.assign(1, 0u);
        while (!pending.empty()) {
            const auto index = pending.back();
            pending.pop_back();
            if (index >= visited.size()) return refuse();
            if (visited[index]) continue;
            visited[index] = 1u;
            const auto& block = plan.controlFlow[index];
            for (const auto source : block.sources) active[source] = 1u;
            for (const auto image : block.images) active[imageBase + image] = 1u;
            for (const auto slot : block.flatSlots) activeFlat[slot] = 1u;
            std::uint64_t condition = 0;
            const bool cleanEvaluable = block.condition != nullptr && runtime.readSpecializationMemory != nullptr && conditionRoots[index] != NoNode && evaluate(frame, conditionRoots[index], condition);
            if (frame.nullRoot) return refuse();
            if (cleanEvaluable) {
                if (block.successors.size() < 2) return refuse();
                pending.push_back(block.successors[static_cast<std::uint32_t>(condition) != 0u ? 0u : 1u]);
            } else {
                pending.insert(pending.end(), block.successors.begin(), block.successors.end());
            }
        }
    }
    auto& evaluated = scratch.evaluated;
    evaluated.clear();
    evaluated.reserve(plan.materializationSources.size());
    for (const auto sourceIndex : plan.materializationSources) {
        if (sourceIndex >= plan.descriptorSources.size()) return refuse();
        const auto& source = plan.descriptorSources[sourceIndex];
        DescriptorValue value;
        value.dwordCount = source.dwordCount;
        if (active[sourceIndex]) {
            for (std::uint32_t dword = 0; dword < source.dwordCount; ++dword) {
                std::uint64_t word = 0;
                frame.descriptorBytes = source.dwordCount > 1u ? static_cast<std::size_t>(source.dwordCount - dword) * sizeof(std::uint32_t) : 0u;
                const bool ok = evaluate(frame, sourceDwords[sourceIndex][dword], word);
                frame.descriptorBytes = 0u;
                if (!ok) return refuse();
                value.dwords[dword] = static_cast<std::uint32_t>(word);
            }
        }
        evaluated.push_back(value);
    }
    auto& flattened = scratch.flattened;
    flattened.assign(plan.srtReads.size(), 0u);
    for (std::size_t slot = 0; slot < plan.srtReads.size(); ++slot) {
        const auto& read = plan.srtReads[slot];
        if (read.flatOffset >= activeFlat.size()) return refuse();
        if (activeFlat[read.flatOffset] == 0u) continue;
        const bool pure = trace != nullptr && read.flatOffset < plan.pureFlatSlots.size() && plan.pureFlatSlots[read.flatOffset] != 0u;
        if (pure) {
            trace->leaf = read.value->Resolve();
            trace->leafSlot = read.flatOffset;
        }
        std::uint64_t word = 0;
        const bool ok = read.flatOffset < flattened.size() && evaluate(frame, flatRoots[slot], word);
        if (pure) trace->leaf = nullptr;
        if (!ok) return refuse();
        flattened[read.flatOffset] = static_cast<std::uint32_t>(word);
    }
    if (runtime.descriptorScope != nullptr && runtime.descriptorScope(runtime.userContext, 0u) != invalidationsBefore) {
        refuse();
        return attempt + 1u < 3u && Run(plan, runtime, results, flat, activeSources);
    }
    results.swap(evaluated);
    activeSources.swap(active);
    flat.assign(flattened.begin(), flattened.end());
    return true;
}

}
