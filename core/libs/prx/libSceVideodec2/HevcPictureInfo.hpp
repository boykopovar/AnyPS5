#pragma once

#include <cstddef>
#include <cstdint>

namespace Videodec2 {

struct HevcPictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint32_t picWidthInLumaSamples;
    std::uint32_t picHeightInLumaSamples;
    std::uint8_t bitDepthLumaMinus8;
    std::uint8_t bitDepthChromaMinus8;
    std::uint8_t timingInfoPresentFlag;
    std::uint32_t numUnitsInTick;
    std::uint32_t timeScale;
    std::uint32_t aspectRatioInfoPresentFlag;
    std::uint8_t aspectRatioIdc;
    std::uint16_t sarWidth;
    std::uint16_t sarHeight;
    std::uint8_t videoSignalTypePresentFlag;
    std::uint8_t videoFormat;
    std::uint8_t videoFullRangeFlag;
    std::uint8_t colourDescriptionPresentFlag;
    std::uint8_t colourPrimaries;
    std::uint8_t transferCharacteristics;
    std::uint8_t matrixCoeffs;
    std::uint8_t frameFieldInfoPresentFlag;
    std::uint32_t picStruct;
    std::uint32_t sourceScanType;
    std::uint32_t duplicateFlag;
    std::uint32_t conformanceWindowFlag;
    std::uint32_t confWinLeftOffset;
    std::uint32_t confWinRightOffset;
    std::uint32_t confWinTopOffset;
    std::uint32_t confWinBottomOffset;
    std::uint32_t defaultDisplayWindowFlag;
    std::uint32_t defDispWinLeftOffset;
    std::uint32_t defDispWinRightOffset;
    std::uint32_t defDispWinTopOffset;
    std::uint32_t defDispWinBottomOffset;
    std::uint8_t chromaLocInfoPresentFlag;
    std::uint8_t chromaSampleLocTypeTopField;
    std::uint8_t chromaSampleLocTypeBottomField;
    std::uint8_t fieldSeqFlag;
    std::uint8_t videoParameterSetPresentFlag;
    std::uint8_t sequenceParameterSetPresentFlag;
    std::uint8_t pictureParameterSetPresentFlag;
    std::uint8_t auDelimiterPresentFlag;
    std::uint8_t endOfSequencePresentFlag;
    std::uint8_t endOfStreamPresentFlag;
    std::uint8_t fillerDataPresentFlag;
    std::uint8_t pictureTimingSeiPresentFlag;
    std::uint8_t bufferingPeriodSeiPresentFlag;
    std::uint8_t framePackingArrangementSeiPresentFlag;
    std::uint8_t alternativeTransferCharacteristicsSeiPresentFlag;
    std::uint8_t idrPictureFlag;
    std::uint8_t irapPictureFlag;
    std::uint8_t generalProfileSpace;
    std::uint8_t generalTierFlag;
    std::uint8_t generalProfileIdc;
    std::uint8_t generalProgressiveSourceFlag;
    std::uint8_t generalInterlacedSourceFlag;
    std::uint8_t generalFrameOnlyConstraintFlag;
    std::uint8_t generalLevelIdc;
    std::uint8_t subLayerProfilePresentFlag;
    std::uint8_t subLayerLevelPresentFlag;
    std::uint8_t subLayerProfileSpace;
    std::uint8_t subLayerTierFlag;
    std::uint8_t subLayerProfileIdc;
    std::uint8_t subLayerLevelIdc;
    std::uint8_t subLayerOrderingInfoPresentFlag;
    std::uint8_t maxDecPicBufferingMinus1;
    std::uint8_t preferredTransferCharacteristics;
    std::uint8_t frameCroppingFlag;
    std::uint32_t frameCropLeftOffset;
    std::uint32_t frameCropRightOffset;
    std::uint32_t frameCropTopOffset;
    std::uint32_t frameCropBottomOffset;
};

static_assert(sizeof(HevcPictureInfo) == 0xb8);
static_assert(offsetof(HevcPictureInfo, ptsData) == 0x10);
static_assert(offsetof(HevcPictureInfo, picWidthInLumaSamples) == 0x28);
static_assert(offsetof(HevcPictureInfo, numUnitsInTick) == 0x34);
static_assert(offsetof(HevcPictureInfo, picStruct) == 0x50);
static_assert(offsetof(HevcPictureInfo, frameCropLeftOffset) == 0xa8);

}
