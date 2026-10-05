#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

extern "C" {
int APS5_VABI sceAjmInitialize(std::int64_t, std::uint32_t*);
int APS5_VABI sceAjmFinalize(std::uint32_t);
int APS5_VABI sceAjmDecAt9ParseConfigData(const void*, AjmDecAt9ConfigDataInfo*);
int APS5_VABI sceAjmDecMp3ParseFrame(const std::uint8_t*, std::uint32_t, int, AjmDecMp3ParseFrame*);
int APS5_VABI sceAjmInstanceCreate(std::uint32_t, std::uint32_t, std::uint64_t, std::uint32_t*);
int APS5_VABI sceAjmInstanceDestroy(std::uint32_t, std::uint32_t);
int APS5_VABI sceAjmBatchInitialize(void*, std::size_t, AjmBatchInfo*);
int APS5_VABI sceAjmBatchJobInitialize(AjmBatchInfo*, std::uint32_t, const void*, std::size_t, void*);
int APS5_VABI sceAjmBatchJobDecode(AjmBatchInfo*, std::uint32_t, const void*, std::size_t, void*, std::size_t, void*);
int APS5_VABI sceAjmBatchJobDecodeSingle(AjmBatchInfo*, std::uint32_t, const void*, std::size_t, void*, std::size_t, void*);
int APS5_VABI sceAjmBatchJobRun(AjmBatchInfo*, std::uint32_t, std::uint64_t, const void*, std::size_t, void*, std::size_t, void*, std::size_t);
int APS5_VABI sceAjmBatchJobSetGaplessDecode(AjmBatchInfo*, std::uint32_t, const void*, int, void*);
int APS5_VABI sceAjmBatchJobGetGaplessDecode(AjmBatchInfo*, std::uint32_t, void*);
int APS5_VABI sceAjmBatchJobGetCodecInfo(AjmBatchInfo*, std::uint32_t, void*, std::size_t);
int APS5_VABI sceAjmBatchStart(std::uint32_t, const AjmBatchInfo*, int, AjmBatchError*, std::uint32_t*);
int APS5_VABI sceAjmBatchWait(std::uint32_t, std::uint32_t, std::uint32_t, AjmBatchError*);
}

static void Require(bool value) { if (!value) std::abort(); }

namespace {

const std::uint8_t MP3_MONO[] = {
    0xFF, 0xFB, 0x14, 0xC4, 0x00, 0x00, 0x03, 0xB0, 0x21, 0x5E, 0xF4, 0x30, 0x80, 0x30, 0x98, 0x88, 0xA9, 0x83, 0x34, 0x70, 0x00, 0x18, 0x89, 0xCB,
    0x20, 0x00, 0x3B, 0xBB, 0xB9, 0x9F, 0xD7, 0x0E, 0x06, 0x06, 0x06, 0x2C, 0x3E, 0xEF, 0x83, 0xE7, 0xF1, 0x00, 0x63, 0x83, 0xFD, 0x4E, 0xFC, 0xFF,
    0x29, 0xE1, 0xF7, 0xD9, 0x87, 0x7E, 0xFF, 0x65, 0x9E, 0x21, 0xAF, 0x46, 0x58, 0xBB, 0x46, 0x58, 0xBA, 0x64, 0x40, 0x21, 0xB2, 0x6C, 0xFA, 0x6C,
    0xF8, 0x42, 0x0E, 0x7C, 0x1E, 0x8F, 0x0D, 0xBF, 0x1B, 0x02, 0xBC, 0x24, 0x0D, 0x37, 0xCA, 0x9D, 0x52, 0xFD, 0x77, 0xE0, 0x16, 0xBA, 0x02, 0x04,
    0xFF, 0xFB, 0x14, 0xC4, 0x03, 0x83, 0xC4, 0xE0, 0x2B, 0x28, 0x1D, 0xE4, 0x80, 0x20, 0x5D, 0x95, 0x60, 0x41, 0x70, 0x16, 0xC8, 0xD3, 0x02, 0x00,
    0x2F, 0x12, 0x02, 0xD0, 0x40, 0x1D, 0x98, 0x4D, 0x06, 0x99, 0x84, 0xF2, 0x68, 0x98, 0xBF, 0x86, 0xD1, 0x84, 0xC8, 0x18, 0x2A, 0xA9, 0x79, 0x50,
    0x85, 0xDE, 0x14, 0x88, 0x9A, 0xF8, 0x44, 0x02, 0xB0, 0x60, 0x62, 0x5E, 0x11, 0xE9, 0xDC, 0x18, 0x65, 0x8F, 0xFF, 0x7F, 0xFC, 0xB3, 0xF8, 0x58,
    0xEF, 0xFF, 0xFF, 0xE1, 0x95, 0xF8, 0x44, 0x02, 0xB0, 0x60, 0x62, 0x5E, 0x11, 0xE9, 0xDC, 0x18, 0x65, 0x8F, 0xFF, 0x7F, 0xFC, 0xB3, 0xF8, 0x58,
    0xFF, 0xFB, 0x14, 0xC4, 0x09, 0x83, 0xC2, 0xEC, 0xAB, 0x02, 0x0B, 0x80, 0xB6, 0x40, 0x5D, 0x95, 0x60, 0x41, 0x70, 0x16, 0xC8, 0xEF, 0xFF, 0xFF,
    0xE1, 0x9F, 0x08, 0x80, 0x56, 0x0C, 0x0C, 0x4B, 0xC2, 0x3D, 0x3B, 0x83, 0x0C, 0xB1, 0xFF, 0xEF, 0xFF, 0x96, 0x7F, 0x0B, 0x1D, 0xFF, 0xFF, 0xFC,
    0x31, 0xF8, 0x44, 0x02, 0xB0, 0x60, 0x62, 0x5E, 0x11, 0xE9, 0xDC, 0x18, 0x65, 0x8F, 0xFF, 0x7F, 0xFC, 0xB3, 0xF8, 0x58, 0xEF, 0xFF, 0xFF, 0xE1,
    0x9F, 0x08, 0x80, 0x56, 0x0C, 0x0C, 0x4B, 0xC2, 0x3D, 0x3B, 0x83, 0x0C, 0xB1, 0xFF, 0xEF, 0xFF, 0x96, 0x7F, 0x0B, 0x1D, 0xFF, 0xFF, 0xFC, 0x31,
    0xFF, 0xFB, 0x14, 0xC4, 0x17, 0x83, 0xC2, 0xEC, 0xAB, 0x02, 0x0B, 0x80, 0xB6, 0x40, 0x5D, 0x95, 0x60, 0x41, 0x70, 0x16, 0xC8, 0x96, 0x55, 0x8D,
    0xBF, 0xE9, 0xD8, 0x50, 0x01, 0xA6, 0x17, 0x80, 0x34, 0x61, 0x00, 0x17, 0xC6, 0x15, 0x40, 0xD4, 0x61, 0x56, 0x25, 0x66, 0x3E, 0xA4, 0x92, 0x62,
    0xC3, 0xED, 0x26, 0x23, 0xA5, 0x5C, 0x63, 0x10, 0x01, 0xC6, 0x0F, 0x20, 0x9E, 0x60, 0x62, 0x08, 0x26, 0x06, 0xE0, 0x12, 0xD6, 0x22, 0x02, 0x6C,
    0x93, 0xC7, 0x04, 0x3E, 0xBF, 0xFE, 0xBF, 0xFF, 0x72, 0x62, 0x28, 0x22, 0xD1, 0x2F, 0xEF, 0xFD, 0xF1, 0x15, 0x09, 0x83, 0x08, 0x8C, 0x00, 0xFE,
    0xFF, 0xFB, 0x14, 0xC4, 0x25, 0x80, 0x07, 0x74, 0x2D, 0x16, 0x15, 0xE7, 0x80, 0x00, 0xF2, 0x0C, 0xE8, 0x83, 0x36, 0xB0, 0x00, 0xC4, 0x42, 0x82,
    0xC0, 0x86, 0x24, 0x2A, 0x02, 0x11, 0xFF, 0x02, 0x70, 0xF6, 0x07, 0x80, 0x9B, 0xCB, 0xCD, 0x0D, 0x00, 0x7C, 0x49, 0x1D, 0xBF, 0xD3, 0xDE, 0x3B,
    0x52, 0x36, 0xFE, 0xFA, 0xF3, 0x66, 0xB5, 0x13, 0xDF, 0x06, 0x84, 0xA1, 0x2F, 0xCB, 0x05, 0x41, 0x5F, 0xF5, 0x2A, 0x4C, 0x41, 0x4D, 0x45, 0x34,
    0x2E, 0x30, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xFF, 0xFB, 0x14, 0xC4, 0x0E, 0x83, 0xC0, 0x00, 0x01, 0xA4, 0x1C, 0x00, 0x00, 0x20, 0x00, 0x00, 0x34, 0x80, 0x00, 0x00, 0x04, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};

const std::uint8_t OPUS_STEREO[] = {
    0x46, 0x00, 0x7C, 0x87, 0xFC, 0xB1, 0x1D, 0xC0, 0xE3, 0x07, 0xD5, 0x9C, 0x4D, 0x91, 0x62, 0x6B, 0x73, 0x86, 0x05, 0xE8, 0xDB, 0xC6, 0x23, 0x6D,
    0x5E, 0x6F, 0x73, 0xF8, 0xF2, 0x47, 0xD9, 0x5E, 0xEA, 0xB3, 0xD2, 0x74, 0x6C, 0xE0, 0xCE, 0xE2, 0x3C, 0xFA, 0x59, 0x4A, 0xBA, 0x8F, 0x76, 0x0F,
    0x15, 0xE1, 0x3E, 0x60, 0xDF, 0x80, 0xC5, 0x44, 0xB1, 0x1B, 0xEF, 0x43, 0x03, 0x4F, 0xF2, 0xDE, 0xE3, 0xAD, 0x8B, 0x20, 0xFF, 0x68, 0x0C, 0x0A,
    0x3F, 0x00, 0x7C, 0x87, 0xFD, 0x45, 0xBD, 0x12, 0x00, 0xA5, 0xB1, 0xA0, 0x0A, 0x62, 0x24, 0x6F, 0x63, 0x70, 0xA5, 0x35, 0x13, 0x03, 0x74, 0x0D,
    0x83, 0xC9, 0x74, 0x1B, 0x79, 0xED, 0xA3, 0x09, 0x83, 0xA5, 0x02, 0xC4, 0x60, 0x49, 0xC9, 0xB6, 0x9A, 0x60, 0xAD, 0x6C, 0x77, 0x84, 0xE5, 0xC2,
    0xCE, 0x2E, 0x71, 0x07, 0x7E, 0x40, 0xCB, 0xDC, 0x13, 0xDF, 0xCF, 0x84, 0x5D, 0x55, 0x3B, 0x6B, 0x6F, 0x44, 0x00, 0x7C, 0x88, 0x01, 0xE8, 0xA1,
    0x2A, 0x12, 0x7D, 0x5E, 0xF6, 0x74, 0x78, 0xB1, 0x27, 0x2B, 0x8C, 0xF0, 0x7F, 0x95, 0x51, 0x71, 0x28, 0x18, 0xEA, 0x66, 0xEB, 0xCA, 0xAC, 0xDF,
    0x6C, 0x9C, 0xFD, 0xED, 0x88, 0x8E, 0x66, 0xD7, 0xDA, 0x36, 0xD7, 0xEB, 0x5F, 0xA6, 0x05, 0x72, 0xDA, 0x52, 0x14, 0x7F, 0xF5, 0x84, 0x54, 0xA1,
    0xA9, 0xF1, 0xAA, 0xA8, 0x73, 0x9A, 0xCC, 0x91, 0x83, 0x92, 0xD0, 0x7F, 0x3B, 0x6B, 0x65, 0x43, 0x00, 0x7C, 0x88, 0x01, 0xE8, 0xA1, 0x2A, 0x12,
    0x7D, 0x5F, 0x04, 0xF8, 0xF5, 0x1F, 0xA0, 0x85, 0x89, 0xED, 0xBA, 0x7C, 0x5F, 0x63, 0x2A, 0x9E, 0x05, 0xE8, 0x63, 0xD1, 0x73, 0x0C, 0xAF, 0xC8,
    0xC9, 0x22, 0xF7, 0x11, 0x1D, 0x8B, 0xA9, 0x9A, 0x90, 0x77, 0xF2, 0x86, 0x49, 0x15, 0x6E, 0xD6, 0x34, 0x1F, 0x50, 0x15, 0xEC, 0x85, 0xC7, 0xF5,
    0xA7, 0xFA, 0x99, 0xF9, 0x47, 0x32, 0x07, 0xAF, 0x24, 0xBF, 0x14, 0xC5,
};

struct DecodeSideband {
    std::int32_t result;
    std::int32_t internalResult;
    std::int32_t inputConsumed;
    std::int32_t outputWritten;
    std::uint64_t totalDecodedSamples;
};

void TestMp3(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 0, 0, &instance) == 0);
    std::vector<std::uint8_t> batch(0x40);
    std::vector<std::int16_t> pcm(1152);
    std::size_t offset = 0;
    std::int16_t peak = 0;
    for (int frame = 0; frame < 6; ++frame) {
        AjmBatchInfo info{};
        DecodeSideband sideband{};
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        Require(sceAjmBatchJobDecode(&info, instance, MP3_MONO + offset, sizeof(MP3_MONO) - offset, pcm.data(), pcm.size() * sizeof(std::int16_t), &sideband) == 0);
        std::uint32_t id = 0;
        AjmBatchError error{};
        Require(sceAjmBatchStart(context, &info, 0, &error, &id) == 0 && sceAjmBatchWait(context, id, 0, &error) == 0);
        Require(sideband.result == 0 && sideband.inputConsumed == 96 && sideband.outputWritten == 1152 * 2);
        Require(sideband.totalDecodedSamples == static_cast<std::uint64_t>(frame + 1) * 1152);
        for (const std::int16_t sample : pcm) peak = std::max<std::int16_t>(peak, static_cast<std::int16_t>(std::abs(sample)));
        offset += static_cast<std::size_t>(sideband.inputConsumed);
    }
    Require(offset == sizeof(MP3_MONO) && peak > 3276 && peak < 4915);
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void RunDecode(std::uint32_t context, std::uint32_t instance, const std::uint8_t* input, std::size_t inputSize, void* output, std::size_t outputSize, DecodeSideband& sideband) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobDecode(&info, instance, input, inputSize, output, outputSize, &sideband) == 0);
    std::uint32_t id = 0;
    AjmBatchError error{};
    Require(sceAjmBatchStart(context, &info, 0, &error, &id) == 0 && sceAjmBatchWait(context, id, 0, &error) == 0);
}

void TestOpus(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 24, 0, &instance) == 0);
    const std::uint32_t parameters[3] = {2, 48000, 0};
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int64_t initResult[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobInitialize(&info, instance, parameters, sizeof(parameters), initResult) == 0);
    std::uint32_t id = 0;
    AjmBatchError error{};
    Require(sceAjmBatchStart(context, &info, 0, &error, &id) == 0 && sceAjmBatchWait(context, id, 0, &error) == 0);
    Require(initResult[0] == 0);

    std::vector<std::int16_t> pcm(960 * 2);
    std::size_t offset = 0;
    std::int16_t peak = 0;
    for (int packet = 0; packet < 3; ++packet) {
        const std::size_t bytes = OPUS_STEREO[offset] | (std::size_t{OPUS_STEREO[offset + 1]} << 8u);
        DecodeSideband sideband{};
        RunDecode(context, instance, OPUS_STEREO + offset, sizeof(OPUS_STEREO) - offset, pcm.data(), pcm.size() * sizeof(std::int16_t), sideband);
        Require(sideband.result == 0 && static_cast<std::size_t>(sideband.inputConsumed) == 2 + bytes && sideband.outputWritten == 960 * 2 * 2);
        Require(sideband.totalDecodedSamples == static_cast<std::uint64_t>(packet + 1) * 960);
        if (packet > 0) {
            for (const std::int16_t sample : pcm) peak = std::max<std::int16_t>(peak, static_cast<std::int16_t>(std::abs(sample)));
        }
        offset += static_cast<std::size_t>(sideband.inputConsumed);
    }
    Require(peak > 900 && peak < 1200);

    std::vector<std::int16_t> small(512 * 2);
    DecodeSideband first{};
    RunDecode(context, instance, OPUS_STEREO + offset, sizeof(OPUS_STEREO) - offset, small.data(), small.size() * sizeof(std::int16_t), first);
    Require(first.result == 0 && static_cast<std::size_t>(first.inputConsumed) == sizeof(OPUS_STEREO) - offset && first.outputWritten == 512 * 2 * 2);
    DecodeSideband rest{};
    RunDecode(context, instance, OPUS_STEREO + sizeof(OPUS_STEREO), 0, small.data(), small.size() * sizeof(std::int16_t), rest);
    Require(rest.result == 0 && rest.inputConsumed == 0 && rest.outputWritten == (960 - 512) * 2 * 2);
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

bool ParsesTo(const std::uint8_t* stream, std::uint32_t streamSize, std::uint64_t frameSize, std::uint32_t channels, std::uint32_t samples, std::uint32_t bitrate, std::uint32_t sampleRate) {
    AjmDecMp3ParseFrame frame;
    std::memset(&frame, 0xFF, sizeof(frame));
    if (sceAjmDecMp3ParseFrame(stream, streamSize, 0, &frame) != 0) return false;
    return frame.frame_size == frameSize && frame.num_channels == channels && frame.samples_per_channel == samples && frame.bitrate == bitrate && frame.sample_rate == sampleRate &&
           frame.encoder_delay == 0 && frame.num_frames == 0 && frame.total_samples == 0 && frame.ofl_type == 0;
}

void TestMp3ParseFrame() {
    constexpr int invalidParameter = static_cast<int>(0x80930005);
    static_assert(sizeof(AjmDecMp3ParseFrame) == 40);
    for (std::size_t offset = 0; offset < sizeof(MP3_MONO); offset += 96) {
        Require(ParsesTo(MP3_MONO + offset, static_cast<std::uint32_t>(sizeof(MP3_MONO) - offset), 96, 1, 1152, 32000, 48000));
    }

    const std::uint8_t mpeg1StereoPadded[4] = {0xFF, 0xFB, 0x92, 0x00};
    Require(ParsesTo(mpeg1StereoPadded, 4, 418, 2, 1152, 128000, 44100));
    const std::uint8_t mpeg1Protected[4] = {0xFF, 0xFA, 0xE8, 0x40};
    Require(ParsesTo(mpeg1Protected, 4, 1440, 2, 1152, 320000, 32000));
    const std::uint8_t mpeg2Mono[4] = {0xFF, 0xF3, 0x80, 0xC0};
    Require(ParsesTo(mpeg2Mono, 4, 208, 1, 576, 64000, 22050));
    const std::uint8_t mpeg2DualChannel[4] = {0xFF, 0xF3, 0xE8, 0x80};
    Require(ParsesTo(mpeg2DualChannel, 4, 720, 2, 576, 160000, 16000));
    const std::uint8_t mpeg25Mono[4] = {0xFF, 0xE3, 0x88, 0xC0};
    Require(ParsesTo(mpeg25Mono, 4, 576, 1, 576, 64000, 8000));
    const std::uint8_t mpeg25StereoPadded[4] = {0xFF, 0xE3, 0x12, 0x00};
    Require(ParsesTo(mpeg25StereoPadded, 4, 53, 2, 576, 8000, 11025));

    AjmDecMp3ParseFrame frame{};
    Require(sceAjmDecMp3ParseFrame(nullptr, 4, 0, &frame) == invalidParameter);
    Require(sceAjmDecMp3ParseFrame(MP3_MONO, 4, 0, nullptr) == invalidParameter);
    Require(sceAjmDecMp3ParseFrame(MP3_MONO, 3, 0, &frame) == invalidParameter);
    const std::uint8_t brokenSync[4] = {0xFF, 0xDB, 0x14, 0xC4};
    Require(sceAjmDecMp3ParseFrame(brokenSync, 4, 0, &frame) == invalidParameter);
    const std::uint8_t reservedVersion[4] = {0xFF, 0xEB, 0x14, 0xC4};
    Require(sceAjmDecMp3ParseFrame(reservedVersion, 4, 0, &frame) == invalidParameter);
    const std::uint8_t freeBitrate[4] = {0xFF, 0xFB, 0x04, 0xC4};
    Require(sceAjmDecMp3ParseFrame(freeBitrate, 4, 0, &frame) == invalidParameter);
    const std::uint8_t forbiddenBitrate[4] = {0xFF, 0xFB, 0xF4, 0xC4};
    Require(sceAjmDecMp3ParseFrame(forbiddenBitrate, 4, 0, &frame) == invalidParameter);
    const std::uint8_t reservedSampleRate[4] = {0xFF, 0xFB, 0x1C, 0xC4};
    Require(sceAjmDecMp3ParseFrame(reservedSampleRate, 4, 0, &frame) == invalidParameter);
    const std::uint8_t mpeg25Above64Kbps[4] = {0xFF, 0xE3, 0x98, 0xC0};
    Require(sceAjmDecMp3ParseFrame(mpeg25Above64Kbps, 4, 0, &frame) == invalidParameter);
}

struct GaplessDecode {
    std::uint32_t totalSamples;
    std::uint16_t skipSamples;
    std::uint16_t skippedSamples;
};

struct GaplessSideband {
    std::int32_t result;
    std::int32_t internalResult;
    std::uint32_t totalSamples;
    std::uint16_t skipSamples;
    std::uint16_t skippedSamples;
};

struct At9CodecInfoSideband {
    std::int32_t result;
    std::int32_t internalResult;
    std::uint32_t superframeSize;
    std::uint32_t framesInSuperframe;
    std::uint32_t nextFrameSize;
    std::uint32_t frameSamples;
};

void Submit(std::uint32_t context, const AjmBatchInfo& info) {
    std::uint32_t id = 0;
    AjmBatchError error{};
    Require(sceAjmBatchStart(context, &info, 0, &error, &id) == 0 && sceAjmBatchWait(context, id, 0, &error) == 0);
}

bool Refused(std::uint32_t context, const AjmBatchInfo& info) {
    std::uint32_t id = 0;
    AjmBatchError error{};
    try {
        static_cast<void>(sceAjmBatchStart(context, &info, 0, &error, &id));
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestDecodeSingle(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 0, 0, &instance) == 0);
    std::vector<std::uint8_t> batch(0x40);
    std::vector<std::int16_t> pcm(1152 * 6);
    AjmBatchInfo info{};
    DecodeSideband sideband{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobDecodeSingle(&info, instance, MP3_MONO, sizeof(MP3_MONO), pcm.data(), pcm.size() * sizeof(std::int16_t), &sideband) == 0);
    Submit(context, info);
    Require(sideband.result == 0 && sideband.inputConsumed == 96 && sideband.outputWritten == 1152 * 2 && sideband.totalDecodedSamples == 1152);
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void TestGaplessDecode(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 0, 0, &instance) == 0);
    std::vector<std::uint8_t> batch(4096);
    const GaplessDecode gapless{2000, 100, 0};
    AjmBatchInfo info{};
    std::int32_t setResult[2] = {-1, -1};
    GaplessSideband before{-1, -1, 0, 0, 0xffff};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobSetGaplessDecode(&info, instance, &gapless, 1, setResult) == 0);
    Require(sceAjmBatchJobGetGaplessDecode(&info, instance, &before) == 0);
    Submit(context, info);
    Require(setResult[0] == 0);
    Require(before.result == 0 && before.internalResult == 0 && before.totalSamples == 2000 && before.skipSamples == 100 && before.skippedSamples == 0);

    std::vector<std::int16_t> pcm(1152);
    DecodeSideband decoded{};
    GaplessSideband after{-1, -1, 0, 0, 0};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobDecodeSingle(&info, instance, MP3_MONO, sizeof(MP3_MONO), pcm.data(), pcm.size() * sizeof(std::int16_t), &decoded) == 0);
    Require(sceAjmBatchJobGetGaplessDecode(&info, instance, &after) == 0);
    Submit(context, info);
    Require(decoded.result == 0 && decoded.inputConsumed == 96 && decoded.outputWritten == (1152 - 100) * 2);
    Require(after.result == 0 && after.skippedSamples == 100);
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void TestCodecInfo(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 1, 0, &instance) == 0);
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    At9CodecInfoSideband early{-1, -1, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobGetCodecInfo(&info, instance, &early, sizeof(early)) == 0);
    Submit(context, info);
    Require(early.result == 1 && early.superframeSize == 0xaaaaaaaau);

    const std::uint8_t config[8] = {0xFE, 0x72, 0x1F, 0xF0};
    std::int32_t initResult[2] = {-1, -1};
    At9CodecInfoSideband codec{-1, -1, 0, 0, 0, 0};
    At9CodecInfoSideband bounded{-1, -1, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobInitialize(&info, instance, config, sizeof(config), initResult) == 0);
    Require(sceAjmBatchJobGetCodecInfo(&info, instance, &codec, sizeof(codec)) == 0);
    Require(sceAjmBatchJobGetCodecInfo(&info, instance, &bounded, 2 * sizeof(std::int32_t)) == 0);
    Submit(context, info);
    Require(initResult[0] == 0);
    Require(codec.result == 0 && codec.internalResult == 0 && codec.superframeSize == 1024 && codec.framesInSuperframe == 4 && codec.nextFrameSize == 1024 && codec.frameSamples == 256);
    Require(bounded.result == 0 && bounded.superframeSize == 0xaaaaaaaau && bounded.frameSamples == 0xaaaaaaaau);

    constexpr std::uint64_t runGetCodecInfo = 1ull << 11;
    constexpr std::uint64_t runMultipleFrames = 1ull << 12;
    std::uint8_t sideband[64] = {};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobRun(&info, instance, runGetCodecInfo | runMultipleFrames, nullptr, 0, nullptr, 0, sideband, sizeof(sideband)) == 0);
    Require(Refused(context, info));
    Require(sceAjmInstanceDestroy(context, instance) == 0);

    Require(sceAjmInstanceCreate(context, 0, 0, &instance) == 0);
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobGetCodecInfo(&info, instance, sideband, sizeof(sideband)) == 0);
    Require(Refused(context, info));
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void TestBatchWaitRelease(std::uint32_t context) {
    constexpr int invalidBatch = static_cast<int>(0x80930004);
    std::vector<std::uint8_t> batch(64);
    AjmBatchInfo info{};
    AjmBatchError error{};
    std::uint32_t first = 0;
    std::uint32_t second = 0;
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchStart(context, &info, 0, &error, &first) == 0);
    Require(sceAjmBatchStart(context, &info, 0, &error, &second) == 0 && second != first);
    Require(sceAjmBatchWait(context, second, 0, &error) == 0);
    Require(sceAjmBatchWait(context, first, 0, &error) == 0);
    Require(sceAjmBatchWait(context, first, 0, &error) == invalidBatch);
    Require(sceAjmBatchWait(context, second, 0, &error) == invalidBatch);
    Require(sceAjmBatchWait(context, 0, 0, &error) == invalidBatch);
    Require(sceAjmBatchWait(context, second + 1, 0, &error) == invalidBatch);
}

}

int main() {
    constexpr int invalidParameter = static_cast<int>(0x80930005);
    std::uint32_t context = 0;
    Require(sceAjmInitialize(0, nullptr) == invalidParameter);
    Require(sceAjmInitialize(0, &context) == 0 && context != 0);

    const std::uint8_t stereo48k[4] = {0xFE, 0x72, 0x1F, 0xF0};
    AjmDecAt9ConfigDataInfo info{};
    Require(sceAjmDecAt9ParseConfigData(stereo48k, &info) == 0);
    Require(info.channels == 2 && info.sample_rate == 48000 && info.frame_samples_per_channel == 256);
    Require(info.superframe_samples_per_channel == 1024 && info.superframe_size == 1024);
    const std::uint8_t badHeader[4] = {0xFD, 0x72, 0x1F, 0xF0};
    Require(sceAjmDecAt9ParseConfigData(badHeader, &info) == invalidParameter);
    Require(sceAjmDecAt9ParseConfigData(nullptr, &info) == invalidParameter);
    TestMp3ParseFrame();
    TestMp3(context);
    TestOpus(context);
    TestDecodeSingle(context);
    TestGaplessDecode(context);
    TestCodecInfo(context);
    TestBatchWaitRelease(context);
    Require(sceAjmFinalize(context) == 0);
}
