#define SPV_ENABLE_UTILITY_CODE
#include <spirv/unified1/spirv.hpp>
#undef SPV_ENABLE_UTILITY_CODE
#include "SpirvBackend/SpirvSpecialization.hpp"
#include "SpirvInstructionOperands.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
namespace {

constexpr std::uint32_t kInvalidIndex = std::numeric_limits<std::uint32_t>::max();

constexpr std::uint8_t kFlagVectorType = 1u << 0u;
constexpr std::uint8_t kFlagRemoved = 1u << 1u;
constexpr std::uint8_t kFlagDecorated = 1u << 2u;
constexpr std::uint8_t kFlagHasValue = 1u << 3u;
constexpr std::uint8_t kFlagBoolType = 1u << 4u;
constexpr std::uint8_t kFlagReferenced = 1u << 5u;
constexpr std::uint8_t kFlagLive = 1u << 6u;
constexpr std::uint8_t kFlagScratchActive = 1u << 7u;

struct InstSlice {
    std::uint32_t offset = 0u;
    std::uint32_t count = 0u;

    bool empty() const { return count == 0u; }
    void clear() { count = 0u; }
};

struct ConstInstView {
    const std::uint32_t* ptr = nullptr;
    std::uint32_t len = 0u;

    bool empty() const { return len == 0u; }
    std::size_t size() const { return len; }
    std::uint32_t operator[](std::size_t index) const { return ptr[index]; }
    std::uint32_t at(std::size_t index) const {
        if (index >= len) throw std::out_of_range("prepared SPIR-V instruction operand out of range");
        return ptr[index];
    }
    const std::uint32_t* begin() const { return ptr; }
    const std::uint32_t* end() const { return ptr + len; }
};

struct InstView {
    std::uint32_t* ptr = nullptr;
    std::uint32_t len = 0u;

    bool empty() const { return len == 0u; }
    std::size_t size() const { return len; }
    std::uint32_t& operator[](std::size_t index) const { return ptr[index]; }
    std::uint32_t& at(std::size_t index) const {
        if (index >= len) throw std::out_of_range("prepared SPIR-V instruction operand out of range");
        return ptr[index];
    }
    operator ConstInstView() const { return {ptr, len}; }
};

using SpirvInstructions::Opcode;
using SpirvInstructions::Result;
using SpirvInstructions::VisitIdOperands;

bool PureInstruction(ConstInstView instruction) {
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

bool InputRange(ConstInstView instruction, std::size_t& first, std::size_t& end) {
    first = 3u;
    end = instruction.size();
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
        first = 0u;
        end = 0u;
        return true;
    default:
        if (!PureInstruction(instruction)) return false;
        break;
    }
    if (end > instruction.size()) throw std::runtime_error("truncated prepared instruction operands");
    return true;
}

template<typename TVisitor>
bool VisitInputs(ConstInstView instruction, const TVisitor& visit) {
    std::size_t first = 0u;
    std::size_t end = 0u;
    if (!InputRange(instruction, first, end)) return false;
    for (auto index = first; index < end; ++index) visit(index);
    return true;
}

template<typename TKey, typename THash>
class SmallBlockMap {
public:
    void clear() {
        small.clear();
        if (usingHash) {
            large.clear();
            usingHash = false;
        }
    }

    std::pair<std::uint32_t, bool> tryEmplace(const TKey& key, std::uint32_t value) {
        if (!usingHash) {
            for (const auto& [existingKey, existingValue] : small) {
                if (existingKey == key) return {existingValue, false};
            }
            if (small.size() < 24u) {
                small.emplace_back(key, value);
                return {value, true};
            }
            usingHash = true;
            large.reserve(64u);
            for (const auto& [existingKey, existingValue] : small) large.emplace(existingKey, existingValue);
            small.clear();
        }
        const auto [it, inserted] = large.try_emplace(key, value);
        return {it->second, inserted};
    }

private:
    std::vector<std::pair<TKey, std::uint32_t>> small;
    std::unordered_map<TKey, std::uint32_t, THash> large;
    bool usingHash = false;
};

struct PairKeyHash {
    std::size_t operator()(const std::pair<std::uint32_t, std::uint32_t>& key) const noexcept {
        std::uint64_t value = (static_cast<std::uint64_t>(key.first) << 32u) | key.second;
        value ^= value >> 33u;
        value *= 0xff51afd7ed558ccdULL;
        value ^= value >> 33u;
        return static_cast<std::size_t>(value);
    }
};

struct TripleKeyHash {
    std::size_t operator()(const std::array<std::uint32_t, 3>& key) const noexcept {
        std::uint64_t h = (static_cast<std::uint64_t>(key[0]) << 32u) | key[1];
        h ^= static_cast<std::uint64_t>(key[2]) * 0x9e3779b97f4a7c15ULL;
        h ^= h >> 33u;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33u;
        return static_cast<std::size_t>(h);
    }
};

std::uint64_t PackPair(std::uint32_t first, std::uint32_t second) {
    return (static_cast<std::uint64_t>(first) << 32u) | second;
}

class Specialization {
public:
    explicit Specialization(std::span<const std::uint32_t> words) {
        if (words.size() < 5u || words[0] != spv::MagicNumber) throw std::runtime_error("invalid prepared SPIR-V header");
        header.assign(words.begin(), words.begin() + 5);
        bound = header[3];
        wordPool.reserve(words.size() + words.size() / 4u + 64u);
        wordPool.assign(words.begin(), words.end());
        instructions.reserve(words.size() / 4u + 16u);
        idFlags.assign(bound, 0u);
        scalarWidth.assign(bound, 0u);
        values.assign(bound, 0u);
        resultTypes.assign(bound, 0u);
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            if (count == 0u || count > words.size() - cursor) throw std::runtime_error("truncated prepared SPIR-V instruction");
            const InstSlice slice{static_cast<std::uint32_t>(cursor), count};
            const auto instIdx = static_cast<std::uint32_t>(instructions.size());
            instructions.push_back(slice);
            const auto instruction = cview(slice);
            const auto op = Opcode(instruction);
            bool hasResult = false;
            bool hasType = false;
            spv::HasResultAndType(op, &hasResult, &hasType);
            if (hasResult && hasType) emplaceResultType(instruction.at(2), instruction.at(1));
            switch (op) {
            case spv::OpFunction:
                if (firstFunctionInst == kInvalidIndex) firstFunctionInst = instIdx;
                break;
            case spv::OpExecutionMode:
                if (instruction.size() >= 3u && instruction[2] == spv::ExecutionModeMaximallyReconvergesKHR) maximallyReconverges = true;
                break;
            case spv::OpDecorate:
            case spv::OpDecorateId:
            case spv::OpDecorateString:
                markFlag(instruction.at(1), kFlagDecorated);
                break;
            case spv::OpGroupDecorate:
                for (std::size_t index = 2; index < instruction.size(); ++index) markFlag(instruction[index], kFlagDecorated);
                break;
            case spv::OpTypeBool: {
                const auto id = instruction.at(1);
                checkBound(id);
                if (scalarWidth[id] == 0u) {
                    scalarWidth[id] = 1u;
                    idFlags[id] |= kFlagBoolType;
                }
                break;
            }
            case spv::OpTypeInt: {
                const auto id = instruction.at(1);
                const auto width = instruction.at(2);
                checkBound(id);
                if (scalarWidth[id] == 0u) {
                    scalarWidth[id] = static_cast<std::uint8_t>(width > 255u ? 255u : width);
                }
                break;
            }
            case spv::OpTypeVector:
                markFlag(instruction.at(1), kFlagVectorType);
                break;
            case spv::OpConstant: {
                const auto typeId = instruction.at(1);
                if (typeId < bound && scalarWidth[typeId] != 0u && scalarWidth[typeId] <= 32u) {
                    const auto resId = instruction.at(2);
                    const auto val = instruction.at(3);
                    emplaceValue(resId, val);
                    canonical.emplace(PackPair(typeId, values[resId]), resId);
                }
                break;
            }
            case spv::OpConstantTrue:
            case spv::OpConstantFalse: {
                const auto typeId = instruction.at(1);
                const auto resId = instruction.at(2);
                emplaceValue(resId, op == spv::OpConstantTrue ? 1u : 0u);
                canonical.emplace(PackPair(typeId, values[resId]), resId);
                break;
            }
            case spv::OpConstantComposite:
                if (instruction.size() >= 3u) composites.emplace(instruction.at(2), InstSlice{slice.offset + 3u, slice.count - 3u});
                break;
            case spv::OpVariable:
                if (instruction.size() == 5u && (instruction[3] == spv::StorageClassPrivate || instruction[3] == spv::StorageClassFunction) && composites.contains(instruction[4])) {
                    privateTables.emplace(instruction.at(2), instruction[4]);
                }
                break;
            case spv::OpCompositeConstruct:
                if (slice.count == 7u) hasCompositeConstruct7 = true;
                break;
            default:
                break;
            }
            cursor += count;
        }
        if (firstFunctionInst == kInvalidIndex) firstFunctionInst = static_cast<std::uint32_t>(instructions.size());
        if (!privateTables.empty()) {
            std::unordered_map<std::uint32_t, std::uint32_t> tablePointers;
            for (const auto slice : instructions) {
                const auto instruction = cview(slice);
                const auto op = Opcode(instruction);
                if ((op == spv::OpAccessChain || op == spv::OpInBoundsAccessChain) && instruction.size() == 5u && privateTables.contains(instruction[3])) {
                    tablePointers.emplace(instruction[2], instruction[3]);
                    continue;
                }
                if (op == spv::OpLoad && instruction.size() == 4u && tablePointers.contains(instruction[3])) continue;
                if (op == spv::OpVariable || op == spv::OpEntryPoint) continue;
                std::uint32_t switchLiteralWords = 1u;
                if (op == spv::OpSwitch && instruction.size() > 1u && is64BitSelector(instruction[1])) switchLiteralWords = 2u;
                VisitIdOperands(instruction, switchLiteralWords, [&](std::size_t index) {
                    privateTables.erase(instruction[index]);
                    const auto found = tablePointers.find(instruction[index]);
                    if (found != tablePointers.end()) privateTables.erase(found->second);
                });
            }
        }
    }

    std::vector<std::uint32_t> Run() {
        scratchId.assign(bound, kInvalidIndex);
        bool changed = true;
        while (changed) {
            changed = fold();
            changed |= prune();
            changed |= propagateCopies();
        }
        shareConstants();
        removeDeadComputations();
        markReferencedIds();
        removeUnreachableBlocks();
        removeUnusedConstants();
        orderPhis();
        std::vector<std::uint32_t> words;
        words.reserve(wordPool.size());
        words.insert(words.end(), header.begin(), header.end());
        bool inserted = false;
        for (const auto slice : instructions) {
            if (slice.empty()) continue;
            const auto instruction = cview(slice);
            const auto op = Opcode(instruction);
            if (!inserted && op == spv::OpFunction) {
                for (const auto constant : constants) {
                    const auto cv = cview(constant);
                    words.insert(words.end(), cv.begin(), cv.end());
                }
                inserted = true;
            }
            if ((op == spv::OpName || op == spv::OpDecorate) && hasFlag(instruction.at(1), kFlagRemoved)) continue;
            words.insert(words.end(), instruction.begin(), instruction.end());
        }
        return words;
    }

private:
    struct BlockInfo {
        std::uint32_t label = 0u;
        std::uint32_t firstInst = 0u;
        std::uint32_t endInst = 0u;
        std::uint32_t lastNonEmpty = 0u;
        std::uint32_t nonEmptyCount = 0u;
        std::uint32_t edgeOffset = 0u;
        std::uint32_t edgeCount = 0u;
        std::uint32_t continueHeader = 0u;
        bool isLoopHeader = false;
        bool hasSelectionMerge = false;
        bool hasPhi = false;
    };

    void checkBound(std::uint32_t id) const {
        if (id >= bound) throw std::runtime_error("prepared SPIR-V id exceeds the module bound");
    }

    bool hasFlag(std::uint32_t id, std::uint8_t flag) const {
        return id < bound && (idFlags[id] & flag) != 0u;
    }

    void markFlag(std::uint32_t id, std::uint8_t flag) {
        checkBound(id);
        idFlags[id] |= flag;
    }

    void emplaceResultType(std::uint32_t id, std::uint32_t type) {
        checkBound(id);
        if (resultTypes[id] == 0u) resultTypes[id] = type;
    }

    void emplaceValue(std::uint32_t id, std::uint32_t v) {
        checkBound(id);
        if ((idFlags[id] & kFlagHasValue) == 0u) {
            idFlags[id] |= kFlagHasValue;
            values[id] = v;
        }
    }

    void setValue(std::uint32_t id, std::uint32_t v) {
        checkBound(id);
        idFlags[id] |= kFlagHasValue;
        values[id] = v;
    }

    bool is64BitSelector(std::uint32_t selectorId) const {
        if (selectorId >= bound) return false;
        const auto selectorType = resultTypes[selectorId];
        return selectorType != 0u && selectorType < bound && scalarWidth[selectorType] == 64u;
    }

    ConstInstView cview(InstSlice slice) const {
        return {wordPool.data() + slice.offset, slice.count};
    }

    InstView view(InstSlice slice) {
        return {wordPool.data() + slice.offset, slice.count};
    }

    InstSlice makeInst(spv::Op op, std::initializer_list<std::uint32_t> operands) {
        const auto offset = static_cast<std::uint32_t>(wordPool.size());
        const auto count = static_cast<std::uint32_t>(operands.size() + 1u);
        wordPool.push_back((count << 16u) | static_cast<std::uint32_t>(op));
        wordPool.insert(wordPool.end(), operands.begin(), operands.end());
        return {offset, count};
    }

    void replaceInst(InstSlice& target, spv::Op op, std::initializer_list<std::uint32_t> operands) {
        const auto count = static_cast<std::uint32_t>(operands.size() + 1u);
        const std::uint32_t head = (count << 16u) | static_cast<std::uint32_t>(op);
        if (count <= target.count) {
            wordPool[target.offset] = head;
            std::uint32_t cursor = target.offset + 1u;
            for (const auto operand : operands) wordPool[cursor++] = operand;
            target.count = count;
            return;
        }
        const auto offset = static_cast<std::uint32_t>(wordPool.size());
        wordPool.push_back(head);
        wordPool.insert(wordPool.end(), operands.begin(), operands.end());
        target = {offset, count};
    }

    void replaceInstSpan(InstSlice& target, std::span<const std::uint32_t> words) {
        const auto count = static_cast<std::uint32_t>(words.size());
        if (count <= target.count) {
            std::copy(words.begin(), words.end(), wordPool.begin() + target.offset);
            target.count = count;
            return;
        }
        const auto offset = static_cast<std::uint32_t>(wordPool.size());
        wordPool.insert(wordPool.end(), words.begin(), words.end());
        target = {offset, count};
    }

    bool equalsInst(InstSlice slice, spv::Op op, std::initializer_list<std::uint32_t> operands) const {
        const auto count = static_cast<std::uint32_t>(operands.size() + 1u);
        if (slice.count != count) return false;
        const auto v = cview(slice);
        if (v[0] != ((count << 16u) | static_cast<std::uint32_t>(op))) return false;
        std::size_t index = 1u;
        for (const auto operand : operands) {
            if (v[index++] != operand) return false;
        }
        return true;
    }

    std::optional<std::uint32_t> value(std::uint32_t id) const {
        return hasFlag(id, kFlagHasValue) ? std::optional(values[id]) : std::nullopt;
    }

    bool sameType(std::uint32_t id, std::uint32_t type) const {
        return type != 0u && id < bound && resultTypes[id] == type;
    }

    std::optional<std::uint32_t> evaluate(spv::Op op, ConstInstView instruction, const std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>>& constPointers, bool hasPrivateTables) const {
        if (instruction.size() < 4u) return std::nullopt;
        const auto typeId = instruction[1];
        if (typeId >= bound) return std::nullopt;
        const auto width = scalarWidth[typeId];
        if (width == 0u || width > 32u) return std::nullopt;
        if (op == spv::OpLoad) {
            if (!hasPrivateTables || instruction.size() != 4u) return std::nullopt;
            const auto found = constPointers.find(instruction[3]);
            if (found == constPointers.end()) return std::nullopt;
            const auto elements = cview(composites.at(found->second.first));
            return found->second.second < elements.size() ? value(elements[found->second.second]) : std::nullopt;
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
        case spv::OpShiftRightLogical: return *right < width ? std::optional(*left >> *right) : std::nullopt;
        case spv::OpShiftLeftLogical: return *right < width ? std::optional(*left << *right) : std::nullopt;
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
        const bool hasPrivateTables = !privateTables.empty();
        std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> constPointers;
        SmallBlockMap<std::pair<std::uint32_t, std::uint32_t>, PairKeyHash> blockBitcasts;
        SmallBlockMap<std::array<std::uint32_t, 3>, TripleKeyHash> blockNanTests;
        dirtyIds.clear();
        if (hasCompositeConstruct7) {
            if (extractIndex.size() != bound) extractIndex.assign(bound, kInvalidIndex);
            else for (const auto id : extractDirty) extractIndex[id] = kInvalidIndex;
            extractDirty.clear();
            extractPool.clear();
        }
        for (std::size_t idx = firstFunctionInst; idx < instructions.size(); ++idx) {
            auto& slice = instructions[idx];
            if (slice.empty()) continue;
            auto instruction = cview(slice);
            auto op = Opcode(instruction);
            if (!function) {
                if (op == spv::OpFunction) {
                    blockBitcasts.clear();
                    blockNanTests.clear();
                    function = true;
                }
                continue;
            }
            bool maybeEval = false;
            switch (op) {
            case spv::OpFunction:
                blockBitcasts.clear();
                blockNanTests.clear();
                continue;
            case spv::OpLabel:
                blockBitcasts.clear();
                blockNanTests.clear();
                continue;
            case spv::OpFunctionEnd:
                function = false;
                continue;
            case spv::OpAccessChain:
            case spv::OpInBoundsAccessChain:
                if (hasPrivateTables && instruction.size() == 5u) {
                    const auto table = privateTables.find(instruction[3]);
                    if (table != privateTables.end()) {
                        if (const auto index = value(instruction[4])) constPointers.emplace(instruction[2], std::pair{table->second, *index});
                    }
                }
                continue;
            case spv::OpSelect:
                if (instruction.size() == 6u) {
                    if (const auto condition = value(instruction[3])) {
                        replaceInst(slice, spv::OpCopyObject, {instruction[1], instruction[2], instruction[*condition != 0u ? 4u : 5u]});
                        changed = true;
                        instruction = cview(slice);
                        op = spv::OpCopyObject;
                        maybeEval = true;
                    } else if (instruction[4] == instruction[5]) {
                        replaceInst(slice, spv::OpCopyObject, {instruction[1], instruction[2], instruction[4]});
                        changed = true;
                        instruction = cview(slice);
                        op = spv::OpCopyObject;
                        maybeEval = true;
                    }
                }
                break;
            case spv::OpLogicalAnd:
                if (instruction.size() == 5u) {
                    if (const auto left = value(instruction[3]); left && *left != 0u) {
                        replaceInst(slice, spv::OpCopyObject, {instruction[1], instruction[2], instruction[4]});
                        changed = true;
                        instruction = cview(slice);
                        op = spv::OpCopyObject;
                    } else if (const auto right = value(instruction[4]); right && *right != 0u) {
                        replaceInst(slice, spv::OpCopyObject, {instruction[1], instruction[2], instruction[3]});
                        changed = true;
                        instruction = cview(slice);
                        op = spv::OpCopyObject;
                    }
                }
                maybeEval = true;
                break;
            case spv::OpLogicalOr:
                if (instruction.size() == 5u) {
                    if (const auto left = value(instruction[3]); left && *left == 0u) {
                        replaceInst(slice, spv::OpCopyObject, {instruction[1], instruction[2], instruction[4]});
                        changed = true;
                        instruction = cview(slice);
                        op = spv::OpCopyObject;
                    } else if (const auto right = value(instruction[4]); right && *right == 0u) {
                        replaceInst(slice, spv::OpCopyObject, {instruction[1], instruction[2], instruction[3]});
                        changed = true;
                        instruction = cview(slice);
                        op = spv::OpCopyObject;
                    }
                }
                maybeEval = true;
                break;
            case spv::OpVectorExtractDynamic:
                if (instruction.size() == 5u) {
                    if (const auto index = value(instruction[4])) {
                        replaceInst(slice, spv::OpCompositeExtract, {instruction[1], instruction[2], instruction[3], *index});
                        changed = true;
                        if (hasCompositeConstruct7) {
                            const auto resId = instruction[2];
                            checkBound(resId);
                            if (extractIndex[resId] == kInvalidIndex) {
                                extractIndex[resId] = static_cast<std::uint32_t>(extractPool.size());
                                extractPool.emplace_back(instruction[3], *index);
                                extractDirty.push_back(resId);
                            }
                        }
                    }
                }
                continue;
            case spv::OpBitcast:
                if (instruction.size() == 4u) {
                    const auto srcId = instruction[3];
                    const auto chained = srcId < bound ? scratchId[srcId] : kInvalidIndex;
                    const auto source = chained != kInvalidIndex && sameType(chained, instruction[1]) ? chained : srcId;
                    if (sameType(source, instruction[1])) {
                        replaceInst(slice, spv::OpCopyObject, {instruction[1], instruction[2], source});
                        changed = true;
                        instruction = cview(slice);
                        op = spv::OpCopyObject;
                        maybeEval = true;
                    } else if (hasFlag(instruction[2], kFlagDecorated)) {
                        const auto resId = instruction[2];
                        checkBound(resId);
                        if (scratchId[resId] == kInvalidIndex) {
                            scratchId[resId] = srcId;
                            dirtyIds.push_back(resId);
                        }
                    } else {
                        const auto [existingId, inserted] = blockBitcasts.tryEmplace(std::pair{instruction[1], srcId}, instruction[2]);
                        if (inserted) {
                            const auto resId = instruction[2];
                            checkBound(resId);
                            if (scratchId[resId] == kInvalidIndex) {
                                scratchId[resId] = srcId;
                                dirtyIds.push_back(resId);
                            }
                        } else {
                            replaceInst(slice, spv::OpCopyObject, {instruction[1], instruction[2], existingId});
                            changed = true;
                            instruction = cview(slice);
                            op = spv::OpCopyObject;
                            maybeEval = true;
                        }
                    }
                }
                break;
            case spv::OpIsNan:
            case spv::OpFUnordNotEqual: {
                const bool nanTest = (op == spv::OpIsNan && instruction.size() == 4u) || (op == spv::OpFUnordNotEqual && instruction.size() == 5u && instruction[3] == instruction[4]);
                if (nanTest && !hasFlag(instruction[2], kFlagDecorated)) {
                    const auto [existingId, inserted] = blockNanTests.tryEmplace(std::array<std::uint32_t, 3>{static_cast<std::uint32_t>(op), instruction[1], instruction[3]}, instruction[2]);
                    if (!inserted) {
                        replaceInst(slice, spv::OpCopyObject, {instruction[1], instruction[2], existingId});
                        changed = true;
                        instruction = cview(slice);
                        op = spv::OpCopyObject;
                        maybeEval = true;
                    }
                }
                break;
            }
            case spv::OpCompositeExtract:
                if (hasCompositeConstruct7 && instruction.size() == 5u) {
                    const auto resId = instruction[2];
                    checkBound(resId);
                    if (extractIndex[resId] == kInvalidIndex) {
                        extractIndex[resId] = static_cast<std::uint32_t>(extractPool.size());
                        extractPool.emplace_back(instruction[3], instruction[4]);
                        extractDirty.push_back(resId);
                    }
                }
                continue;
            case spv::OpCompositeConstruct:
                if (hasCompositeConstruct7 && instruction.size() == 7u) {
                    const auto firstId = instruction[3];
                    const auto firstIdx = firstId < bound ? extractIndex[firstId] : kInvalidIndex;
                    if (firstIdx != kInvalidIndex) {
                        const auto firstSource = extractPool[firstIdx].first;
                        const auto typeId = instruction[1];
                        const auto firstSourceType = firstSource < bound ? resultTypes[firstSource] : 0u;
                        bool shuffle = hasFlag(typeId, kFlagVectorType) && firstSourceType != 0u && hasFlag(firstSourceType, kFlagVectorType);
                        for (std::size_t index = 4; shuffle && index < 7u; ++index) {
                            const auto elemId = instruction[index];
                            const auto elemIdx = elemId < bound ? extractIndex[elemId] : kInvalidIndex;
                            shuffle = elemIdx != kInvalidIndex && extractPool[elemIdx].first == firstSource;
                        }
                        if (shuffle) {
                            const auto resultId = instruction[2];
                            bool identity = firstSourceType == typeId;
                            std::array<std::uint32_t, 9> words{
                                (9u << 16u) | static_cast<std::uint32_t>(spv::OpVectorShuffle),
                                typeId,
                                resultId,
                                firstSource,
                                firstSource,
                                0u,
                                0u,
                                0u,
                                0u,
                            };
                            for (std::size_t index = 3; index < 7u; ++index) {
                                const auto channel = extractPool[extractIndex[instruction[index]]].second;
                                words[2u + index] = channel;
                                identity &= channel == index - 3u;
                            }
                            if (identity) {
                                replaceInst(slice, spv::OpCopyObject, {typeId, resultId, firstSource});
                                instruction = cview(slice);
                                op = spv::OpCopyObject;
                                maybeEval = true;
                            } else {
                                replaceInstSpan(slice, words);
                            }
                            changed = true;
                        }
                    }
                }
                break;
            case spv::OpLoad:
                maybeEval = hasPrivateTables;
                break;
            case spv::OpCopyObject:
            case spv::OpLogicalNot:
            case spv::OpNot:
            case spv::OpIAdd:
            case spv::OpISub:
            case spv::OpIMul:
            case spv::OpUDiv:
            case spv::OpUMod:
            case spv::OpBitwiseAnd:
            case spv::OpBitwiseOr:
            case spv::OpBitwiseXor:
            case spv::OpShiftRightLogical:
            case spv::OpShiftLeftLogical:
            case spv::OpIEqual:
            case spv::OpINotEqual:
            case spv::OpULessThan:
            case spv::OpULessThanEqual:
            case spv::OpUGreaterThan:
            case spv::OpUGreaterThanEqual:
            case spv::OpLogicalEqual:
            case spv::OpLogicalNotEqual:
            case spv::OpBitFieldUExtract:
                maybeEval = true;
                break;
            default:
                continue;
            }
            if (!maybeEval) continue;
            if (op == spv::OpCopyObject && hasFlag(instruction.at(2), kFlagHasValue)) continue;
            const auto result = evaluate(op, instruction, constPointers, hasPrivateTables);
            if (!result) continue;
            const auto typeId = instruction[1];
            const auto resultId = instruction[2];
            const auto width = scalarWidth[typeId];
            const bool isBool = (idFlags[typeId] & kFlagBoolType) != 0u;
            const auto bits = width == 32u ? *result : *result & ((1u << width) - 1u);
            setValue(resultId, bits);
            const auto [existing, created] = canonical.emplace(PackPair(typeId, bits), resultId);
            if (!created) aliases.emplace(resultId, existing->second);
            constants.push_back(isBool
                ? makeInst(bits != 0u ? spv::OpConstantTrue : spv::OpConstantFalse, {typeId, resultId})
                : makeInst(spv::OpConstant, {typeId, resultId, bits}));
            slice.clear();
            changed = true;
        }
        for (const auto id : dirtyIds) scratchId[id] = kInvalidIndex;
        dirtyIds.clear();
        return changed;
    }

    bool prune() {
        blockList.clear();
        edgePool.clear();
        std::vector<std::uint32_t> entries;
        std::uint32_t currentBlock = kInvalidIndex;
        bool entry = false;
        for (std::uint32_t index = firstFunctionInst; index < instructions.size(); ++index) {
            const auto slice = instructions[index];
            if (slice.empty()) continue;
            const auto op = static_cast<spv::Op>(wordPool[slice.offset] & 0xffffu);
            if (op == spv::OpFunction) {
                entry = true;
                continue;
            }
            if (op == spv::OpFunctionEnd) {
                if (currentBlock != kInvalidIndex) {
                    blockList[currentBlock].endInst = index;
                    currentBlock = kInvalidIndex;
                }
                continue;
            }
            if (op == spv::OpLabel) {
                if (currentBlock != kInvalidIndex) blockList[currentBlock].endInst = index;
                const auto instruction = cview(slice);
                const auto label = instruction.at(1);
                checkBound(label);
                currentBlock = static_cast<std::uint32_t>(blockList.size());
                scratchId[label] = currentBlock;
                blockList.push_back(BlockInfo{
                    .label = label,
                    .firstInst = index,
                    .endInst = static_cast<std::uint32_t>(instructions.size()),
                    .lastNonEmpty = index,
                    .nonEmptyCount = 1u,
                });
                if (entry) entries.push_back(currentBlock);
                entry = false;
                continue;
            }
            if (currentBlock != kInvalidIndex) {
                auto& blk = blockList[currentBlock];
                blk.lastNonEmpty = index;
                ++blk.nonEmptyCount;
                if (op == spv::OpLoopMerge) blk.isLoopHeader = true;
                else if (op == spv::OpSelectionMerge) blk.hasSelectionMerge = true;
                else if (op == spv::OpPhi) blk.hasPhi = true;
            }
        }
        if (currentBlock != kInvalidIndex) blockList[currentBlock].endInst = static_cast<std::uint32_t>(instructions.size());

        const auto clearLabels = [&]() {
            for (const auto& blk : blockList) scratchId[blk.label] = kInvalidIndex;
        };

        try {
            for (auto& blk : blockList) {
                if (!blk.isLoopHeader) continue;
                for (std::uint32_t idx = blk.firstInst; idx < blk.endInst; ++idx) {
                    const auto slice = instructions[idx];
                    if (slice.empty()) continue;
                    const auto inst = cview(slice);
                    if (Opcode(inst) != spv::OpLoopMerge) continue;
                    const auto contLabel = inst.at(2);
                    if (contLabel < bound && scratchId[contLabel] != kInvalidIndex) {
                        auto& contBlk = blockList[scratchId[contLabel]];
                        if (contBlk.continueHeader == 0u) contBlk.continueHeader = blk.label;
                    }
                }
            }

            blockOrder.resize(blockList.size());
            bool sorted = true;
            for (std::uint32_t i = 0u; i < blockList.size(); ++i) {
                blockOrder[i] = i;
                if (i > 0u && blockList[i].label < blockList[i - 1u].label) sorted = false;
            }
            if (!sorted) {
                std::sort(blockOrder.begin(), blockOrder.end(), [&](std::uint32_t a, std::uint32_t b) { return blockList[a].label < blockList[b].label; });
            }

            if (linearStamp.size() < blockList.size()) linearStamp.assign(blockList.size(), 0u);
            const auto linearSelection = [&](std::uint32_t targetLabel, std::uint32_t mergeLabel) {
                ++linearEpoch;
                if (linearEpoch == 0u) {
                    std::fill(linearStamp.begin(), linearStamp.end(), 0u);
                    linearEpoch = 1u;
                }
                std::uint32_t target = targetLabel;
                while (target != mergeLabel) {
                    if (target >= bound || scratchId[target] == kInvalidIndex) throw std::out_of_range("prepared SPIR-V id has no entry");
                    const auto bi = scratchId[target];
                    if (linearStamp[bi] == linearEpoch) return false;
                    linearStamp[bi] = linearEpoch;
                    const auto& blk = blockList[bi];
                    if (blk.hasSelectionMerge || blk.isLoopHeader) return false;
                    const auto terminal = cview(instructions[blk.lastNonEmpty]);
                    const auto op = Opcode(terminal);
                    if (op == spv::OpBranchConditional || op == spv::OpSwitch) return false;
                    if (op != spv::OpBranch) return true;
                    target = terminal.at(1);
                }
                return true;
            };

            bool changed = false;
            for (const auto bi : blockOrder) {
                auto& blk = blockList[bi];
                auto& terminalSlice = instructions[blk.lastNonEmpty];
                auto terminal = cview(terminalSlice);
                auto op = Opcode(terminal);
                std::uint32_t target = 0u;
                if (!blk.isLoopHeader && op == spv::OpBranchConditional) {
                    if (const auto condition = value(terminal.at(1)); condition.has_value()) {
                        const auto droppedLabel = terminal.at(*condition != 0u ? 3u : 2u);
                        const bool droppedIsLoop = droppedLabel < bound && scratchId[droppedLabel] != kInvalidIndex && blockList[scratchId[droppedLabel]].isLoopHeader;
                        if (!droppedIsLoop) target = terminal.at(*condition != 0u ? 2u : 3u);
                    }
                } else if (!blk.isLoopHeader && op == spv::OpSwitch) {
                    if (const auto selector = value(terminal.at(1))) {
                        target = terminal.at(2);
                        if ((terminal.size() - 3u) % 2u != 0u) throw std::runtime_error("invalid prepared 32-bit switch");
                        for (std::size_t index = 3; index < terminal.size(); index += 2u) if (terminal[index] == *selector) target = terminal[index + 1u];
                    }
                }
                if (target != 0u) {
                    bool retainMerge = false;
                    if (blk.hasSelectionMerge) {
                        for (std::uint32_t idx = blk.firstInst; idx < blk.endInst; ++idx) {
                            auto& instSlice = instructions[idx];
                            if (instSlice.empty()) continue;
                            const auto inst = cview(instSlice);
                            if (Opcode(inst) != spv::OpSelectionMerge) continue;
                            retainMerge = !linearSelection(target, inst.at(1));
                            if (!retainMerge) {
                                instSlice.clear();
                                --blk.nonEmptyCount;
                            }
                        }
                        if (!retainMerge) blk.hasSelectionMerge = false;
                    }
                    terminal = cview(terminalSlice);
                    if (!retainMerge) {
                        if (!equalsInst(terminalSlice, spv::OpBranch, {target})) {
                            replaceInst(terminalSlice, spv::OpBranch, {target});
                            changed = true;
                        }
                    } else if (op == spv::OpSwitch) {
                        const auto selectorId = terminal.at(1);
                        if (!equalsInst(terminalSlice, spv::OpSwitch, {selectorId, target})) {
                            replaceInst(terminalSlice, spv::OpSwitch, {selectorId, target});
                            changed = true;
                        }
                    } else if (header.at(1) < 0x00010600u && !maximallyReconverges) {
                        const auto condId = terminal.at(1);
                        if (!equalsInst(terminalSlice, spv::OpBranchConditional, {condId, target, target})) {
                            replaceInst(terminalSlice, spv::OpBranchConditional, {condId, target, target});
                            changed = true;
                        }
                    }
                    terminal = cview(terminalSlice);
                    op = Opcode(terminal);
                }
                blk.edgeOffset = static_cast<std::uint32_t>(edgePool.size());
                if (op == spv::OpBranch) edgePool.push_back(terminal.at(1));
                else if (op == spv::OpBranchConditional) {
                    edgePool.push_back(terminal.at(2));
                    edgePool.push_back(terminal.at(3));
                } else if (op == spv::OpSwitch) {
                    edgePool.push_back(terminal.at(2));
                    const auto stride = is64BitSelector(terminal.at(1)) ? 3u : 2u;
                    for (std::size_t index = 3; index + stride <= terminal.size(); index += stride) edgePool.push_back(terminal[index + stride - 1u]);
                }
                blk.edgeCount = static_cast<std::uint32_t>(edgePool.size()) - blk.edgeOffset;
            }

            liveBlocks.assign(blockList.size(), 0u);
            auto pending = std::move(entries);
            while (!pending.empty()) {
                const auto bi = pending.back();
                pending.pop_back();
                if (liveBlocks[bi] != 0u) continue;
                liveBlocks[bi] = 1u;
                const auto& blk = blockList[bi];
                for (std::uint32_t e = 0u; e < blk.edgeCount; ++e) {
                    const auto succLabel = edgePool[blk.edgeOffset + e];
                    if (succLabel >= bound || scratchId[succLabel] == kInvalidIndex) throw std::runtime_error("prepared branch references a missing block");
                    const auto succIdx = scratchId[succLabel];
                    if (liveBlocks[succIdx] == 0u) pending.push_back(succIdx);
                }
            }

            const auto backEdge = [&](std::uint32_t label) -> std::uint32_t {
                if (label >= bound) return 0u;
                const auto bi = scratchId[label];
                if (bi == kInvalidIndex || liveBlocks[bi] != 0u) return 0u;
                const auto headerLabel = blockList[bi].continueHeader;
                if (headerLabel == 0u) return 0u;
                const auto hi = scratchId[headerLabel];
                return hi != kInvalidIndex && liveBlocks[hi] != 0u ? headerLabel : 0u;
            };

            std::vector<std::uint32_t> phiScratch;
            for (std::uint32_t bi = 0u; bi < blockList.size(); ++bi) {
                const auto& blk = blockList[bi];
                if (const auto headerLabel = backEdge(blk.label)) {
                    if (blk.nonEmptyCount == 2u && equalsInst(instructions[blk.lastNonEmpty], spv::OpBranch, {headerLabel})) continue;
                    for (std::uint32_t idx = blk.firstInst + 1u; idx < blk.endInst; ++idx) {
                        auto& instSlice = instructions[idx];
                        if (instSlice.empty()) continue;
                        if (const auto result = Result(cview(instSlice))) markFlag(result, kFlagRemoved);
                        instSlice.clear();
                    }
                    instructions[blk.lastNonEmpty] = makeInst(spv::OpBranch, {headerLabel});
                    changed = true;
                    continue;
                }
                if (liveBlocks[bi] == 0u) {
                    if (blk.nonEmptyCount == 2u && Opcode(cview(instructions[blk.lastNonEmpty])) == spv::OpUnreachable) continue;
                    for (std::uint32_t idx = blk.firstInst + 1u; idx < blk.endInst; ++idx) {
                        auto& instSlice = instructions[idx];
                        if (instSlice.empty()) continue;
                        if (const auto result = Result(cview(instSlice))) markFlag(result, kFlagRemoved);
                        instSlice.clear();
                    }
                    instructions[blk.lastNonEmpty] = makeInst(spv::OpUnreachable, {});
                    changed = true;
                    continue;
                }
                if (!blk.hasPhi) continue;
                for (std::uint32_t idx = blk.firstInst + 1u; idx < blk.endInst; ++idx) {
                    auto& instSlice = instructions[idx];
                    if (instSlice.empty()) continue;
                    const auto instruction = cview(instSlice);
                    if (Opcode(instruction) != spv::OpPhi) continue;
                    phiScratch.clear();
                    phiScratch.push_back(instruction[0]);
                    phiScratch.push_back(instruction[1]);
                    phiScratch.push_back(instruction[2]);
                    for (std::size_t operand = 3; operand + 1u < instruction.size(); operand += 2u) {
                        const auto parent = instruction[operand + 1u];
                        const bool parentLive = parent < bound && scratchId[parent] != kInvalidIndex && liveBlocks[scratchId[parent]] != 0u;
                        bool hasEdge = false;
                        if (parentLive) {
                            const auto& pblk = blockList[scratchId[parent]];
                            for (std::uint32_t e = 0u; e < pblk.edgeCount; ++e) {
                                if (edgePool[pblk.edgeOffset + e] == blk.label) {
                                    hasEdge = true;
                                    break;
                                }
                            }
                        }
                        if (hasEdge) {
                            phiScratch.push_back(instruction[operand]);
                            phiScratch.push_back(parent);
                        } else if (backEdge(parent) == blk.label) {
                            phiScratch.push_back(instruction[2]);
                            phiScratch.push_back(parent);
                        }
                    }
                    if (phiScratch.size() == 3u) throw std::runtime_error("reachable prepared phi has no predecessor");
                    const bool same = phiScratch.size() == instruction.size() && std::equal(phiScratch.begin(), phiScratch.end(), instruction.begin());
                    if (same && phiScratch.size() != 5u) continue;
                    if (phiScratch.size() == 5u) {
                        replaceInst(instSlice, spv::OpCopyObject, {phiScratch[1], phiScratch[2], phiScratch[3]});
                    } else {
                        phiScratch[0] = (static_cast<std::uint32_t>(phiScratch.size()) << 16u) | static_cast<std::uint32_t>(spv::OpPhi);
                        replaceInstSpan(instSlice, phiScratch);
                    }
                    changed = true;
                }
            }
            clearLabels();
            return changed;
        } catch (...) {
            clearLabels();
            throw;
        }
    }

    void markReferencedIds() {
        for (const auto slice : instructions) {
            if (slice.empty()) continue;
            const auto instruction = cview(slice);
            std::uint32_t switchLiteralWords = 1u;
            if (Opcode(instruction) == spv::OpSwitch && instruction.size() > 1u && is64BitSelector(instruction[1])) switchLiteralWords = 2u;
            VisitIdOperands(instruction, switchLiteralWords, [&](std::size_t index) { markFlag(instruction[index], kFlagReferenced); });
        }
    }

    void removeUnreachableBlocks() {
        const auto next = [&](std::size_t index) {
            while (++index < instructions.size() && instructions[index].empty()) {}
            return index;
        };
        std::size_t previous = instructions.size();
        for (std::size_t index = firstFunctionInst; index < instructions.size(); ++index) {
            if (instructions[index].empty()) continue;
            const auto inst = cview(instructions[index]);
            const auto op = Opcode(inst);
            if (op == spv::OpLabel) {
                const auto following = next(index);
                const bool entry = previous < instructions.size() && (Opcode(cview(instructions[previous])) == spv::OpFunction || Opcode(cview(instructions[previous])) == spv::OpFunctionParameter);
                if (!entry && following < instructions.size() && Opcode(cview(instructions[following])) == spv::OpUnreachable && !hasFlag(inst.at(1), kFlagReferenced)) {
                    markFlag(inst.at(1), kFlagRemoved);
                    instructions[index].clear();
                    instructions[following].clear();
                    index = following;
                    continue;
                }
            }
            previous = index;
        }
    }

    void removeUnusedConstants() {
        if (constants.empty()) return;
        std::erase_if(constants, [&](const InstSlice constant) {
            const auto id = Result(cview(constant));
            if (hasFlag(id, kFlagReferenced)) return false;
            markFlag(id, kFlagRemoved);
            return true;
        });
    }

    void orderPhis() {
        std::vector<InstSlice> ordered;
        ordered.reserve(instructions.size());
        ordered.insert(ordered.end(), instructions.begin(), instructions.begin() + firstFunctionInst);
        std::vector<InstSlice> copies;
        bool prefix = false;
        for (std::size_t idx = firstFunctionInst; idx < instructions.size(); ++idx) {
            const auto slice = instructions[idx];
            if (slice.empty()) continue;
            const auto op = static_cast<spv::Op>(wordPool[slice.offset] & 0xffffu);
            if (op == spv::OpLabel) prefix = true;
            else if (prefix && op == spv::OpCopyObject) {
                copies.push_back(slice);
                continue;
            } else if (prefix && op != spv::OpPhi && op != spv::OpLine && op != spv::OpNoLine) {
                ordered.insert(ordered.end(), copies.begin(), copies.end());
                copies.clear();
                prefix = false;
            }
            ordered.push_back(slice);
        }
        if (!copies.empty()) throw std::runtime_error("unterminated prepared phi block");
        instructions = std::move(ordered);
    }

    void shareConstants() {
        if (aliases.empty()) return;
        for (const auto& [aliasId, canonicalId] : aliases) {
            checkBound(aliasId);
            scratchId[aliasId] = canonicalId;
            idFlags[aliasId] |= kFlagScratchActive;
        }
        bool function = false;
        for (std::size_t idx = firstFunctionInst; idx < instructions.size(); ++idx) {
            const auto slice = instructions[idx];
            if (slice.empty()) continue;
            auto instruction = view(slice);
            const auto op = Opcode(instruction);
            if (op == spv::OpFunction) {
                function = true;
                continue;
            }
            if (op == spv::OpFunctionEnd) {
                function = false;
                continue;
            }
            if (!function) continue;
            VisitInputs(instruction, [&](std::size_t index) {
                const auto id = instruction[index];
                if (id < bound && (idFlags[id] & kFlagScratchActive) != 0u) instruction[index] = scratchId[id];
            });
        }
        for (const auto& [aliasId, canonicalId] : aliases) {
            scratchId[aliasId] = kInvalidIndex;
            idFlags[aliasId] &= ~kFlagScratchActive;
        }
    }

    bool propagateCopies() {
        dirtyIds.clear();
        std::size_t liveCopyCount = 0u;
        for (std::size_t idx = firstFunctionInst; idx < instructions.size(); ++idx) {
            const auto slice = instructions[idx];
            if (slice.empty()) continue;
            if ((wordPool[slice.offset] & 0xffffu) != static_cast<std::uint32_t>(spv::OpCopyObject)) continue;
            const auto instruction = cview(slice);
            if (instruction.size() != 4u) throw std::runtime_error("invalid prepared copy instruction");
            const auto resId = instruction[2];
            checkBound(resId);
            if ((idFlags[resId] & kFlagScratchActive) == 0u) {
                idFlags[resId] |= kFlagScratchActive;
                scratchId[resId] = instruction[3];
                dirtyIds.push_back(resId);
                ++liveCopyCount;
            }
        }
        if (liveCopyCount == 0u) return false;
        const auto clearCopies = [&]() {
            for (const auto id : dirtyIds) {
                idFlags[id] &= ~kFlagScratchActive;
                scratchId[id] = kInvalidIndex;
            }
            dirtyIds.clear();
        };
        try {
            for (const auto slice : instructions) {
                if (slice.empty()) continue;
                const auto instruction = cview(slice);
                const auto op = Opcode(instruction);
                if (op == spv::OpName || op == spv::OpMemberName) continue;
                std::size_t first = 0u;
                std::size_t end = 0u;
                if (InputRange(instruction, first, end)) continue;
                bool hasResult = false;
                bool hasType = false;
                spv::HasResultAndType(op, &hasResult, &hasType);
                const auto resultIndex = hasResult ? (hasType ? 2u : 1u) : 0u;
                for (std::size_t index = 1; index < instruction.size(); ++index) {
                    if (index == resultIndex) continue;
                    const auto id = instruction[index];
                    if (id < bound && (idFlags[id] & kFlagScratchActive) != 0u) {
                        idFlags[id] &= ~kFlagScratchActive;
                        --liveCopyCount;
                    }
                }
            }
            if (liveCopyCount == 0u) {
                clearCopies();
                return false;
            }
            const auto resolve = [&](std::uint32_t id) {
                std::size_t count = 0;
                while (id < bound && (idFlags[id] & kFlagScratchActive) != 0u) {
                    if (++count > liveCopyCount) throw std::runtime_error("cyclic prepared copy chain");
                    id = scratchId[id];
                }
                return id;
            };
            bool function = false;
            for (std::size_t idx = firstFunctionInst; idx < instructions.size(); ++idx) {
                auto& slice = instructions[idx];
                if (slice.empty()) continue;
                auto instruction = view(slice);
                const auto op = Opcode(instruction);
                if (op == spv::OpFunction) {
                    function = true;
                    continue;
                }
                if (op == spv::OpFunctionEnd) {
                    function = false;
                    continue;
                }
                if (!function) continue;
                if (op == spv::OpCopyObject && hasFlag(instruction[2], kFlagScratchActive)) {
                    markFlag(instruction[2], kFlagRemoved);
                    slice.clear();
                    continue;
                }
                VisitInputs(instruction, [&](std::size_t index) {
                    const auto id = instruction[index];
                    if (id < bound && (idFlags[id] & kFlagScratchActive) != 0u) instruction[index] = resolve(id);
                });
            }
            clearCopies();
            return true;
        } catch (...) {
            clearCopies();
            throw;
        }
    }

    void removeDeadComputations() {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> pureDefs;
        std::vector<std::uint32_t> roots;
        bool function = false;
        for (std::uint32_t index = 0u; index < instructions.size(); ++index) {
            const auto slice = instructions[index];
            if (slice.empty()) continue;
            const auto instruction = cview(slice);
            const auto op = Opcode(instruction);
            if (!function) {
                if (op == spv::OpFunction) {
                    function = true;
                    continue;
                }
                if (op == spv::OpDecorateId || op == spv::OpGroupDecorate || op == spv::OpGroupMemberDecorate) {
                    for (std::size_t operand = 1; operand < instruction.size(); ++operand) roots.push_back(instruction[operand]);
                }
                continue;
            }
            if (op == spv::OpFunctionEnd) {
                function = false;
                continue;
            }
            if (PureInstruction(instruction)) {
                const auto id = Result(instruction);
                checkBound(id);
                if (scratchId[id] == kInvalidIndex) {
                    scratchId[id] = index;
                    pureDefs.emplace_back(id, index);
                }
                continue;
            }
            if (VisitInputs(instruction, [&](std::size_t operand) { roots.push_back(instruction[operand]); })) continue;
            bool hasResult = false;
            bool hasType = false;
            spv::HasResultAndType(op, &hasResult, &hasType);
            const auto resultIndex = hasResult ? (hasType ? 2u : 1u) : 0u;
            for (std::size_t operand = 1; operand < instruction.size(); ++operand) if (operand != resultIndex) roots.push_back(instruction[operand]);
        }
        std::vector<std::uint32_t> pending;
        pending.reserve(pureDefs.size() / 4u + 64u);
        for (const auto id : roots) {
            if (id < bound && scratchId[id] != kInvalidIndex && (idFlags[id] & kFlagLive) == 0u) {
                idFlags[id] |= kFlagLive;
                pending.push_back(id);
            }
        }
        while (!pending.empty()) {
            const auto id = pending.back();
            pending.pop_back();
            const auto instruction = cview(instructions[scratchId[id]]);
            if (!VisitInputs(instruction, [&](std::size_t operand) {
                const auto inputId = instruction[operand];
                if (inputId < bound && scratchId[inputId] != kInvalidIndex && (idFlags[inputId] & kFlagLive) == 0u) {
                    idFlags[inputId] |= kFlagLive;
                    pending.push_back(inputId);
                }
            })) throw std::runtime_error("missing prepared pure instruction operands");
        }
        for (const auto& [id, index] : pureDefs) {
            if ((idFlags[id] & kFlagLive) != 0u) continue;
            markFlag(id, kFlagRemoved);
            instructions[index].clear();
        }
    }

    std::vector<std::uint32_t> header;
    std::vector<std::uint32_t> wordPool;
    std::vector<InstSlice> instructions;
    std::vector<InstSlice> constants;
    std::uint32_t bound = 0u;
    std::uint32_t firstFunctionInst = kInvalidIndex;
    bool maximallyReconverges = false;
    bool hasCompositeConstruct7 = false;
    std::vector<std::uint8_t> idFlags;
    std::vector<std::uint8_t> scalarWidth;
    std::vector<std::uint32_t> values;
    std::vector<std::uint32_t> resultTypes;
    std::vector<std::uint32_t> scratchId;
    std::vector<std::uint32_t> dirtyIds;
    std::vector<std::uint32_t> extractIndex;
    std::vector<std::uint32_t> extractDirty;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> extractPool;
    std::unordered_map<std::uint32_t, std::uint32_t> aliases;
    std::unordered_map<std::uint32_t, InstSlice> composites;
    std::unordered_map<std::uint32_t, std::uint32_t> privateTables;
    std::unordered_map<std::uint64_t, std::uint32_t> canonical;
    std::vector<BlockInfo> blockList;
    std::vector<std::uint32_t> blockOrder;
    std::vector<std::uint32_t> edgePool;
    std::vector<std::uint32_t> linearStamp;
    std::uint32_t linearEpoch = 0u;
    std::vector<std::uint8_t> liveBlocks;
};

}

std::vector<std::uint32_t> SpecializeSpirv(std::span<const std::uint32_t> words) {
    return Specialization(words).Run();
}

}
