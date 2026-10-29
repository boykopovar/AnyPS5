#include "SpirvBackend/SpirvModule.hpp"
#include "SpirvBackend/SpirvSpecialization.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <map>
#include <set>
#include <string>
#include <spirv/unified1/spirv.hpp>
#include <vector>

using namespace ShaderRecompiler;

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", what);
        ++failures;
    }
}

std::map<std::uint32_t, std::size_t> opcodeCounts(const std::vector<std::uint32_t>& words) {
    std::map<std::uint32_t, std::size_t> counts;
    std::size_t offset = 5;
    while (offset < words.size()) {
        const auto header = words[offset];
        const auto wordCount = header >> spv::WordCountShift;
        if (wordCount == 0 || offset + wordCount > words.size()) {
            return counts;
        }
        ++counts[header & 0xffffu];
        offset += wordCount;
    }
    return counts;
}

std::size_t declared(const SpirvModule& module, std::uint32_t opcode) {
    const auto counts = opcodeCounts(module.Finalize());
    const auto found = counts.find(opcode);
    return found == counts.end() ? 0 : found->second;
}

void testRepeatedTypesShareOneId() {
    SpirvModule module;
    const auto first = module.Type(spv::OpTypeInt, 32u, 0u);
    const auto second = module.Type(spv::OpTypeInt, 32u, 0u);
    check(first == second, "a repeated OpTypeInt 32 0 returned a different id");
    check(declared(module, spv::OpTypeInt) == 1, "a repeated OpTypeInt 32 0 was declared more than once");
    check(module.Type(spv::OpTypeVoid) == module.Type(spv::OpTypeVoid), "a repeated OpTypeVoid returned a different id");
    check(declared(module, spv::OpTypeVoid) == 1, "OpTypeVoid was not declared exactly once");
    check(module.Type(spv::OpTypeBool) == module.Type(spv::OpTypeBool), "a repeated OpTypeBool returned a different id");
    check(module.Type(spv::OpTypeFloat, 32u) == module.Type(spv::OpTypeFloat, 32u), "a repeated OpTypeFloat 32 returned a different id");
    check(declared(module, spv::OpTypeFloat) == 1, "OpTypeFloat was not declared exactly once");
}

void testOperandShapesDoNotCollide() {
    SpirvModule module;
    const auto oneWord = module.Type(spv::OpTypeInt, 32u);
    const auto twoWords = module.Type(spv::OpTypeInt, 32u, 0u);
    check(oneWord != twoWords, "OpTypeInt with one operand collided with OpTypeInt with two");
    check(declared(module, spv::OpTypeInt) == 2, "the two OpTypeInt shapes were not both declared");
    const auto voidType = module.Type(spv::OpTypeVoid);
    const auto boolType = module.Type(spv::OpTypeBool);
    check(voidType != boolType, "OpTypeVoid collided with OpTypeBool");
    check(module.Type(spv::OpTypeInt, 32u, 0u) != module.Type(spv::OpTypeInt, 32u, 1u), "unsigned and signed OpTypeInt 32 collided");
    check(module.Type(spv::OpTypeInt, 32u, 0u) != module.Type(spv::OpTypeInt, 64u, 0u), "OpTypeInt 32 collided with OpTypeInt 64");
    check(module.Type(spv::OpTypeFloat, 32u) != module.Type(spv::OpTypeFloat, 64u), "OpTypeFloat 32 collided with OpTypeFloat 64");
}

void testOperandKindsAgree() {
    SpirvModule module;
    const auto fromUnsigned = module.Type(spv::OpTypeInt, 32u, 0u);
    const auto fromSigned = module.Type(spv::OpTypeInt, static_cast<std::int32_t>(32), static_cast<std::int32_t>(0));
    check(fromUnsigned == fromSigned, "OpTypeInt disagreed between unsigned and signed operands");
    const auto typeU32 = module.Type(spv::OpTypeInt, 32u, 0u);
    const auto fromEnum = module.Type(spv::OpTypePointer, spv::StorageClassFunction, typeU32);
    const auto fromWord = module.Type(spv::OpTypePointer, static_cast<std::uint32_t>(spv::StorageClassFunction), typeU32);
    check(fromEnum == fromWord, "OpTypePointer disagreed between enum and integer operands");
    check(fromEnum == module.Type(spv::OpTypePointer, spv::StorageClassFunction, typeU32), "a repeated enum-operand OpTypePointer returned a different id");
    check(declared(module, spv::OpTypePointer) == 1, "OpTypePointer was not declared exactly once");
}

void testDecoratedTypesStayDistinct() {
    SpirvModule module;
    const auto typeU32 = module.Type(spv::OpTypeInt, 32u, 0u);
    const auto plain = module.Type(spv::OpTypeRuntimeArray, typeU32);
    const auto decorated = module.DecoratedType(spv::OpTypeRuntimeArray, {{spv::OpDecorate, {spv::DecorationArrayStride, 4u}}}, typeU32);
    check(plain != decorated, "a plain OpTypeRuntimeArray collided with the decorated one");
    check(decorated == module.DecoratedType(spv::OpTypeRuntimeArray, {{spv::OpDecorate, {spv::DecorationArrayStride, 4u}}}, typeU32), "a repeated decorated type returned a different id");
    check(declared(module, spv::OpTypeRuntimeArray) == 2, "the plain and decorated OpTypeRuntimeArray were not both declared");
}

void testConstantsShareOneId() {
    SpirvModule module;
    const auto typeU32 = module.Type(spv::OpTypeInt, 32u, 0u);
    const auto typeBool = module.Type(spv::OpTypeBool);
    check(module.Constant(spv::OpConstant, typeU32, 0u) == module.Constant(spv::OpConstant, typeU32, 0u), "a repeated OpConstant 0 returned a different id");
    check(module.Constant(spv::OpConstant, typeU32, 0u) != module.Constant(spv::OpConstant, typeU32, 1u), "OpConstant 0 collided with OpConstant 1");
    check(module.Constant(spv::OpConstant, typeU32, 1000u) != module.Constant(spv::OpConstant, typeU32, 0u), "OpConstant 1000 collided with OpConstant 0");
    check(module.Constant(spv::OpConstantTrue, typeBool) == module.Constant(spv::OpConstantTrue, typeBool), "a repeated OpConstantTrue returned a different id");
    check(module.Constant(spv::OpConstantTrue, typeBool) != module.Constant(spv::OpConstantFalse, typeBool), "OpConstantTrue collided with OpConstantFalse");
    check(declared(module, spv::OpConstant) == 3, "OpConstant was not declared once per distinct value");
    check(declared(module, spv::OpConstantTrue) == 1, "OpConstantTrue was not declared exactly once");
    const auto signedId = module.Constant(spv::OpConstant, module.Type(spv::OpTypeInt, 32u, 1u), static_cast<std::uint32_t>(-1));
    check(signedId != 0, "a signed OpConstant produced a zero id");
}

void testOperandFreeConstantsShareOneId() {
    SpirvModule module;
    const auto typeBool = module.Type(spv::OpTypeBool);
    const auto typeVoid = module.Type(spv::OpTypeVoid);
    check(module.Constant(spv::OpConstantTrue, typeBool) == module.Constant(spv::OpConstantTrue, typeBool), "a repeated OpConstantTrue returned a different id");
    check(module.Constant(spv::OpConstantTrue, typeBool) != module.Constant(spv::OpConstantFalse, typeBool), "OpConstantTrue collided with OpConstantFalse");
    check(module.Constant(spv::OpConstantTrue, typeVoid) != module.Constant(spv::OpConstantTrue, typeBool), "OpConstantTrue of different types collided");
    check(declared(module, spv::OpConstantTrue) == 2, "OpConstantTrue was not declared once per type");
    check(declared(module, spv::OpConstantFalse) == 1, "OpConstantFalse was not declared exactly once");
    check(module.Type(spv::OpTypeVoid) == typeVoid, "a repeated operand-free type returned a different id");
}

void testIdsSurviveInterleavedAllocation() {
    SpirvModule module;
    const auto typeU32 = module.Type(spv::OpTypeInt, 32u, 0u);
    const auto constant = module.Constant(spv::OpConstant, typeU32, 4u);
    for (int i = 0; i < 8; ++i) {
        check(module.AllocateId() != 0, "AllocateId returned zero");
        check(module.Type(spv::OpTypeInt, 32u, 0u) == typeU32, "a memoized type changed id after AllocateId");
        check(module.Constant(spv::OpConstant, typeU32, 4u) == constant, "a memoized constant changed id after AllocateId");
    }
}

void testModulesAreIndependent() {
    SpirvModule first;
    SpirvModule second;
    const auto firstId = first.Type(spv::OpTypeInt, 32u, 0u);
    const auto secondId = second.Type(spv::OpTypeInt, 32u, 0u);
    check(firstId == 1, "the first id of a fresh module was not 1");
    check(secondId == 1, "a second module did not start its ids from 1");
    first.Type(spv::OpTypeVoid);
    check(second.Type(spv::OpTypeInt, 32u, 0u) == secondId, "a second module's ids moved when the first module was used");
}

void testIdsStayUniqueBeyondCacheCapacity() {
    SpirvModule module;
    const auto typeU32 = module.Type(spv::OpTypeInt, 32u, 0u);
    std::vector<std::uint32_t> ids;
    ids.reserve(4096);
    for (std::uint32_t value = 0; value < 4096; ++value) {
        ids.push_back(module.Constant(spv::OpConstant, typeU32, value));
    }
    const std::set<std::uint32_t> distinct(ids.begin(), ids.end());
    check(distinct.size() == ids.size(), "distinct constants shared an id once the cache was overrun");
    for (std::uint32_t value = 0; value < 4096; ++value) {
        if (module.Constant(spv::OpConstant, typeU32, value) != ids[value]) {
            std::fprintf(stderr, "constant %u changed id after the cache was overrun\n", value);
            ++failures;
            break;
        }
    }
    std::uint32_t largest = 0;
    for (const auto id : distinct) {
        largest = largest < id ? id : largest;
    }
    const auto first = ids.front();
    check(largest - first + 1 == distinct.size(), "the constant ids were not contiguous, so some were never declared");
    check(first > typeU32, "a constant was numbered before the type it uses");
}

void testEmittedWordsAreStable() {
    SpirvModule module;
    const auto typeU32 = module.Type(spv::OpTypeInt, 32u, 0u);
    for (int i = 0; i < 32; ++i) {
        module.Type(spv::OpTypeVector, typeU32, 4u);
        module.Type(spv::OpTypePointer, spv::StorageClassFunction, typeU32);
        module.Constant(spv::OpConstant, typeU32, static_cast<std::uint32_t>(i % 8));
        module.AddFunction(spv::OpNop);
    }
    const auto words = module.Finalize();
    check(!words.empty(), "Finalize produced no words");
    const auto counts = opcodeCounts(words);
    check(counts.at(spv::OpTypeVector) == 1, "OpTypeVector was declared more than once");
    check(counts.at(spv::OpTypePointer) == 1, "OpTypePointer was declared more than once");
    check(counts.at(spv::OpConstant) == 8, "OpConstant was not declared once per distinct value");
    check(counts.at(spv::OpNop) == 32, "the function body lost instructions");
}

}

void testStructExtractsAreNotShuffled() {
    const auto op = [](std::uint32_t count, spv::Op code) { return (count << 16u) | static_cast<std::uint32_t>(code); };
    for (const bool vectorSource : {false, true}) {
        const std::vector<std::uint32_t> words{
            spv::MagicNumber, 0x00010300u, 0u, 20u, 0u,
            op(2, spv::OpCapability), spv::CapabilityShader,
            op(3, spv::OpMemoryModel), spv::AddressingModelLogical, spv::MemoryModelGLSL450,
            op(6, spv::OpEntryPoint), spv::ExecutionModelGLCompute, 10u, 0x6e69616du, 0u, 9u,
            op(6, spv::OpExecutionMode), 10u, spv::ExecutionModeLocalSize, 1u, 1u, 1u,
            op(2, spv::OpTypeVoid), 1u,
            op(4, spv::OpTypeInt), 2u, 32u, 0u,
            vectorSource ? op(4, spv::OpTypeVector) : op(4, spv::OpTypeStruct), 3u, 2u, 2u,
            op(4, spv::OpTypeVector), 4u, 2u, 4u,
            op(4, spv::OpTypePointer), 5u, spv::StorageClassPrivate, 4u,
            op(3, spv::OpTypeFunction), 6u, 1u,
            op(4, spv::OpConstant), 2u, 7u, 1u,
            op(4, spv::OpConstant), 2u, 8u, 2u,
            op(4, spv::OpVariable), 5u, 9u, spv::StorageClassPrivate,
            op(5, spv::OpFunction), 1u, 10u, 0u, 6u,
            op(2, spv::OpLabel), 11u,
            op(5, spv::OpCompositeConstruct), 3u, 12u, 7u, 8u,
            op(5, spv::OpCompositeExtract), 2u, 13u, 12u, 0u,
            op(5, spv::OpCompositeExtract), 2u, 14u, 12u, 1u,
            op(7, spv::OpCompositeConstruct), 4u, 15u, 13u, 14u, 13u, 13u,
            op(3, spv::OpStore), 9u, 15u,
            op(1, spv::OpReturn),
            op(1, spv::OpFunctionEnd),
        };
        const auto specialized = ShaderRecompiler::SpecializeSpirv(words);
        bool shuffled = false;
        for (std::size_t cursor = 5; cursor < specialized.size(); cursor += specialized[cursor] >> 16u) {
            if ((specialized[cursor] >> 16u) == 0u) break;
            if ((specialized[cursor] & 0xffffu) == spv::OpVectorShuffle) shuffled = true;
        }
        check(shuffled == vectorSource, vectorSource ? "extracts of one vector were not folded into a shuffle" : "extracts of a struct were folded into a vector shuffle");
    }
}

std::vector<std::uint32_t> foldedSwitchModule(std::uint32_t deadSum, std::array<std::uint32_t, 3> localSize) {
    const auto op = [](std::uint32_t count, spv::Op code) { return (count << 16u) | static_cast<std::uint32_t>(code); };
    return {
        spv::MagicNumber, 0x00010300u, 0u, 40u, 0u,
        op(2, spv::OpCapability), spv::CapabilityShader,
        op(3, spv::OpMemoryModel), spv::AddressingModelLogical, spv::MemoryModelGLSL450,
        op(6, spv::OpEntryPoint), spv::ExecutionModelGLCompute, 11u, 0x6e69616du, 0u, 10u,
        op(6, spv::OpExecutionMode), 11u, spv::ExecutionModeLocalSize, localSize[0], localSize[1], localSize[2],
        op(2, spv::OpTypeVoid), 1u,
        op(4, spv::OpTypeInt), 2u, 32u, 0u,
        op(4, spv::OpTypePointer), 4u, spv::StorageClassPrivate, 2u,
        op(3, spv::OpTypeFunction), 5u, 1u,
        op(4, spv::OpConstant), 2u, 6u, 2u,
        op(4, spv::OpConstant), 2u, 7u, 10u,
        op(4, spv::OpConstant), 2u, 8u, 20u,
        op(4, spv::OpVariable), 4u, 10u, spv::StorageClassPrivate,
        op(5, spv::OpFunction), 1u, 11u, 0u, 5u,
        op(2, spv::OpLabel), 12u,
        op(5, spv::OpIAdd), 2u, deadSum, 6u, 7u,
        op(5, spv::OpIMul), 2u, 14u, deadSum, 7u,
        op(3, spv::OpSelectionMerge), 20u, spv::SelectionControlMaskNone,
        op(7, spv::OpSwitch), 6u, 15u, 1u, 16u, 2u, 17u,
        op(2, spv::OpLabel), 15u,
        op(3, spv::OpStore), 10u, 7u,
        op(2, spv::OpBranch), 20u,
        op(2, spv::OpLabel), 16u,
        op(3, spv::OpStore), 10u, 8u,
        op(2, spv::OpBranch), 20u,
        op(2, spv::OpLabel), 17u,
        op(3, spv::OpStore), 10u, 14u,
        op(2, spv::OpBranch), 20u,
        op(2, spv::OpLabel), 20u,
        op(1, spv::OpReturn),
        op(1, spv::OpFunctionEnd),
    };
}

void checkFoldedSwitch(const std::vector<std::uint32_t>& words, std::uint32_t deadSum, const char* what) {
    const auto specialized = ShaderRecompiler::SpecializeSpirv(words);
    const auto counts = opcodeCounts(specialized);
    std::set<std::uint32_t> constants;
    bool storesFolded = false;
    for (std::size_t cursor = 5; cursor < specialized.size(); cursor += specialized[cursor] >> 16u) {
        if ((specialized[cursor] >> 16u) == 0u) break;
        const auto opcode = specialized[cursor] & 0xffffu;
        if (opcode == spv::OpConstant) constants.insert(specialized[cursor + 2u]);
        if (opcode == spv::OpStore && specialized[cursor + 2u] == 14u) storesFolded = true;
    }
    const std::string context = std::string(" (") + what + ")";
    check(!counts.contains(spv::OpUnreachable), ("blocks the folded switch left unreachable were kept" + context).c_str());
    check(counts.at(spv::OpLabel) == 3u, ("the specialized function does not have exactly its three live blocks" + context).c_str());
    check(storesFolded && constants.contains(14u), ("the folded product the live case stores is missing" + context).c_str());
    check(!constants.contains(deadSum), ("a folded constant without uses was kept" + context).c_str());
}

void testFoldingLeavesNoDeadBlocksOrConstants() {
    checkFoldedSwitch(foldedSwitchModule(13u, {1u, 1u, 1u}), 13u, "no literal names a dead id");
    checkFoldedSwitch(foldedSwitchModule(32u, {15u, 16u, 1u}), 32u, "the OpTypeInt width and the LocalSize literals equal the dead ids");
}

void testIdsBeyondTheBoundAreRejected() {
    auto words = foldedSwitchModule(13u, {1u, 1u, 1u});
    words[3] = 12u;
    bool rejected = false;
    try {
        static_cast<void>(ShaderRecompiler::SpecializeSpirv(words));
    } catch (const std::exception&) {
        rejected = true;
    }
    check(rejected, "a module whose ids reach its bound was specialized");
}

void testPrivateCompositeTableLoadFolding() {
    for (const auto storageClass : {spv::StorageClassPrivate, spv::StorageClassFunction}) {
        for (const auto accessOp : {spv::OpAccessChain, spv::OpInBoundsAccessChain}) {
            const auto build = [&](bool dynamicIndex, bool storeThroughChain) {
                SpirvModule module;
                module.EmitCapability(spv::CapabilityShader);
                const auto typeVoid = module.Type(spv::OpTypeVoid);
                const auto typeBool = module.Type(spv::OpTypeBool);
                const auto typeU32 = module.Type(spv::OpTypeInt, 32u, 0u);
                const auto falseVal = module.Constant(spv::OpConstantFalse, typeBool);
                const auto zero = module.Constant(spv::OpConstant, typeU32, 0u);
                const auto one = module.Constant(spv::OpConstant, typeU32, 1u);
                const auto three = module.Constant(spv::OpConstant, typeU32, 3u);
                const auto sixteen = module.Constant(spv::OpConstant, typeU32, 16u);
                const auto twenty = module.Constant(spv::OpConstant, typeU32, 20u);
                const auto thirty = module.Constant(spv::OpConstant, typeU32, 30u);
                const auto typeArray = module.Type(spv::OpTypeArray, typeU32, three);
                const auto initial = module.Constant(spv::OpConstantComposite, typeArray, std::vector<std::uint32_t>{sixteen, twenty, thirty});
                const auto typeInputPtr = module.Type(spv::OpTypePointer, spv::StorageClassInput, typeU32);
                const auto typeTablePtr = module.Type(spv::OpTypePointer, storageClass, typeArray);
                const auto typeElementPtr = module.Type(spv::OpTypePointer, storageClass, typeU32);
                const auto typePrivatePtr = module.Type(spv::OpTypePointer, spv::StorageClassPrivate, typeU32);
                const auto typeFunction = module.Type(spv::OpTypeFunction, typeVoid);
                const auto input = module.DefineGlobalVariable(typeInputPtr, spv::StorageClassInput);
                const auto table = storageClass == spv::StorageClassPrivate
                    ? module.DefineInitializedGlobalVariable(typeTablePtr, spv::StorageClassPrivate, initial)
                    : module.AllocateId();
                const auto output = module.DefineGlobalVariable(typePrivatePtr, spv::StorageClassPrivate);
                const auto mainId = module.AllocateId();
                const auto lane = module.AllocateId();
                const auto rawCond = module.AllocateId();
                const auto cond = module.AllocateId();
                const auto baseIndex = module.AllocateId();
                const auto index = module.AllocateId();
                const auto element = module.AllocateId();
                const auto loaded = module.AllocateId();
                const auto sum = module.AllocateId();
                module.AddFunction(spv::OpFunction, typeVoid, mainId, spv::FunctionControlMaskNone, typeFunction);
                module.AddFunction(spv::OpLabel, module.AllocateId());
                if (storageClass == spv::StorageClassFunction) module.AddFunction(spv::OpVariable, typeTablePtr, table, spv::StorageClassFunction, initial);
                module.AddFunction(spv::OpLoad, typeU32, lane, input);
                if (storeThroughChain) {
                    const auto writePtr = module.AllocateId();
                    module.AddFunction(accessOp, typeElementPtr, writePtr, table, zero);
                    module.AddFunction(spv::OpStore, writePtr, lane);
                }
                module.AddFunction(spv::OpUGreaterThan, typeBool, rawCond, lane, zero);
                module.AddFunction(spv::OpLogicalOr, typeBool, cond, rawCond, falseVal);
                module.AddFunction(spv::OpIAdd, typeU32, baseIndex, one, one);
                module.AddFunction(spv::OpSelect, typeU32, index, cond, baseIndex, baseIndex);
                module.AddFunction(accessOp, typeElementPtr, element, table, dynamicIndex ? lane : index);
                module.AddFunction(spv::OpLoad, typeU32, loaded, element);
                module.AddFunction(spv::OpIAdd, typeU32, sum, lane, loaded);
                module.AddFunction(spv::OpStore, output, sum);
                module.AddFunction(spv::OpReturn);
                module.AddFunction(spv::OpFunctionEnd);
                return std::pair{ShaderRecompiler::SpecializeSpirv(module.Finalize()), thirty};
            };
            const auto [folded, expectedConstant] = build(false, false);
            const auto foldedCounts = opcodeCounts(folded);
            check(!foldedCounts.contains(accessOp), "a constant-index composite table access chain was not removed");
            check(!foldedCounts.contains(spv::OpLogicalOr), "a right-false OpLogicalOr was not simplified");
            check(!foldedCounts.contains(spv::OpSelect), "an identical-arm OpSelect was not simplified");
            check(foldedCounts.contains(spv::OpLoad) && foldedCounts.at(spv::OpLoad) == 1u, "a constant-index composite table load was not folded");
            bool addedThirty = false;
            for (std::size_t cursor = 5; cursor < folded.size(); cursor += folded[cursor] >> 16u) {
                if ((folded[cursor] >> 16u) == 0u) break;
                if ((folded[cursor] & 0xffffu) == spv::OpIAdd && folded[cursor + 4u] == expectedConstant) addedThirty = true;
            }
            check(addedThirty, "a constant-index composite table load did not fold to its element constant");

            const auto [dynamic, unusedDynamic] = build(true, false);
            static_cast<void>(unusedDynamic);
            const auto dynamicCounts = opcodeCounts(dynamic);
            check(dynamicCounts.contains(accessOp) && dynamicCounts.at(accessOp) == 1u, "a dynamic-index composite table access chain was removed");
            check(dynamicCounts.contains(spv::OpLoad) && dynamicCounts.at(spv::OpLoad) == 2u, "a dynamic-index composite table load was folded");

            const auto [mutated, unusedMutated] = build(false, true);
            static_cast<void>(unusedMutated);
            const auto mutatedCounts = opcodeCounts(mutated);
            check(mutatedCounts.contains(accessOp) && mutatedCounts.at(accessOp) == 2u, "a mutated composite table access chain was removed");
            check(mutatedCounts.contains(spv::OpLoad) && mutatedCounts.at(spv::OpLoad) == 2u, "a mutated composite table load was folded despite a store through an access chain");
        }
    }
}

void testBlockBallotsOfOnePredicateShareOneBallot() {
    SpirvModule module;
    const auto typeBool = module.Type(spv::OpTypeBool);
    const auto typeU32 = module.Type(spv::OpTypeInt, 32u, 0u);
    const auto typeBallot = module.Type(spv::OpTypeVector, typeU32, 4u);
    const auto typeVoid = module.Type(spv::OpTypeVoid);
    const auto typeFunction = module.Type(spv::OpTypeFunction, typeVoid);
    const auto scope = module.Constant(spv::OpConstant, typeU32, static_cast<std::uint32_t>(spv::ScopeSubgroup));
    const auto truth = module.Constant(spv::OpConstantTrue, typeBool);
    const auto other = module.Constant(spv::OpConstantFalse, typeBool);
    const auto helper = module.AllocateId();
    const auto main = module.AllocateId();
    const auto first = module.AllocateId();
    const auto repeated = module.AllocateId();
    const auto distinct = module.AllocateId();
    const auto inHelper = module.AllocateId();
    const auto afterHelper = module.AllocateId();
    const auto afterCall = module.AllocateId();
    const auto afterLabel = module.AllocateId();
    const auto next = module.AllocateId();
    module.AddFunction(spv::OpFunction, typeVoid, main, spv::FunctionControlMaskNone, typeFunction);
    module.AddFunction(spv::OpLabel, module.AllocateId());
    module.AddSubgroupBallot(typeBallot, first, scope, truth);
    module.AddSubgroupBallot(typeBallot, repeated, scope, truth);
    module.AddSubgroupBallot(typeBallot, distinct, scope, other);
    module.BeginHelperFunction();
    module.AddFunction(spv::OpFunction, typeVoid, helper, spv::FunctionControlMaskNone, typeFunction);
    module.AddFunction(spv::OpLabel, module.AllocateId());
    module.AddSubgroupBallot(typeBallot, inHelper, scope, truth);
    module.AddFunction(spv::OpReturn);
    module.AddFunction(spv::OpFunctionEnd);
    module.EndHelperFunction();
    module.AddSubgroupBallot(typeBallot, afterHelper, scope, truth);
    module.AddFunction(spv::OpFunctionCall, typeVoid, module.AllocateId(), helper);
    module.AddSubgroupBallot(typeBallot, afterCall, scope, truth);
    module.AddFunction(spv::OpBranch, next);
    module.AddFunction(spv::OpLabel, next);
    module.AddSubgroupBallot(typeBallot, afterLabel, scope, truth);
    module.AddFunction(spv::OpReturn);
    module.AddFunction(spv::OpFunctionEnd);
    const auto words = module.Finalize();
    std::map<std::uint32_t, std::uint32_t> ballotSources;
    std::map<std::uint32_t, std::uint32_t> copySources;
    for (std::size_t offset = 5; offset < words.size(); offset += words[offset] >> spv::WordCountShift) {
        if ((words[offset] >> spv::WordCountShift) == 0u) break;
        const auto opcode = words[offset] & 0xffffu;
        if (opcode == spv::OpGroupNonUniformBallot) ballotSources[words[offset + 2u]] = words[offset + 4u];
        if (opcode == spv::OpCopyObject) copySources[words[offset + 2u]] = words[offset + 3u];
    }
    check(ballotSources.size() == 5u, "a block did not ballot each predicate exactly once");
    check(copySources.size() == 2u && copySources.contains(repeated) && copySources.contains(afterHelper), "a repeated ballot in one block was not a copy of the first");
    check(copySources[repeated] == first && copySources[afterHelper] == first, "a repeated ballot did not copy the block's first ballot");
    check(ballotSources[distinct] == other, "a ballot of another predicate was reused");
    check(ballotSources.contains(inHelper), "a helper function reused a ballot of the function it interrupted");
    check(ballotSources.contains(afterCall), "a ballot after a function call reused one from before the call");
    check(ballotSources.contains(afterLabel), "a ballot in a later block reused one from an earlier block");
}

void testInternedSpirvSharesEqualWords() {
    const std::vector<std::uint32_t> words{spv::MagicNumber, 0x00010400u, 0u, 7u, 0u, (2u << 16u) | spv::OpCapability, spv::CapabilityShader};
    auto changed = words;
    changed.back() = spv::CapabilityInt64;
    const auto first = InternSpirv(words);
    const auto second = InternSpirv(words);
    const auto other = InternSpirv(changed);
    check(std::ranges::equal(first.Words(), words) && std::ranges::equal(other.Words(), changed), "interned SPIR-V changed its words");
    check(first.data() == second.data(), "two equal specialized modules kept separate copies of their words");
    check(first.data() != other.data(), "different SPIR-V was interned into one copy");
    auto edited = second;
    edited[6] = spv::CapabilityFloat64;
    const auto again = InternSpirv(words);
    check(edited[6] == spv::CapabilityFloat64 && first[6] == spv::CapabilityShader && again.data() == first.data(), "a write to one holder of interned SPIR-V changed the others");
}

void testInternedSpirvReleasesUnusedWords() {
    const std::vector<std::uint32_t> words{spv::MagicNumber, 0x00010400u, 0u, 7u, 0u, (2u << 16u) | spv::OpCapability, spv::CapabilityGeometry};
    SharedSpirv::Weak released;
    {
        const auto first = InternSpirv(words);
        const auto second = InternSpirv(words);
        check(first.data() == second.data(), "two equal specialized modules kept separate copies of their words");
        released = first.Weaken();
    }
    check(released.Expired(), "the SPIR-V interning table kept words no holder uses");
    const auto again = InternSpirv(words);
    check(std::ranges::equal(again.Words(), words) && released.Lock().empty(), "SPIR-V interned again after its release changed its words");
}

std::vector<std::uint32_t> foldedBitcasts(bool separateBlocks, bool differentInput, bool differentType, std::uint32_t nanOpcode, std::uint32_t decorationMask = 0u) {
    SpirvModule module;
    module.EmitCapability(spv::CapabilityShader);
    module.AddMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
    const auto voidType = module.Type(spv::OpTypeVoid);
    const auto boolType = module.Type(spv::OpTypeBool);
    const auto wordType = module.Type(spv::OpTypeInt, 32u, 0u);
    const auto floatType = module.Type(spv::OpTypeFloat, 32u);
    const auto otherType = differentType ? module.Type(spv::OpTypeInt, 32u, 1u) : floatType;
    const auto vectorType = module.Type(spv::OpTypeVector, wordType, 3u);
    const auto input = module.DefineGlobalVariable(module.Type(spv::OpTypePointer, spv::StorageClassInput, vectorType), spv::StorageClassInput);
    module.AddAnnotation(spv::OpDecorate, input, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId);
    const auto firstOutputType = nanOpcode != 0u ? boolType : floatType;
    const auto secondOutputType = nanOpcode != 0u ? boolType : otherType;
    const std::array outputs{
        module.DefineGlobalVariable(module.Type(spv::OpTypePointer, spv::StorageClassPrivate, firstOutputType), spv::StorageClassPrivate),
        module.DefineGlobalVariable(module.Type(spv::OpTypePointer, spv::StorageClassPrivate, secondOutputType), spv::StorageClassPrivate)};
    const auto mainId = module.AllocateId();
    const auto functionType = module.Type(spv::OpTypeFunction, voidType);
    const auto zero = module.Constant(spv::OpConstant, wordType, 0u);
    const auto trueValue = module.Constant(spv::OpConstantTrue, boolType);
    module.EmitEntryPoint(spv::ExecutionModelGLCompute, mainId, "main", {input});
    module.AddExecutionMode(mainId, spv::ExecutionModeLocalSize, 1u, 1u, 1u);
    module.AddFunction(spv::OpFunction, voidType, mainId, spv::FunctionControlMaskNone, functionType);
    module.AddFunction(spv::OpLabel, module.AllocateId());
    const auto loaded = module.AllocateId();
    const auto first = module.AllocateId();
    const auto second = module.AllocateId();
    const auto alias = module.AllocateId();
    module.AddFunction(spv::OpLoad, vectorType, loaded, input);
    module.AddFunction(spv::OpCompositeExtract, wordType, first, loaded, 0u);
    module.AddFunction(spv::OpCompositeExtract, wordType, second, loaded, 1u);
    module.AddFunction(spv::OpSelect, wordType, alias, trueValue, first, second);
    const auto emit = [&](bool other) {
        const auto cast = module.AllocateId();
        module.AddFunction(spv::OpBitcast, other ? otherType : floatType, cast, other ? (differentInput ? second : alias) : first);
        if ((decorationMask & (other ? 2u : 1u)) != 0u) module.AddAnnotation(spv::OpDecorate, cast, spv::DecorationRelaxedPrecision);
        if ((decorationMask & (other ? 8u : 4u)) != 0u) {
            const auto group = module.AllocateId();
            module.AddAnnotation(spv::OpDecorationGroup, group);
            module.AddAnnotation(spv::OpDecorate, group, spv::DecorationRelaxedPrecision);
            module.AddAnnotation(spv::OpGroupDecorate, group, cast);
        }
        auto result = cast;
        if (nanOpcode != 0u) {
            result = module.AllocateId();
            if (nanOpcode == spv::OpIsNan) module.AddFunction(nanOpcode, boolType, result, cast);
            else module.AddFunction(nanOpcode, boolType, result, cast, cast);
        }
        module.AddFunction(spv::OpStore, outputs[other ? 1u : 0u], result);
    };
    if (separateBlocks) {
        const auto condition = module.AllocateId();
        const auto left = module.AllocateId();
        const auto right = module.AllocateId();
        const auto merge = module.AllocateId();
        module.AddFunction(spv::OpINotEqual, boolType, condition, first, zero);
        module.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
        module.AddFunction(spv::OpBranchConditional, condition, left, right);
        module.AddFunction(spv::OpLabel, left);
        emit(false);
        module.AddFunction(spv::OpBranch, merge);
        module.AddFunction(spv::OpLabel, right);
        emit(true);
        module.AddFunction(spv::OpBranch, merge);
        module.AddFunction(spv::OpLabel, merge);
    } else {
        emit(false);
        emit(true);
    }
    module.AddFunction(spv::OpReturn);
    module.AddFunction(spv::OpFunctionEnd);
    return SpecializeSpirv(module.Finalize());
}

void testFoldedBitcastsAndNanTests() {
    for (const auto nanOpcode : std::array<std::uint32_t, 3>{0u, spv::OpIsNan, spv::OpFUnordNotEqual}) {
        const auto same = opcodeCounts(foldedBitcasts(false, false, false, nanOpcode));
        const auto other = opcodeCounts(foldedBitcasts(false, true, false, nanOpcode));
        const auto branches = opcodeCounts(foldedBitcasts(true, false, false, nanOpcode));
        const auto count = [](const auto& counts, std::uint32_t op) {
            const auto found = counts.find(op);
            return found == counts.end() ? 0u : found->second;
        };
        check(count(same, spv::OpBitcast) == 1u, "folding a select left repeated bitcasts of one value in a block");
        check(count(other, spv::OpBitcast) == 2u, "folding merged bitcasts of different input values");
        check(count(branches, spv::OpBitcast) == 2u, "folding reused a bitcast from a sibling branch");
        if (nanOpcode != 0u) {
            check(count(same, nanOpcode) == 1u, "folding left repeated NaN tests of one value in a block");
            check(count(other, nanOpcode) == 2u, "folding merged NaN tests of different input values");
            check(count(branches, nanOpcode) == 2u, "folding reused a NaN test from a sibling branch");
        }
    }
    check(opcodeCounts(foldedBitcasts(false, false, true, 0u))[spv::OpBitcast] == 2u, "folding merged bitcasts to different result types");
    for (const auto decorations : {1u, 2u, 3u, 4u, 8u}) {
        check(opcodeCounts(foldedBitcasts(false, false, false, 0u, decorations))[spv::OpBitcast] == 2u, "folding merged a decorated bitcast with another value");
    }
}

int main() {
    testFoldedBitcastsAndNanTests();
    testFoldingLeavesNoDeadBlocksOrConstants();
    testIdsBeyondTheBoundAreRejected();
    testStructExtractsAreNotShuffled();
    testPrivateCompositeTableLoadFolding();
    testRepeatedTypesShareOneId();
    testOperandShapesDoNotCollide();
    testOperandKindsAgree();
    testDecoratedTypesStayDistinct();
    testConstantsShareOneId();
    testOperandFreeConstantsShareOneId();
    testIdsSurviveInterleavedAllocation();
    testModulesAreIndependent();
    testIdsStayUniqueBeyondCacheCapacity();
    testEmittedWordsAreStable();
    testBlockBallotsOfOnePredicateShareOneBallot();
    testInternedSpirvSharesEqualWords();
    testInternedSpirvReleasesUnusedWords();
    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
