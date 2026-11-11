#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_HEAPSTAGES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_HEAPSTAGES_HPP

#include "Optimization/ResourceProgram.hpp"
#include "UserDataUse.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

bool HeapStageExcluded(ShaderRecompiler::ShaderStage stage);
bool HeapPreRasterStage(ShaderRecompiler::ShaderStage stage);
const char* HeapStageIneligibility(ShaderRecompiler::ShaderStage stage, const ShaderRecompiler::IrResourcePlan* plan);
bool HeapStageEligible(ShaderRecompiler::ShaderStage stage, const ShaderRecompiler::IrResourcePlan* plan);
bool HeapPlanExcluded(const ShaderRecompiler::IrResourcePlan& plan);
bool HeapClassesExcluded();
void RequireHeapStageMappable(const ShaderRecompiler::RecompileResult& result, bool workgroupAddress = false);
bool HeapStageFits(const ShaderRecompiler::RecompileResult& result, bool workgroupAddress = false);

bool SampleHeapStageVerify();

struct HeapStageReads {
    std::shared_ptr<const std::vector<std::pair<std::uint64_t, std::uint64_t>>> ranges;
    std::shared_ptr<const std::vector<std::uint32_t>> values;
    ShaderRecompiler::SrtUserDataUse userDataUse;
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> capture;
    bool complete = false;
};

struct HeapStageMaterialized {
    std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
    std::shared_ptr<const ShaderRecompiler::HeapStageWords> words;
    HeapStageReads reads;
};

void ResetHeapStageWords(ShaderRecompiler::HeapStageWords& words);

void AppendWalkRead(std::vector<std::pair<std::uint64_t, std::uint64_t>>& runs, std::uint64_t address);

void CoalesceWalkReads(std::vector<std::pair<std::uint64_t, std::uint64_t>>& runs, std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges);

HeapStageMaterialized MaterializeHeapStage(const ShaderRecompiler::PreparedShaderInvocation& invocation, std::span<const std::uint32_t> userData,
                                           std::uint64_t shaderBase);
void NoteHeapDirect();
void NoteHeapDirectVerify(const ShaderRecompiler::RecompileResult& direct, const ShaderRecompiler::RecompileResult& expected,
                          const ShaderRecompiler::ResourceCapture* expectedCapture = nullptr);
std::uint64_t HeapStagesTaken();

std::uint64_t NextHeapDataOrigin();
std::uint64_t HeapRingHitsVerified();
std::uint64_t HeapRingHitsDiffering();
std::shared_ptr<const ShaderRecompiler::RecompileResult> PatchedHeapResult(const ShaderRecompiler::RecompileResult& shared,
                                                                           const ShaderRecompiler::HeapStageWords& words);
void VerifyHeapAccessor(const Graphics::CompiledShader& stage);
void VerifyRingWords(const ShaderRecompiler::RecompileResult& kept, const ShaderRecompiler::HeapStageWords& ring,
                     const ShaderRecompiler::RecompileResult& fresh, const ShaderRecompiler::HeapStageWords* freshWords);

}

#endif
