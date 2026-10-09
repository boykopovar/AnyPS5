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
const char* APS5_VABI sceAjmStrError(int);
int APS5_VABI sceAjmBatchJobRunSplit(AjmBatchInfo*, std::uint32_t, std::uint64_t, const AjmBuffer*, std::size_t, const AjmBuffer*, std::size_t, void*, std::size_t);
int APS5_VABI sceAjmBatchJobSetGaplessDecode(AjmBatchInfo*, std::uint32_t, const void*, int, void*);
int APS5_VABI sceAjmBatchJobControl(AjmBatchInfo*, std::uint32_t, std::uint64_t, const void*, std::size_t, void*, std::size_t);
int APS5_VABI sceAjmBatchJobGetGaplessDecode(AjmBatchInfo*, std::uint32_t, void*);
int APS5_VABI sceAjmBatchJobGetCodecInfo(AjmBatchInfo*, std::uint32_t, void*, std::size_t);
int APS5_VABI sceAjmBatchJobGetInfo(AjmBatchInfo*, std::uint32_t, void*);
int APS5_VABI sceAjmBatchStart(std::uint32_t, const AjmBatchInfo*, int, AjmBatchError*, std::uint32_t*);
int APS5_VABI sceAjmBatchWait(std::uint32_t, std::uint32_t, std::uint32_t, AjmBatchError*);
int APS5_VABI sceAjmBatchCancel(std::uint32_t, std::uint32_t);
int APS5_VABI sceAjmBatchJobClearContext(AjmBatchInfo*, std::uint32_t, void*);
int APS5_VABI sceAjmBatchJobSetResampleParameters(AjmBatchInfo*, std::uint32_t, float, std::uint32_t, void*);
int APS5_VABI sceAjmBatchJobGetResampleInfo(AjmBatchInfo*, std::uint32_t, void*);
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

const std::uint8_t LAME_MPEG1_STEREO_VBR[] = {
    0xFF, 0xFB, 0x90, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x69, 0x6E, 0x67, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0A,
    0x00, 0x00, 0x09, 0x28, 0x00, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x9F, 0x9F, 0x9F, 0x9F,
    0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE,
    0xBE, 0xBE, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x0A, 0x4C, 0x41, 0x4D, 0x45, 0x34, 0x2E, 0x30, 0x20, 0x00, 0x04, 0x48, 0x00,
    0x00, 0x00, 0x00, 0x2E, 0x1A, 0x00, 0x00, 0x15, 0x20, 0x24, 0x03, 0xB0, 0x45, 0x00, 0x01, 0x9A, 0x00, 0x00, 0x09, 0x28, 0x06, 0x5C, 0x53, 0xC5,
};

const std::uint8_t LAME_MPEG1_MONO_CBR[] = {
    0xFF, 0xFB, 0x98, 0xC4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x6E, 0x66,
    0x6F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x18, 0xC0, 0x00, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x33,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0x99, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xE6, 0xE6, 0xE6,
    0xE6, 0xE6, 0xE6, 0xE6, 0xE6, 0xE6, 0xE6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x38, 0x4C, 0x41, 0x4D,
    0x45, 0x34, 0x2E, 0x30, 0x20, 0x00, 0x01, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x2E, 0x15, 0x00, 0x00, 0x14, 0x80, 0x24, 0x03, 0xB0, 0x22, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x18, 0xC0, 0x9A, 0x66, 0xF0, 0x95,
};

const std::uint8_t LAME_MPEG2_MONO_CBR[] = {
    0xFF, 0xF3, 0x80, 0xC4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x6E, 0x66, 0x6F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x11, 0x23, 0x00, 0x0C, 0x0C, 0x0C, 0x0C, 0x19, 0x19, 0x19, 0x19, 0x19, 0x26, 0x26, 0x26, 0x26, 0x26, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x40, 0x40, 0x40, 0x40, 0x40, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x59, 0x59, 0x59, 0x59, 0x59, 0x66, 0x66, 0x66, 0x66, 0x66, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x80, 0x80, 0x80, 0x80, 0x80, 0x8C, 0x8C, 0x8C, 0x8C, 0x8C, 0x99, 0x99, 0x99, 0x99, 0x99, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xB3, 0xB3,
    0xB3, 0xB3, 0xB3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xD9, 0xD9, 0xD9, 0xD9, 0xD9, 0xE6, 0xE6, 0xE6, 0xE6, 0xE6, 0xF3,
    0xF3, 0xF3, 0xF3, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x38, 0x4C, 0x41, 0x4D, 0x45, 0x34, 0x2E, 0x30, 0x20, 0x00, 0x01, 0x6E,
    0x00, 0x00, 0x00, 0x00, 0x2E, 0x1E, 0x00, 0x00, 0x14, 0x40, 0x24, 0x03, 0xB0, 0x22, 0x00, 0x00, 0x40, 0x00, 0x00, 0x11, 0x23, 0xF6, 0xE8, 0xE2,
    0xEB,
};

const std::uint8_t FGH_FRAME[] = {
    0xFF, 0xFB, 0x90, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x04, 0x51, 0x00, 0x06, 0xBA, 0xA8, 0x00, 0x00, 0xA0,
};

const std::uint8_t VBRI_FRAME[] = {
    0xFF, 0xFB, 0x90, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x42, 0x52, 0x49, 0x00, 0x01, 0x02, 0x40, 0x00, 0x4B, 0x00, 0x00,
    0x10, 0x4A, 0x00, 0x00, 0x00, 0x0A,
};

const std::uint8_t FGH_BAD_CRC_FRAME[] = {
    0xFF, 0xFB, 0x90, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x04, 0x51, 0x00, 0x06, 0xBA, 0xA8, 0x00, 0x00, 0xA1,
};

std::vector<std::uint8_t> Mp3Frame(const std::uint8_t* prefix, std::size_t prefixSize, std::size_t frameSize) {
    std::vector<std::uint8_t> frame(frameSize, 0);
    std::memcpy(frame.data(), prefix, prefixSize);
    return frame;
}

bool OflParsesTo(const std::vector<std::uint8_t>& stream, std::uint32_t streamSize, std::uint32_t frames, std::uint32_t delay, std::uint32_t total, std::uint32_t type) {
    AjmDecMp3ParseFrame frame;
    std::memset(&frame, 0xFF, sizeof(frame));
    if (sceAjmDecMp3ParseFrame(stream.data(), streamSize, 1, &frame) != 0) return false;
    return frame.num_frames == frames && frame.encoder_delay == delay && frame.total_samples == total && frame.ofl_type == type;
}

bool OflThrows(const std::vector<std::uint8_t>& stream) {
    AjmDecMp3ParseFrame frame{};
    try {
        sceAjmDecMp3ParseFrame(stream.data(), static_cast<std::uint32_t>(stream.size()), 1, &frame);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestMp3ParseOfl() {
    const auto stereo = Mp3Frame(LAME_MPEG1_STEREO_VBR, sizeof(LAME_MPEG1_STEREO_VBR), 417);
    Require(OflParsesTo(stereo, 417, 10, 1152 + 576 + 529, 10000, 1));
    Require(ParsesTo(stereo.data(), 417, 417, 2, 1152, 128000, 44100));
    Require(OflParsesTo(stereo, 48, 10, 0, 0, 0));
    Require(OflParsesTo(stereo, 46, 0, 0, 0, 0));
    const auto mono = Mp3Frame(LAME_MPEG1_MONO_CBR, sizeof(LAME_MPEG1_MONO_CBR), 576);
    Require(OflParsesTo(mono, 576, 10, 1152 + 576 + 529, 10000, 1));
    const auto mpeg2 = Mp3Frame(LAME_MPEG2_MONO_CBR, sizeof(LAME_MPEG2_MONO_CBR), 208);
    Require(OflParsesTo(mpeg2, 208, 20, 576 + 576 + 529, 10000, 1));

    const auto fgh = Mp3Frame(FGH_FRAME, sizeof(FGH_FRAME), 417);
    Require(OflParsesTo(fgh, 417, 0, 1105, 441000, 3));
    Require(OflParsesTo(Mp3Frame(FGH_BAD_CRC_FRAME, sizeof(FGH_BAD_CRC_FRAME), 417), 417, 0, 0, 0, 0));
    auto vbri = Mp3Frame(VBRI_FRAME, sizeof(VBRI_FRAME), 417);
    Require(OflParsesTo(vbri, 417, 0, 576, 0, 2));
    vbri.insert(vbri.end(), fgh.begin(), fgh.end());
    Require(OflParsesTo(vbri, 834, 0, 576 + 1105, 441000, 4));
    Require(OflParsesTo(vbri, 480, 0, 576, 0, 2));

    auto protectedFrame = stereo;
    protectedFrame[1] = 0xFA;
    Require(OflThrows(protectedFrame));
    auto layer2 = stereo;
    layer2[1] = 0xFD;
    Require(OflThrows(layer2));
    auto monoVbri = Mp3Frame(VBRI_FRAME, sizeof(VBRI_FRAME), 417);
    monoVbri[3] = 0xC4;
    Require(OflThrows(monoVbri));
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

struct FormatSideband {
    std::int32_t result;
    std::int32_t internalResult;
    std::uint32_t numChannels;
    std::uint32_t channelMask;
    std::uint32_t sampleRate;
    std::uint32_t sampleEncoding;
    std::uint32_t bitrate;
    std::uint32_t reserved;
};

void TestGetInfo(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 1, 0, &instance) == 0);
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    FormatSideband early{-1, -1, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobGetInfo(&info, instance, &early) == 0);
    Submit(context, info);
    Require(early.result == 1 && early.numChannels == 0xaaaaaaaau && early.sampleRate == 0xaaaaaaaau);

    const std::uint8_t config[8] = {0xFE, 0x72, 0x1F, 0xF0};
    std::int32_t initResult[2] = {-1, -1};
    FormatSideband at9{-1, -1, 0, 0, 0, 0xaaaaaaaau, 0, 0xaaaaaaaau};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobInitialize(&info, instance, config, sizeof(config), initResult) == 0);
    Require(sceAjmBatchJobGetInfo(&info, instance, &at9) == 0);
    Submit(context, info);
    Require(initResult[0] == 0);
    Require(at9.result == 0 && at9.internalResult == 0 && at9.numChannels == 2 && at9.channelMask == 0x3 && at9.sampleRate == 48000 && at9.sampleEncoding == 0 && at9.reserved == 0);
    Require(sceAjmInstanceDestroy(context, instance) == 0);

    Require(sceAjmInstanceCreate(context, 0, 0, &instance) == 0);
    std::vector<std::int16_t> pcm(1152);
    DecodeSideband decoded{};
    FormatSideband mp3{-1, -1, 0, 0, 0, 0xaaaaaaaau, 0, 0xaaaaaaaau};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobDecodeSingle(&info, instance, MP3_MONO, sizeof(MP3_MONO), pcm.data(), pcm.size() * sizeof(std::int16_t), &decoded) == 0);
    Require(sceAjmBatchJobGetInfo(&info, instance, &mp3) == 0);
    Submit(context, info);
    Require(decoded.result == 0 && decoded.inputConsumed == 96);
    Require(mp3.result == 0 && mp3.internalResult == 0 && mp3.numChannels == 1 && mp3.channelMask == 0x4 && mp3.sampleRate == 48000 && mp3.sampleEncoding == 0 && mp3.bitrate == 32000 && mp3.reserved == 0);
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

void TestBatchCancel(std::uint32_t context) {
    constexpr int invalidContext = static_cast<int>(0x80930002);
    constexpr int invalidBatch = static_cast<int>(0x80930004);
    std::vector<std::uint8_t> batch(64);
    AjmBatchInfo info{};
    AjmBatchError error{};
    std::uint32_t id = 0;
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchStart(context, &info, 0, &error, &id) == 0);
    Require(sceAjmBatchCancel(context, id) == 0);
    Require(sceAjmBatchCancel(context, id) == 0);
    Require(sceAjmBatchCancel(0, id) == invalidContext);
    Require(sceAjmBatchCancel(context + 1, id) == invalidContext);
    Require(sceAjmBatchWait(context, id, 0, &error) == 0);
    Require(sceAjmBatchCancel(context, id) == invalidBatch);
    Require(sceAjmBatchCancel(context, 0) == invalidBatch);
    Require(sceAjmBatchCancel(context, id + 1) == invalidBatch);
}

constexpr std::uint64_t CONTROL_RESET = 1ull << 13;
constexpr std::uint64_t CONTROL_INITIALIZE = 1ull << 14;
constexpr std::uint64_t SIDEBAND_GAPLESS_DECODE = 1ull << 45;
constexpr std::uint64_t CONTROL_START = CONTROL_RESET | CONTROL_INITIALIZE | SIDEBAND_GAPLESS_DECODE;

struct At9Control {
    GaplessDecode gapless;
    std::uint8_t config[4];
    std::uint32_t reserved;
};

struct OpusControl {
    GaplessDecode gapless;
    std::uint32_t channels;
    std::uint32_t sampleRate;
    std::uint32_t third;
};

void RunControl(std::uint32_t context, std::uint32_t instance, std::uint64_t flags, const void* input, std::size_t inputSize, std::int32_t* result) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobControl(&info, instance, flags, input, inputSize, result, 2 * sizeof(std::int32_t)) == 0);
    Submit(context, info);
}

bool ControlThrows(std::uint32_t instance, std::uint64_t flags, const void* input, std::size_t inputSize, std::size_t outputSize) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int32_t result[4] = {};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    try {
        static_cast<void>(sceAjmBatchJobControl(&info, instance, flags, input, inputSize, result, outputSize));
    } catch (const std::runtime_error&) {
        return info.offset == 0;
    }
    return false;
}

bool ControlRefused(std::uint32_t context, std::uint32_t instance, std::uint64_t flags, const void* input, std::size_t inputSize) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int32_t result[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobControl(&info, instance, flags, input, inputSize, result, sizeof(result)) == 0);
    return Refused(context, info);
}

struct SyntheticAt9Channel {
    std::uint8_t header;
    std::uint8_t fill;
    std::size_t blockBytes;
};

std::vector<std::uint8_t> SyntheticAt9Block(const SyntheticAt9Channel& channel, std::uint32_t frame) {
    std::vector<std::uint8_t> block(channel.blockBytes, channel.fill);
    block[0] = frame == 0 ? channel.header : static_cast<std::uint8_t>(channel.header | 0x80u);
    return block;
}

std::vector<std::int16_t> DecodeAt9Superframe(std::uint32_t context, const std::uint8_t (&config)[4], const std::vector<std::uint8_t>& superframe, std::size_t channels) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 1, 0, &instance) == 0);
    constexpr std::uint64_t runMultipleFrames = 1ull << 12;
    constexpr std::uint64_t sidebandStream = 1ull << 47;
    std::vector<std::uint8_t> batch(4096);
    std::vector<std::int16_t> pcm(1024 * channels);
    std::int32_t initResult[2] = {-1, -1};
    struct {
        DecodeSideband stream;
        std::uint32_t frames;
        std::uint32_t reserved;
    } decoded{};
    AjmBatchInfo info{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobInitialize(&info, instance, config, sizeof(config), initResult) == 0);
    Require(sceAjmBatchJobRun(&info, instance, sidebandStream | runMultipleFrames, superframe.data(), superframe.size(), pcm.data(), pcm.size() * sizeof(std::int16_t), &decoded, sizeof(decoded)) == 0);
    Submit(context, info);
    Require(initResult[0] == 0);
    Require(decoded.stream.result == 0 && static_cast<std::size_t>(decoded.stream.inputConsumed) == superframe.size());
    Require(static_cast<std::size_t>(decoded.stream.outputWritten) == pcm.size() * sizeof(std::int16_t) && decoded.stream.totalDecodedSamples == 1024 && decoded.frames == 4);
    Require(sceAjmInstanceDestroy(context, instance) == 0);
    return pcm;
}

void TestMultichannelAt9(std::uint32_t context) {
    constexpr int invalidParameter = static_cast<int>(0x80930005);
    const std::uint8_t thirdOrder[4] = {0x30, 0x73, 0xC1, 0x7E};
    AjmDecAt9ConfigDataInfo parsed{};
    Require(sceAjmDecAt9ParseConfigData(thirdOrder, &parsed) == 0);
    Require(parsed.channels == 16 && parsed.sample_rate == 48000 && parsed.frame_samples_per_channel == 256);
    Require(parsed.superframe_samples_per_channel == 1024 && parsed.superframe_size == 6144);
    const std::uint8_t validationBitSet[4] = {0x30, 0x73, 0xE1, 0x7E};
    Require(sceAjmDecAt9ParseConfigData(validationBitSet, &parsed) == invalidParameter);

    const SyntheticAt9Channel channels[2] = {{0x00, 0xA5, 27}, {0x01, 0x0E, 22}};
    const std::uint8_t mono[4] = {0xFE, 0x70, 0x0B, 0xF0};
    std::vector<std::int16_t> reference[2];
    for (std::size_t channel = 0; channel < 2; ++channel) {
        std::vector<std::uint8_t> superframe;
        for (std::uint32_t frame = 0; frame < 4; ++frame) {
            const auto block = SyntheticAt9Block(channels[channel], frame);
            superframe.insert(superframe.end(), block.begin(), block.end());
        }
        superframe.resize(384, 0x01);
        reference[channel] = DecodeAt9Superframe(context, mono, superframe, 1);
    }

    const std::uint8_t stereoAmbisonic[4] = {0x30, 0x70, 0x41, 0x7E};
    std::vector<std::uint8_t> superframe;
    for (std::uint32_t frame = 0; frame < 4; ++frame) {
        for (const auto& channel : channels) {
            auto block = SyntheticAt9Block(channel, frame);
            if (frame == 3) block.resize(384 - 3 * channel.blockBytes, 0x01);
            superframe.insert(superframe.end(), block.begin(), block.end());
        }
    }
    Require(superframe.size() == 768);
    const auto decoded = DecodeAt9Superframe(context, stereoAmbisonic, superframe, 2);
    bool audible = false;
    for (std::size_t sample = 0; sample < 1024; ++sample) {
        Require(decoded[sample * 2] == reference[0][sample] && decoded[sample * 2 + 1] == reference[1][sample]);
        audible = audible || reference[0][sample] != 0 || reference[1][sample] != 0;
    }
    Require(audible);
}

void TestControlAt9(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 1, 0, &instance) == 0);
    std::vector<std::uint8_t> batch(4096);
    const At9Control start{{2000, 100, 0}, {0xFE, 0x72, 0x1F, 0xF0}, 0xDEADBEEFu};
    std::int32_t result[4] = {-1, -1, -1, -1};
    At9CodecInfoSideband codec{-1, -1, 0, 0, 0, 0};
    GaplessSideband gapless{-1, -1, 0, 0, 0xffff};
    AjmBatchInfo info{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobControl(&info, instance, CONTROL_START, &start, sizeof(start), result, 2 * sizeof(std::int32_t)) == 0);
    Require(sceAjmBatchJobGetCodecInfo(&info, instance, &codec, sizeof(codec)) == 0);
    Require(sceAjmBatchJobGetGaplessDecode(&info, instance, &gapless) == 0);
    Submit(context, info);
    Require(result[0] == 0 && result[1] == 0 && result[2] == -1 && result[3] == -1);
    Require(codec.result == 0 && codec.superframeSize == 1024 && codec.framesInSuperframe == 4 && codec.nextFrameSize == 1024 && codec.frameSamples == 256);
    Require(gapless.result == 0 && gapless.totalSamples == 2000 && gapless.skipSamples == 100 && gapless.skippedSamples == 0);

    At9Control broken = start;
    broken.config[0] = 0xFD;
    At9CodecInfoSideband uninitialized{-1, -1, 0, 0, 0, 0};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobControl(&info, instance, CONTROL_START, &broken, sizeof(broken), result, 2 * sizeof(std::int32_t)) == 0);
    Require(sceAjmBatchJobGetCodecInfo(&info, instance, &uninitialized, sizeof(uninitialized)) == 0);
    Submit(context, info);
    Require(result[0] == 4 && uninitialized.result == 1);

    result[0] = -1;
    RunControl(context, instance, CONTROL_INITIALIZE, start.config, sizeof(start) - sizeof(start.gapless), result);
    Require(result[0] == 0);

    Require(ControlRefused(context, instance, CONTROL_INITIALIZE, &start, sizeof(start)));
    Require(ControlRefused(context, instance, CONTROL_RESET | SIDEBAND_GAPLESS_DECODE, &start, sizeof(start)));
    Require(ControlThrows(instance, CONTROL_INITIALIZE | SIDEBAND_GAPLESS_DECODE, &start, sizeof(start), 8));
    Require(ControlThrows(instance, CONTROL_START | (1ull << 15), &start, sizeof(start), 8));
    Require(ControlThrows(instance, CONTROL_START | (1ull << 46), &start, sizeof(start), 8));
    Require(ControlThrows(instance, CONTROL_START | (1ull << 12), &start, sizeof(start), 8));
    Require(ControlThrows(instance, 0, nullptr, 0, 8));
    Require(ControlThrows(instance, CONTROL_START, &start, sizeof(start), 16));
    Require(ControlThrows(instance, CONTROL_START, nullptr, sizeof(start), 8));
    Require(sceAjmInstanceDestroy(context, instance) == 0);

    result[0] = -1;
    RunControl(context, instance, CONTROL_START, &start, sizeof(start), result);
    Require(result[0] == 4);

    const std::uint32_t aac[4] = {0, 0, 1, 0};
    Require(sceAjmInstanceCreate(context, 2, 0, &instance) == 0);
    Require(ControlRefused(context, instance, CONTROL_START, aac, sizeof(aac)));
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void TestControlMp3(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 0, 0, &instance) == 0);
    std::vector<std::int16_t> pcm(1152);
    const std::size_t pcmBytes = pcm.size() * sizeof(std::int16_t);
    DecodeSideband decoded{};
    RunDecode(context, instance, MP3_MONO, sizeof(MP3_MONO), pcm.data(), pcmBytes, decoded);
    RunDecode(context, instance, MP3_MONO + 96, sizeof(MP3_MONO) - 96, pcm.data(), pcmBytes, decoded);
    Require(decoded.result == 0 && decoded.totalDecodedSamples == 2 * 1152);

    const GaplessDecode skip{0, 100, 0};
    std::int32_t result[2] = {-1, -1};
    RunControl(context, instance, CONTROL_START, &skip, sizeof(skip), result);
    RunDecode(context, instance, MP3_MONO, sizeof(MP3_MONO), pcm.data(), pcmBytes, decoded);
    Require(result[0] == 0 && decoded.result == 0 && decoded.inputConsumed == 96 && decoded.outputWritten == (1152 - 100) * 2 && decoded.totalDecodedSamples == 1152 - 100);

    result[0] = -1;
    RunControl(context, instance, CONTROL_RESET, nullptr, 0, result);
    RunDecode(context, instance, MP3_MONO, sizeof(MP3_MONO), pcm.data(), pcmBytes, decoded);
    Require(result[0] == 0 && decoded.result == 0 && decoded.outputWritten == (1152 - 100) * 2 && decoded.totalDecodedSamples == 1152 - 100);

    const GaplessDecode none{0, 0, 0};
    Require(ControlThrows(instance, SIDEBAND_GAPLESS_DECODE, &none, sizeof(none), 8));

    const GaplessDecode longer{5000, 200, 0};
    result[0] = -1;
    RunControl(context, instance, CONTROL_RESET | SIDEBAND_GAPLESS_DECODE, &longer, sizeof(longer), result);
    RunDecode(context, instance, MP3_MONO, sizeof(MP3_MONO), pcm.data(), pcmBytes, decoded);
    Require(result[0] == 0 && decoded.result == 0 && decoded.outputWritten == (1152 - 200) * 2 && decoded.totalDecodedSamples == 1152 - 200);

    const std::uint32_t parameters[2] = {1, 0};
    Require(ControlRefused(context, instance, CONTROL_INITIALIZE, parameters, sizeof(parameters)));
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void TestControlOpus(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 24, 0, &instance) == 0);
    const OpusControl start{{0, 0, 0}, 2, 48000, 0};
    std::int32_t result[2] = {-1, -1};
    RunControl(context, instance, CONTROL_START, &start, sizeof(start), result);
    Require(result[0] == 0);
    std::vector<std::int16_t> pcm(960 * 2);
    DecodeSideband decoded{};
    RunDecode(context, instance, OPUS_STEREO, sizeof(OPUS_STEREO), pcm.data(), pcm.size() * sizeof(std::int16_t), decoded);
    const std::size_t bytes = OPUS_STEREO[0] | (std::size_t{OPUS_STEREO[1]} << 8u);
    Require(decoded.result == 0 && static_cast<std::size_t>(decoded.inputConsumed) == 2 + bytes && decoded.outputWritten == 960 * 2 * 2);
    Require(ControlRefused(context, instance, CONTROL_START, &start, sizeof(start) - sizeof(start.third)));
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

struct ResampleInfo {
    std::int32_t result;
    std::int32_t internalResult;
    float ratio;
    std::int32_t samples;
    std::uint32_t reserved[8];
};
static_assert(sizeof(ResampleInfo) == 48);

std::uint32_t CreateOpus(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 24, 0, &instance) == 0);
    const std::uint32_t parameters[3] = {2, 48000, 0};
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int64_t result[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobInitialize(&info, instance, parameters, sizeof(parameters), result) == 0);
    Submit(context, info);
    Require(result[0] == 0);
    return instance;
}

ResampleInfo GetResampleInfo(std::uint32_t context, std::uint32_t instance) {
    ResampleInfo resample;
    std::memset(&resample, 0xAA, sizeof(resample));
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobGetResampleInfo(&info, instance, &resample) == 0);
    Submit(context, info);
    return resample;
}

struct ResampledStream {
    std::vector<std::int16_t> pcm;
    std::size_t jobs = 0;
    std::size_t shortJobs = 0;
    ResampleInfo afterFirstJob{};
    ResampleInfo atEnd{};
};

ResampledStream Stream(std::uint32_t context, std::uint32_t instance, const std::uint8_t* stream, std::size_t size, std::size_t channels, std::size_t frames, float ratio) {
    ResampledStream out;
    std::vector<std::int16_t> pcm(frames * channels);
    std::size_t offset = 0;
    for (int guard = 0; guard < 4096; ++guard) {
        std::vector<std::uint8_t> batch(4096);
        AjmBatchInfo info{};
        std::int64_t setResult[2] = {-1, -1};
        DecodeSideband sideband{};
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        if (ratio > 0) Require(sceAjmBatchJobSetResampleParameters(&info, instance, ratio, 1, setResult) == 0);
        Require(sceAjmBatchJobDecode(&info, instance, stream + offset, size - offset, pcm.data(), pcm.size() * sizeof(std::int16_t), &sideband) == 0);
        Submit(context, info);
        if (ratio > 0) Require(setResult[0] == 0);
        Require(sideband.result == 0 || (offset == size && sideband.outputWritten == 0));
        offset += static_cast<std::size_t>(sideband.inputConsumed);
        if (offset < size && static_cast<std::size_t>(sideband.outputWritten) < pcm.size() * sizeof(std::int16_t)) ++out.shortJobs;
        out.pcm.insert(out.pcm.end(), pcm.begin(), pcm.begin() + sideband.outputWritten / static_cast<std::int32_t>(sizeof(std::int16_t)));
        if (++out.jobs == 1) out.afterFirstJob = GetResampleInfo(context, instance);
        if (offset == size && sideband.outputWritten == 0) break;
    }
    Require(offset == size);
    out.atEnd = GetResampleInfo(context, instance);
    return out;
}

void RequireDecimated(const std::vector<std::int16_t>& reference, const std::vector<std::int16_t>& resampled, std::size_t channels, std::size_t step) {
    const std::size_t frames = reference.size() / channels;
    Require(resampled.size() / channels == (frames - 3) / step + 1);
    for (std::size_t frame = 0; frame < resampled.size() / channels; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) Require(resampled[frame * channels + channel] == reference[frame * step * channels + channel]);
    }
}

void RequireInterpolated(const std::vector<std::int16_t>& reference, const std::vector<std::int16_t>& resampled, std::size_t channels) {
    const std::size_t frames = reference.size() / channels;
    Require(resampled.size() / channels == 2 * frames - 4);
    const auto at = [&](std::ptrdiff_t frame, std::size_t channel) { return static_cast<double>(reference[static_cast<std::size_t>(std::max<std::ptrdiff_t>(frame, 0)) * channels + channel]); };
    for (std::size_t frame = 0; frame < resampled.size() / channels; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const auto source = static_cast<std::ptrdiff_t>(frame / 2);
            const double midpoint = (-at(source - 1, channel) + 9.0 * at(source, channel) + 9.0 * at(source + 1, channel) - at(source + 2, channel)) / 16.0;
            if (frame % 2 == 0) Require(resampled[frame * channels + channel] == reference[frame / 2 * channels + channel]);
            else Require(std::fabs(resampled[frame * channels + channel] - midpoint) <= 1.5);
        }
    }
}

void TestResampleOpus(std::uint32_t context) {
    const std::uint32_t plain = CreateOpus(context);
    const auto reference = Stream(context, plain, OPUS_STEREO, sizeof(OPUS_STEREO), 2, 960, 0);
    Require(reference.pcm.size() / 2 >= 960 * 4);

    const std::uint32_t unity = CreateOpus(context);
    const auto passthrough = Stream(context, unity, OPUS_STEREO, sizeof(OPUS_STEREO), 2, 512, 1.0f);
    Require(passthrough.pcm == reference.pcm);
    Require(passthrough.afterFirstJob.result == 0 && passthrough.afterFirstJob.internalResult == 0 && passthrough.afterFirstJob.ratio == 1.0f && passthrough.afterFirstJob.samples == 960 - 512);
    for (const auto word : passthrough.afterFirstJob.reserved) Require(word == 0);
    Require(passthrough.atEnd.samples == 0);

    const std::uint32_t faster = CreateOpus(context);
    const auto decimated = Stream(context, faster, OPUS_STEREO, sizeof(OPUS_STEREO), 2, 512, 2.0f);
    RequireDecimated(reference.pcm, decimated.pcm, 2, 2);
    Require(decimated.afterFirstJob.ratio == 2.0f && decimated.afterFirstJob.samples == 2);
    Require(decimated.atEnd.result == 0 && decimated.atEnd.samples >= 0 && decimated.atEnd.samples <= 2);

    const std::uint32_t slower = CreateOpus(context);
    const auto interpolated = Stream(context, slower, OPUS_STEREO, sizeof(OPUS_STEREO), 2, 512, 0.5f);
    RequireInterpolated(reference.pcm, interpolated.pcm, 2);
    Require(interpolated.shortJobs == 0 && passthrough.shortJobs == 0);
    Require(interpolated.afterFirstJob.ratio == 0.5f && interpolated.afterFirstJob.samples == 960 - 256);
    Require(interpolated.atEnd.samples >= 0 && interpolated.atEnd.samples <= 2);

    const std::uint32_t hades = CreateOpus(context);
    const auto pitched = Stream(context, hades, OPUS_STEREO, sizeof(OPUS_STEREO), 2, 512, 0.890899f);
    const double expected = static_cast<double>(reference.pcm.size() / 2 - 2) / 0.890899;
    Require(std::fabs(static_cast<double>(pitched.pcm.size() / 2) - expected) <= 2.0);

    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int64_t result[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    for (const float bad : {0.0f, -1.0f, std::nanf(""), INFINITY}) Require(sceAjmBatchJobSetResampleParameters(&info, slower, bad, 1, result) == static_cast<int>(0x80930005));
    Require(info.offset == 0);

    const std::size_t firstPacket = 2 + (OPUS_STEREO[0] | (std::size_t{OPUS_STEREO[1]} << 8u));
    const auto clear = [&] {
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        Require(sceAjmBatchJobClearContext(&info, slower, result) == 0);
        Submit(context, info);
        Require(result[0] == 0 && GetResampleInfo(context, slower).samples == 0);
    };
    clear();
    const auto before = Stream(context, slower, OPUS_STEREO, firstPacket, 2, 512, 0.5f);
    Require(before.pcm.size() / 2 == 2 * 960 - 4);
    clear();
    const auto after = Stream(context, slower, OPUS_STEREO, firstPacket, 2, 512, 0.5f);
    Require(after.pcm == before.pcm);

    const std::uint32_t switched = CreateOpus(context);
    const auto head = Stream(context, switched, OPUS_STEREO, 2 + (OPUS_STEREO[0] | (std::size_t{OPUS_STEREO[1]} << 8u)), 2, 512, 1.0f);
    Require(head.jobs >= 1 && head.pcm.size() == 960 * 2);
    const std::uint32_t resumed = CreateOpus(context);
    std::vector<std::int16_t> pcm(512 * 2);
    DecodeSideband first{};
    RunDecode(context, resumed, OPUS_STEREO, sizeof(OPUS_STEREO), pcm.data(), pcm.size() * sizeof(std::int16_t), first);
    Require(first.result == 0 && first.outputWritten == 512 * 2 * 2 && GetResampleInfo(context, resumed).samples == 960 - 512);
    std::vector<std::int16_t> rest(pcm.begin(), pcm.end());
    const auto tail = Stream(context, resumed, OPUS_STEREO + first.inputConsumed, sizeof(OPUS_STEREO) - static_cast<std::size_t>(first.inputConsumed), 2, 512, 2.0f);
    const std::vector<std::int16_t> later(reference.pcm.begin() + 512 * 2, reference.pcm.end());
    RequireDecimated(later, tail.pcm, 2, 2);
    Require(std::equal(rest.begin(), rest.end(), reference.pcm.begin()));

    const auto unknown = GetResampleInfo(context, 0x3FFF);
    Require(unknown.result == 4);

    for (const std::uint32_t instance : {plain, unity, faster, slower, hades, switched, resumed}) Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void TestResampleMp3(std::uint32_t context) {
    std::uint32_t plain = 0;
    std::uint32_t faster = 0;
    std::uint32_t slower = 0;
    Require(sceAjmInstanceCreate(context, 0, 0, &plain) == 0 && sceAjmInstanceCreate(context, 0, 0, &faster) == 0 && sceAjmInstanceCreate(context, 0, 0, &slower) == 0);
    const auto reference = Stream(context, plain, MP3_MONO, sizeof(MP3_MONO), 1, 1152, 0);
    Require(reference.pcm.size() == 1152 * 6);
    RequireDecimated(reference.pcm, Stream(context, faster, MP3_MONO, sizeof(MP3_MONO), 1, 512, 2.0f).pcm, 1, 2);
    const auto interpolated = Stream(context, slower, MP3_MONO, sizeof(MP3_MONO), 1, 512, 0.5f);
    RequireInterpolated(reference.pcm, interpolated.pcm, 1);
    Require(interpolated.shortJobs == 0);
    for (const std::uint32_t instance : {plain, faster, slower}) Require(sceAjmInstanceDestroy(context, instance) == 0);
}

const std::uint8_t AT9_MONO_SILENT_SUPERFRAME[] = {
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void TestResampleAt9(std::uint32_t context) {
    const auto create = [&] {
        std::uint32_t instance = 0;
        Require(sceAjmInstanceCreate(context, 1, 0, &instance) == 0);
        const std::uint8_t config[8] = {0xFE, 0x70, 0x1F, 0xF0};
        std::vector<std::uint8_t> batch(4096);
        AjmBatchInfo info{};
        std::int32_t result[2] = {-1, -1};
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        Require(sceAjmBatchJobInitialize(&info, instance, config, sizeof(config), result) == 0);
        Submit(context, info);
        Require(result[0] == 0);
        return instance;
    };
    std::vector<std::uint8_t> stream(1024 * 3, 0);
    for (std::size_t superframe = 0; superframe < 3; ++superframe) std::memcpy(stream.data() + superframe * 1024, AT9_MONO_SILENT_SUPERFRAME, sizeof(AT9_MONO_SILENT_SUPERFRAME));
    const std::uint32_t plain = create();
    const std::uint32_t faster = create();
    const std::uint32_t slower = create();
    const auto reference = Stream(context, plain, stream.data(), stream.size(), 1, 256, 0);
    Require(reference.pcm.size() == 3 * 4 * 256);
    const auto decimated = Stream(context, faster, stream.data(), stream.size(), 1, 512, 2.0f);
    RequireDecimated(reference.pcm, decimated.pcm, 1, 2);
    Require(decimated.atEnd.samples == 2);
    const auto interpolated = Stream(context, slower, stream.data(), stream.size(), 1, 256, 0.5f);
    RequireInterpolated(reference.pcm, interpolated.pcm, 1);
    Require(interpolated.shortJobs == 0 && interpolated.afterFirstJob.samples == 256 - 256 / 2 && interpolated.atEnd.samples == 2);
    for (const std::uint32_t instance : {plain, faster, slower}) Require(sceAjmInstanceDestroy(context, instance) == 0);
}

struct At9Run {
    DecodeSideband stream;
    std::uint32_t frames;
    std::uint32_t reserved;
};

At9Run RunAt9Job(std::uint32_t context, std::uint32_t instance, std::uint64_t flags, const std::uint8_t* input, std::size_t inputSize) {
    constexpr std::uint64_t runMultipleFrames = 1ull << 12;
    constexpr std::uint64_t sidebandStream = 1ull << 47;
    std::vector<std::uint8_t> batch(4096);
    std::vector<std::int16_t> pcm(1024);
    At9Run decoded{{-1, -1, 0, 0, 0}, 0, 0};
    AjmBatchInfo info{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    const std::size_t sidebandSize = (flags & runMultipleFrames) != 0 ? sizeof(decoded) : sizeof(decoded.stream);
    Require(sceAjmBatchJobRun(&info, instance, sidebandStream | flags, input, inputSize, pcm.data(), pcm.size() * sizeof(std::int16_t), &decoded, sidebandSize) == 0);
    Submit(context, info);
    Require(decoded.stream.result == 0);
    return decoded;
}

void TestAt9GaplessSegments(std::uint32_t context) {
    constexpr std::uint64_t runMultipleFrames = 1ull << 12;
    std::vector<std::uint8_t> stream(1024, 0);
    std::memcpy(stream.data(), AT9_MONO_SILENT_SUPERFRAME, sizeof(AT9_MONO_SILENT_SUPERFRAME));
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 1, 0, &instance) == 0);
    const At9Control start{{512, 0, 0}, {0xFE, 0x70, 0x1F, 0xF0}, 0};
    std::int32_t result[2] = {-1, -1};
    RunControl(context, instance, CONTROL_START, &start, sizeof(start), result);
    Require(result[0] == 0);
    const auto first = RunAt9Job(context, instance, runMultipleFrames, stream.data(), stream.size());
    Require(first.stream.outputWritten == 512 * 2 && first.frames == 2);
    Require(first.stream.inputConsumed > 0 && static_cast<std::size_t>(first.stream.inputConsumed) < stream.size());
    const auto consumed = static_cast<std::size_t>(first.stream.inputConsumed);
    const auto second = RunAt9Job(context, instance, runMultipleFrames, stream.data() + consumed, stream.size() - consumed);
    Require(second.stream.outputWritten == 512 * 2 && second.frames == 2);
    Require(consumed + static_cast<std::size_t>(second.stream.inputConsumed) == stream.size());
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void TestAt9RunDecodesWholeInput(std::uint32_t context) {
    std::vector<std::uint8_t> stream(1024, 0);
    std::memcpy(stream.data(), AT9_MONO_SILENT_SUPERFRAME, sizeof(AT9_MONO_SILENT_SUPERFRAME));
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 1, 0, &instance) == 0);
    const At9Control start{{0, 0, 0}, {0xFE, 0x70, 0x1F, 0xF0}, 0};
    std::int32_t result[2] = {-1, -1};
    RunControl(context, instance, CONTROL_START, &start, sizeof(start), result);
    Require(result[0] == 0);
    const auto run = RunAt9Job(context, instance, 0, stream.data(), stream.size());
    Require(static_cast<std::size_t>(run.stream.inputConsumed) == stream.size() && run.stream.outputWritten == 1024 * 2);
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

}

void TestStrError() {
    const char* invalidContext = sceAjmStrError(static_cast<int>(0x80930002));
    const char* invalidInstance = sceAjmStrError(static_cast<int>(0x80930003));
    const char* invalidBatch = sceAjmStrError(static_cast<int>(0x80930004));
    const char* invalidParameter = sceAjmStrError(static_cast<int>(0x80930005));
    const char* outOfResources = sceAjmStrError(static_cast<int>(0x80930007));
    for (const char* message : {invalidContext, invalidInstance, invalidBatch, invalidParameter, outOfResources}) Require(message != nullptr && message[0] != '\0');
    Require(std::strcmp(invalidContext, invalidInstance) != 0 && std::strcmp(invalidContext, invalidBatch) != 0 && std::strcmp(invalidContext, invalidParameter) != 0 && std::strcmp(invalidContext, outOfResources) != 0);
    Require(std::strcmp(invalidInstance, invalidBatch) != 0 && std::strcmp(invalidInstance, invalidParameter) != 0 && std::strcmp(invalidInstance, outOfResources) != 0);
    Require(std::strcmp(invalidBatch, invalidParameter) != 0 && std::strcmp(invalidBatch, outOfResources) != 0);
    Require(std::strcmp(invalidParameter, outOfResources) != 0);
    const char* unknown = sceAjmStrError(static_cast<int>(0x80930006));
    const char* zero = sceAjmStrError(0);
    Require(unknown != nullptr && unknown[0] != '\0' && zero != nullptr && zero[0] != '\0');
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
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo batchInfo{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &batchInfo) == 0);
    bool nullBuffersThrow = false;
    try {
        sceAjmBatchJobRunSplit(&batchInfo, 0, 0, nullptr, 1, nullptr, 0, nullptr, 0);
    } catch (const std::runtime_error&) {
        nullBuffersThrow = true;
    }
    Require(nullBuffersThrow && batchInfo.offset == 0);
    TestMp3ParseFrame();
    TestMp3ParseOfl();
    TestMp3(context);
    TestOpus(context);
    TestDecodeSingle(context);
    TestGaplessDecode(context);
    TestCodecInfo(context);
    TestGetInfo(context);
    TestBatchWaitRelease(context);
    TestBatchCancel(context);
    TestControlAt9(context);
    TestMultichannelAt9(context);
    TestControlMp3(context);
    TestControlOpus(context);
    TestResampleOpus(context);
    TestResampleMp3(context);
    TestResampleAt9(context);
    TestAt9GaplessSegments(context);
    TestAt9RunDecodesWholeInput(context);
    TestStrError();
    Require(sceAjmFinalize(context) == 0);
}
