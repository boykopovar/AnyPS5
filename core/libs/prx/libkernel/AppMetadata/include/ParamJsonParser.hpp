#ifndef CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_PARAMJSONPARSER_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_PARAMJSONPARSER_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

struct ParsedParamJson {
    std::string title;
    std::string titleId;
    std::string contentVersion;
    std::uint64_t downloadDataSizeMiB = 0;
    std::uint32_t attribute3 = 0;
    std::optional<std::uint32_t> paramSfoAttribute;
    std::optional<std::uint64_t> flexibleMemorySizeBytes;
    std::int32_t userDefinedParams[4] = {};
};

ParsedParamJson parseParamJson(const std::filesystem::path& paramJsonPath);

#endif
