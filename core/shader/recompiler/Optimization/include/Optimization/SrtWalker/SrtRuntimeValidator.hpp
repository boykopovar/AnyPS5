#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTRUNTIMEVALIDATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTRUNTIMEVALIDATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/SrtWalker.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <unordered_set>
#include <vector>

namespace ShaderRecompiler::Detail {

class RuntimeValidator {
public:
    RuntimeValidator(const IrResourcePlan& program, RuntimeValueType type) : _program(program), _type(type) {}

    bool Run(IrValue* value) { return Validate(value); }

private:
    class PointerSet {
    public:
        bool contains(IrValue* value) const {
            for (std::size_t index = 0; index < _inlineSize; ++index) {
                if (_inlineValues[index] == value) return true;
            }
            return _hashed ? _overflowSet.contains(value) : std::find(_overflow.begin(), _overflow.end(), value) != _overflow.end();
        }

        bool insert(IrValue* value) {
            if (contains(value)) return false;
            if (_inlineSize < _inlineValues.size()) {
                _inlineValues[_inlineSize++] = value;
            } else if (_hashed) {
                _overflowSet.insert(value);
            } else {
                _overflow.push_back(value);
                if (_overflow.size() > 32u) {
                    _overflowSet.insert(_overflow.begin(), _overflow.end());
                    _overflow.clear();
                    _hashed = true;
                }
            }
            return true;
        }

        bool erase(IrValue* value) {
            for (std::size_t index = 0; index < _inlineSize; ++index) {
                if (_inlineValues[index] != value) continue;
                --_inlineSize;
                _inlineValues[index] = _inlineValues[_inlineSize];
                if (!_hashed && !_overflow.empty()) {
                    _inlineValues[_inlineSize++] = _overflow.back();
                    _overflow.pop_back();
                }
                return true;
            }
            if (_hashed) return _overflowSet.erase(value) != 0;
            for (std::size_t index = 0; index < _overflow.size(); ++index) {
                if (_overflow[index] != value) continue;
                _overflow[index] = _overflow.back();
                _overflow.pop_back();
                return true;
            }
            return false;
        }

    private:
        std::array<IrValue*, 8> _inlineValues{};
        std::size_t _inlineSize = 0;
        std::vector<IrValue*> _overflow;
        std::unordered_set<IrValue*> _overflowSet;
        bool _hashed = false;
    };

    bool ValidateArguments(IrValue& inst, bool requireUniform);
    bool Validate(IrValue* raw, bool requireUniform = true);

    const IrResourcePlan& _program;
    RuntimeValueType _type;
    IrValue* _activeMask = nullptr;
    PointerSet _visiting;
    PointerSet _validatedDependencies;
};

}

#endif
