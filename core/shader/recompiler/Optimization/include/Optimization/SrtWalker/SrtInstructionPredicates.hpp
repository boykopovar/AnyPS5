#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTINSTRUCTIONPREDICATES_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTINSTRUCTIONPREDICATES_HPP

#include "IntermediateRepresentation/IrProgram.hpp"

#include <concepts>
#include <type_traits>

namespace ShaderRecompiler::Detail {

bool IsRawRead(const IrResourcePlan& program, const IrValue& inst);
bool IsRawRead(const IrResourcePlan& program, IrOpcode op, std::uint32_t memoryIndex);
bool IsMemoryRead(IrOpcode opcode);
bool LoadedFromMemory(const IrValue& handle);
bool IsDescriptorHandle(IrOpcode opcode);
bool IsRuntimeSelect(IrOpcode opcode);
bool IsRuntimeUniformOp(IrOpcode opcode);

bool ActiveMaskImplies(const IrValue* mask, const IrValue* condition);

inline bool ActiveMaskImplies(IrValue* mask, IrValue* condition) {
    return ActiveMaskImplies(static_cast<const IrValue*>(mask), static_cast<const IrValue*>(condition));
}

inline bool ActiveMaskImplies(const IrValue* mask, IrValue* condition) {
    return ActiveMaskImplies(mask, static_cast<const IrValue*>(condition));
}

inline bool ActiveMaskImplies(IrValue* mask, const IrValue* condition) {
    return ActiveMaskImplies(static_cast<const IrValue*>(mask), condition);
}

template<typename TNode>
requires (!std::is_pointer_v<std::decay_t<TNode>>)
bool ActiveMaskImplies(const TNode& mask, const TNode& condition) {
    return ActiveMaskImplies(mask.Value(), condition.Value());
}

}

#endif
