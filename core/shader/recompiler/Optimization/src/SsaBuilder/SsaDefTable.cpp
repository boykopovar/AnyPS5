#include "Optimization/SsaBuilder/SsaDefTable.hpp"
#include <stdexcept>

namespace ShaderRecompiler::Detail {

IrValue* DefTable::Get(IrBlock& block, ScalarReg reg) const {
    const auto index = RegIndex(reg);
    if (index >= NumScalarRegs) {
        throw std::out_of_range("DefTable::Get scalar register index is out of range");
    }
    return block.ssaScalarValues[index];
}

void DefTable::Set(IrBlock& block, ScalarReg reg, IrValue* value) {
    const auto index = RegIndex(reg);
    if (index >= NumScalarRegs) {
        throw std::out_of_range("DefTable::Set scalar register index is out of range");
    }
    block.ssaScalarValues[index] = value;
}

IrValue* DefTable::Get(IrBlock& block, ThreadBitScalarReg variable) const {
    const auto index = RegIndex(variable.reg);
    if (index >= NumScalarRegs) {
        throw std::out_of_range("DefTable::Get thread bit scalar register index is out of range");
    }
    return block.ssaThreadBitScalarValues[index];
}

void DefTable::Set(IrBlock& block, ThreadBitScalarReg variable, IrValue* value) {
    const auto index = RegIndex(variable.reg);
    if (index >= NumScalarRegs) {
        throw std::out_of_range("DefTable::Set thread bit scalar register index is out of range");
    }
    block.ssaThreadBitScalarValues[index] = value;
}

IrValue* DefTable::Get(IrBlock& block, ScalarMaskTag variable) const {
    const auto index = RegIndex(variable.reg);
    if (index >= NumScalarRegs) {
        throw std::out_of_range("DefTable::Get scalar mask tag index is out of range");
    }
    return block.ssaScalarMaskTags[index];
}

void DefTable::Set(IrBlock& block, ScalarMaskTag variable, IrValue* value) {
    const auto index = RegIndex(variable.reg);
    if (index >= NumScalarRegs) {
        throw std::out_of_range("DefTable::Set scalar mask tag index is out of range");
    }
    block.ssaScalarMaskTags[index] = value;
}

IrValue* DefTable::Get(IrBlock& block, VectorReg reg) const {
    return block.ssaVectorValues[RegIndex(reg)];
}

void DefTable::Set(IrBlock& block, VectorReg reg, IrValue* value) {
    block.ssaVectorValues[RegIndex(reg)] = value;
}

IrValue* DefTable::Get(IrBlock& block, GotoVariable variable) const {
    const auto found = _gotoVariables.find(variable.index);
    if (found == _gotoVariables.end()) {
        return nullptr;
    }
    const auto blockFound = found->second.find(&block);
    if (blockFound == found->second.end()) {
        return nullptr;
    }
    return blockFound->second;
}

void DefTable::Set(IrBlock& block, GotoVariable variable, IrValue* value) {
    _gotoVariables[variable.index][&block] = value;
}

IrValue* DefTable::Get(IrBlock& block, SccTag) const {
    const auto found = _scc.find(&block);
    return found == _scc.end() ? nullptr : found->second;
}

void DefTable::Set(IrBlock& block, SccTag, IrValue* value) {
    _scc[&block] = value;
}

IrValue* DefTable::Get(IrBlock& block, ExecTag) const {
    const auto found = _exec.find(&block);
    return found == _exec.end() ? nullptr : found->second;
}

void DefTable::Set(IrBlock& block, ExecTag, IrValue* value) {
    _exec[&block] = value;
}

IrValue* DefTable::Get(IrBlock& block, ExecLoTag) const {
    const auto found = _execLo.find(&block);
    return found == _execLo.end() ? nullptr : found->second;
}

void DefTable::Set(IrBlock& block, ExecLoTag, IrValue* value) {
    _execLo[&block] = value;
}

IrValue* DefTable::Get(IrBlock& block, ExecHiTag) const {
    const auto found = _execHi.find(&block);
    return found == _execHi.end() ? nullptr : found->second;
}

void DefTable::Set(IrBlock& block, ExecHiTag, IrValue* value) {
    _execHi[&block] = value;
}

IrValue* DefTable::Get(IrBlock& block, VccTag) const {
    const auto found = _vcc.find(&block);
    return found == _vcc.end() ? nullptr : found->second;
}

void DefTable::Set(IrBlock& block, VccTag, IrValue* value) {
    _vcc[&block] = value;
}

IrValue* DefTable::Get(IrBlock& block, VccLoTag) const {
    const auto found = _vccLo.find(&block);
    return found == _vccLo.end() ? nullptr : found->second;
}

void DefTable::Set(IrBlock& block, VccLoTag, IrValue* value) {
    _vccLo[&block] = value;
}

IrValue* DefTable::Get(IrBlock& block, VccHiTag) const {
    const auto found = _vccHi.find(&block);
    return found == _vccHi.end() ? nullptr : found->second;
}

void DefTable::Set(IrBlock& block, VccHiTag, IrValue* value) {
    _vccHi[&block] = value;
}

IrValue* DefTable::Get(IrBlock& block, M0Tag) const {
    const auto found = _m0.find(&block);
    return found == _m0.end() ? nullptr : found->second;
}

void DefTable::Set(IrBlock& block, M0Tag, IrValue* value) {
    _m0[&block] = value;
}

}
