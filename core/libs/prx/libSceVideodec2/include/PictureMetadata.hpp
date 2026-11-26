#ifndef CORE_LIBS_PRX_LIBSCEVIDEODEC2_INCLUDE_PICTUREMETADATA_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEODEC2_INCLUDE_PICTUREMETADATA_HPP

#include "Api.hpp"

namespace SceVideodec2::Internal {

struct Crop {
    std::uint32_t left = 0;
    std::uint32_t right = 0;
    std::uint32_t top = 0;
    std::uint32_t bottom = 0;
    std::uint8_t log2ChromaWidth = 0;
    std::uint8_t log2ChromaHeight = 0;
};

struct PictureHeader {
    std::uint64_t pts;
    std::uint64_t dts;
    std::uint64_t attached;
    bool keyFrame;
    bool hevcIdr;
    std::uint32_t profile;
    std::uint32_t level;
    Crop crop;
    std::uint8_t bitDepthMinus8;
    bool avcIdr;
};

}

extern "C" {

bool Videodec2CopyParsedPictureInfo_nid_no_patch(std::uint64_t handle, const void* frameBuffer, void* info, std::size_t size);
bool Videodec2DecoderExists_nid_no_patch(std::uint64_t handle);
void Videodec2PictureHeader_nid_no_patch(std::uint64_t handle, const void* frameBuffer, SceVideodec2::Internal::PictureHeader* header);
void Videodec2Vp9PictureInfo_nid_no_patch(std::uint64_t handle, const void* frameBuffer, SceVideodec2::Vp9PictureInfo* info);

}

#endif
