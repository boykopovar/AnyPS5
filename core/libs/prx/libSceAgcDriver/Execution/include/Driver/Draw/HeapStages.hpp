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

namespace AgcDriver {
class VulkanDevice;
}

namespace AgcDriver::DriverDetail {

bool HeapFragmentsEnabled();
bool HeapRefused();
bool HeapStageExcluded(ShaderRecompiler::ShaderStage stage);
bool HeapPreRasterStage(ShaderRecompiler::ShaderStage stage);
Graphics::HeapStageReason HeapStageIneligibility(ShaderRecompiler::ShaderStage stage, const ShaderRecompiler::IrResourcePlan* plan,
                                                 bool heapVertex);
bool HeapStageEligible(ShaderRecompiler::ShaderStage stage, const ShaderRecompiler::IrResourcePlan* plan, bool heapVertex);
std::optional<bool> HeapFragmentKnown(const void* snapshot, std::size_t codeOffset);
void NoteHeapFragment(const void* snapshot, std::size_t codeOffset, bool eligible);
bool HeapStageMappable(const ShaderRecompiler::RecompileResult& result, bool workgroupAddress = false);
void SetHeapStageUnmapped(std::optional<ShaderRecompiler::ShaderStage> stage);
bool HeapDrawStageMappable(const ShaderRecompiler::RecompileResult& result, ShaderRecompiler::ShaderStage stage);
bool HeapPairRefused(std::uint64_t pair);
bool ExpectDeviceImages(const VulkanDevice& device, std::span<const std::uint32_t> userData, std::uint64_t codeAddress, const ShaderRecompiler::RecompileResult& result, bool dispatch);
void RefuseHeapPair(std::uint64_t pair);

class HeapDrawOff {
public:
    HeapDrawOff();
    ~HeapDrawOff();
    HeapDrawOff(const HeapDrawOff&) = delete;
    HeapDrawOff& operator=(const HeapDrawOff&) = delete;

private:
    bool previous;
};

bool SampleHeapStageVerify();

struct HeapStageReads {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::vector<std::uint32_t> values;
    ShaderRecompiler::SrtUserDataUse userDataUse;
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> capture;
    bool complete = false;
};

std::shared_ptr<const ShaderRecompiler::RecompileResult>
MaterializeHeapStage(const ShaderRecompiler::PreparedShaderInvocation& invocation, std::span<const std::uint32_t> userData,
                     std::uint64_t shaderBase, ShaderRecompiler::HeapStageWords* words = nullptr,
                     std::shared_ptr<const ShaderRecompiler::HeapStageWords>* sharedWords = nullptr, HeapStageReads* stageReads = nullptr);
void NoteHeapDirect();
void NoteHeapDirectVerify(const ShaderRecompiler::RecompileResult& direct, const ShaderRecompiler::RecompileResult& expected,
                          const ShaderRecompiler::ResourceCapture* expectedCapture = nullptr);
std::uint64_t HeapStagesTaken();
std::shared_ptr<const ShaderRecompiler::RecompileResult> PatchedHeapResult(const ShaderRecompiler::RecompileResult& shared,
                                                                           const ShaderRecompiler::HeapStageWords& words);
void VerifyHeapAccessor(const Graphics::CompiledShader& stage);
void VerifyRingWords(const ShaderRecompiler::RecompileResult& kept, const ShaderRecompiler::HeapStageWords& ring,
                     const ShaderRecompiler::RecompileResult& fresh, const ShaderRecompiler::HeapStageWords* freshWords);

}

#endif
