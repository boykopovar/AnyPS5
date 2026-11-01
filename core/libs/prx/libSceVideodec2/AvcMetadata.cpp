#include "AvcMetadata.hpp"
#include <algorithm>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t SliceHeaderBytes = 32;

class Bits {
public:
    explicit Bits(std::span<const std::uint8_t> bytes) : bytes(bytes) {}

    std::uint32_t Read(unsigned count) {
        if (count > 32 || count > bytes.size() * 8 - position) throw std::runtime_error("truncated H.264 syntax");
        std::uint32_t value = 0;
        for (unsigned bit = 0; bit < count; ++bit, ++position)
            value = (value << 1) | ((bytes[position / 8] >> (7 - position % 8)) & 1);
        return value;
    }

    void Skip(std::size_t count) {
        if (count > bytes.size() * 8 - position) throw std::runtime_error("truncated H.264 syntax");
        position += count;
    }

    std::uint32_t Unsigned(std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max()) {
        unsigned zeros = 0;
        while (Read(1) == 0) if (++zeros > 31) throw std::runtime_error("H.264 exponential code overflow");
        const auto value = ((std::uint32_t{1} << zeros) - 1) + Read(zeros);
        if (value > maximum) throw std::runtime_error("H.264 syntax value outside its domain");
        return value;
    }

    std::int64_t Signed() {
        const std::int64_t code = Unsigned();
        return (code & 1) != 0 ? (code + 1) / 2 : -(code / 2);
    }

private:
    std::span<const std::uint8_t> bytes;
    std::size_t position = 0;
};

struct Slice {
    unsigned pps = 0;
    std::uint32_t frameNum = 0;
    std::uint32_t field = 0;
    std::uint32_t bottom = 0;
    bool idr = false;
    bool reference = false;

    bool operator==(const Slice&) const = default;
};

std::vector<std::uint8_t> Rbsp(std::span<const std::uint8_t> nal) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(nal.size());
    unsigned zeros = 0;
    for (std::size_t index = 0; index < nal.size(); ++index) {
        const auto value = nal[index];
        if (zeros == 2 && value == 3) {
            if (index + 1 < nal.size() && nal[index + 1] > 3) throw std::runtime_error("invalid H.264 emulation prevention");
            zeros = 0;
            continue;
        }
        if (zeros == 2 && value < 3) throw std::runtime_error("missing H.264 emulation prevention");
        bytes.push_back(value);
        zeros = value == 0 ? std::min(zeros + 1, 2u) : 0;
    }
    return bytes;
}

void ScalingList(Bits& bits, unsigned size) {
    std::int64_t last = 8;
    for (unsigned index = 0; index < size; ++index) {
        const auto delta = bits.Signed();
        if (delta < -128 || delta > 127) throw std::runtime_error("H.264 scaling list delta outside its domain");
        const auto next = (last + delta + 256) % 256;
        if (next == 0) return;
        last = next;
    }
}

std::pair<unsigned, unsigned> Hrd(Bits& bits) {
    const auto count = bits.Unsigned(31) + 1;
    bits.Skip(8);
    for (unsigned entry = 0; entry < count; ++entry) {
        bits.Unsigned();
        bits.Unsigned();
        bits.Skip(1);
    }
    bits.Skip(5);
    const auto removal = bits.Read(5) + 1;
    const auto output = bits.Read(5) + 1;
    bits.Skip(5);
    return {removal, output};
}

void Vui(Bits& bits, Videodec2Avc::Sps& sps) {
    auto& info = sps.info;
    info.aspectRatioInfoPresentFlag = static_cast<std::uint8_t>(bits.Read(1));
    if (info.aspectRatioInfoPresentFlag) {
        info.aspectRatioIdc = static_cast<std::uint8_t>(bits.Read(8));
        if (info.aspectRatioIdc == 255) {
            info.sarWidth = static_cast<std::uint16_t>(bits.Read(16));
            info.sarHeight = static_cast<std::uint16_t>(bits.Read(16));
        } else if (info.aspectRatioIdc <= 16) {
            constexpr std::uint16_t ratios[17][2] = {{0, 0}, {1, 1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11}, {20, 11}, {32, 11}, {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99}, {4, 3}, {3, 2}, {2, 1}};
            info.sarWidth = ratios[info.aspectRatioIdc][0];
            info.sarHeight = ratios[info.aspectRatioIdc][1];
        }
    }
    if (bits.Read(1) != 0) bits.Skip(1);
    info.videoSignalTypePresentFlag = static_cast<std::uint8_t>(bits.Read(1));
    if (info.videoSignalTypePresentFlag) {
        info.videoFormat = static_cast<std::uint8_t>(bits.Read(3));
        info.videoFullRangeFlag = static_cast<std::uint8_t>(bits.Read(1));
        info.colourDescriptionPresentFlag = static_cast<std::uint8_t>(bits.Read(1));
        if (info.colourDescriptionPresentFlag) {
            info.colourPrimaries = static_cast<std::uint8_t>(bits.Read(8));
            info.transferCharacteristics = static_cast<std::uint8_t>(bits.Read(8));
            info.matrixCoefficients = static_cast<std::uint8_t>(bits.Read(8));
        }
    }
    if (bits.Read(1) != 0) {
        bits.Unsigned(5);
        bits.Unsigned(5);
    }
    info.timingInfoPresentFlag = static_cast<std::uint8_t>(bits.Read(1));
    if (info.timingInfoPresentFlag) {
        info.numUnitsInTick = bits.Read(32);
        info.timeScale = bits.Read(32);
        info.fixedFrameRateFlag = static_cast<std::uint8_t>(bits.Read(1));
    }
    for (unsigned kind = 0; kind < 2; ++kind) {
        if (bits.Read(1) == 0) continue;
        const auto delays = Hrd(bits);
        if (sps.delays && *sps.delays != delays) sps.unsupported = "differing H.264 NAL and VCL HRD delay lengths";
        sps.delays = delays;
    }
    if (sps.delays) bits.Skip(1);
    info.picStructPresentFlag = static_cast<std::uint8_t>(bits.Read(1));
    info.bitstreamRestrictionFlag = static_cast<std::uint8_t>(bits.Read(1));
    if (info.bitstreamRestrictionFlag) {
        bits.Skip(1);
        for (unsigned value = 0; value < 5; ++value) bits.Unsigned();
        const auto buffering = bits.Unsigned();
        if (buffering > 16) sps.unsupported = "H.264 max_dec_frame_buffering above 16";
        info.maxDecFrameBuffering = static_cast<std::uint8_t>(std::min(buffering, 16u));
    }
}

std::uint32_t MaxDpbMbs(const Videodec2Avc::PictureInfo& info) {
    const unsigned level = info.levelIdc;
    if (level == 9 || (level == 11 && info.constraintSet3Flag && (info.profileIdc == 66 || info.profileIdc == 77 || info.profileIdc == 88))) return 396;
    constexpr std::pair<unsigned, std::uint32_t> limits[] = {{10, 396}, {11, 900}, {12, 2376}, {13, 2376}, {20, 2376}, {21, 4752}, {22, 8100}, {30, 8100}, {31, 18000}, {32, 20480},
        {40, 32768}, {41, 32768}, {42, 34816}, {50, 110400}, {51, 184320}, {52, 184320}, {60, 696320}, {61, 696320}, {62, 696320}};
    for (const auto& [idc, mbs] : limits) if (idc == level) return mbs;
    return 0;
}

std::pair<unsigned, Videodec2Avc::Sps> ParseSps(Bits& bits) {
    Videodec2Avc::Sps sps;
    auto& info = sps.info;
    info.thisSize = sizeof(info);
    info.isValid = true;
    info.videoFormat = 5;
    info.colourPrimaries = 2;
    info.transferCharacteristics = 2;
    info.matrixCoefficients = 2;
    info.profileIdc = static_cast<std::uint8_t>(bits.Read(8));
    for (auto* flag : {&info.constraintSet0Flag, &info.constraintSet1Flag, &info.constraintSet2Flag, &info.constraintSet3Flag, &info.constraintSet4Flag, &info.constraintSet5Flag})
        *flag = static_cast<std::uint8_t>(bits.Read(1));
    bits.Skip(2);
    info.levelIdc = static_cast<std::uint8_t>(bits.Read(8));
    const auto id = bits.Unsigned(31);
    const auto profile = info.profileIdc;
    if (profile == 100 || profile == 110 || profile == 122 || profile == 244 || profile == 44 || profile == 83 || profile == 86 || profile == 118 || profile == 128 || profile == 138 || profile == 139 || profile == 134 || profile == 135) {
        const auto chroma = bits.Unsigned(3);
        if (chroma == 3) sps.separateColourPlanes = bits.Read(1) != 0;
        bits.Unsigned(6);
        bits.Unsigned(6);
        bits.Skip(1);
        if (bits.Read(1) != 0)
            for (unsigned list = 0; list < (chroma != 3 ? 8u : 12u); ++list)
                if (bits.Read(1) != 0) ScalingList(bits, list < 6 ? 16 : 64);
    }
    sps.frameNumBits = bits.Unsigned(12) + 4;
    const auto order = bits.Unsigned(2);
    if (order == 0) bits.Unsigned(12);
    else if (order == 1) {
        bits.Skip(1);
        bits.Signed();
        bits.Signed();
        const auto cycle = bits.Unsigned(255);
        for (unsigned frame = 0; frame < cycle; ++frame) bits.Signed();
    }
    bits.Unsigned();
    bits.Skip(1);
    info.picWidthInMbsMinus1 = bits.Unsigned();
    info.picHeightInMapUnitsMinus1 = bits.Unsigned();
    info.frameMbsOnlyFlag = static_cast<std::uint8_t>(bits.Read(1));
    if (!info.frameMbsOnlyFlag) bits.Skip(1);
    bits.Skip(1);
    info.frameCroppingFlag = static_cast<std::uint8_t>(bits.Read(1));
    if (info.frameCroppingFlag) {
        info.frameCropLeftOffset = bits.Unsigned();
        info.frameCropRightOffset = bits.Unsigned();
        info.frameCropTopOffset = bits.Unsigned();
        info.frameCropBottomOffset = bits.Unsigned();
    }
    if (bits.Read(1) != 0) Vui(bits, sps);
    if (!info.bitstreamRestrictionFlag) {
        const bool intra = info.constraintSet3Flag && (profile == 44 || profile == 86 || profile == 100 || profile == 110 || profile == 122 || profile == 244);
        const auto mbs = MaxDpbMbs(info);
        if (!intra && mbs == 0) sps.unsupported = "H.264 level outside Table A-1";
        const auto frames = std::uint64_t{mbs} / (std::uint64_t{info.picWidthInMbsMinus1} + 1) / ((2u - info.frameMbsOnlyFlag) * (std::uint64_t{info.picHeightInMapUnitsMinus1} + 1));
        info.maxDecFrameBuffering = intra ? 0 : static_cast<std::uint8_t>(std::min<std::uint64_t>(frames, 16));
    }
    return {id, std::move(sps)};
}

void Sei(std::span<const std::uint8_t> bytes, Videodec2Avc::PictureInfo& flags, std::vector<std::uint8_t>& timing) {
    std::size_t position = 0;
    while (position < bytes.size() && !(bytes[position] == 0x80 && position + 1 == bytes.size())) {
        const auto number = [&]() {
            std::size_t value = 0;
            for (;;) {
                if (position == bytes.size()) throw std::runtime_error("truncated H.264 SEI header");
                const auto next = bytes[position++];
                value += next;
                if (next != 255) return value;
            }
        };
        const auto type = number();
        const auto size = number();
        if (size > bytes.size() - position) throw std::runtime_error("truncated H.264 SEI payload");
        if (type == 0) flags.bufferingPeriodSeiPresentFlag = 1;
        else if (type == 1) {
            if (flags.pictureTimingSeiPresentFlag) throw std::runtime_error("multiple H.264 picture-timing messages");
            flags.pictureTimingSeiPresentFlag = 1;
            timing.assign(bytes.begin() + position, bytes.begin() + position + size);
        }
        position += size;
    }
}

} // namespace

namespace Videodec2Avc {

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
    std::optional<Slice> first;
    std::optional<Sps> selected;
    std::vector<std::uint8_t> timing;
    bool sliced = false;
    for (std::size_t index = 0; index < starts.size(); ++index) {
        const auto start = starts[index];
        auto end = index + 1 == starts.size() ? annexB.size() : starts[index + 1] - 3;
        while (end > start && annexB[end - 1] == 0) --end;
        const unsigned type = end > start ? annexB[start] & 0x1f : 0;
        try {
            if (end == start || (annexB[start] & 0x80) != 0) throw std::runtime_error("invalid H.264 NAL header");
            if (sliced && type >= 6 && type <= 9 && metadata.unsupported.empty()) metadata.unsupported = "H.264 SEI, parameter set or delimiter after the slices of an access unit is unsupported";
            const bool slice = type == 1 || type == 5;
            sliced = sliced || slice;
            const auto payload = annexB.subspan(start + 1, end - start - 1);
            const auto bytes = Rbsp(slice ? payload.first(std::min(payload.size(), SliceHeaderBytes)) : payload);
            Bits bits(bytes);
            if (type == 7) {
                flags.sequenceParameterSetPresentFlag = 1;
                auto [id, sps] = ParseSps(bits);
                sps.bytes = bytes;
                if (state.sps[id]) {
                    if (!state.sps[id]->unsupported.empty()) sps.unsupported = state.sps[id]->unsupported;
                    if (state.sps[id]->bytes != bytes) sps.unsupported = "H.264 SPS replacement metadata is unsupported until decoder recreation";
                }
                state.sps[id] = std::move(sps);
            } else if (type == 8) {
                flags.pictureParameterSetPresentFlag = 1;
                const auto id = bits.Unsigned(255);
                Pps pps{bits.Unsigned(31), bytes, {}};
                if (state.pps[id]) {
                    pps.unsupported = state.pps[id]->unsupported;
                    if (state.pps[id]->bytes != bytes) pps.unsupported = "H.264 PPS replacement metadata is unsupported until decoder recreation";
                }
                state.pps[id] = std::move(pps);
            } else if (type == 6) Sei(bytes, flags, timing);
            else if (type == 9) {
                if (flags.auDelimiterPresentFlag) throw std::runtime_error("multiple H.264 access unit delimiters");
                flags.auDelimiterPresentFlag = 1;
            } else if (type == 10) flags.endOfSequencePresentFlag = 1;
            else if (type == 11) flags.endOfStreamPresentFlag = 1;
            else if (type == 12) flags.fillerDataPresentFlag = 1;
            else if (slice) {
                const auto firstMb = bits.Unsigned();
                bits.Unsigned(9);
                const auto ppsId = bits.Unsigned(255);
                if (!state.pps[ppsId]) throw std::runtime_error("H.264 picture parameter set is unavailable");
                const auto& pps = *state.pps[ppsId];
                if (!state.sps[pps.sps]) throw std::runtime_error("H.264 sequence parameter set is unavailable");
                const auto& sps = *state.sps[pps.sps];
                if (sps.separateColourPlanes) bits.Skip(2);
                Slice current{ppsId, bits.Read(sps.frameNumBits), 0, 0, type == 5, (annexB[start] & 0x60) != 0};
                if (!sps.info.frameMbsOnlyFlag) {
                    current.field = bits.Read(1);
                    if (current.field) current.bottom = bits.Read(1);
                }
                if (first) {
                    if (firstMb == 0 || current != *first) throw std::runtime_error("several H.264 primary pictures or mixed slice types in one access unit");
                } else {
                    first = current;
                    selected = sps;
                    if (!pps.unsupported.empty() && metadata.unsupported.empty()) metadata.unsupported = pps.unsupported;
                }
            } else throw std::runtime_error("H.264 NAL unit type " + std::to_string(type) + " metadata is unsupported");
        } catch (const std::runtime_error& error) {
            if (metadata.unsupported.empty()) metadata.unsupported = error.what();
            if ((type == 7 || type == 8) && state.unsupported.empty()) state.unsupported = error.what();
        }
    }
    if (!selected) {
        if (metadata.unsupported.empty()) metadata.unsupported = "H.264 access unit carries no picture";
        return metadata;
    }
    auto& info = metadata.info;
    info = selected->info;
    if (!selected->unsupported.empty() && metadata.unsupported.empty()) metadata.unsupported = selected->unsupported;
    if (first->field && metadata.unsupported.empty()) metadata.unsupported = "H.264 field picture metadata is unsupported";
    info.idrPictureFlag = first->idr;
    info.fieldPicFlag = static_cast<std::uint8_t>(first->field);
    info.bottomFieldFlag = static_cast<std::uint8_t>(first->bottom);
    if (info.picStructPresentFlag) {
        try {
            if (!flags.pictureTimingSeiPresentFlag) throw std::runtime_error("H.264 picture timing is unavailable");
            Bits bits(timing);
            if (selected->delays) bits.Skip(std::size_t{selected->delays->first} + selected->delays->second);
            info.picStruct = static_cast<std::uint8_t>(bits.Read(4));
            if (info.picStruct > 8) throw std::runtime_error("reserved H.264 picture structure");
        } catch (const std::runtime_error& error) {
            if (metadata.unsupported.empty()) metadata.unsupported = error.what();
        }
    }
    info.sequenceParameterSetPresentFlag = flags.sequenceParameterSetPresentFlag;
    info.pictureParameterSetPresentFlag = flags.pictureParameterSetPresentFlag;
    info.auDelimiterPresentFlag = flags.auDelimiterPresentFlag;
    info.endOfSequencePresentFlag = flags.endOfSequencePresentFlag;
    info.endOfStreamPresentFlag = flags.endOfStreamPresentFlag;
    info.fillerDataPresentFlag = flags.fillerDataPresentFlag;
    info.pictureTimingSeiPresentFlag = flags.pictureTimingSeiPresentFlag;
    info.bufferingPeriodSeiPresentFlag = flags.bufferingPeriodSeiPresentFlag;
    return metadata;
}

} // namespace Videodec2Avc
