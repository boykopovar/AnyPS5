#ifndef CORE_LIBS_PRX_LIBSCEAGC_SHADER_INCLUDE_SHADERUTILS_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_SHADER_INCLUDE_SHADERUTILS_HPP

#include <cstdint>
#include "SceShaders.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

template <typename T>
void ResolveRelativePtr(T*& field) {
    if (field == nullptr) {
        return;
    }
    field = reinterpret_cast<T*>(
        reinterpret_cast<std::uintptr_t>(field) + reinterpret_cast<std::uintptr_t>(&field)
    );
}

bool GetProgramAddressRegisterOffset(std::uint8_t type, std::uint32_t& loOffset);
int PatchProgramAddressRegister(ShaderRegister* regs, std::uint32_t numRegs, std::uint8_t type, std::uint64_t base);
std::uint32_t GraphicsPrimTypeToGsOut(std::uint32_t primType);
std::uint32_t ShaderSemanticWord(const ShaderSemantic& s);
std::uint32_t ApplyInterpolantDefaultValue(std::uint32_t value, std::uint32_t psWord);
std::uint32_t ApplyInterpolantDefaultValueHi(std::uint32_t value, std::uint32_t psWord);
std::uint32_t CreateInterpolantF16Value(std::uint32_t psWord, const ShaderSemantic* gsSemantic);
std::uint32_t CreateInterpolantNonF16Value(std::uint32_t psWord, const ShaderSemantic* gsSemantic);
std::uint32_t CreateInterpolantMappingValue(std::uint32_t value, std::uint32_t psWord, std::uint32_t gsWord);
std::uint32_t CreateInterpolantDefaultValue(std::uint32_t value, std::uint32_t psWord);
const ShaderSemantic* FindOutputSemantic(const Shader* gs, std::uint32_t semantic);
void SetInterpolantRegister(ShaderRegister* regs, std::uint32_t index, std::uint32_t value);
void FillIdentityInterpolants(ShaderRegister* regs, std::uint32_t firstIndex);

#endif
