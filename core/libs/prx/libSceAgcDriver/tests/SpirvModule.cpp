#include "SpirvBackend/SpirvModule.hpp"
#include "SpirvBackend/SpirvSpecialization.hpp"
#include <cstdio>
#include <map>
#include <set>
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

int main() {
    testStructExtractsAreNotShuffled();
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
    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
