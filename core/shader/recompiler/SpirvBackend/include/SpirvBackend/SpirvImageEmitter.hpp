#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVIMAGEEMITTER_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVIMAGEEMITTER_HPP

#include "IntermediateRepresentation/IrValue.hpp"
#include "SpirvBackend/SpirvModule.hpp"
#include <cstdint>

#include "SpirvBackend/SpirvEmitterState.hpp"

namespace ShaderRecompiler {

struct TableWord {
    std::uint32_t code = 0;
    std::uint32_t addressLow = 0;
    std::uint32_t addressHigh = 0;
    std::uint32_t poisoned = 0;
};

std::uint32_t LoadTableMap(SpirvEmitterState& state, std::uint32_t index);
std::uint32_t TableMapAt(SpirvEmitterState& state, std::uint32_t base, std::uint32_t offset);
TableWord EmitTableWord(SpirvValueEmitContext& ctx, const IrValue& inst, std::uint32_t source, std::uint32_t table, std::size_t argument);
void EmitTableFault(SpirvValueEmitContext& ctx, const IrValue& inst, const TableWord& word, std::uint32_t poison, std::uint32_t condition);

void EmitImageOperation(SpirvModule& module, const IrValue& value, std::uint32_t resultId);

void EmitImageOperation(SpirvValueEmitContext& context, const IrValue& value, std::uint32_t resultId);

}

#endif
