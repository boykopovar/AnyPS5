#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Aot::Compiler {

struct ElfFunction {
    std::string Name;
    std::vector<uint8_t> Code;
};

class ElfBinaryParser {
public:
    static ElfFunction ReadFunction(const std::filesystem::path& path, std::string_view name);
};

} // namespace Aot::Compiler
