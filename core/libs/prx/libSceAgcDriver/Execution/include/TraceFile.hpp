#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_TRACEFILE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_TRACEFILE_HPP

#include <filesystem>
#include <string_view>

namespace AgcDriver {

class TraceFile {
public:
    static void WriteGzip(const std::filesystem::path& path, std::string_view text);
    static void ReplaceGzip(const std::filesystem::path& path, std::string_view text);
    static void AppendLine(const std::filesystem::path& path, std::string_view line);
};

}

#endif
