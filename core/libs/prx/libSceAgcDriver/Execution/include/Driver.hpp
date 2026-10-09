#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_HPP

#include <span>
#include "SceTypes.hpp"
#include "SceShaders.hpp"

namespace AgcDriver {

void Submit(const Packet* packet, std::uint32_t queue);

}

extern "C" void AgcDriverWaitIdle_nid_postfix();
extern "C" void AgcDriverLockVulkanLoader_nid_postfix();
extern "C" void AgcDriverUnlockVulkanLoader_nid_postfix();
extern "C" void AgcDriverShutdown_nid_postfix();
extern "C" void AgcDriverRequestScreenshot_nid_postfix();
extern "C" void AgcDriverSuspendPoint_nid_postfix();
extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader);
extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive);
extern "C" void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive);
extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType);

#endif
