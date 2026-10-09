#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTEVALUATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTEVALUATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"

#include <cstdint>
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

struct InaccessibleRead {
    const IrValue* read = nullptr;
    std::uint64_t address = 0;
};

class Evaluator {
public:
    Evaluator(const IrResourcePlan& program, const SrtRuntime& runtime, std::span<const std::uint8_t> cleanFlatSlots = {}, Evaluator* cleanEvaluator = nullptr, IrValue* activeMask = nullptr) : _program(program), _runtime(runtime), _cleanFlatSlots(cleanFlatSlots), _cleanEvaluator(cleanEvaluator), _activeMask(activeMask != nullptr ? activeMask->Resolve() : nullptr) {}

    bool Evaluate(IrValue* value, std::uint32_t& result);
    bool EvaluateWide(IrValue* raw, std::uint64_t& result);
    void ReportInaccessibleReads(InaccessibleRead* sink) { _inaccessible = sink; }

private:
    static float Float32(std::uint64_t bits);
    static std::uint64_t Float32Bits(float value);

    bool Arg(IrValue& inst, std::size_t index, std::uint64_t& result);
    bool EvaluatePhi(IrValue& inst, std::uint64_t& result);
    bool EvaluateExtract(IrValue& inst, std::uint64_t& result);
    bool EvaluateRawRead(IrValue& inst, std::uint64_t& result);
    bool EvaluateInst(IrValue& inst, std::uint64_t& result);

    const IrResourcePlan& _program;
    const SrtRuntime& _runtime;
    std::span<const std::uint8_t> _cleanFlatSlots;
    Evaluator* _cleanEvaluator = nullptr;
    IrValue* _activeMask = nullptr;
    InaccessibleRead* _inaccessible = nullptr;
    EvaluatedValues _cache;
    std::vector<IrValue*> _visiting;
};

}

#endif
