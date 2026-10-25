#ifndef CORE_LIBS_PRX_LIBSCEAGC_COMMAND_INCLUDE_REGISTERDEFAULTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_COMMAND_INCLUDE_REGISTERDEFAULTS_HPP

#include <cstdint>

namespace Agc::Command {

enum class RegisterDefaultsTypeLayout {
    Compact,
    KeyBankPairs,
};

void* GetRegisterDefaults(std::uint32_t version, bool internal, const char* function,
                          RegisterDefaultsTypeLayout typeLayout = RegisterDefaultsTypeLayout::Compact);
int GetDefaultCxStateFlat(void* destination, std::uint32_t size);

}

#endif
