#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERPREPARATIONSCOPE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERPREPARATIONSCOPE_HPP

namespace AgcDriver::DriverDetail {
class ShaderPreparationTransaction;
}

extern "C" AgcDriver::DriverDetail::ShaderPreparationTransaction* AgcDriverBeginShaderPreparation_nid_postfix();
extern "C" void AgcDriverCommitShaderPreparation_nid_postfix(AgcDriver::DriverDetail::ShaderPreparationTransaction* transaction);
extern "C" void AgcDriverEndShaderPreparation_nid_postfix(AgcDriver::DriverDetail::ShaderPreparationTransaction* transaction) noexcept;

namespace AgcDriver {

class ShaderPreparationScope {
public:
    ShaderPreparationScope() : transaction(AgcDriverBeginShaderPreparation_nid_postfix()) {}
    ~ShaderPreparationScope() { AgcDriverEndShaderPreparation_nid_postfix(transaction); }
    ShaderPreparationScope(const ShaderPreparationScope&) = delete;
    ShaderPreparationScope& operator=(const ShaderPreparationScope&) = delete;
    void Commit() { AgcDriverCommitShaderPreparation_nid_postfix(transaction); }

private:
    DriverDetail::ShaderPreparationTransaction* transaction;
};

}

#endif
