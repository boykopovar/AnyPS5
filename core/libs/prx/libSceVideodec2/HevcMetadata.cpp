#include "HevcMetadata.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

class Bits {
public:
    explicit Bits(std::span<const std::uint8_t> bytes) : bytes(bytes) {}

    std::uint32_t Read(unsigned count) {
        if (count > 32 || count > bytes.size() * 8 - position) throw std::runtime_error("truncated HEVC syntax");
        std::uint32_t value = 0;
        for (unsigned bit = 0; bit < count; ++bit, ++position)
            value = (value << 1) | ((bytes[position / 8] >> (7 - position % 8)) & 1);
        return value;
    }

    void Skip(std::size_t count) {
        if (count > bytes.size() * 8 - position) throw std::runtime_error("truncated HEVC syntax");
        position += count;
    }

    std::uint32_t Unsigned(std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max()) {
        unsigned zeros = 0;
        while (Read(1) == 0) if (++zeros > 31) throw std::runtime_error("HEVC exponential code overflow");
        const auto value = ((std::uint32_t{1} << zeros) - 1) + Read(zeros);
        if (value > maximum) throw std::runtime_error("HEVC syntax value outside its domain");
        return value;
    }

    void Signed() { Unsigned(); }

private:
    std::span<const std::uint8_t> bytes;
    std::size_t position = 0;
};

std::vector<std::uint8_t> Rbsp(std::span<const std::uint8_t> nal) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(nal.size());
    unsigned zeros = 0;
    for (std::size_t index = 0; index < nal.size(); ++index) {
        const auto value = nal[index];
        if (zeros == 2 && value == 3) {
            if (index + 1 < nal.size() && nal[index + 1] > 3) throw std::runtime_error("invalid HEVC emulation prevention");
            zeros = 0;
            continue;
        }
        if (zeros == 2 && value < 3) throw std::runtime_error("missing HEVC emulation prevention");
        bytes.push_back(value);
        zeros = value == 0 ? std::min(zeros + 1, 2u) : 0;
    }
    return bytes;
}

std::uint32_t Profile(Bits& bits, unsigned sublayers, bool* mixed = nullptr, Videodec2Hevc::PictureInfo* info = nullptr) {
    const auto space = bits.Read(2);
    const auto tier = bits.Read(1);
    const auto profileId = bits.Read(5);
    bits.Skip(32);
    const bool progressive = bits.Read(1) != 0;
    const bool interlaced = bits.Read(1) != 0;
    if (mixed) *mixed = progressive && interlaced;
    bits.Skip(1);
    const auto frameOnly = bits.Read(1);
    bits.Skip(44);
    const auto levelId = bits.Read(8);
    if (info) {
        info->profileSpace = static_cast<std::uint8_t>(space);
        info->tier = static_cast<std::uint8_t>(tier);
        info->profile = static_cast<std::uint8_t>(profileId);
        info->progressiveSource = progressive;
        info->interlacedSource = interlaced;
        info->frameOnlyConstraint = static_cast<std::uint8_t>(frameOnly);
        info->level = static_cast<std::uint8_t>(levelId);
    }
    std::array<bool, 7> profile{};
    std::array<bool, 7> level{};
    for (unsigned layer = 0; layer < sublayers; ++layer) {
        profile[layer] = bits.Read(1) != 0;
        level[layer] = bits.Read(1) != 0;
    }
    if (sublayers != 0) bits.Skip(2 * (8 - sublayers));
    for (unsigned layer = 0; layer < sublayers; ++layer) {
        if (profile[layer]) bits.Skip(88);
        if (level[layer]) bits.Skip(8);
    }
    return progressive == interlaced ? 2 : progressive ? 1 : 0;
}

void Ordering(Bits& bits, unsigned sublayers, Videodec2Hevc::PictureInfo* info = nullptr) {
    const auto present = bits.Read(1);
    const unsigned begin = present != 0 ? 0 : sublayers;
    unsigned buffering = 0;
    for (unsigned layer = begin; layer <= sublayers; ++layer) {
        buffering = std::max(buffering, bits.Unsigned(15));
        bits.Unsigned();
        bits.Unsigned();
    }
    if (info) {
        info->subLayerOrderingPresent = static_cast<std::uint8_t>(present);
        info->maxDecPicBufferingMinus1 = static_cast<std::uint8_t>(buffering);
    }
}

std::pair<unsigned, Videodec2Hevc::Vps> ParseVps(Bits& bits) {
    const auto id = bits.Read(4);
    bits.Skip(2);
    const auto layers = bits.Read(6);
    const auto sublayers = bits.Read(3);
    if (layers != 0 || sublayers > 6) throw std::runtime_error("multilayer HEVC VPS metadata is unsupported");
    bits.Skip(17);
    Profile(bits, sublayers);
    Ordering(bits, sublayers);
    const auto maxLayer = bits.Read(6);
    const auto sets = bits.Unsigned(1023);
    if (maxLayer != 0 || sets != 0) throw std::runtime_error("HEVC layer-set metadata is unsupported");
    Videodec2Hevc::Vps vps;
    vps.timing = bits.Read(1) != 0;
    if (vps.timing) {
        vps.numUnitsInTick = bits.Read(32);
        vps.timeScale = bits.Read(32);
    }
    return {id, vps};
}

void ScalingLists(Bits& bits) {
    for (unsigned size = 0; size < 4; ++size) {
        for (unsigned matrix = 0; matrix < 6; matrix += size == 3 ? 3 : 1) {
            if (bits.Read(1) == 0) bits.Unsigned();
            else {
                if (size > 1) bits.Signed();
                const unsigned coefficients = size == 0 ? 16 : 64;
                for (unsigned coefficient = 0; coefficient < coefficients; ++coefficient) bits.Signed();
            }
        }
    }
}

void ShortTermReferences(Bits& bits) {
    const auto count = bits.Unsigned(64);
    std::array<unsigned, 64> deltas{};
    for (unsigned index = 0; index < count; ++index) {
        if (index != 0 && bits.Read(1) != 0) {
            bits.Skip(1);
            bits.Unsigned();
            for (unsigned reference = 0; reference <= deltas[index - 1]; ++reference) {
                const bool used = bits.Read(1) != 0;
                if (used || bits.Read(1) != 0) ++deltas[index];
            }
        } else {
            const auto negative = bits.Unsigned(16);
            const auto positive = bits.Unsigned(16);
            if (negative + positive > 16) throw std::runtime_error("too many HEVC short-term references");
            deltas[index] = negative + positive;
            for (unsigned reference = 0; reference < deltas[index]; ++reference) {
                bits.Unsigned();
                bits.Skip(1);
            }
        }
        if (deltas[index] > 16) throw std::runtime_error("too many predicted HEVC references");
    }
}

void Hrd(Bits& bits, unsigned sublayers) {
    const bool nal = bits.Read(1) != 0;
    const bool vcl = bits.Read(1) != 0;
    bool subpicture = false;
    if (nal || vcl) {
        subpicture = bits.Read(1) != 0;
        if (subpicture) bits.Skip(19);
        bits.Skip(8);
        if (subpicture) bits.Skip(4);
        bits.Skip(15);
    }
    for (unsigned layer = 0; layer <= sublayers; ++layer) {
        const bool general = bits.Read(1) != 0;
        const bool fixed = general || bits.Read(1) != 0;
        bool lowDelay = false;
        if (fixed) bits.Unsigned();
        else lowDelay = bits.Read(1) != 0;
        const auto count = lowDelay ? 0 : bits.Unsigned(31);
        for (unsigned kind = 0; kind < 2; ++kind) {
            if (!(kind == 0 ? nal : vcl)) continue;
            for (unsigned entry = 0; entry <= count; ++entry) {
                bits.Unsigned();
                bits.Unsigned();
                if (subpicture) {
                    bits.Unsigned();
                    bits.Unsigned();
                }
                bits.Skip(1);
            }
        }
    }
}

void Vui(Bits& bits, unsigned sublayers, Videodec2Hevc::PictureInfo& info) {
    info.aspectRatioInfoPresent = bits.Read(1);
    if (info.aspectRatioInfoPresent) {
        info.aspectRatioIdc = static_cast<std::uint8_t>(bits.Read(8));
        if (info.aspectRatioIdc == 255) {
            info.sarWidth = static_cast<std::uint16_t>(bits.Read(16));
            info.sarHeight = static_cast<std::uint16_t>(bits.Read(16));
        } else {
            constexpr std::uint16_t ratios[17][2] = {{0, 0}, {1, 1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11}, {20, 11}, {32, 11}, {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99}, {4, 3}, {3, 2}, {2, 1}};
            if (info.aspectRatioIdc > 16) throw std::runtime_error("reserved HEVC aspect ratio");
            info.sarWidth = ratios[info.aspectRatioIdc][0];
            info.sarHeight = ratios[info.aspectRatioIdc][1];
        }
    }
    if (bits.Read(1) != 0) bits.Skip(1);
    info.videoSignalTypePresent = static_cast<std::uint8_t>(bits.Read(1));
    if (info.videoSignalTypePresent) {
        info.videoFormat = static_cast<std::uint8_t>(bits.Read(3));
        info.fullRange = static_cast<std::uint8_t>(bits.Read(1));
        info.colourDescriptionPresent = static_cast<std::uint8_t>(bits.Read(1));
        if (info.colourDescriptionPresent) {
            info.colourPrimaries = static_cast<std::uint8_t>(bits.Read(8));
            info.transferCharacteristics = static_cast<std::uint8_t>(bits.Read(8));
            info.matrixCoefficients = static_cast<std::uint8_t>(bits.Read(8));
        }
    }
    info.chromaLocInfoPresent = static_cast<std::uint8_t>(bits.Read(1));
    if (info.chromaLocInfoPresent) {
        info.chromaLocTop = static_cast<std::uint8_t>(bits.Unsigned(5));
        info.chromaLocBottom = static_cast<std::uint8_t>(bits.Unsigned(5));
    }
    bits.Skip(1);
    info.fieldSequence = static_cast<std::uint8_t>(bits.Read(1));
    info.frameFieldInfoPresent = static_cast<std::uint8_t>(bits.Read(1));
    info.defaultDisplayWindow = bits.Read(1);
    if (info.defaultDisplayWindow) {
        info.displayLeft = bits.Unsigned();
        info.displayRight = bits.Unsigned();
        info.displayTop = bits.Unsigned();
        info.displayBottom = bits.Unsigned();
    }
    info.timingInfoPresent = static_cast<std::uint8_t>(bits.Read(1));
    if (info.timingInfoPresent) {
        info.numUnitsInTick = bits.Read(32);
        info.timeScale = bits.Read(32);
        if (bits.Read(1) != 0) bits.Unsigned();
        if (bits.Read(1) != 0) Hrd(bits, sublayers);
    }
    if (bits.Read(1) != 0) {
        bits.Skip(3);
        for (unsigned value = 0; value < 5; ++value) bits.Unsigned();
    }
}

std::pair<unsigned, Videodec2Hevc::Sps> ParseSps(Bits& bits) {
    Videodec2Hevc::Sps sps;
    auto& info = sps.info;
    info.thisSize = sizeof(info);
    info.isValid = true;
    info.videoFormat = 5;
    info.colourPrimaries = 2;
    info.transferCharacteristics = 2;
    info.matrixCoefficients = 2;
    sps.vps = bits.Read(4);
    const auto sublayers = bits.Read(3);
    if (sublayers > 6) throw std::runtime_error("invalid HEVC temporal layers");
    bits.Skip(1);
    bool mixed = false;
    info.sourceScanType = Profile(bits, sublayers, &mixed, &info);
    info.frameFieldInfoPresent = mixed ? 1 : 0;
    const auto id = bits.Unsigned(15);
    const auto chroma = bits.Unsigned(3);
    if (chroma == 3) bits.Skip(1);
    info.width = bits.Unsigned();
    info.height = bits.Unsigned();
    info.conformanceWindow = bits.Read(1);
    if (info.conformanceWindow) {
        info.confLeft = bits.Unsigned();
        info.confRight = bits.Unsigned();
        info.confTop = bits.Unsigned();
        info.confBottom = bits.Unsigned();
    }
    info.bitDepthLumaMinus8 = static_cast<std::uint8_t>(bits.Unsigned(8));
    info.bitDepthChromaMinus8 = static_cast<std::uint8_t>(bits.Unsigned(8));
    const auto orderBits = bits.Unsigned(12) + 4;
    Ordering(bits, sublayers, &info);
    for (unsigned value = 0; value < 6; ++value) bits.Unsigned();
    if (bits.Read(1) != 0 && bits.Read(1) != 0) ScalingLists(bits);
    bits.Skip(2);
    if (bits.Read(1) != 0) {
        bits.Skip(8);
        bits.Unsigned();
        bits.Unsigned();
        bits.Skip(1);
    }
    ShortTermReferences(bits);
    if (bits.Read(1) != 0) {
        const auto count = bits.Unsigned(32);
        for (unsigned reference = 0; reference < count; ++reference) bits.Skip(orderBits + 1);
    }
    bits.Skip(2);
    if (bits.Read(1) != 0) Vui(bits, sublayers, info);
    if (bits.Read(1) != 0 && bits.Read(8) != 0) sps.unsupported = "HEVC SPS extension metadata is unsupported";
    if (sublayers != 0) sps.unsupported = "HEVC sub-layer metadata projection is unsupported";
    if (info.conformanceWindow || info.defaultDisplayWindow) sps.unsupported = "HEVC crop/window metadata projection is unsupported";
    return {id, std::move(sps)};
}

std::vector<std::uint8_t> PictureTiming(std::span<const std::uint8_t> bytes, Videodec2Hevc::PictureInfo& flags, bool& transferAmbiguous) {
    std::vector<std::uint8_t> timing;
    std::size_t position = 0;
    while (position < bytes.size() && !(bytes[position] == 0x80 && position + 1 == bytes.size())) {
        const auto number = [&]() {
            std::size_t value = 0;
            for (;;) {
                if (position == bytes.size()) throw std::runtime_error("truncated HEVC SEI header");
                const auto next = bytes[position++];
                if (value > std::numeric_limits<std::size_t>::max() - next) throw std::runtime_error("HEVC SEI length overflow");
                value += next;
                if (next != 255) return value;
            }
        };
        const auto type = number();
        const auto size = number();
        if (size > bytes.size() - position) throw std::runtime_error("truncated HEVC SEI payload");
        if (type == 1) {
            if (flags.pictureTimingPresent) throw std::runtime_error("multiple HEVC picture-timing messages");
            flags.pictureTimingPresent = 1;
            timing.assign(bytes.begin() + position, bytes.begin() + position + size);
        } else if (type == 0) flags.bufferingPeriodPresent = 1;
        else if (type == 45) flags.framePackingPresent = 1;
        else if (type == 147) {
            if (size != 1 || (flags.alternativeTransferPresent && flags.preferredTransfer != bytes[position])) {
                transferAmbiguous = true;
                throw std::runtime_error("invalid or conflicting HEVC alternative-transfer metadata");
            }
            flags.alternativeTransferPresent = 1;
            flags.preferredTransfer = bytes[position];
        } else if (type != 4 && type != 5 && type != 6 && type != 129 && type != 132 && type != 136 && type != 137 && type != 144) {
            throw std::runtime_error("HEVC SEI metadata payload type " + std::to_string(type) + " is unsupported");
        }
        position += size;
    }
    return timing;
}

} // namespace

namespace Videodec2Hevc {

Metadata Parse_nid_no_patch(State& state, std::span<const std::uint8_t> annexB) {
    Metadata metadata;
    metadata.unsupported = state.unsupported;
    PictureInfo flags{};
    std::vector<std::size_t> starts;
    for (std::size_t position = 0; position + 2 < annexB.size(); ++position) {
        if (annexB[position] == 0 && annexB[position + 1] == 0 && annexB[position + 2] == 1) {
            starts.push_back(position + 3);
            position += 2;
        }
    }
    std::optional<unsigned> pictureSps;
    std::optional<unsigned> pictureType;
    std::optional<Sps> selectedSps;
    std::optional<Vps> selectedVps;
    std::vector<std::uint8_t> timing;
    bool transferAmbiguous = false;
    for (std::size_t index = 0; index < starts.size(); ++index) {
        const auto start = starts[index];
        auto end = index + 1 == starts.size() ? annexB.size() : starts[index + 1] - 3;
        while (end > start && annexB[end - 1] == 0) --end;
        unsigned type = 64;
        try {
            if (end - start < 2) throw std::runtime_error("truncated HEVC NAL header");
            type = (annexB[start] >> 1) & 63;
            if ((annexB[start] & 0x81) != 0 || (annexB[start + 1] & 0xf8) != 0)
                throw std::runtime_error("multilayer HEVC picture metadata is unsupported");
            if ((annexB[start + 1] & 7) == 0) throw std::runtime_error("invalid HEVC temporal identifier");
            const auto bytes = Rbsp(annexB.subspan(start + 2, end - start - 2));
            Bits bits(bytes);
            if (type == 32) {
                flags.vpsPresent = 1;
                auto [id, vps] = ParseVps(bits);
                vps.bytes = bytes;
                if (state.vps[id]) {
                    vps.unsupported = state.vps[id]->unsupported;
                    if (state.vps[id]->bytes != bytes) vps.unsupported = "HEVC VPS replacement metadata is unsupported until decoder recreation";
                }
                state.vps[id] = std::move(vps);
            } else if (type == 33) {
                flags.spsPresent = 1;
                auto [id, sps] = ParseSps(bits);
                sps.bytes = bytes;
                if (state.sps[id]) {
                    if (!state.sps[id]->unsupported.empty()) sps.unsupported = state.sps[id]->unsupported;
                    if (state.sps[id]->bytes != bytes) sps.unsupported = "HEVC SPS replacement metadata is unsupported until decoder recreation";
                }
                state.sps[id] = std::move(sps);
            } else if (type == 34) {
                flags.ppsPresent = 1;
                const auto id = bits.Unsigned(63);
                Pps pps{bits.Unsigned(15), bytes, {}};
                if (state.pps[id]) {
                    pps.unsupported = state.pps[id]->unsupported;
                    if (state.pps[id]->bytes != bytes) pps.unsupported = "HEVC PPS replacement metadata is unsupported until decoder recreation";
                }
                state.pps[id] = std::move(pps);
            } else if (type == 35) flags.delimiterPresent = 1;
            else if (type == 36) flags.endSequencePresent = 1;
            else if (type == 37) flags.endStreamPresent = 1;
            else if (type == 39 || type == 40) {
                auto message = PictureTiming(bytes, flags, transferAmbiguous);
                if (!message.empty()) {
                    if (type != 39 || !timing.empty()) throw std::runtime_error("unsupported HEVC picture-timing association");
                    timing = std::move(message);
                }
            } else if (type < 32) {
                const bool first = bits.Read(1) != 0;
                if (type >= 16 && type <= 23) bits.Skip(1);
                const auto pps = bits.Unsigned(63);
                if (!state.pps[pps]) throw std::runtime_error("HEVC picture parameter set is unavailable");
                if (!state.pps[pps]->unsupported.empty() && metadata.unsupported.empty()) metadata.unsupported = state.pps[pps]->unsupported;
                const auto sps = state.pps[pps]->sps;
                if (pictureSps && (first || *pictureSps != sps)) throw std::runtime_error("multiple HEVC pictures in one access unit");
                if (pictureType && *pictureType != type) throw std::runtime_error("mixed HEVC slice NAL types in one access unit");
                if (!pictureSps) {
                    if (!state.sps[sps]) throw std::runtime_error("HEVC sequence parameter set is unavailable");
                    selectedSps = state.sps[sps];
                    selectedVps = state.vps[selectedSps->vps];
                    pictureType = type;
                    flags.idr = type == 19 || type == 20;
                    flags.irap = type >= 16 && type <= 23;
                }
                pictureSps = sps;
            } else if (type == 38) flags.fillerPresent = 1;
            else throw std::runtime_error("reserved HEVC NAL metadata is unsupported");
        } catch (const std::runtime_error& error) {
            if (metadata.unsupported.empty()) metadata.unsupported = error.what();
            if (type >= 32 && type <= 34 && state.unsupported.empty()) state.unsupported = error.what();
        }
    }
    if (!selectedSps) {
        if (flags.endSequencePresent || flags.endStreamPresent) state.afterEnd = true;
        if ((flags.alternativeTransferPresent || transferAmbiguous) && state.unsupported.empty())
            state.unsupported = "HEVC alternative transfer in a pictureless access unit is unsupported";
        if (metadata.unsupported.empty()) metadata.unsupported = "HEVC sequence parameter set is unavailable";
        return metadata;
    }
    const auto& sps = *selectedSps;
    metadata.info = sps.info;
    const bool newSequence = !state.hadPicture || (*pictureType >= 16 && *pictureType <= 20) || (*pictureType == 21 && state.afterEnd);
    if (newSequence) {
        state.preferredTransfer.reset();
        state.transferUnsupported.clear();
    }
    if ((state.afterEnd || !state.hadPicture) && !flags.irap)
        state.transferUnsupported = "HEVC non-IRAP continuation after reset/flush is unsupported";
    if (transferAmbiguous) state.transferUnsupported = "ambiguous HEVC alternative-transfer metadata";
    if (flags.alternativeTransferPresent) {
        if (state.preferredTransfer && *state.preferredTransfer != flags.preferredTransfer)
            state.transferUnsupported = "conflicting HEVC alternative transfers within one CLVS";
        state.preferredTransfer = flags.preferredTransfer;
    }
    if (state.preferredTransfer) metadata.info.preferredTransfer = *state.preferredTransfer;
    if (!state.transferUnsupported.empty() && metadata.unsupported.empty()) metadata.unsupported = state.transferUnsupported;
    state.hadPicture = true;
    state.afterEnd = flags.endSequencePresent || flags.endStreamPresent;
    if (!sps.unsupported.empty() && metadata.unsupported.empty()) metadata.unsupported = sps.unsupported;
    if (!selectedVps) {
        if (metadata.unsupported.empty()) metadata.unsupported = "HEVC video parameter set is unavailable";
    } else if (!metadata.info.timingInfoPresent && selectedVps->timing) {
        if (metadata.unsupported.empty()) metadata.unsupported = "HEVC VPS-only timing projection is unsupported";
    }
    if (selectedVps && !selectedVps->unsupported.empty() && metadata.unsupported.empty()) metadata.unsupported = selectedVps->unsupported;
    if (metadata.info.frameFieldInfoPresent) {
        try {
            if (timing.empty()) throw std::runtime_error("HEVC picture timing is unavailable");
            Bits bits(timing);
            metadata.info.picStruct = bits.Read(4);
            metadata.info.sourceScanType = bits.Read(2);
            metadata.info.duplicate = bits.Read(1);
            if (metadata.info.picStruct > 12 || metadata.info.sourceScanType > 2)
                throw std::runtime_error("reserved HEVC picture-timing metadata");
        } catch (const std::runtime_error& error) {
            if (metadata.unsupported.empty()) metadata.unsupported = error.what();
        }
    }
    metadata.info.vpsPresent = flags.vpsPresent;
    metadata.info.spsPresent = flags.spsPresent;
    metadata.info.ppsPresent = flags.ppsPresent;
    metadata.info.delimiterPresent = flags.delimiterPresent;
    metadata.info.endSequencePresent = flags.endSequencePresent;
    metadata.info.endStreamPresent = flags.endStreamPresent;
    metadata.info.fillerPresent = flags.fillerPresent;
    metadata.info.pictureTimingPresent = flags.pictureTimingPresent;
    metadata.info.bufferingPeriodPresent = flags.bufferingPeriodPresent;
    metadata.info.framePackingPresent = flags.framePackingPresent;
    metadata.info.alternativeTransferPresent = flags.alternativeTransferPresent;
    metadata.info.idr = flags.idr;
    metadata.info.irap = flags.irap;
    return metadata;
}

} // namespace Videodec2Hevc
