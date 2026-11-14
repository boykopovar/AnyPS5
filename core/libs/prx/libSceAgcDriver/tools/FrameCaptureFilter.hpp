#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TOOLS_FRAMECAPTUREFILTER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TOOLS_FRAMECAPTUREFILTER_HPP

#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include <filesystem>
#include <set>
#include <string>
#include <string_view>

namespace AgcDriverTools {

struct FrameCaptureFilterPlan {
    std::set<std::uint64_t> calls;
    std::set<std::uint64_t> chunks;
};

FrameCaptureFilterPlan ReadFrameCaptureFilterPlan(const std::filesystem::path& path);
std::string DescribeFrameCapture(const std::filesystem::path& path);
void FilterFrameCapture(const std::filesystem::path& source, const std::filesystem::path& destination, const FrameCaptureFilterPlan& plan);

}

#endif
