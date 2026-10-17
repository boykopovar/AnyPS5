#pragma once
#include "Common.hpp"
#include "CompiledVariant.hpp"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

namespace ShaderArchive {
class GpuCompiler {
  public:
    GpuCompiler(const std::string &deviceName, unsigned workers, bool validation);
    ~GpuCompiler();
    GpuCompiler(const GpuCompiler &) = delete;
    GpuCompiler &operator=(const GpuCompiler &) = delete;
    ShaderRecompiler::SpirvTarget Target() const;
    unsigned Subgroup(unsigned guestWave, ShaderRecompiler::ShaderStage stage) const;
    Json::Value Info() const;
    void Compile(const ShaderRecompiler::RecompileRequest &request, const ShaderRecompiler::CompiledVariant &artifact,
                 unsigned worker);


  private:
    struct State;
    std::unique_ptr<State> state;
};
} // namespace ShaderArchive
