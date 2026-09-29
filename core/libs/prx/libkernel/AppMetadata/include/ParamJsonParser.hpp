#ifndef CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_PARAMJSONPARSER_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_PARAMJSONPARSER_HPP

#include <cstdint>
#include <filesystem>
#include <string>

struct ParsedParamJson {
    std::string title;
    std::string titleId;
    // The download data quota in MiB; 0 when the title declares none.
    std::uint64_t downloadDataSizeMiB = 0;
};

ParsedParamJson parseParamJson(const std::filesystem::path& paramJsonPath);

#endif
