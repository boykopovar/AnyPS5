#ifndef CORE_LIBS_PRX_LIBSCEAGC_COMMAND_INCLUDE_REGISTERDEFAULTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_COMMAND_INCLUDE_REGISTERDEFAULTS_HPP

#include "SceShaders.hpp"

#include <cstdint>
#include <span>

namespace Agc::Command {

enum class RegisterDefaultsTypeLayout {
    Compact,
    KeyBankPairs,
};

void* GetRegisterDefaults(std::uint32_t version, bool internal, const char* function,
                          RegisterDefaultsTypeLayout typeLayout = RegisterDefaultsTypeLayout::Compact);
struct QueueResetState {
    std::uint32_t instance;
    std::uint32_t version;
    bool external;
    std::span<const std::uint32_t> commands;
};

QueueResetState GetQueueResetState(std::uint32_t instance, bool active);
int InitializeDefaultCxState(std::uint32_t version);
int CopyDefaultCxStateRange(void* destination, std::uint32_t first, std::uint32_t last, std::span<const ShaderRegister> registers);
int GetDefaultCxStateFlat(void* destination, std::uint32_t first, std::uint32_t last, std::uint32_t instance);

}

#endif
