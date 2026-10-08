#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTEVALUATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTEVALUATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <span>
#include <vector>

namespace ShaderRecompiler::Detail {

class EvaluatedValues {
public:
    bool Find(const IrValue* key, std::uint64_t& value) const {
        if (_slots.empty()) {
            return false;
        }
        for (std::size_t index = Home(key);; index = (index + 1u) & (_slots.size() - 1u)) {
            const auto& slot = _slots[index];
            if (slot.key == key) {
                value = slot.value;
                return true;
            }
            if (slot.key == nullptr) {
                return false;
            }
        }
    }
    void Insert(const IrValue* key, std::uint64_t value) {
        if ((_count + 1u) * 2u > _slots.size()) {
            Grow();
        }
        for (std::size_t index = Home(key);; index = (index + 1u) & (_slots.size() - 1u)) {
            auto& slot = _slots[index];
            if (slot.key == key) {
                return;
            }
            if (slot.key == nullptr) {
                slot = {key, value};
                ++_count;
                return;
            }
        }
    }

private:
    struct Slot {
        const IrValue* key = nullptr;
        std::uint64_t value = 0;
    };
    std::size_t Home(const IrValue* key) const {
        return static_cast<std::size_t>((reinterpret_cast<std::uintptr_t>(key) >> 4u) * 0x9e3779b97f4a7c15ull >> 32u) & (_slots.size() - 1u);
    }
    void Grow() {
        std::vector<Slot> previous(_slots.empty() ? 64u : _slots.size() * 2u);
        previous.swap(_slots);
        _count = 0;
        for (const auto& slot : previous) {
            if (slot.key != nullptr) {
                Insert(slot.key, slot.value);
            }
        }
    }
    std::vector<Slot> _slots;
    std::size_t _count = 0;
};

class DenseValues {
public:
    struct Slot {
        std::uint64_t stamp = 0;
        std::uint64_t value = 0;
    };
    explicit DenseValues(std::size_t values);
    ~DenseValues();
    DenseValues(const DenseValues&) = delete;
    DenseValues& operator=(const DenseValues&) = delete;
    Slot& At(std::uint32_t id) { return _slots[id]; }
    [[nodiscard]] std::size_t Size() const { return _size; }
    [[nodiscard]] std::uint64_t Done() const { return _stamp; }
    [[nodiscard]] std::uint64_t Visiting() const { return _stamp + 1u; }

private:
    std::vector<Slot>* _array = nullptr;
    Slot* _slots = nullptr;
    std::size_t _size = 0;
    std::uint64_t _stamp = 0;
};

class IrNode {
public:
    IrNode() = default;
    explicit IrNode(IrValue* value) : _value(value->Resolve()) {}
    static IrNode From(const IrResourcePlan&, IrValue* value) { return IrNode(value); }
    static IrNode SrtRead(const IrResourcePlan& program, std::uint32_t slot) { return IrNode(program.srtReads[slot].value); }
    static IrNode SourceDword(const IrResourcePlan& program, std::uint32_t source, std::uint32_t dword) { return IrNode(program.descriptorSources[source].dwords[dword]); }
    static bool Condition(const IrResourcePlan& program, std::uint32_t block, IrNode& condition) {
        if (program.controlFlow[block].condition == nullptr) return false;
        condition = IrNode(program.controlFlow[block].condition);
        return true;
    }
    [[nodiscard]] IrOpcode Opcode() const { return _value->Opcode(); }
    [[nodiscard]] IrType Type() const { return _value->Type(); }
    [[nodiscard]] bool HasImmediate() const { return _value->HasImmediate(); }
    [[nodiscard]] std::uint64_t ImmediateBits() const { return _value->ImmediateU64(); }
    [[nodiscard]] std::size_t ArgumentCount() const { return _value->ArgumentCount(); }
    [[nodiscard]] IrNode Argument(std::size_t index) const { return IrNode(_value->Argument(index)); }
    template<typename TValue>
    [[nodiscard]] TValue Flags() const { return _value->Flags<TValue>(); }
    [[nodiscard]] std::uint32_t RegisterIndex() const { return _value->Register().index; }
    [[nodiscard]] std::uint32_t Id() const { return _value->Id(); }
    [[nodiscard]] IrValue* Value() const { return _value; }
    bool operator==(const IrNode& other) const { return _value == other._value; }

private:
    IrValue* _value = nullptr;
};

class CompactNode {
public:
    CompactNode() = default;
    CompactNode(const IrResourcePlan& program, std::uint32_t id) : _program(&program), _id(id) {}
    static CompactNode From(const IrResourcePlan& program, IrValue* value) { return {program, value->Resolve()->Id()}; }
    static CompactNode SrtRead(const IrResourcePlan& program, std::uint32_t slot) { return {program, program.compact.srtReads[slot]}; }
    static CompactNode SourceDword(const IrResourcePlan& program, std::uint32_t source, std::uint32_t dword) { return {program, program.compact.sourceDwords[source * 8u + dword]}; }
    static bool Condition(const IrResourcePlan& program, std::uint32_t block, CompactNode& condition) {
        if (program.compact.conditions[block] == CompactResourcePlan::NoValue) return false;
        condition = {program, program.compact.conditions[block]};
        return true;
    }
    [[nodiscard]] IrOpcode Opcode() const { return entry().opcode; }
    [[nodiscard]] IrType Type() const { return entry().type; }
    [[nodiscard]] bool HasImmediate() const { return entry().hasImmediate; }
    [[nodiscard]] std::uint64_t ImmediateBits() const { return entry().immediate; }
    [[nodiscard]] std::size_t ArgumentCount() const { return entry().argumentCount; }
    [[nodiscard]] CompactNode Argument(std::size_t index) const {
        const auto& value = entry();
        if (index >= value.argumentCount) throw std::out_of_range("IrValue::Argument index is out of range");
        return {*_program, _program->compact.arguments[value.firstArgument + index]};
    }
    template<typename TValue>
    [[nodiscard]] TValue Flags() const {
        TValue result{};
        std::memcpy(&result, &entry().flags, sizeof(result));
        return result;
    }
    [[nodiscard]] std::uint32_t RegisterIndex() const { return entry().registerIndex; }
    [[nodiscard]] std::uint32_t Id() const { return _id; }
    [[nodiscard]] IrValue* Value() const { return _program->valueStorage[_id].get(); }
    bool operator==(const CompactNode& other) const { return _id == other._id; }

private:
    [[nodiscard]] const CompactPlanValue& entry() const { return _program->compact.values[_id]; }

    const IrResourcePlan* _program = nullptr;
    std::uint32_t _id = 0;
};

template<typename TNode>
class BasicEvaluator {
public:
    BasicEvaluator(const IrResourcePlan& program, const SrtRuntime& runtime, std::span<const std::uint8_t> cleanFlatSlots = {}, BasicEvaluator* cleanEvaluator = nullptr, const TNode* activeMask = nullptr) : _program(program), _runtime(runtime), _cleanFlatSlots(cleanFlatSlots), _cleanEvaluator(cleanEvaluator), _activeMask(activeMask != nullptr ? *activeMask : TNode{}), _masked(activeMask != nullptr), _dense(program.denseValueIds ? program.valueStorage.size() : 0u) {}
    BasicEvaluator(const BasicEvaluator&) = delete;
    BasicEvaluator& operator=(const BasicEvaluator&) = delete;

    bool Evaluate(TNode value, std::uint32_t& result);
    bool EvaluateWide(TNode value, std::uint64_t& result);

private:
    static float Float32(std::uint64_t bits);
    static std::uint64_t Float32Bits(float value);

    bool Arg(const TNode& inst, std::size_t index, std::uint64_t& result);
    bool EvaluatePhi(const TNode& inst, std::uint64_t& result);
    bool EvaluateExtract(const TNode& inst, std::uint64_t& result);
    bool EvaluateRawRead(const TNode& inst, std::uint64_t& result);
    bool EvaluateInst(const TNode& inst, std::uint64_t& result);

    const IrResourcePlan& _program;
    const SrtRuntime& _runtime;
    std::span<const std::uint8_t> _cleanFlatSlots;
    BasicEvaluator* _cleanEvaluator = nullptr;
    TNode _activeMask;
    bool _masked = false;
    EvaluatedValues _cache;
    std::vector<IrValue*> _visiting;
    DenseValues _dense;
};

using Evaluator = BasicEvaluator<IrNode>;
using CompactEvaluator = BasicEvaluator<CompactNode>;

}

#endif
