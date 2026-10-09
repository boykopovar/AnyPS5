#pragma once

#include <array>
#include <span>
#include <stdexcept>
#include <vector>
#include "HevcPictureInfo.hpp"

namespace {

class HevcBits {
    std::span<const std::uint8_t> data;
    std::size_t position = 0;

public:
    explicit HevcBits(std::span<const std::uint8_t> bytes) : data(bytes) {}

    std::uint32_t Read(unsigned count) {
        if (count > 32 || position > data.size() * 8 || count > data.size() * 8 - position)
            throw std::runtime_error("Videodec2: truncated HEVC metadata");
        std::uint32_t value = 0;
        for (unsigned bit = 0; bit < count; ++bit, ++position)
            value = (value << 1) | ((data[position / 8] >> (7 - position % 8)) & 1);
        return value;
    }

    std::uint32_t Exp(std::uint32_t maximum = 0xfffffffeu) {
        unsigned zeros = 0;
        while (Read(1) == 0) {
            if (++zeros == 32) throw std::runtime_error("Videodec2: HEVC metadata integer overflow");
        }
        const auto value = ((std::uint32_t{1} << zeros) - 1) + Read(zeros);
        if (value > maximum) throw std::runtime_error("Videodec2: HEVC metadata value out of range");
        return value;
    }

    void End() {
        if (Read(1) != 1) throw std::runtime_error("Videodec2: invalid HEVC metadata trailing bits");
        while (position < data.size() * 8)
            if (Read(1) != 0) throw std::runtime_error("Videodec2: invalid HEVC metadata trailing bits");
    }
};

class HevcMetadata {
    std::array<std::vector<std::uint8_t>, 16> sequences;
    std::array<int, 64> pictures;
    bool unsupportedPersistentSei = false;

    static std::vector<std::uint8_t> unescape(std::span<const std::uint8_t> nal, std::size_t limit) {
        std::vector<std::uint8_t> bytes;
        unsigned zeros = 0;
        for (std::size_t index = 0; index < nal.size() && bytes.size() < limit; ++index) {
            const auto value = nal[index];
            if (zeros == 2 && value == 3) {
                if (index + 1 == nal.size() || nal[index + 1] > 3)
                    throw std::runtime_error("Videodec2: invalid HEVC emulation prevention");
                zeros = 0;
                continue;
            }
            bytes.push_back(value);
            zeros = value == 0 ? zeros + 1 : 0;
        }
        return bytes;
    }

    static std::size_t findStart(std::span<const std::uint8_t> bytes, std::size_t index) {
        for (; index + 2 < bytes.size(); ++index)
            if (bytes[index] == 0 && bytes[index + 1] == 0 && bytes[index + 2] == 1) return index;
        return bytes.size();
    }

    static unsigned sequenceId(std::span<const std::uint8_t> bytes) {
        HevcBits bits(bytes);
        bits.Read(4);
        const auto layers = bits.Read(3);
        bits.Read(1);
        bits.Read(32);
        bits.Read(32);
        bits.Read(32);
        std::array<std::uint32_t, 7> profiles{};
        std::array<std::uint32_t, 7> levels{};
        for (unsigned layer = 0; layer < layers; ++layer) {
            profiles[layer] = bits.Read(1);
            levels[layer] = bits.Read(1);
        }
        if (layers) bits.Read(2 * (8 - layers));
        for (unsigned layer = 0; layer < layers; ++layer) {
            if (profiles[layer]) {
                bits.Read(32);
                bits.Read(32);
                bits.Read(24);
            }
            if (levels[layer]) bits.Read(8);
        }
        return bits.Exp(15);
    }

    static void vui(HevcBits& bits, Videodec2::HevcPictureInfo& info) {
        info.aspectRatioInfoPresentFlag = bits.Read(1);
        if (info.aspectRatioInfoPresentFlag) {
            info.aspectRatioIdc = bits.Read(8);
            if (info.aspectRatioIdc == 255) {
                info.sarWidth = bits.Read(16);
                info.sarHeight = bits.Read(16);
            }
        }
        if (bits.Read(1)) bits.Read(1);
        info.videoSignalTypePresentFlag = bits.Read(1);
        if (info.videoSignalTypePresentFlag) {
            info.videoFormat = bits.Read(3);
            info.videoFullRangeFlag = bits.Read(1);
            info.colourDescriptionPresentFlag = bits.Read(1);
            if (info.colourDescriptionPresentFlag) {
                info.colourPrimaries = bits.Read(8);
                info.transferCharacteristics = bits.Read(8);
                info.matrixCoeffs = bits.Read(8);
            }
        }
        info.chromaLocInfoPresentFlag = bits.Read(1);
        if (info.chromaLocInfoPresentFlag) {
            info.chromaSampleLocTypeTopField = bits.Exp(5);
            info.chromaSampleLocTypeBottomField = bits.Exp(5);
        }
        bits.Read(1);
        info.fieldSeqFlag = bits.Read(1);
        info.frameFieldInfoPresentFlag = bits.Read(1);
        info.defaultDisplayWindowFlag = bits.Read(1);
        if (info.fieldSeqFlag || info.defaultDisplayWindowFlag)
            throw std::runtime_error("Videodec2: HEVC field or display-window picture info is not implemented");
        info.timingInfoPresentFlag = bits.Read(1);
        if (info.timingInfoPresentFlag) {
            info.numUnitsInTick = bits.Read(32);
            info.timeScale = bits.Read(32);
            if (info.numUnitsInTick == 0 || info.timeScale == 0)
                throw std::runtime_error("Videodec2: invalid HEVC picture timing");
            if (bits.Read(1)) bits.Exp();
            if (bits.Read(1)) throw std::runtime_error("Videodec2: HEVC HRD picture info is not implemented");
        }
        if (bits.Read(1)) {
            bits.Read(3);
            bits.Exp(4095);
            for (unsigned index = 0; index < 4; ++index) bits.Exp(16);
        }
    }

    static Videodec2::HevcPictureInfo readSequence(std::span<const std::uint8_t> bytes) {
        HevcBits bits(bytes);
        Videodec2::HevcPictureInfo info{};
        info.thisSize = sizeof(info);
        info.videoFormat = 5;
        info.colourPrimaries = info.transferCharacteristics = info.matrixCoeffs = 2;
        bits.Read(4);
        if (bits.Read(3) != 0) throw std::runtime_error("Videodec2: HEVC temporal-layer picture info is not implemented");
        bits.Read(1);
        info.generalProfileSpace = bits.Read(2);
        info.generalTierFlag = bits.Read(1);
        info.generalProfileIdc = bits.Read(5);
        bits.Read(32);
        info.generalProgressiveSourceFlag = bits.Read(1);
        info.generalInterlacedSourceFlag = bits.Read(1);
        bits.Read(1);
        info.generalFrameOnlyConstraintFlag = bits.Read(1);
        bits.Read(32);
        bits.Read(12);
        info.generalLevelIdc = bits.Read(8);
        bits.Exp(15);
        if (bits.Exp(3) != 1) throw std::runtime_error("Videodec2: HEVC picture info requires 4:2:0 chroma");
        info.picWidthInLumaSamples = bits.Exp(65535);
        info.picHeightInLumaSamples = bits.Exp(65535);
        if (info.picWidthInLumaSamples == 0 || info.picHeightInLumaSamples == 0)
            throw std::runtime_error("Videodec2: invalid HEVC picture dimensions");
        info.conformanceWindowFlag = bits.Read(1);
        if (info.conformanceWindowFlag)
            throw std::runtime_error("Videodec2: HEVC cropped picture info is not implemented");
        info.bitDepthLumaMinus8 = bits.Exp(8);
        info.bitDepthChromaMinus8 = bits.Exp(8);
        if (info.bitDepthLumaMinus8 || info.bitDepthChromaMinus8)
            throw std::runtime_error("Videodec2: HEVC picture info deeper than 8 bits is not implemented");
        const auto pocBits = bits.Exp(12) + 4;
        info.subLayerOrderingInfoPresentFlag = bits.Read(1);
        info.maxDecPicBufferingMinus1 = bits.Exp(15);
        bits.Exp(info.maxDecPicBufferingMinus1);
        bits.Exp();
        for (unsigned index = 0; index < 6; ++index) bits.Exp(5);
        if (bits.Read(1) && bits.Read(1))
            throw std::runtime_error("Videodec2: HEVC scaling-list picture info is not implemented");
        bits.Read(2);
        if (bits.Read(1)) {
            bits.Read(8);
            bits.Exp(3);
            bits.Exp(3);
            bits.Read(1);
        }
        const auto sets = bits.Exp(64);
        for (unsigned set = 0; set < sets; ++set) {
            if (set != 0 && bits.Read(1))
                throw std::runtime_error("Videodec2: HEVC predicted reference-set picture info is not implemented");
            const auto negative = bits.Exp(15);
            const auto positive = bits.Exp(15 - negative);
            for (unsigned ref = 0; ref < negative + positive; ++ref) {
                bits.Exp(32767);
                bits.Read(1);
            }
        }
        if (bits.Read(1)) {
            const auto refs = bits.Exp(32);
            for (unsigned ref = 0; ref < refs; ++ref) bits.Read(pocBits + 1);
        }
        bits.Read(2);
        if (bits.Read(1)) vui(bits, info);
        if (bits.Read(1)) throw std::runtime_error("Videodec2: HEVC SPS-extension picture info is not implemented");
        bits.End();
        return info;
    }

    void sei(std::span<const std::uint8_t> bytes, bool& bufferingPeriod) {
        std::size_t index = 0;
        const auto number = [&]() {
            std::size_t value = 0;
            for (;;) {
                if (index == bytes.size()) throw std::runtime_error("Videodec2: truncated HEVC SEI");
                const auto part = bytes[index++];
                value += part;
                if (part != 255) return value;
            }
        };
        while (index < bytes.size()) {
            if (index + 1 == bytes.size() && bytes[index] == 0x80) return;
            const auto type = number();
            const auto size = number();
            if (size > bytes.size() - index) throw std::runtime_error("Videodec2: truncated HEVC SEI payload");
            if (type == 45 || type == 147) unsupportedPersistentSei = true;
            if (type == 1 || unsupportedPersistentSei)
                throw std::runtime_error("Videodec2: HEVC timing, frame-packing or transfer SEI picture info is not implemented");
            if (type == 0) bufferingPeriod = true;
            index += size;
        }
        throw std::runtime_error("Videodec2: missing HEVC SEI trailing bits");
    }

public:
    HevcMetadata() { pictures.fill(-1); }

    void Reset() { unsupportedPersistentSei = false; }

    Videodec2::HevcPictureInfo Read(std::span<const std::uint8_t> bytes) {
        std::array<bool, 64> present{};
        std::vector<std::uint8_t> sequence;
        bool bufferingPeriod = false;
        int selectedPps = -1;
        unsigned pictureType = 0;
        auto start = findStart(bytes, 0);
        while (start < bytes.size()) {
            const auto begin = start + 3;
            start = findStart(bytes, begin);
            auto end = start;
            while (end > begin && bytes[end - 1] == 0) --end;
            if (end - begin < 2) throw std::runtime_error("Videodec2: truncated HEVC NAL header");
            const auto header = (static_cast<unsigned>(bytes[begin]) << 8) | bytes[begin + 1];
            if ((header & 0x8000) != 0 || (header & 0x1ff) != 1)
                throw std::runtime_error("Videodec2: HEVC layered picture info is not implemented");
            const auto type = (header >> 9) & 63;
            present[type] = true;
            if (type > 31 && type != 33 && type != 34 && type != 39 && type != 40) continue;
            const auto rbsp = unescape(bytes.subspan(begin + 2, end - begin - 2), type <= 31 ? 2 : end - begin - 2);
            HevcBits bits(rbsp);
            if (type == 33) {
                sequences[sequenceId(rbsp)] = rbsp;
            } else if (type == 34) {
                const auto pps = bits.Exp(63);
                pictures[pps] = static_cast<int>(bits.Exp(15));
            } else if (type <= 31) {
                const auto firstSlice = bits.Read(1);
                if (type >= 16 && type <= 23) bits.Read(1);
                const auto pps = bits.Exp(63);
                if (selectedPps >= 0 && (firstSlice || pps != static_cast<unsigned>(selectedPps)))
                    throw std::runtime_error("Videodec2: multiple HEVC pictures in one access unit");
                if (selectedPps < 0) {
                    if (!firstSlice || pictures[pps] < 0 || sequences[pictures[pps]].empty())
                        throw std::runtime_error("Videodec2: missing HEVC picture parameter set");
                    selectedPps = static_cast<int>(pps);
                    sequence = sequences[pictures[pps]];
                    pictureType = type;
                }
            } else if (type == 39 || type == 40) {
                sei(rbsp, bufferingPeriod);
            }
        }
        if (sequence.empty()) throw std::runtime_error("Videodec2: HEVC access unit has no picture");
        if (unsupportedPersistentSei)
            throw std::runtime_error("Videodec2: persistent HEVC SEI picture info requires a decoder reset");
        auto info = readSequence(sequence);
        info.videoParameterSetPresentFlag = present[32];
        info.sequenceParameterSetPresentFlag = present[33];
        info.pictureParameterSetPresentFlag = present[34];
        info.auDelimiterPresentFlag = present[35];
        info.endOfSequencePresentFlag = present[36];
        info.endOfStreamPresentFlag = present[37];
        info.fillerDataPresentFlag = present[38];
        info.bufferingPeriodSeiPresentFlag = bufferingPeriod;
        info.idrPictureFlag = pictureType == 19 || pictureType == 20;
        info.irapPictureFlag = pictureType >= 16 && pictureType <= 23;
        info.isValid = true;
        return info;
    }
};

}
