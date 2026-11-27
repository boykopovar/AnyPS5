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
bool HoldsFoldableValue(IrValue* value, std::uint32_t depth);

bool ActiveMaskImplies(const IrResourcePlan& program, const IrValue* mask, const IrValue* condition);

inline bool ActiveMaskImplies(const IrResourcePlan& program, IrValue* mask, IrValue* condition) {
    return ActiveMaskImplies(program, static_cast<const IrValue*>(mask), static_cast<const IrValue*>(condition));
}

inline bool ActiveMaskImplies(const IrResourcePlan& program, const IrValue* mask, IrValue* condition) {
    return ActiveMaskImplies(program, mask, static_cast<const IrValue*>(condition));
}

inline bool ActiveMaskImplies(const IrResourcePlan& program, IrValue* mask, const IrValue* condition) {
    return ActiveMaskImplies(program, static_cast<const IrValue*>(mask), condition);
}

template<typename TNode>
requires (!std::is_pointer_v<std::decay_t<TNode>>)
bool ActiveMaskImplies(const IrResourcePlan& program, const TNode& mask, const TNode& condition) {
    return ActiveMaskImplies(program, mask.Value(), condition.Value());
}

}

#endif
