#ifndef CORE_LIBS_PRX_LIBSCEVIDEODEC2_HEVCMETADATA_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEODEC2_HEVCMETADATA_HPP

#include "prx/libSceVideodec2/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Videodec2Hevc {

using PictureInfo = SceVideodec2::HevcPictureInfo;

static_assert(sizeof(PictureInfo) == 184 && offsetof(PictureInfo, width) == 40 && offsetof(PictureInfo, picStruct) == 80 && offsetof(PictureInfo, vpsPresent) == 136);
static_assert(offsetof(PictureInfo, fillerPresent) == 142 && offsetof(PictureInfo, idr) == 147 && offsetof(PictureInfo, profile) == 151 && offsetof(PictureInfo, cropping) == 165 && offsetof(PictureInfo, cropBottom) == 180);

struct Metadata {
    PictureInfo info{};
    std::string unsupported;
};

struct Vps {
    bool timing = false;
    std::uint32_t numUnitsInTick = 0;
    std::uint32_t timeScale = 0;
    std::vector<std::uint8_t> bytes;
    std::string unsupported;
};

struct Sps {
    PictureInfo info{};
    unsigned vps = 0;
    std::string unsupported;
    std::vector<std::uint8_t> bytes;
};

struct Pps {
    unsigned sps = 0;
    std::vector<std::uint8_t> bytes;
    std::string unsupported;
};

struct State {
    std::array<std::optional<Vps>, 16> vps;
    std::array<std::optional<Sps>, 16> sps;
    std::array<std::optional<Pps>, 64> pps;
    std::string unsupported;
    std::optional<std::uint8_t> preferredTransfer;
    std::string transferUnsupported;
    bool hadPicture = false;
    bool afterEnd = false;
};

Metadata Parse_nid_no_patch(State& state, std::span<const std::uint8_t> annexB);

} // namespace Videodec2Hevc

#endif
