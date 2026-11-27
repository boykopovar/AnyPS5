#pragma once

#include <cstddef>

namespace NKWebKit::JscAbi {

inline constexpr std::size_t ConfigFilePathMax = 1024;

struct ConfigFile {
    char filename[ConfigFilePathMax + 1];
    char configDirectory[ConfigFilePathMax + 1];
};

static_assert(sizeof(ConfigFile) == 2 * (ConfigFilePathMax + 1));
static_assert(offsetof(ConfigFile, configDirectory) == ConfigFilePathMax + 1);

}  // namespace NKWebKit::JscAbi
