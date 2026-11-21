#include "prx/libSceAgcDriver/Execution/include/AspectFit.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace AgcDriver {
namespace {

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("AspectFit: ") + reason);
}

std::uint32_t roundToExtent(double value) {
    require(value >= 0.0 && value <= static_cast<double>(std::numeric_limits<std::uint32_t>::max()), "computed extent is out of range");
    return static_cast<std::uint32_t>(std::lround(value));
}

}

PresentationAspect SelectPresentationAspect(const char* value) {
    const std::string_view text = value == nullptr ? std::string_view{} : std::string_view{value};
    if (text.empty() || text == "keep") return PresentationAspect::Keep;
    if (text == "stretch") return PresentationAspect::Stretch;
    throw std::runtime_error("ANYPS5_ASPECT must be keep or stretch, not \"" + std::string(text) + "\"");
}

AspectFitRect ComputePresentationRect(PresentationAspect aspect, std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t destinationWidth, std::uint32_t destinationHeight) {
    if (aspect == PresentationAspect::Keep) return ComputeContainRect_nid_postfix(sourceWidth, sourceHeight, destinationWidth, destinationHeight);
    require(sourceWidth != 0 && sourceHeight != 0, "source extent must be non-zero");
    require(destinationWidth != 0 && destinationHeight != 0, "bounds extent must be non-zero");
    return AspectFitRect{0, 0, destinationWidth, destinationHeight};
}

extern "C" {

double ComputeContainScale_nid_postfix(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t boundsWidth, std::uint32_t boundsHeight, bool allowUpscale) {
    require(sourceWidth != 0 && sourceHeight != 0, "source extent must be non-zero");
    require(boundsWidth != 0 && boundsHeight != 0, "bounds extent must be non-zero");
    const auto scale = std::min(static_cast<double>(boundsWidth) / static_cast<double>(sourceWidth), static_cast<double>(boundsHeight) / static_cast<double>(sourceHeight));
    return allowUpscale ? scale : std::min(scale, 1.0);
}

AspectFitSize ComputeContainSize_nid_postfix(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t boundsWidth, std::uint32_t boundsHeight, bool allowUpscale) {
    const auto scale = ComputeContainScale_nid_postfix(sourceWidth, sourceHeight, boundsWidth, boundsHeight, allowUpscale);
    return AspectFitSize{std::max<std::uint32_t>(1, roundToExtent(sourceWidth * scale)), std::max<std::uint32_t>(1, roundToExtent(sourceHeight * scale))};
}

AspectFitRect ComputeContainRect_nid_postfix(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t destinationWidth, std::uint32_t destinationHeight) {
    const auto size = ComputeContainSize_nid_postfix(sourceWidth, sourceHeight, destinationWidth, destinationHeight, true);
    const auto x = (static_cast<std::int64_t>(destinationWidth) - static_cast<std::int64_t>(size.width)) / 2;
    const auto y = (static_cast<std::int64_t>(destinationHeight) - static_cast<std::int64_t>(size.height)) / 2;
    return AspectFitRect{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), size.width, size.height};
}

std::uint32_t ComputeWidthForHeight_nid_postfix(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t height) {
    require(sourceWidth != 0 && sourceHeight != 0, "source extent must be non-zero");
    return std::max<std::uint32_t>(1, roundToExtent(static_cast<double>(height) * sourceWidth / sourceHeight));
}

std::uint32_t ComputeHeightForWidth_nid_postfix(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t width) {
    require(sourceWidth != 0 && sourceHeight != 0, "source extent must be non-zero");
    return std::max<std::uint32_t>(1, roundToExtent(static_cast<double>(width) * sourceHeight / sourceWidth));
}

bool PresentationStretches_nid_postfix() {
    static const bool stretches = SelectPresentationAspect(std::getenv("ANYPS5_ASPECT")) == PresentationAspect::Stretch;
    return stretches;
}

AspectFitSize ComputePresentationSize_nid_postfix(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t boundsWidth, std::uint32_t boundsHeight) {
    if (!PresentationStretches_nid_postfix()) return ComputeContainSize_nid_postfix(sourceWidth, sourceHeight, boundsWidth, boundsHeight, true);
    const auto rect = ComputePresentationRect(PresentationAspect::Stretch, sourceWidth, sourceHeight, boundsWidth, boundsHeight);
    return AspectFitSize{rect.width, rect.height};
}

AspectFitRect ComputePresentationRect_nid_postfix(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t destinationWidth, std::uint32_t destinationHeight) {
    return ComputePresentationRect(PresentationStretches_nid_postfix() ? PresentationAspect::Stretch : PresentationAspect::Keep, sourceWidth, sourceHeight, destinationWidth, destinationHeight);
}

}
}
