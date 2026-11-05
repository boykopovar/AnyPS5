#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERPREPARATIONSCOPE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERPREPARATIONSCOPE_HPP

#include <cstddef>
#include <initializer_list>

struct Shader;

namespace AgcDriver::DriverDetail {
class ShaderPreparationTransaction;
}

extern "C" AgcDriver::DriverDetail::ShaderPreparationTransaction* AgcDriverBeginShaderPreparation_nid_postfix(const Shader* const* shaders, std::size_t count);
extern "C" void AgcDriverCommitShaderPreparation_nid_postfix(AgcDriver::DriverDetail::ShaderPreparationTransaction* transaction);
extern "C" void AgcDriverEndShaderPreparation_nid_postfix(AgcDriver::DriverDetail::ShaderPreparationTransaction* transaction) noexcept;

namespace AgcDriver {

class ShaderPreparationScope {
public:
    explicit ShaderPreparationScope(std::initializer_list<const Shader*> shaders) : transaction(AgcDriverBeginShaderPreparation_nid_postfix(shaders.begin(), shaders.size())) {}
    ~ShaderPreparationScope() { AgcDriverEndShaderPreparation_nid_postfix(transaction); }
    ShaderPreparationScope(const ShaderPreparationScope&) = delete;
    ShaderPreparationScope& operator=(const ShaderPreparationScope&) = delete;
    void Commit() { AgcDriverCommitShaderPreparation_nid_postfix(transaction); }

private:
    DriverDetail::ShaderPreparationTransaction* transaction;
};

}

#endif
