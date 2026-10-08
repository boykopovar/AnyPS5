#include "Optimization/SrtWalker/SrtExecutionPlan.hpp"
#include "Optimization/SrtWalker/SrtDescriptorEvaluation.hpp"
#include "Optimization/SrtWalker/SrtFlatSlotClasses.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

using namespace ShaderRecompiler;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Fixture {
    IrResourcePlan plan;

    IrValue& Make(IrOpcode op, IrType type, std::initializer_list<IrValue*> arguments = {}) {
        require(arguments.size() == IrOpcodeOperandCount(op), "fixture operands disagree with the IR instruction schema");
        auto value = std::make_unique<IrValue>(op, type, static_cast<std::uint32_t>(plan.valueStorage.size()));
        for (auto* argument : arguments) value->AddArgument(argument);
        plan.valueStorage.push_back(std::move(value));
        return *plan.valueStorage.back();
    }

    IrValue& Constant(std::uint32_t value) {
        auto& constant = Make(IrOpcode::Void, IrType::U32);
        constant.SetImmediateU32(value);
        return constant;
    }

    IrValue& User(std::uint32_t index) {
        auto& reg = Make(IrOpcode::Void, IrType::ScalarReg);
        reg.SetRegister({RegisterBank::Scalar, index + 8});
        return Make(IrOpcode::GetUserData, IrType::U32, {&reg});
    }

    explicit Fixture(bool buffer, std::uint32_t descriptors = 8) {
        plan.userDataBase = 8;
        plan.srtPlanComplete = true;
        plan.resourceTrackingComplete = true;
        MemoryInfo memory{};
        memory.kind = buffer ? ResourceKind::ScalarBuffer : ResourceKind::ScalarAddress;
        plan.memoryInfo.push_back(memory);
        auto& low = User(0);
        auto& high = User(1);
        auto& offset = User(2);
        auto& handle = buffer ? Make(IrOpcode::GetBufferResource, IrType::BufferResource, {&low, &high, &User(3), &User(4)}) : Make(IrOpcode::GetAddressResource, IrType::AddressResource, {&low, &high});
        auto& srt = Make(IrOpcode::GetSrtResource, IrType::SrtResource);
        auto& active = Make(IrOpcode::Void, IrType::Bool);
        active.SetImmediateBool(true);
        for (std::uint32_t i = 0; i < descriptors * 8 + 8; ++i) {
            auto& relative = Make(IrOpcode::IAdd32, IrType::U32, {&offset, &Constant(i * 4)});
            auto& read = buffer ? Make(IrOpcode::ReadConstBuffer, IrType::U32, {&handle, &relative}) : Make(IrOpcode::LoadAddressU32, IrType::U32, {&handle, &relative, &Constant(0), &active});
            plan.srtReads.push_back({&read, i});
            if (i >= descriptors * 8) continue;
            if (i % 8 == 0) {
                plan.materializationSources.push_back(static_cast<std::uint32_t>(plan.descriptorSources.size()));
                plan.descriptorSources.push_back({});
                plan.descriptorSources.back().dwordCount = 8;
            }
            plan.descriptorSources.back().dwords[i % 8] = &Make(IrOpcode::ReadConst, IrType::U32, {&srt, &Constant(i)});
        }
        plan.pureFlatSlots = Detail::ComputePureFlatSlots(plan);
        plan.executionPlan = Detail::CompileSrtExecutionPlan(plan);
        require(plan.executionPlan != nullptr, "ordinary descriptor expressions did not compile");
    }
};

struct Memory {
    std::uint64_t reject = ~std::uint64_t{0};
    std::vector<std::uint64_t> reads;
    bool record = true;

    static bool Read(void* context, std::uint64_t address, std::uint32_t* word) {
        auto& memory = *static_cast<Memory*>(context);
        if (memory.record) memory.reads.push_back(address);
        if (address == memory.reject) return false;
        *word = static_cast<std::uint32_t>((address * 0x9e3779b9u) ^ (address >> 32u));
        return true;
    }
};

void compare(Fixture& fixture, SrtRuntime runtime, bool expected = true) {
    auto& memory = *static_cast<Memory*>(runtime.userContext);
    const auto compiled = std::exchange(fixture.plan.executionPlan, nullptr);
    std::vector<DescriptorValue> reference, actual;
    std::vector<std::uint32_t> referenceFlat, actualFlat;
    std::vector<std::uint8_t> referenceActive, actualActive;
    SrtReadTrace referenceTrace, actualTrace;
    runtime.readTrace = &referenceTrace;
    memory.reads.clear();
    const bool referenceOk = Detail::EvaluateRuntimeSourcesImpl(fixture.plan, fixture.plan.materializationSources, runtime, reference, referenceFlat, true, {}, referenceActive);
    const auto reads = std::move(memory.reads);
    fixture.plan.executionPlan = compiled;
    runtime.readTrace = &actualTrace;
    const bool actualOk = Detail::EvaluateRuntimeSourcesImpl(fixture.plan, fixture.plan.materializationSources, runtime, actual, actualFlat, true, {}, actualActive);
    require(referenceOk == expected && actualOk == expected, "compiled evaluation changed success or failure");
    require(reference == actual && referenceFlat == actualFlat && referenceActive == actualActive, "compiled evaluation changed descriptors, flat values or active sources");
    require(reads == memory.reads, "compiled evaluation changed guest read order");
    require(referenceTrace.leaves == actualTrace.leaves && referenceTrace.otherReads == actualTrace.otherReads, "compiled evaluation changed read dependency classification");
}

void correctness() {
    Memory memory;
    std::array<std::uint32_t, 5> user{0x1000, 0, 0, 4096, 0};
    SrtRuntime runtime{user, 0x3000, Memory::Read, &memory};
    for (const bool buffer : {false, true}) {
        Fixture fixture(buffer);
        for (std::uint32_t iteration = 0; iteration < 128; ++iteration) {
            user[0] = 0x1000 + iteration * 1024;
            user[1] = iteration * 0x4000001u;
            user[2] = iteration % 32;
            compare(fixture, runtime);
        }
        user = {0x1000, 0, 0, 4096, 0};
        memory.reject = 0x1014;
        compare(fixture, runtime, false);
        memory.reject = ~std::uint64_t{0};
        fixture.plan.memoryInfo[0].offset = static_cast<std::uint32_t>(-8);
        fixture.plan.executionPlan = Detail::CompileSrtExecutionPlan(fixture.plan);
        compare(fixture, runtime, !buffer);
        user[0] = 4;
        compare(fixture, runtime, false);
        fixture.plan.memoryInfo[0].offset = 0;
        fixture.plan.executionPlan = Detail::CompileSrtExecutionPlan(fixture.plan);
        user = {0x1000, 0, 0, 8, 0};
        compare(fixture, runtime, !buffer);
        fixture.plan.cleanFlatSlots = {1};
        require(Detail::CompileSrtExecutionPlan(fixture.plan) == nullptr, "specialized memory selected the ordinary execution plan");
        fixture.plan.cleanFlatSlots.clear();
        fixture.plan.controlFlow.push_back({&fixture.Constant(1), {}, {}});
        require(Detail::CompileSrtExecutionPlan(fixture.plan) == nullptr, "dynamic control flow selected the unconditional execution plan");
        fixture.plan.controlFlow = {{nullptr, {0}, {0}}, {nullptr, {}, {1}}};
        fixture.plan.executionPlan = Detail::CompileSrtExecutionPlan(fixture.plan);
        user = {0x1000, 0, 0, 4096, 0};
        compare(fixture, runtime);
    }
    Fixture nested(false, 1);
    auto& first = *nested.plan.srtReads[0].value;
    auto& next = *nested.plan.srtReads[8].value;
    auto& address = nested.Make(IrOpcode::GetAddressResource, IrType::AddressResource, {&first, &nested.Constant(0)});
    next.ReplaceArgument(0, &address);
    nested.plan.pureFlatSlots = Detail::ComputePureFlatSlots(nested.plan);
    nested.plan.executionPlan = Detail::CompileSrtExecutionPlan(nested.plan);
    compare(nested, runtime);
    next.ReplaceArgument(0, &nested.Make(IrOpcode::GetAddressResource, IrType::AddressResource, {&next, &nested.Constant(0)}));
    require(Detail::CompileSrtExecutionPlan(nested.plan) == nullptr, "cyclic expression was compiled");
    for (const bool buffer : {false, true}) {
        Fixture malformed(buffer, 1);
        malformed.plan.srtReads.front().value->AddArgument(&malformed.Constant(0));
        require(Detail::CompileSrtExecutionPlan(malformed.plan) == nullptr, "malformed memory operands selected compiled evaluation");
    }
    Fixture generalAddress(false, 1);
    generalAddress.plan.memoryInfo[0].kind = ResourceKind::Global;
    require(Detail::CompileSrtExecutionPlan(generalAddress.plan) == nullptr, "non-scalar memory selected scalar descriptor evaluation");
    Fixture large(false, 32);
    compare(large, runtime);
    Fixture arithmetic(false, 1);
    arithmetic.plan.descriptorSources[0].dwordCount = 1;
    arithmetic.plan.srtReads.resize(1);
    auto* expression = &arithmetic.Make(IrOpcode::CompositeConstructU64, IrType::U64, {&arithmetic.User(0), &arithmetic.User(1)});
    auto& operand = arithmetic.User(2);
    for (const auto op : {IrOpcode::IAdd32, IrOpcode::IAdd64, IrOpcode::ISub32, IrOpcode::ISub64, IrOpcode::IMul32, IrOpcode::IMul64, IrOpcode::UMin32, IrOpcode::BitwiseAnd32, IrOpcode::BitwiseAnd64, IrOpcode::BitwiseOr32, IrOpcode::BitwiseXor32, IrOpcode::ShiftLeftLogical32, IrOpcode::ShiftLeftLogical64, IrOpcode::ShiftRightLogical32, IrOpcode::ShiftRightLogical64}) {
        expression = &arithmetic.Make(op, IrType::U64, {expression, &operand});
        arithmetic.plan.srtReads[0].value = expression;
        arithmetic.plan.executionPlan = Detail::CompileSrtExecutionPlan(arithmetic.plan);
        require(arithmetic.plan.executionPlan != nullptr, "integer expression did not compile");
        for (const auto word : {0u, 1u, 31u, 32u, 63u, 64u, 0x80000000u, 0xffffffffu}) {
            user = {word, ~word, word, 4096, 0};
            compare(arithmetic, runtime);
        }
    }
    for (const auto op : {IrOpcode::CompositeExtractU64, IrOpcode::CompositeExtractU32x2}) {
        auto& source = arithmetic.Make(op == IrOpcode::CompositeExtractU64 ? IrOpcode::CompositeConstructU64 : IrOpcode::IAddCarry32, IrType::U64, {&arithmetic.User(0), &arithmetic.User(1)});
        for (std::uint32_t component = 0; component < 2; ++component) {
            arithmetic.plan.srtReads[0].value = &arithmetic.Make(op, IrType::U32, {&source, &arithmetic.Constant(component)});
            arithmetic.plan.executionPlan = Detail::CompileSrtExecutionPlan(arithmetic.plan);
            require(arithmetic.plan.executionPlan != nullptr, "packed arithmetic did not compile");
            user = {0xffffffffu, 1u, 0, 4096, 0};
            compare(arithmetic, runtime);
        }
    }
    arithmetic.plan.srtReads[0].value = &arithmetic.Make(IrOpcode::BitwiseNot32, IrType::U32, {&arithmetic.User(0)});
    arithmetic.plan.executionPlan = Detail::CompileSrtExecutionPlan(arithmetic.plan);
    compare(arithmetic, runtime);
    arithmetic.plan.srtReads[0].value = &arithmetic.Make(IrOpcode::SelectU32, IrType::U32, {&arithmetic.User(0), &arithmetic.User(1), &arithmetic.User(2)});
    require(Detail::CompileSrtExecutionPlan(arithmetic.plan) == nullptr, "unsupported expression bypassed the generic evaluator");
    std::cout << "Compiled SRT values, read ordering, dependencies, failures, cycles and generic fallback passed\n";
}

void benchmark() {
    for (const bool buffer : {false, true}) {
        Fixture fixture(buffer);
        const auto compiled = fixture.plan.executionPlan;
        Memory memory;
        memory.record = false;
        std::array<std::uint32_t, 5> user{0x1000, 0, 0, 4096, 0};
        SrtRuntime runtime{user, 0x3000, Memory::Read, &memory};
        std::array<std::vector<double>, 2> samples;
        std::uint64_t checksum = 0;
        for (unsigned pass = 0; pass < 10; ++pass) {
            for (unsigned step = 0; step < 2; ++step) {
                const auto mode = (pass + step) % 2;
                fixture.plan.executionPlan = mode ? compiled : nullptr;
                const auto started = std::chrono::steady_clock::now();
                for (unsigned draw = 0; draw < 4096; ++draw) {
                    user[0] = 0x1000 + (draw % 32) * 4096;
                    std::vector<DescriptorValue> values;
                    std::vector<std::uint32_t> flat;
                    std::vector<std::uint8_t> active;
                    if (!Detail::EvaluateRuntimeSourcesImpl(fixture.plan, fixture.plan.materializationSources, runtime, values, flat, true, {}, active)) throw std::runtime_error("benchmark evaluation failed");
                    checksum += values[0].dwords[0] + flat.back();
                }
                if (pass != 0) samples[mode].push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count() / 4096);
            }
        }
        for (auto& sample : samples) std::ranges::sort(sample);
        std::cout << (buffer ? "Scalar buffer" : "Scalar address") << " SRT: generic " << samples[0][4] << " us, compiled " << samples[1][4] << " us, p95 " << samples[0].back() << '/' << samples[1].back() << " us, checksum " << checksum << '\n';
    }
}

}

int main(int argc, char** argv) {
    try {
        correctness();
        if (argc == 2 && std::string_view(argv[1]) == "--benchmark") benchmark();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
