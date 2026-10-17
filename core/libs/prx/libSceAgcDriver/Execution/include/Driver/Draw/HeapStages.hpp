#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_HEAPSTAGES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_HEAPSTAGES_HPP

#include "Optimization/ResourceProgram.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

bool HeapFragmentsEnabled();
bool HeapRefused();
bool HeapStageEligible(ShaderRecompiler::ShaderStage stage, const ShaderRecompiler::IrResourcePlan* plan, bool gpuConstantsVertex);
std::optional<bool> HeapFragmentKnown(const void* snapshot, std::size_t codeOffset);
void NoteHeapFragment(const void* snapshot, std::size_t codeOffset, bool eligible);

bool SampleHeapStageVerify();

std::shared_ptr<const ShaderRecompiler::RecompileResult> MaterializeHeapStage(const ShaderRecompiler::PreparedShaderInvocation& invocation, std::span<const std::uint32_t> userData, std::uint64_t shaderBase, ShaderRecompiler::HeapStageWords* words = nullptr, std::shared_ptr<const ShaderRecompiler::HeapStageWords>* sharedWords = nullptr);
void NoteHeapDirect();
void NoteHeapDirectVerify(const ShaderRecompiler::RecompileResult& direct, const ShaderRecompiler::RecompileResult& expected, const ShaderRecompiler::ResourceCapture* expectedCapture = nullptr);
std::uint64_t HeapStagesTaken();
std::shared_ptr<const ShaderRecompiler::RecompileResult> PatchedHeapResult(const ShaderRecompiler::RecompileResult& shared, const ShaderRecompiler::HeapStageWords& words);
void VerifyHeapAccessor(const Graphics::CompiledShader& stage);
void VerifyRingWords(const ShaderRecompiler::RecompileResult& kept, const ShaderRecompiler::HeapStageWords& ring, const ShaderRecompiler::RecompileResult& fresh, const ShaderRecompiler::HeapStageWords* freshWords);

}

#endif
