#ifndef CORE_LIBS_PRX_LIBSCEVIDEODEC2_AVCMETADATA_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEODEC2_AVCMETADATA_HPP

#include "prx/libSceVideodec2/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Videodec2Avc {

using PictureInfo = SceVideodec2::AvcPictureInfo;

static_assert(sizeof(PictureInfo) == 0x78 && offsetof(PictureInfo, picWidthInMbsMinus1) == 0x2c && offsetof(PictureInfo, frameCroppingFlag) == 0x35 && offsetof(PictureInfo, frameCropLeftOffset) == 0x38);
static_assert(offsetof(PictureInfo, aspectRatioInfoPresentFlag) == 0x48 && offsetof(PictureInfo, sarWidth) == 0x4a && offsetof(PictureInfo, timingInfoPresentFlag) == 0x55 && offsetof(PictureInfo, numUnitsInTick) == 0x58);
static_assert(offsetof(PictureInfo, fixedFrameRateFlag) == 0x60 && offsetof(PictureInfo, sequenceParameterSetPresentFlag) == 0x67 && offsetof(PictureInfo, constraintSet0Flag) == 0x6f && offsetof(PictureInfo, constraintSet5Flag) == 0x74);

struct Metadata {
    PictureInfo info{};
    std::string unsupported;
};

struct Sps {
    PictureInfo info{};
    bool separateColourPlanes = false;
    unsigned frameNumBits = 0;
    std::optional<std::pair<unsigned, unsigned>> delays;
    std::vector<std::uint8_t> bytes;
    std::string unsupported;
};

struct Pps {
    unsigned sps = 0;
    std::vector<std::uint8_t> bytes;
    std::string unsupported;
};

struct State {
    std::array<std::optional<Sps>, 32> sps;
    std::array<std::optional<Pps>, 256> pps;
    std::string unsupported;
};

Metadata Parse_nid_no_patch(State& state, std::span<const std::uint8_t> annexB);

} // namespace Videodec2Avc

#endif
