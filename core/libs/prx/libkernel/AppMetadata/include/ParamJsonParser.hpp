#ifndef CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_PARAMJSONPARSER_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_PARAMJSONPARSER_HPP

#include <cstdint>
#include <filesystem>
#include <string>

struct ParsedParamJson {
    std::string title;
    std::string titleId;
    std::uint64_t downloadDataSizeMiB = 0;
    std::int32_t userDefinedParams[4] = {};
};

ParsedParamJson parseParamJson(const std::filesystem::path& paramJsonPath);

#endif
