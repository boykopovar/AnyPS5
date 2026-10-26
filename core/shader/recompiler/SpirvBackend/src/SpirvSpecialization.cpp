#define SPV_ENABLE_UTILITY_CODE
#include <spirv/unified1/spirv.hpp>
#undef SPV_ENABLE_UTILITY_CODE
#include "SpirvBackend/SpirvSpecialization.hpp"
#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ShaderRecompiler {
namespace {

class WordArena {
public:
    explicit WordArena(std::size_t words) : next(std::max<std::size_t>(words, 1024u)) {}

    std::uint32_t* Allocate(std::size_t count) {
        if (count > available) {
            const auto size = std::max(count, next);
            chunks.push_back(std::make_unique<std::uint32_t[]>(size));
            cursor = chunks.back().get();
            available = size;
            next = size * 2u;
        }
        auto* words = cursor;
        cursor += count;
        available -= count;
        return words;
    }

private:
    std::vector<std::unique_ptr<std::uint32_t[]>> chunks;
    std::uint32_t* cursor = nullptr;
    std::size_t available = 0;
    std::size_t next;
};

template<typename T>
struct ArenaAllocator {
    using value_type = T;

    ArenaAllocator() = default;
    explicit ArenaAllocator(WordArena* arena) : arena(arena) {}
    template<typename U>
    ArenaAllocator(const ArenaAllocator<U>& other) : arena(other.arena) {}

    T* allocate(std::size_t count) {
        static_assert(std::is_same_v<T, std::uint32_t>);
        return arena != nullptr ? arena->Allocate(count) : std::allocator<T>{}.allocate(count);
    }

    void deallocate(T* words, std::size_t count) {
        if (arena == nullptr) std::allocator<T>{}.deallocate(words, count);
    }

    friend bool operator==(const ArenaAllocator& left, const ArenaAllocator& right) {
        return left.arena == right.arena;
    }

    WordArena* arena = nullptr;
};

using Instruction = std::vector<std::uint32_t, ArenaAllocator<std::uint32_t>>;

spv::Op Opcode(const Instruction& instruction) {
    return instruction.empty() ? spv::OpNop : static_cast<spv::Op>(instruction[0] & 0xffffu);
}

std::uint32_t Result(const Instruction& instruction) {
    if (instruction.empty()) return 0u;
    bool result = false;
    bool type = false;
    spv::HasResultAndType(Opcode(instruction), &result, &type);
    return result ? instruction.at(type ? 2u : 1u) : 0u;
}

Instruction Make(spv::Op op, std::initializer_list<std::uint32_t> operands) {
    Instruction result{(static_cast<std::uint32_t>(operands.size() + 1u) << 16u) | op};
    result.insert(result.end(), operands);
    return result;
}

bool PureInstruction(const Instruction& instruction) {
    switch (Opcode(instruction)) {
    case spv::OpVectorExtractDynamic:
    case spv::OpVectorInsertDynamic:
    case spv::OpVectorShuffle:
    case spv::OpCompositeConstruct:
    case spv::OpCompositeExtract:
    case spv::OpCompositeInsert:
    case spv::OpCopyObject:
    case spv::OpTranspose:
    case spv::OpConvertFToU:
    case spv::OpConvertFToS:
    case spv::OpConvertSToF:
    case spv::OpConvertUToF:
    case spv::OpUConvert:
    case spv::OpSConvert:
    case spv::OpFConvert:
    case spv::OpQuantizeToF16:
    case spv::OpConvertPtrToU:
    case spv::OpSatConvertSToU:
    case spv::OpSatConvertUToS:
    case spv::OpConvertUToPtr:
    case spv::OpPtrCastToGeneric:
    case spv::OpGenericCastToPtr:
    case spv::OpGenericCastToPtrExplicit:
    case spv::OpBitcast:
    case spv::OpSNegate:
    case spv::OpFNegate:
    case spv::OpIAdd:
    case spv::OpFAdd:
    case spv::OpISub:
    case spv::OpFSub:
    case spv::OpIMul:
    case spv::OpFMul:
    case spv::OpUDiv:
    case spv::OpSDiv:
    case spv::OpFDiv:
    case spv::OpUMod:
    case spv::OpSRem:
    case spv::OpSMod:
    case spv::OpFRem:
    case spv::OpFMod:
    case spv::OpVectorTimesScalar:
    case spv::OpMatrixTimesScalar:
    case spv::OpVectorTimesMatrix:
    case spv::OpMatrixTimesVector:
    case spv::OpMatrixTimesMatrix:
    case spv::OpOuterProduct:
    case spv::OpDot:
    case spv::OpIAddCarry:
    case spv::OpISubBorrow:
    case spv::OpUMulExtended:
    case spv::OpSMulExtended:
    case spv::OpAny:
    case spv::OpAll:
    case spv::OpIsNan:
    case spv::OpIsInf:
    case spv::OpIsFinite:
    case spv::OpIsNormal:
    case spv::OpSignBitSet:
    case spv::OpLessOrGreater:
    case spv::OpOrdered:
    case spv::OpUnordered:
    case spv::OpLogicalEqual:
    case spv::OpLogicalNotEqual:
    case spv::OpLogicalOr:
    case spv::OpLogicalAnd:
    case spv::OpLogicalNot:
    case spv::OpSelect:
    case spv::OpIEqual:
    case spv::OpINotEqual:
    case spv::OpUGreaterThan:
    case spv::OpSGreaterThan:
    case spv::OpUGreaterThanEqual:
    case spv::OpSGreaterThanEqual:
    case spv::OpULessThan:
    case spv::OpSLessThan:
    case spv::OpULessThanEqual:
    case spv::OpSLessThanEqual:
    case spv::OpFOrdEqual:
    case spv::OpFUnordEqual:
    case spv::OpFOrdNotEqual:
    case spv::OpFUnordNotEqual:
    case spv::OpFOrdLessThan:
    case spv::OpFUnordLessThan:
    case spv::OpFOrdGreaterThan:
    case spv::OpFUnordGreaterThan:
    case spv::OpFOrdLessThanEqual:
    case spv::OpFUnordLessThanEqual:
    case spv::OpFOrdGreaterThanEqual:
    case spv::OpFUnordGreaterThanEqual:
    case spv::OpShiftRightLogical:
    case spv::OpShiftRightArithmetic:
    case spv::OpShiftLeftLogical:
    case spv::OpBitwiseOr:
    case spv::OpBitwiseXor:
    case spv::OpBitwiseAnd:
    case spv::OpNot:
    case spv::OpBitFieldInsert:
    case spv::OpBitFieldSExtract:
    case spv::OpBitFieldUExtract:
    case spv::OpBitReverse:
    case spv::OpBitCount:
    case spv::OpDPdx:
    case spv::OpDPdy:
    case spv::OpFwidth:
    case spv::OpDPdxFine:
    case spv::OpDPdyFine:
    case spv::OpFwidthFine:
    case spv::OpDPdxCoarse:
    case spv::OpDPdyCoarse:
    case spv::OpFwidthCoarse:
    case spv::OpAccessChain:
    case spv::OpInBoundsAccessChain:
    case spv::OpPtrAccessChain:
    case spv::OpInBoundsPtrAccessChain:
    case spv::OpArrayLength:
    case spv::OpPhi:
        return true;
    case spv::OpLoad:
        return instruction.size() == 4u || (instruction.size() == 5u && instruction[4] == spv::MemoryAccessMaskNone);
    default:
        return false;
    }
}

template<typename TVisitor>
bool VisitInputs(const Instruction& instruction, const TVisitor& visit) {
    std::size_t first = 3u;
    std::size_t end = instruction.size();
    switch (Opcode(instruction)) {
    case spv::OpVectorShuffle:
    case spv::OpCompositeInsert:
        end = 5u;
        break;
    case spv::OpCompositeExtract:
    case spv::OpGenericCastToPtrExplicit:
    case spv::OpArrayLength:
        end = 4u;
        break;
    case spv::OpLoad:
        if (instruction.size() > 5u) return false;
        end = 4u;
        break;
    case spv::OpStore:
        if (instruction.size() > 4u) return false;
        first = 1u;
        end = 3u;
        break;
    case spv::OpBranch:
    case spv::OpReturnValue:
        first = 1u;
        end = 2u;
        break;
    case spv::OpBranchConditional:
        first = 1u;
        end = 4u;
        break;
    case spv::OpSelectionMerge:
        first = 1u;
        end = 2u;
        break;
    case spv::OpLoopMerge:
        first = 1u;
        end = 3u;
        break;
    case spv::OpFunctionCall:
        break;
    case spv::OpReturn:
    case spv::OpUnreachable:
    case spv::OpKill:
    case spv::OpLabel:
    case spv::OpFunction:
    case spv::OpFunctionEnd:
    case spv::OpFunctionParameter:
    case spv::OpLine:
    case spv::OpNoLine:
        return true;
    default:
        if (!PureInstruction(instruction)) return false;
        break;
    }
    if (end > instruction.size()) throw std::runtime_error("truncated prepared instruction operands");
    for (auto index = first; index < end; ++index) visit(index);
    return true;
}

template<typename TVisitor>
void VisitIdOperands(const Instruction& instruction, std::uint32_t switchLiteralWords, const TVisitor& visit) {
    const auto size = instruction.size();
    const auto from = [&](std::size_t first) {
        for (auto index = first; index < size; ++index) visit(index);
    };
    const auto only = [&](std::initializer_list<std::size_t> indices) {
        for (const auto index : indices) {
            if (index < size) visit(index);
        }
    };
    constexpr std::uint32_t knownMemoryAccess = static_cast<std::uint32_t>(spv::MemoryAccessVolatileMask) | static_cast<std::uint32_t>(spv::MemoryAccessAlignedMask) |
        static_cast<std::uint32_t>(spv::MemoryAccessNontemporalMask) | static_cast<std::uint32_t>(spv::MemoryAccessMakePointerAvailableMask) |
        static_cast<std::uint32_t>(spv::MemoryAccessMakePointerVisibleMask) | static_cast<std::uint32_t>(spv::MemoryAccessNonPrivatePointerMask);
    const auto memoryOperands = [&](std::size_t index) {
        while (index < size) {
            const auto mask = instruction[index++];
            if ((mask & ~knownMemoryAccess) != 0u) {
                from(index);
                return;
            }
            if ((mask & static_cast<std::uint32_t>(spv::MemoryAccessAlignedMask)) != 0u) ++index;
            if ((mask & static_cast<std::uint32_t>(spv::MemoryAccessMakePointerAvailableMask)) != 0u && index < size) visit(index++);
            if ((mask & static_cast<std::uint32_t>(spv::MemoryAccessMakePointerVisibleMask)) != 0u && index < size) visit(index++);
        }
    };
    switch (Opcode(instruction)) {
    case spv::OpNop:
    case spv::OpCapability:
    case spv::OpExtension:
    case spv::OpExtInstImport:
    case spv::OpMemoryModel:
    case spv::OpSource:
    case spv::OpSourceContinued:
    case spv::OpSourceExtension:
    case spv::OpString:
    case spv::OpName:
    case spv::OpMemberName:
    case spv::OpModuleProcessed:
    case spv::OpDecorate:
    case spv::OpMemberDecorate:
    case spv::OpDecorateString:
    case spv::OpMemberDecorateString:
    case spv::OpTypeVoid:
    case spv::OpTypeBool:
    case spv::OpTypeInt:
    case spv::OpTypeFloat:
    case spv::OpTypeSampler:
    case spv::OpConstantTrue:
    case spv::OpConstantFalse:
    case spv::OpConstant:
    case spv::OpConstantSampler:
    case spv::OpConstantNull:
    case spv::OpSpecConstantTrue:
    case spv::OpSpecConstantFalse:
    case spv::OpSpecConstant:
    case spv::OpLabel:
    case spv::OpNoLine:
    case spv::OpReturn:
    case spv::OpUnreachable:
    case spv::OpKill:
    case spv::OpFunctionEnd:
        return;
    case spv::OpTypeVector:
    case spv::OpTypeMatrix:
    case spv::OpTypeImage:
        only({2});
        return;
    case spv::OpTypePointer:
        only({3});
        return;
    case spv::OpTypeForwardPointer:
    case spv::OpExecutionMode:
    case spv::OpLine:
    case spv::OpSelectionMerge:
        only({1});
        return;
    case spv::OpSpecConstantOp:
        only({1});
        from(4);
        return;
    case spv::OpVariable:
    case spv::OpFunction:
        only({1, 4});
        return;
    case spv::OpLoad:
        only({1, 3});
        memoryOperands(4);
        return;
    case spv::OpStore:
    case spv::OpCopyMemory:
        only({1, 2});
        memoryOperands(3);
        return;
    case spv::OpCopyMemorySized:
        only({1, 2, 3});
        memoryOperands(4);
        return;
    case spv::OpArrayLength:
    case spv::OpCompositeExtract:
        only({1, 3});
        return;
    case spv::OpCompositeInsert:
    case spv::OpVectorShuffle:
        only({1, 3, 4});
        return;
    case spv::OpExtInst:
        only({1, 3});
        from(5);
        return;
    case spv::OpEntryPoint: {
        only({2});
        std::size_t index = 3;
        while (index < size) {
            const auto word = instruction[index++];
            if ((word & 0xffu) == 0u || (word & 0xff00u) == 0u || (word & 0xff0000u) == 0u || (word & 0xff000000u) == 0u) break;
        }
        from(index);
        return;
    }
    case spv::OpExecutionModeId:
    case spv::OpDecorateId:
        only({1});
        from(3);
        return;
    case spv::OpLoopMerge:
        only({1, 2});
        return;
    case spv::OpBranchConditional:
        only({1, 2, 3});
        return;
    case spv::OpSwitch:
        only({1, 2});
        for (std::size_t index = 3; index + switchLiteralWords < size; index += switchLiteralWords + 1u) visit(index + switchLiteralWords);
        return;
    case spv::OpImageSampleImplicitLod:
    case spv::OpImageSampleExplicitLod:
    case spv::OpImageSampleProjImplicitLod:
    case spv::OpImageSampleProjExplicitLod:
    case spv::OpImageFetch:
    case spv::OpImageRead:
    case spv::OpImageSparseSampleImplicitLod:
    case spv::OpImageSparseSampleExplicitLod:
    case spv::OpImageSparseSampleProjImplicitLod:
    case spv::OpImageSparseSampleProjExplicitLod:
    case spv::OpImageSparseFetch:
    case spv::OpImageSparseRead:
        only({1, 3, 4});
        from(6);
        return;
    case spv::OpImageSampleDrefImplicitLod:
    case spv::OpImageSampleDrefExplicitLod:
    case spv::OpImageSampleProjDrefImplicitLod:
    case spv::OpImageSampleProjDrefExplicitLod:
    case spv::OpImageGather:
    case spv::OpImageDrefGather:
    case spv::OpImageSparseSampleDrefImplicitLod:
    case spv::OpImageSparseSampleDrefExplicitLod:
    case spv::OpImageSparseSampleProjDrefImplicitLod:
    case spv::OpImageSparseSampleProjDrefExplicitLod:
    case spv::OpImageSparseGather:
    case spv::OpImageSparseDrefGather:
        only({1, 3, 4, 5});
        from(7);
        return;
    case spv::OpImageWrite:
        only({1, 2, 3});
        from(5);
        return;
    case spv::OpGroupIAdd:
    case spv::OpGroupFAdd:
    case spv::OpGroupFMin:
    case spv::OpGroupUMin:
    case spv::OpGroupSMin:
    case spv::OpGroupFMax:
    case spv::OpGroupUMax:
    case spv::OpGroupSMax:
    case spv::OpGroupNonUniformBallotBitCount:
    case spv::OpGroupNonUniformIAdd:
    case spv::OpGroupNonUniformFAdd:
    case spv::OpGroupNonUniformIMul:
    case spv::OpGroupNonUniformFMul:
    case spv::OpGroupNonUniformSMin:
    case spv::OpGroupNonUniformUMin:
    case spv::OpGroupNonUniformFMin:
    case spv::OpGroupNonUniformSMax:
    case spv::OpGroupNonUniformUMax:
    case spv::OpGroupNonUniformFMax:
    case spv::OpGroupNonUniformBitwiseAnd:
    case spv::OpGroupNonUniformBitwiseOr:
    case spv::OpGroupNonUniformBitwiseXor:
    case spv::OpGroupNonUniformLogicalAnd:
    case spv::OpGroupNonUniformLogicalOr:
    case spv::OpGroupNonUniformLogicalXor:
        only({1, 3});
        from(5);
        return;
    default:
        from(1);
        return;
    }
}

struct ScalarType {
    std::uint32_t width;
    bool boolean;
};

template <typename T>
class IdTable {
public:
    void Reset(std::size_t bound) {
        items.assign(bound, T{});
        present.assign(bound, 0u);
        count = 0;
    }
    bool contains(std::uint32_t id) const { return id < present.size() && present[id] != 0u; }
    const T* find(std::uint32_t id) const { return contains(id) ? &items[id] : nullptr; }
    const T& at(std::uint32_t id) const {
        if (!contains(id)) throw std::out_of_range("prepared SPIR-V id has no entry");
        return items[id];
    }
    T& at(std::uint32_t id) {
        if (!contains(id)) throw std::out_of_range("prepared SPIR-V id has no entry");
        return items[id];
    }
    bool emplace(std::uint32_t id, T value) {
        check(id);
        if (present[id] != 0u) return false;
        items[id] = std::move(value);
        present[id] = 1u;
        ++count;
        return true;
    }
    T& operator[](std::uint32_t id) {
        check(id);
        if (present[id] == 0u) {
            items[id] = T{};
            present[id] = 1u;
            ++count;
        }
        return items[id];
    }
    void erase(std::uint32_t id) {
        if (!contains(id)) return;
        present[id] = 0u;
        items[id] = T{};
        --count;
    }
    std::size_t size() const { return count; }
    bool empty() const { return count == 0; }
    class Iterator {
    public:
        Iterator(IdTable& table, std::uint32_t id) : table(table), id(id) { skip(); }
        std::pair<std::uint32_t, T&> operator*() const { return {id, table.items[id]}; }
        Iterator& operator++() {
            ++id;
            skip();
            return *this;
        }
        bool operator!=(const Iterator& other) const { return id != other.id; }

    private:
        void skip() {
            while (id < table.present.size() && table.present[id] == 0u) ++id;
        }
        IdTable& table;
        std::uint32_t id;
    };
    Iterator begin() { return Iterator(*this, 0u); }
    Iterator end() { return Iterator(*this, static_cast<std::uint32_t>(present.size())); }

private:
    void check(std::uint32_t id) const {
        if (id >= present.size()) throw std::runtime_error("prepared SPIR-V id exceeds the module bound");
    }
    std::vector<T> items;
    std::vector<std::uint8_t> present;
    std::size_t count = 0;
};

class IdSet {
public:
    void Reset(std::size_t bound) { present.assign(bound, 0u); }
    bool contains(std::uint32_t id) const { return id < present.size() && present[id] != 0u; }
    bool insert(std::uint32_t id) {
        if (id >= present.size()) throw std::runtime_error("prepared SPIR-V id exceeds the module bound");
        if (present[id] != 0u) return false;
        present[id] = 1u;
        return true;
    }

private:
    std::vector<std::uint8_t> present;
};

class Specialization {
public:
    explicit Specialization(std::span<const std::uint32_t> words) : arena(words.size()) {
        if (words.size() < 5u || words[0] != spv::MagicNumber) throw std::runtime_error("invalid prepared SPIR-V header");
        header.assign(words.begin(), words.begin() + 5);
        bound = header[3];
        types.Reset(bound);
        values.Reset(bound);
        resultTypes.Reset(bound);
        vectorTypes.Reset(bound);
        removed.Reset(bound);
        aliases.Reset(bound);
        composites.Reset(bound);
        privateTables.Reset(bound);
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            if (count == 0u || count > words.size() - cursor) throw std::runtime_error("truncated prepared SPIR-V instruction");
            instructions.emplace_back(words.begin() + cursor, words.begin() + cursor + count, ArenaAllocator<std::uint32_t>(&arena));
            const auto& instruction = instructions.back();
            const auto op = Opcode(instruction);
            bool hasResult = false;
            bool hasType = false;
            spv::HasResultAndType(op, &hasResult, &hasType);
            if (hasResult && hasType) resultTypes.emplace(instruction.at(2), instruction.at(1));
            if (op == spv::OpTypeBool) types.emplace(instruction.at(1), ScalarType{1u, true});
            if (op == spv::OpTypeInt) types.emplace(instruction.at(1), ScalarType{instruction.at(2), false});
            if (op == spv::OpTypeVector) vectorTypes.insert(instruction.at(1));
            if (op == spv::OpConstant && types.contains(instruction.at(1)) && types.at(instruction[1]).width <= 32u) values.emplace(instruction.at(2), instruction.at(3));
            if (op == spv::OpConstantTrue || op == spv::OpConstantFalse) values.emplace(instruction.at(2), op == spv::OpConstantTrue ? 1u : 0u);
            if ((op == spv::OpConstant || op == spv::OpConstantTrue || op == spv::OpConstantFalse) && values.contains(instruction.at(2))) canonical.emplace(std::pair{instruction.at(1), values.at(instruction[2])}, instruction[2]);
            if (op == spv::OpConstantComposite && instruction.size() >= 3u) composites.emplace(instruction.at(2), std::vector<std::uint32_t>(instruction.begin() + 3, instruction.end()));
            if (op == spv::OpVariable && instruction.size() == 5u && (instruction[3] == spv::StorageClassPrivate || instruction[3] == spv::StorageClassFunction) && composites.contains(instruction[4])) privateTables.emplace(instruction.at(2), instruction[4]);
            cursor += count;
        }
        IdTable<std::uint32_t> tablePointers;
        tablePointers.Reset(bound);
        for (const auto& instruction : instructions) {
            const auto op = Opcode(instruction);
            if ((op == spv::OpAccessChain || op == spv::OpInBoundsAccessChain) && instruction.size() == 5u && privateTables.contains(instruction[3])) {
                tablePointers.emplace(instruction[2], instruction[3]);
                continue;
            }
            if (op == spv::OpLoad && instruction.size() == 4u && tablePointers.contains(instruction[3])) continue;
            if (op == spv::OpVariable || op == spv::OpEntryPoint) continue;
            std::uint32_t switchLiteralWords = 1u;
            if (op == spv::OpSwitch && instruction.size() > 1u) {
                const auto* selectorType = resultTypes.find(instruction[1]);
                if (selectorType != nullptr && types.contains(*selectorType) && types.at(*selectorType).width == 64u) switchLiteralWords = 2u;
            }
            VisitIdOperands(instruction, switchLiteralWords, [&](std::size_t index) {
                privateTables.erase(instruction[index]);
                if (const auto* table = tablePointers.find(instruction[index])) privateTables.erase(*table);
            });
        }
    }

    std::vector<std::uint32_t> Run() {
        bool changed = true;
        while (changed) {
            changed = fold();
            changed |= prune();
            changed |= propagateCopies();
        }
        shareConstants();
        removeDeadComputations();
        removeUnreachableBlocks();
        removeUnusedConstants();
        orderPhis();
        std::vector<std::uint32_t> words = header;
        bool inserted = false;
        for (const auto& instruction : instructions) {
            if (instruction.empty()) continue;
            if (!inserted && Opcode(instruction) == spv::OpFunction) {
                for (const auto& constant : constants) words.insert(words.end(), constant.begin(), constant.end());
                inserted = true;
            }
            if ((Opcode(instruction) == spv::OpName || Opcode(instruction) == spv::OpDecorate) && removed.contains(instruction.at(1))) continue;
            words.insert(words.end(), instruction.begin(), instruction.end());
        }
        return words;
    }

private:
    std::optional<std::uint32_t> value(std::uint32_t id) const {
        const auto* found = values.find(id);
        return found != nullptr ? std::optional(*found) : std::nullopt;
    }

    bool sameType(std::uint32_t id, std::uint32_t type) const {
        const auto* found = resultTypes.find(id);
        return found != nullptr && *found == type;
    }

    std::optional<std::uint32_t> evaluate(const Instruction& instruction, const IdTable<std::pair<std::uint32_t, std::uint32_t>>& constPointers) const {
        const auto op = Opcode(instruction);
        if (instruction.size() < 4u) return std::nullopt;
        const auto* type = types.find(instruction[1]);
        if (type == nullptr || type->width > 32u) return std::nullopt;
        if (op == spv::OpLoad && instruction.size() == 4u) {
            const auto* pointer = constPointers.find(instruction[3]);
            if (pointer == nullptr) return std::nullopt;
            const auto& elements = composites.at(pointer->first);
            return pointer->second < elements.size() ? value(elements[pointer->second]) : std::nullopt;
        }
        const auto left = value(instruction[3]);
        if (!left) return std::nullopt;
        if (op == spv::OpCopyObject) return left;
        if (op == spv::OpLogicalNot) return *left == 0u;
        if (op == spv::OpNot) return ~*left;
        if (instruction.size() < 5u) return std::nullopt;
        const auto right = value(instruction[4]);
        if (!right) return std::nullopt;
        switch (op) {
        case spv::OpIAdd: return *left + *right;
        case spv::OpISub: return *left - *right;
        case spv::OpIMul: return *left * *right;
        case spv::OpUDiv: return *right != 0u ? std::optional(*left / *right) : std::nullopt;
        case spv::OpUMod: return *right != 0u ? std::optional(*left % *right) : std::nullopt;
        case spv::OpBitwiseAnd: return *left & *right;
        case spv::OpBitwiseOr: return *left | *right;
        case spv::OpBitwiseXor: return *left ^ *right;
        case spv::OpShiftRightLogical: return *right < type->width ? std::optional(*left >> *right) : std::nullopt;
        case spv::OpShiftLeftLogical: return *right < type->width ? std::optional(*left << *right) : std::nullopt;
        case spv::OpIEqual: return *left == *right;
        case spv::OpINotEqual: return *left != *right;
        case spv::OpULessThan: return *left < *right;
        case spv::OpULessThanEqual: return *left <= *right;
        case spv::OpUGreaterThan: return *left > *right;
        case spv::OpUGreaterThanEqual: return *left >= *right;
        case spv::OpLogicalEqual: return (*left != 0u) == (*right != 0u);
        case spv::OpLogicalNotEqual: return (*left != 0u) != (*right != 0u);
        case spv::OpLogicalAnd: return *left != 0u && *right != 0u;
        case spv::OpLogicalOr: return *left != 0u || *right != 0u;
        case spv::OpBitFieldUExtract: {
            if (instruction.size() != 6u) throw std::runtime_error("invalid prepared bitfield instruction");
            const auto count = value(instruction[5]);
            if (!count || *right > 32u || *count > 32u - *right) return std::nullopt;
            if (*count == 0u) return 0u;
            const auto mask = *count == 32u ? UINT32_MAX : (1u << *count) - 1u;
            return (*left >> *right) & mask;
        }
        default: return std::nullopt;
        }
    }

    bool fold() {
        bool changed = false;
        bool function = false;
        IdTable<std::pair<std::uint32_t, std::uint32_t>> extracts;
        extracts.Reset(bound);
        IdTable<std::uint32_t> bitcasts;
        bitcasts.Reset(bound);
        IdTable<std::pair<std::uint32_t, std::uint32_t>> constPointers;
        constPointers.Reset(bound);
        for (auto& instruction : instructions) {
            if (instruction.empty()) continue;
            auto op = Opcode(instruction);
            if (op == spv::OpFunction) function = true;
            if (op == spv::OpFunctionEnd) function = false;
            if (!function) continue;
            if ((op == spv::OpAccessChain || op == spv::OpInBoundsAccessChain) && instruction.size() == 5u && privateTables.contains(instruction[3])) {
                if (const auto index = value(instruction[4])) constPointers.emplace(instruction[2], std::pair{privateTables.at(instruction[3]), *index});
            }
            if (op == spv::OpSelect && instruction.size() == 6u) {
                if (const auto condition = value(instruction[3])) {
                    instruction = Make(spv::OpCopyObject, {instruction[1], instruction[2], instruction[*condition != 0u ? 4u : 5u]});
                    changed = true;
                } else if (instruction[4] == instruction[5]) {
                    instruction = Make(spv::OpCopyObject, {instruction[1], instruction[2], instruction[4]});
                    changed = true;
                }
            }
            if (op == spv::OpLogicalAnd && instruction.size() == 5u) {
                if (const auto left = value(instruction[3]); left && *left != 0u) {
                    instruction = Make(spv::OpCopyObject, {instruction[1], instruction[2], instruction[4]});
                    changed = true;
                } else if (const auto right = value(instruction[4]); right && *right != 0u) {
                    instruction = Make(spv::OpCopyObject, {instruction[1], instruction[2], instruction[3]});
                    changed = true;
                }
            }
            if (op == spv::OpLogicalOr && instruction.size() == 5u) {
                if (const auto left = value(instruction[3]); left && *left == 0u) {
                    instruction = Make(spv::OpCopyObject, {instruction[1], instruction[2], instruction[4]});
                    changed = true;
                } else if (const auto right = value(instruction[4]); right && *right == 0u) {
                    instruction = Make(spv::OpCopyObject, {instruction[1], instruction[2], instruction[3]});
                    changed = true;
                }
            }
            if (op == spv::OpVectorExtractDynamic && instruction.size() == 5u) {
                if (const auto index = value(instruction[4])) {
                    instruction = Make(spv::OpCompositeExtract, {instruction[1], instruction[2], instruction[3], *index});
                    changed = true;
                }
            }
            if (op == spv::OpBitcast && instruction.size() == 4u) {
                const auto* chained = bitcasts.find(instruction[3]);
                const auto source = chained != nullptr && sameType(*chained, instruction[1]) ? *chained : instruction[3];
                if (sameType(source, instruction[1])) {
                    instruction = Make(spv::OpCopyObject, {instruction[1], instruction[2], source});
                    changed = true;
                } else {
                    bitcasts.emplace(instruction[2], instruction[3]);
                }
            }
            op = Opcode(instruction);
            if (op == spv::OpCompositeExtract && instruction.size() == 5u) extracts.emplace(instruction[2], std::pair{instruction[3], instruction[4]});
            if (op == spv::OpCompositeConstruct && instruction.size() == 7u) {
                const auto* first = extracts.find(instruction[3]);
                bool shuffle = first != nullptr && vectorTypes.contains(instruction[1]) && resultTypes.contains(first->first) && vectorTypes.contains(resultTypes.at(first->first));
                for (std::size_t index = 4; shuffle && index < instruction.size(); ++index) {
                    const auto* found = extracts.find(instruction[index]);
                    shuffle = found != nullptr && found->first == first->first;
                }
                if (shuffle) {
                    const auto source = first->first;
                    bool identity = resultTypes.contains(source) && resultTypes.at(source) == instruction[1];
                    Instruction replacement = Make(spv::OpVectorShuffle, {instruction[1], instruction[2], source, source});
                    for (std::size_t index = 3; index < instruction.size(); ++index) {
                        const auto channel = extracts.at(instruction[index]).second;
                        replacement.push_back(channel);
                        identity &= channel == index - 3u;
                    }
                    replacement[0] = (static_cast<std::uint32_t>(replacement.size()) << 16u) | spv::OpVectorShuffle;
                    instruction = identity ? Make(spv::OpCopyObject, {instruction[1], instruction[2], source}) : std::move(replacement);
                    changed = true;
                }
            }
            if (op == spv::OpCopyObject && values.contains(instruction.at(2))) continue;
            const auto result = evaluate(instruction, constPointers);
            if (!result) continue;
            const auto type = types.at(instruction[1]);
            const auto bits = type.width == 32u ? *result : *result & ((1u << type.width) - 1u);
            values[instruction[2]] = bits;
            const auto [existing, created] = canonical.emplace(std::pair{instruction[1], bits}, instruction[2]);
            if (!created) aliases.emplace(instruction[2], existing->second);
            constants.push_back(type.boolean ? Make(bits != 0u ? spv::OpConstantTrue : spv::OpConstantFalse, {instruction[1], instruction[2]}) : Make(spv::OpConstant, {instruction[1], instruction[2], bits}));
            instruction.clear();
            changed = true;
        }
        return changed;
    }

    bool prune() {
        IdTable<std::vector<std::size_t>> blocks;
        blocks.Reset(bound);
        std::vector<std::uint32_t> entries;
        std::uint32_t block = 0;
        bool entry = false;
        for (std::size_t index = 0; index < instructions.size(); ++index) {
            const auto& instruction = instructions[index];
            if (instruction.empty()) continue;
            const auto op = Opcode(instruction);
            if (op == spv::OpFunction) entry = true;
            if (op == spv::OpFunctionEnd) block = 0;
            if (op == spv::OpLabel) {
                block = instruction.at(1);
                if (entry) entries.push_back(block);
                entry = false;
            }
            if (block != 0u) blocks[block].push_back(index);
        }
        bool changed = false;
        const auto linearSelection = [&](std::uint32_t target, std::uint32_t merge) {
            std::set<std::uint32_t> visited;
            while (target != merge) {
                if (!visited.insert(target).second) return false;
                const auto& indices = blocks.at(target);
                for (const auto index : indices) {
                    const auto op = Opcode(instructions[index]);
                    if (op == spv::OpSelectionMerge || op == spv::OpLoopMerge) return false;
                }
                const auto& terminal = instructions[indices.back()];
                const auto op = Opcode(terminal);
                if (op == spv::OpBranchConditional || op == spv::OpSwitch) return false;
                if (op != spv::OpBranch) return true;
                target = terminal.at(1);
            }
            return true;
        };
        IdSet loopHeaders;
        loopHeaders.Reset(bound);
        std::map<std::uint32_t, std::uint32_t> continueHeaders;
        for (const auto& [label, indices] : blocks) {
            for (const auto index : indices) {
                if (Opcode(instructions[index]) != spv::OpLoopMerge) continue;
                loopHeaders.insert(label);
                continueHeaders.emplace(instructions[index].at(2), label);
            }
        }
        IdTable<std::vector<std::uint32_t>> edges;
        edges.Reset(bound);
        for (const auto& [label, indices] : blocks) {
            auto& terminal = instructions[indices.back()];
            auto op = Opcode(terminal);
            std::uint32_t target = 0;
            bool loop = false;
            for (const auto index : indices) loop |= Opcode(instructions[index]) == spv::OpLoopMerge;
            if (!loop && op == spv::OpBranchConditional) {
                if (const auto condition = value(terminal.at(1)); condition.has_value() && !loopHeaders.contains(terminal.at(*condition != 0u ? 3u : 2u))) target = terminal.at(*condition != 0u ? 2u : 3u);
            } else if (!loop && op == spv::OpSwitch) {
                if (const auto selector = value(terminal.at(1))) {
                    target = terminal.at(2);
                    if ((terminal.size() - 3u) % 2u != 0u) throw std::runtime_error("invalid prepared 32-bit switch");
                    for (std::size_t index = 3; index < terminal.size(); index += 2u) if (terminal[index] == *selector) target = terminal[index + 1u];
                }
            }
            if (target != 0u) {
                bool retainMerge = false;
                for (const auto index : indices) {
                    auto& instruction = instructions[index];
                    if (Opcode(instruction) != spv::OpSelectionMerge) continue;
                    retainMerge = !linearSelection(target, instruction.at(1));
                    if (!retainMerge) instruction.clear();
                }
                const auto replacement = !retainMerge ? Make(spv::OpBranch, {target}) : op == spv::OpSwitch ? Make(spv::OpSwitch, {terminal.at(1), target}) :
                    header.at(1) >= 0x00010600u ? terminal : Make(spv::OpBranchConditional, {terminal.at(1), target, target});
                if (terminal != replacement) {
                    terminal = replacement;
                    changed = true;
                }
                op = Opcode(terminal);
            }
            auto& successors = edges[label];
            if (op == spv::OpBranch) successors.push_back(terminal.at(1));
            else if (op == spv::OpBranchConditional) successors = {terminal.at(2), terminal.at(3)};
            else if (op == spv::OpSwitch) {
                successors.push_back(terminal.at(2));
                const auto* selectorType = resultTypes.find(terminal.at(1));
                const auto stride = selectorType != nullptr && types.contains(*selectorType) && types.at(*selectorType).width == 64u ? 3u : 2u;
                for (std::size_t index = 3; index + stride <= terminal.size(); index += stride) successors.push_back(terminal[index + stride - 1u]);
            }
        }
        IdSet live;
        live.Reset(bound);
        auto pending = entries;
        while (!pending.empty()) {
            const auto label = pending.back();
            pending.pop_back();
            if (!blocks.contains(label)) throw std::runtime_error("prepared branch references a missing block");
            if (!live.insert(label)) continue;
            const auto& successors = edges.at(label);
            pending.insert(pending.end(), successors.begin(), successors.end());
        }
        const auto backEdge = [&](std::uint32_t label) {
            const auto header = continueHeaders.find(label);
            return !live.contains(label) && header != continueHeaders.end() && live.contains(header->second) ? header->second : 0u;
        };
        for (const auto& [label, indices] : blocks) {
            if (const auto header = backEdge(label)) {
                const auto branch = Make(spv::OpBranch, {header});
                if (indices.size() == 2u && instructions[indices.back()] == branch) continue;
                for (std::size_t index = 1; index < indices.size(); ++index) {
                    auto& instruction = instructions[indices[index]];
                    if (const auto result = Result(instruction)) removed.insert(result);
                    instruction.clear();
                }
                instructions[indices.back()] = branch;
                changed = true;
                continue;
            }
            if (!live.contains(label)) {
                if (indices.size() == 2u && Opcode(instructions[indices.back()]) == spv::OpUnreachable) continue;
                for (std::size_t index = 1; index < indices.size(); ++index) {
                    auto& instruction = instructions[indices[index]];
                    if (const auto result = Result(instruction)) removed.insert(result);
                    instruction.clear();
                }
                instructions[indices.back()] = Make(spv::OpUnreachable, {});
                changed = true;
                continue;
            }
            for (const auto index : indices) {
                auto& instruction = instructions[index];
                if (instruction.empty() || Opcode(instruction) != spv::OpPhi) continue;
                Instruction phi(instruction.begin(), instruction.begin() + 3);
                for (std::size_t operand = 3; operand + 1u < instruction.size(); operand += 2u) {
                    const auto parent = instruction[operand + 1u];
                    if (live.contains(parent) && std::ranges::find(edges.at(parent), label) != edges.at(parent).end()) phi.insert(phi.end(), {instruction[operand], parent});
                    else if (backEdge(parent) == label) phi.insert(phi.end(), {instruction[2], parent});
                }
                if (phi.size() == 3u) throw std::runtime_error("reachable prepared phi has no predecessor");
                if (phi == instruction && phi.size() != 5u) continue;
                phi[0] = (static_cast<std::uint32_t>(phi.size()) << 16u) | spv::OpPhi;
                instruction = phi.size() == 5u ? Make(spv::OpCopyObject, {phi[1], phi[2], phi[3]}) : std::move(phi);
                changed = true;
            }
        }
        return changed;
    }

    IdSet referencedIds() const {
        IdSet referenced;
        referenced.Reset(bound);
        for (const auto& instruction : instructions) {
            if (instruction.empty()) continue;
            std::uint32_t switchLiteralWords = 1u;
            if (Opcode(instruction) == spv::OpSwitch && instruction.size() > 1u) {
                const auto* selectorType = resultTypes.find(instruction[1]);
                if (selectorType != nullptr && types.contains(*selectorType) && types.at(*selectorType).width == 64u) switchLiteralWords = 2u;
            }
            VisitIdOperands(instruction, switchLiteralWords, [&](std::size_t index) { referenced.insert(instruction[index]); });
        }
        return referenced;
    }

    void removeUnreachableBlocks() {
        const auto referenced = referencedIds();
        const auto next = [&](std::size_t index) {
            while (++index < instructions.size() && instructions[index].empty()) {}
            return index;
        };
        std::size_t previous = instructions.size();
        for (std::size_t index = 0; index < instructions.size(); ++index) {
            if (instructions[index].empty()) continue;
            const auto op = Opcode(instructions[index]);
            const auto following = next(index);
            const bool entry = previous < instructions.size() && (Opcode(instructions[previous]) == spv::OpFunction || Opcode(instructions[previous]) == spv::OpFunctionParameter);
            if (op == spv::OpLabel && !entry && following < instructions.size() && Opcode(instructions[following]) == spv::OpUnreachable && !referenced.contains(instructions[index].at(1))) {
                removed.insert(instructions[index].at(1));
                instructions[index].clear();
                instructions[following].clear();
                index = following;
                continue;
            }
            previous = index;
        }
    }

    void removeUnusedConstants() {
        const auto referenced = referencedIds();
        std::erase_if(constants, [&](const Instruction& constant) {
            const auto id = Result(constant);
            if (referenced.contains(id)) return false;
            removed.insert(id);
            return true;
        });
    }

    void orderPhis() {
        std::vector<Instruction> ordered;
        std::vector<Instruction> copies;
        bool prefix = false;
        for (auto& instruction : instructions) {
            if (instruction.empty()) continue;
            const auto op = Opcode(instruction);
            if (op == spv::OpLabel) prefix = true;
            else if (prefix && op == spv::OpCopyObject) {
                copies.push_back(std::move(instruction));
                continue;
            } else if (prefix && op != spv::OpPhi && op != spv::OpLine && op != spv::OpNoLine) {
                for (auto& copy : copies) ordered.push_back(std::move(copy));
                copies.clear();
                prefix = false;
            }
            ordered.push_back(std::move(instruction));
        }
        if (!copies.empty()) throw std::runtime_error("unterminated prepared phi block");
        instructions = std::move(ordered);
    }

    void shareConstants() {
        if (aliases.empty()) return;
        bool function = false;
        for (auto& instruction : instructions) {
            if (instruction.empty()) continue;
            const auto op = Opcode(instruction);
            if (op == spv::OpFunction) function = true;
            if (op == spv::OpFunctionEnd) function = false;
            if (!function) continue;
            VisitInputs(instruction, [&](std::size_t index) {
                const auto* found = aliases.find(instruction[index]);
                if (found != nullptr) instruction[index] = *found;
            });
        }
    }

    bool propagateCopies() {
        IdTable<std::uint32_t> copies;
        copies.Reset(bound);
        for (const auto& instruction : instructions) {
            if (Opcode(instruction) != spv::OpCopyObject) continue;
            if (instruction.size() != 4u) throw std::runtime_error("invalid prepared copy instruction");
            copies.emplace(instruction[2], instruction[3]);
        }
        if (copies.empty()) return false;
        for (const auto& instruction : instructions) {
            if (instruction.empty()) continue;
            const auto op = Opcode(instruction);
            if (op == spv::OpName || op == spv::OpMemberName) continue;
            if (VisitInputs(instruction, [](std::size_t) {})) continue;
            bool hasResult = false;
            bool hasType = false;
            spv::HasResultAndType(op, &hasResult, &hasType);
            const auto resultIndex = hasResult ? (hasType ? 2u : 1u) : 0u;
            for (std::size_t index = 1; index < instruction.size(); ++index) if (index != resultIndex) copies.erase(instruction[index]);
        }
        if (copies.empty()) return false;
        const auto resolve = [&](std::uint32_t id) {
            std::size_t count = 0;
            while (copies.contains(id)) {
                if (++count > copies.size()) throw std::runtime_error("cyclic prepared copy chain");
                id = copies.at(id);
            }
            return id;
        };
        bool function = false;
        for (auto& instruction : instructions) {
            if (instruction.empty()) continue;
            const auto op = Opcode(instruction);
            if (op == spv::OpFunction) function = true;
            if (op == spv::OpFunctionEnd) function = false;
            if (!function) continue;
            if (op == spv::OpCopyObject && copies.contains(instruction[2])) {
                removed.insert(instruction[2]);
                instruction.clear();
                continue;
            }
            VisitInputs(instruction, [&](std::size_t index) { instruction[index] = resolve(instruction[index]); });
        }
        return true;
    }

    void removeDeadComputations() {
        IdTable<std::size_t> definitions;
        definitions.Reset(bound);
        std::vector<std::uint32_t> pending;
        bool function = false;
        for (std::size_t index = 0; index < instructions.size(); ++index) {
            const auto& instruction = instructions[index];
            if (instruction.empty()) continue;
            const auto op = Opcode(instruction);
            if (op == spv::OpFunction) function = true;
            if (op == spv::OpFunctionEnd) function = false;
            if (!function) {
                if (op == spv::OpDecorateId || op == spv::OpGroupDecorate || op == spv::OpGroupMemberDecorate) {
                    for (std::size_t operand = 1; operand < instruction.size(); ++operand) pending.push_back(instruction[operand]);
                }
                continue;
            }
            if (PureInstruction(instruction)) {
                definitions.emplace(Result(instruction), index);
                continue;
            }
            if (VisitInputs(instruction, [&](std::size_t operand) { pending.push_back(instruction[operand]); })) continue;
            bool hasResult = false;
            bool hasType = false;
            spv::HasResultAndType(op, &hasResult, &hasType);
            const auto resultIndex = hasResult ? (hasType ? 2u : 1u) : 0u;
            for (std::size_t operand = 1; operand < instruction.size(); ++operand) if (operand != resultIndex) pending.push_back(instruction[operand]);
        }
        IdSet live;
        live.Reset(bound);
        while (!pending.empty()) {
            const auto id = pending.back();
            pending.pop_back();
            const auto* found = definitions.find(id);
            if (found == nullptr || !live.insert(id)) continue;
            const auto& instruction = instructions[*found];
            if (!VisitInputs(instruction, [&](std::size_t operand) { pending.push_back(instruction[operand]); })) throw std::runtime_error("missing prepared pure instruction operands");
        }
        for (const auto& [id, index] : definitions) {
            if (live.contains(id)) continue;
            removed.insert(id);
            instructions[index].clear();
        }
    }

    std::vector<std::uint32_t> header;
    WordArena arena;
    std::vector<Instruction> instructions;
    std::vector<Instruction> constants;
    std::uint32_t bound = 0;
    IdTable<ScalarType> types;
    IdTable<std::uint32_t> values;
    IdTable<std::uint32_t> resultTypes;
    IdSet vectorTypes;
    IdSet removed;
    IdTable<std::uint32_t> aliases;
    IdTable<std::vector<std::uint32_t>> composites;
    IdTable<std::uint32_t> privateTables;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> canonical;
};

}

std::vector<std::uint32_t> SpecializeSpirv(std::span<const std::uint32_t> words) {
    return Specialization(words).Run();
}

}
