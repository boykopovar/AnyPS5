#include "SpirvBackend/SpirvAnalysis.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

SpirvRequirements AnalyzeProgramRequirements(const IrProgram& program) {
    SpirvRequirements requirements {};
    for (const IrBlock* block : program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) {
            if (BufferAccessOf(inst->Opcode()) == BufferAccess::Atomic && inst->Type() == IrType::U64) {
                requirements.bufferInt64Atomics = true;
            }
            const auto addressAccess = AddressOpcodeInfoOf(inst->Opcode()).access;
            if (addressAccess != AddressAccess::None) {
                const auto memoryIndex = inst->Flags<MemoryFlags>().index;
                if (memoryIndex >= program.Resources().memoryInfo.size()) {
                    throw std::runtime_error("address operation has invalid memory metadata");
                }
                if (program.Resources().memoryInfo.at(memoryIndex).kind == ResourceKind::Scratch) {
                    if (program.Info().scratchDwords == 0u) {
                        throw std::runtime_error("scratch operation has no per-thread storage");
                    }
                    requirements.functionScratch = true;
                } else if (addressAccess == AddressAccess::Write) {
                    throw std::runtime_error("writable FLAT/GLOBAL addresses require GPU ownership tracking");
                }
            }
            if (BufferAccessOf(inst->Opcode()) != BufferAccess::None) {
                const auto memoryIndex = inst->Flags<MemoryFlags>().index;
                if (memoryIndex >= program.Resources().memoryInfo.size()) {
                    throw std::runtime_error("buffer operation has invalid memory metadata");
                }
                const auto& memory = program.Resources().memoryInfo.at(memoryIndex);
                if (memory.gpuDescriptor) {
                    // The V#'s ADD_TID is known at run time only.
                    if (program.Resources().stage != IrShaderStage::Compute) {
                        throw std::runtime_error("GPU-selected buffer descriptors outside compute shaders are not implemented");
                    }
                    requirements.subgroupLocalInvocationId = true;
                } else if (memory.kind == ResourceKind::Buffer) {
                    if (memory.resource >= program.Info().buffers.size()) {
                        throw std::runtime_error("buffer operation has invalid resource metadata");
                    }
                    if ((program.Info().buffers.at(memory.resource).packedStride & (1u << 20u)) != 0u) {
                        if (program.Resources().stage != IrShaderStage::Compute) {
                            throw std::runtime_error("buffer ADD_TID is only valid for compute shaders");
                        }
                        requirements.subgroupLocalInvocationId = true;
                    }
                }
            }
            const auto sharedAccess = SharedAccessOf(inst->Opcode());
            if (sharedAccess != SharedAccess::None) {
                const auto index = inst->Flags<MemoryFlags>().index;
                if (index >= program.Resources().memoryInfo.size()) {
                    throw std::runtime_error("shared operation has invalid memory metadata");
                }
                const auto kind = program.Resources().memoryInfo.at(index).kind;
                if (kind != ResourceKind::Lds && kind != ResourceKind::Gds) {
                    throw std::runtime_error("shared operation has invalid resource kind");
                }
                if (program.Resources().stage != IrShaderStage::Compute && program.Resources().stage != IrShaderStage::Mesh && kind == ResourceKind::Lds) {
                    requirements.functionLds = true;
                }
                if (sharedAccess == SharedAccess::Append || sharedAccess == SharedAccess::Consume) {
                    requirements.subgroupBallot = true;
                    requirements.subgroupShuffle = true;
                    requirements.subgroupLocalInvocationId = true;
                }
            }
            switch (inst->Opcode()) {
            case IrOpcode::Ballot:
                requirements.subgroupBallot = true;
                break;
            case IrOpcode::DppMoveU32:
            case IrOpcode::ReadFirstLane:
            case IrOpcode::ReadLane: {
                requirements.subgroupBallot = true;
                requirements.subgroupShuffle = true;
                if (inst->Opcode() == IrOpcode::DppMoveU32) {
                    requirements.subgroupLocalInvocationId = true;
                }
                break;
            }
            case IrOpcode::DppUpdateU32:
            case IrOpcode::WriteLane: {
                requirements.subgroupBallot = true;
                requirements.subgroupLocalInvocationId = true;
                break;
            }
            case IrOpcode::Permlane16U32: {
                requirements.subgroupBallot = true;
                requirements.subgroupShuffle = true;
                requirements.subgroupLocalInvocationId = true;
                break;
            }
            case IrOpcode::SwizzleU32:
            case IrOpcode::BpermuteU32: {
                requirements.subgroupBallot = true;
                requirements.subgroupShuffle = true;
                requirements.subgroupLocalInvocationId = true;
                break;
            }
            case IrOpcode::LaneId:
                requirements.subgroupLocalInvocationId = requirements.subgroupLocalInvocationId || program.Resources().stage != IrShaderStage::TessellationControl;
                break;
            case IrOpcode::ImageQueryLod:
                requirements.computeDerivatives = true;
                break;
            case IrOpcode::ImageGatherRaw:
                requirements.imageGatherExtended = true;
                break;
            case IrOpcode::SetAttribute: {
                const auto index = inst->Flags<ExportFlags>().index;
                if (index >= program.Metadata().exportInfo.size()) {
                    throw std::runtime_error("attribute export has invalid metadata");
                }
                if (program.Resources().stage == IrShaderStage::Pixel && program.Metadata().exportInfo.at(index).vm) {
                    requirements.pixelValidMask = true;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    return requirements;
}

}
