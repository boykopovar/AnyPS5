#include "SpirvBackend/SpirvSpannedUniformity.hpp"
#include <spirv/unified1/spirv.hpp>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <vector>

namespace {

enum Id : std::uint32_t {
    Void = 1u,
    Bool,
    Uint,
    FunctionType,
    InputPointer,
    Index,
    Two,
    Three,
    Semantics,
    WorkgroupPointer,
    WorkgroupArrayType,
    WorkgroupArrayPointer,
    Exchange,
    FunctionPointer,
    True,
    Main,
    Helper,
    StoragePointer,
    Storage,
    PhysicalPointer,
    First = 100u,
};

class Module {
public:
    Module() {
        words = {spv::MagicNumber, 0x00010300u, 0u, 1000u, 0u};
        emit(spv::OpCapability, {spv::CapabilityShader});
        emit(spv::OpMemoryModel, {spv::AddressingModelLogical, spv::MemoryModelGLSL450});
        emit(spv::OpEntryPoint, {spv::ExecutionModelGLCompute, Main, 0x6e69616du, 0u, Index});
        emit(spv::OpExecutionMode, {Main, spv::ExecutionModeLocalSize, 64u, 1u, 1u});
        emit(spv::OpDecorate, {Index, spv::DecorationBuiltIn, spv::BuiltInLocalInvocationIndex});
        emit(spv::OpTypeVoid, {Void});
        emit(spv::OpTypeBool, {Bool});
        emit(spv::OpTypeInt, {Uint, 32u, 0u});
        emit(spv::OpTypeFunction, {FunctionType, Void});
        emit(spv::OpTypePointer, {InputPointer, spv::StorageClassInput, Uint});
        emit(spv::OpVariable, {InputPointer, Index, spv::StorageClassInput});
        emit(spv::OpConstant, {Uint, Two, 2u});
        emit(spv::OpConstant, {Uint, Three, 3u});
        emit(spv::OpConstant, {Uint, Semantics, 264u});
        emit(spv::OpConstantTrue, {Bool, True});
        emit(spv::OpTypePointer, {WorkgroupPointer, spv::StorageClassWorkgroup, Uint});
        emit(spv::OpTypeArray, {WorkgroupArrayType, Uint, Three});
        emit(spv::OpTypePointer, {WorkgroupArrayPointer, spv::StorageClassWorkgroup, WorkgroupArrayType});
        emit(spv::OpVariable, {WorkgroupArrayPointer, Exchange, spv::StorageClassWorkgroup});
        emit(spv::OpTypePointer, {FunctionPointer, spv::StorageClassFunction, Uint});
        emit(spv::OpTypePointer, {StoragePointer, spv::StorageClassStorageBuffer, Uint});
        emit(spv::OpVariable, {StoragePointer, Storage, spv::StorageClassStorageBuffer});
        emit(spv::OpTypePointer, {PhysicalPointer, spv::StorageClassPhysicalStorageBuffer, Uint});
    }

    void emit(spv::Op op, std::initializer_list<std::uint32_t> operands) {
        words.push_back((static_cast<std::uint32_t>(operands.size() + 1u) << 16u) | op);
        words.insert(words.end(), operands);
    }

    void function(std::uint32_t id) {
        emit(spv::OpFunction, {Void, id, spv::FunctionControlMaskNone, FunctionType});
    }

    void barrier() {
        emit(spv::OpControlBarrier, {Two, Two, Semantics});
    }

    void lane(std::uint32_t result) {
        emit(spv::OpLoad, {Uint, result, Index});
    }

    void end() {
        emit(spv::OpReturn, {});
        emit(spv::OpFunctionEnd, {});
    }

    std::vector<std::uint32_t> words;
};

bool failed = false;

void expect(const char* name, const Module& module, bool rejected) {
    const auto reason = ShaderRecompiler::SpannedBarrierInDivergentFlow(module.words);
    if (reason.has_value() == rejected) return;
    std::fprintf(stderr, "%s: expected %s, got %s\n", name, rejected ? "a rejection" : "no rejection", reason ? reason->c_str() : "none");
    failed = true;
}

Module selection(bool perLane) {
    Module module;
    module.function(Main);
    module.emit(spv::OpLabel, {First});
    module.lane(First + 1u);
    module.emit(spv::OpULessThan, {Bool, First + 2u, perLane ? First + 1u : Two, Three});
    module.emit(spv::OpSelectionMerge, {First + 4u, spv::SelectionControlMaskNone});
    module.emit(spv::OpBranchConditional, {First + 2u, First + 3u, First + 4u});
    module.emit(spv::OpLabel, {First + 3u});
    module.barrier();
    module.emit(spv::OpBranch, {First + 4u});
    module.emit(spv::OpLabel, {First + 4u});
    module.barrier();
    module.end();
    return module;
}

Module loop(bool laneExit, bool barrierInSelection) {
    Module module;
    module.function(Main);
    module.emit(spv::OpLabel, {First});
    module.lane(First + 1u);
    module.emit(spv::OpULessThan, {Bool, First + 2u, First + 1u, Three});
    module.emit(spv::OpBranch, {First + 3u});
    module.emit(spv::OpLabel, {First + 3u});
    module.emit(spv::OpLoopMerge, {First + 6u, First + 5u, spv::LoopControlMaskNone});
    module.emit(spv::OpBranch, {First + 4u});
    module.emit(spv::OpLabel, {First + 4u});
    if (barrierInSelection) {
        module.emit(spv::OpSelectionMerge, {First + 8u, spv::SelectionControlMaskNone});
        module.emit(spv::OpBranchConditional, {First + 2u, First + 7u, First + 8u});
        module.emit(spv::OpLabel, {First + 7u});
        module.emit(spv::OpBranch, {First + 8u});
        module.emit(spv::OpLabel, {First + 8u});
    }
    module.barrier();
    module.emit(spv::OpBranch, {First + 5u});
    module.emit(spv::OpLabel, {First + 5u});
    module.emit(spv::OpBranchConditional, {laneExit ? First + 2u : True, First + 3u, First + 6u});
    module.emit(spv::OpLabel, {First + 6u});
    module.end();
    return module;
}

Module call(bool perLane) {
    Module module;
    module.function(Helper);
    module.emit(spv::OpLabel, {First + 10u});
    module.barrier();
    module.end();
    module.function(Main);
    module.emit(spv::OpLabel, {First});
    module.lane(First + 1u);
    module.emit(spv::OpULessThan, {Bool, First + 2u, perLane ? First + 1u : Two, Three});
    module.emit(spv::OpSelectionMerge, {First + 4u, spv::SelectionControlMaskNone});
    module.emit(spv::OpBranchConditional, {First + 2u, First + 3u, First + 4u});
    module.emit(spv::OpLabel, {First + 3u});
    module.emit(spv::OpFunctionCall, {Void, First + 5u, Helper});
    module.emit(spv::OpBranch, {First + 4u});
    module.emit(spv::OpLabel, {First + 4u});
    module.end();
    return module;
}

Module early(bool perLane, spv::Op exit) {
    Module module;
    module.function(Main);
    module.emit(spv::OpLabel, {First});
    module.lane(First + 1u);
    module.emit(spv::OpULessThan, {Bool, First + 2u, perLane ? First + 1u : Two, Three});
    module.emit(spv::OpBranch, {First + 10u});
    module.emit(spv::OpLabel, {First + 10u});
    module.emit(spv::OpLoopMerge, {First + 13u, First + 12u, spv::LoopControlMaskNone});
    module.emit(spv::OpBranch, {First + 11u});
    module.emit(spv::OpLabel, {First + 11u});
    module.emit(spv::OpSelectionMerge, {First + 4u, spv::SelectionControlMaskNone});
    module.emit(spv::OpBranchConditional, {First + 2u, First + 3u, First + 4u});
    module.emit(spv::OpLabel, {First + 3u});
    if (exit == spv::OpReturn) module.emit(spv::OpReturn, {});
    else module.emit(spv::OpBranch, {First + 12u});
    module.emit(spv::OpLabel, {First + 4u});
    module.barrier();
    module.emit(spv::OpBranch, {First + 12u});
    module.emit(spv::OpLabel, {First + 12u});
    module.emit(spv::OpBranchConditional, {True, First + 10u, First + 13u});
    module.emit(spv::OpLabel, {First + 13u});
    module.end();
    return module;
}

Module unmergedContinue(bool perLane) {
    Module module;
    module.function(Main);
    module.emit(spv::OpLabel, {First});
    module.lane(First + 1u);
    module.emit(spv::OpULessThan, {Bool, First + 2u, perLane ? First + 1u : Two, Three});
    module.emit(spv::OpBranch, {First + 10u});
    module.emit(spv::OpLabel, {First + 10u});
    module.emit(spv::OpLoopMerge, {First + 13u, First + 12u, spv::LoopControlMaskNone});
    module.emit(spv::OpBranch, {First + 11u});
    module.emit(spv::OpLabel, {First + 11u});
    module.emit(spv::OpBranchConditional, {First + 2u, First + 12u, First + 14u});
    module.emit(spv::OpLabel, {First + 14u});
    module.barrier();
    module.emit(spv::OpBranch, {First + 12u});
    module.emit(spv::OpLabel, {First + 12u});
    module.emit(spv::OpBranchConditional, {True, First + 10u, First + 13u});
    module.emit(spv::OpLabel, {First + 13u});
    module.end();
    return module;
}

Module joined(const std::function<void(Module&)>& condition) {
    Module module;
    module.function(Main);
    module.emit(spv::OpLabel, {First});
    module.emit(spv::OpVariable, {FunctionPointer, First + 20u, spv::StorageClassFunction});
    module.lane(First + 1u);
    condition(module);
    module.emit(spv::OpSelectionMerge, {First + 9u, spv::SelectionControlMaskNone});
    module.emit(spv::OpBranchConditional, {First + 8u, First + 7u, First + 9u});
    module.emit(spv::OpLabel, {First + 7u});
    module.barrier();
    module.emit(spv::OpBranch, {First + 9u});
    module.emit(spv::OpLabel, {First + 9u});
    module.end();
    return module;
}

void laneSplit(Module& module, std::uint32_t merge) {
    module.emit(spv::OpULessThan, {Bool, First + 2u, First + 1u, Three});
    module.emit(spv::OpSelectionMerge, {merge, spv::SelectionControlMaskNone});
    module.emit(spv::OpBranchConditional, {First + 2u, First + 3u, merge});
    module.emit(spv::OpLabel, {First + 3u});
}

}

int main() {
    expect("a barrier under a lane comparison", selection(true), true);
    expect("a barrier under a constant comparison", selection(false), false);
    expect("a barrier in a loop a lane value leaves", loop(true, false), true);
    expect("a barrier in a uniform loop after a per-lane selection", loop(false, true), false);
    expect("a call reaching a barrier under a lane comparison", call(true), true);
    expect("a barrier after a per-lane return", early(true, spv::OpReturn), true);
    expect("a barrier after a per-lane continue", early(true, spv::OpBranch), true);
    expect("a barrier after a uniform return", early(false, spv::OpReturn), false);
    expect("a barrier after a per-lane continue without a selection merge", unmergedContinue(true), true);
    expect("a barrier after a uniform continue without a selection merge", unmergedContinue(false), false);
    expect("a call reaching a barrier under a constant comparison", call(false), false);
    expect("a barrier under a phi joining a per-lane selection", joined([](Module& module) {
        laneSplit(module, First + 4u);
        module.emit(spv::OpBranch, {First + 4u});
        module.emit(spv::OpLabel, {First + 4u});
        module.emit(spv::OpPhi, {Bool, First + 8u, True, First + 3u, True, First});
    }), true);
    expect("a barrier under a variable stored on one side of a per-lane selection", joined([](Module& module) {
        module.emit(spv::OpStore, {First + 20u, Two});
        laneSplit(module, First + 4u);
        module.emit(spv::OpStore, {First + 20u, Three});
        module.emit(spv::OpBranch, {First + 4u});
        module.emit(spv::OpLabel, {First + 4u});
        module.emit(spv::OpLoad, {Uint, First + 5u, First + 20u});
        module.emit(spv::OpIEqual, {Bool, First + 8u, First + 5u, Two});
    }), true);
    expect("a barrier under an atomic result", joined([](Module& module) {
        module.emit(spv::OpAccessChain, {WorkgroupPointer, First + 5u, Exchange, Two});
        module.emit(spv::OpAtomicIAdd, {Uint, First + 6u, First + 5u, Two, Semantics, Three});
        module.emit(spv::OpIEqual, {Bool, First + 8u, First + 6u, Two});
    }), true);
    expect("a barrier under a workgroup word read at a uniform index", joined([](Module& module) {
        module.emit(spv::OpAccessChain, {WorkgroupPointer, First + 5u, Exchange, Two});
        module.emit(spv::OpLoad, {Uint, First + 6u, First + 5u});
        module.emit(spv::OpIEqual, {Bool, First + 8u, First + 6u, Two});
    }), false);
    expect("a barrier under a workgroup word read at a uniform index after a store", joined([](Module& module) {
        module.emit(spv::OpAccessChain, {WorkgroupPointer, First + 5u, Exchange, Two});
        module.emit(spv::OpStore, {First + 5u, Three});
        module.emit(spv::OpLoad, {Uint, First + 6u, First + 5u});
        module.emit(spv::OpIEqual, {Bool, First + 8u, First + 6u, Two});
    }), true);
    expect("a barrier under a workgroup word read at a uniform index after a store and a barrier", joined([](Module& module) {
        module.emit(spv::OpAccessChain, {WorkgroupPointer, First + 5u, Exchange, Two});
        module.emit(spv::OpStore, {First + 5u, Three});
        module.barrier();
        module.emit(spv::OpLoad, {Uint, First + 6u, First + 5u});
        module.emit(spv::OpIEqual, {Bool, First + 8u, First + 6u, Two});
    }), false);
    expect("a barrier under a workgroup word read at a uniform index after a store on one path past the barrier", joined([](Module& module) {
        module.emit(spv::OpAccessChain, {WorkgroupPointer, First + 5u, Exchange, Two});
        module.barrier();
        module.emit(spv::OpSelectionMerge, {First + 4u, spv::SelectionControlMaskNone});
        module.emit(spv::OpBranchConditional, {True, First + 3u, First + 4u});
        module.emit(spv::OpLabel, {First + 3u});
        module.emit(spv::OpStore, {First + 5u, Three});
        module.emit(spv::OpBranch, {First + 4u});
        module.emit(spv::OpLabel, {First + 4u});
        module.emit(spv::OpLoad, {Uint, First + 6u, First + 5u});
        module.emit(spv::OpIEqual, {Bool, First + 8u, First + 6u, Two});
    }), true);
    expect("a barrier under a buffer word read at a uniform index", joined([](Module& module) {
        module.emit(spv::OpLoad, {Uint, First + 6u, Storage});
        module.emit(spv::OpIEqual, {Bool, First + 8u, First + 6u, Two});
    }), false);
    expect("a barrier under a buffer word read at a uniform index after a store through a device address", joined([](Module& module) {
        module.emit(spv::OpConvertUToPtr, {PhysicalPointer, First + 10u, Two});
        module.emit(spv::OpStore, {First + 10u, Three});
        module.emit(spv::OpLoad, {Uint, First + 6u, Storage});
        module.emit(spv::OpIEqual, {Bool, First + 8u, First + 6u, Two});
    }), true);
    expect("a barrier under a workgroup word read at a lane index", joined([](Module& module) {
        module.emit(spv::OpAccessChain, {WorkgroupPointer, First + 5u, Exchange, First + 1u});
        module.emit(spv::OpLoad, {Uint, First + 6u, First + 5u});
        module.emit(spv::OpIEqual, {Bool, First + 8u, First + 6u, Two});
    }), true);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
