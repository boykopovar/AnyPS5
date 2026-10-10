#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int invalidContext = static_cast<int>(0x80930002);
constexpr int invalidBatch = static_cast<int>(0x80930004);
constexpr int invalidParameter = static_cast<int>(0x80930005);

constexpr std::uint32_t codecMp3 = 0;
constexpr std::uint32_t codecAt9 = 1;
constexpr std::uint32_t codecAac = 2;
constexpr std::uint32_t codecOpus = 24;

constexpr std::uint8_t mp3Mono[] = {
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

constexpr std::uint8_t opusStereo[] = {
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

constexpr std::uint8_t lameMpeg1StereoVbr[] = {
    0xFF, 0xFB, 0x90, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x69, 0x6E, 0x67, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0A,
    0x00, 0x00, 0x09, 0x28, 0x00, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x8D, 0x9F, 0x9F, 0x9F, 0x9F,
    0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0x9F, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xAC, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE, 0xBE,
    0xBE, 0xBE, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x0A, 0x4C, 0x41, 0x4D, 0x45, 0x34, 0x2E, 0x30, 0x20, 0x00, 0x04, 0x48, 0x00,
    0x00, 0x00, 0x00, 0x2E, 0x1A, 0x00, 0x00, 0x15, 0x20, 0x24, 0x03, 0xB0, 0x45, 0x00, 0x01, 0x9A, 0x00, 0x00, 0x09, 0x28, 0x06, 0x5C, 0x53, 0xC5,
};

constexpr std::uint8_t lameMpeg1MonoCbr[] = {
    0xFF, 0xFB, 0x98, 0xC4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x6E, 0x66,
    0x6F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x18, 0xC0, 0x00, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x19, 0x33,
    0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0x99, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xB3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xE6, 0xE6, 0xE6,
    0xE6, 0xE6, 0xE6, 0xE6, 0xE6, 0xE6, 0xE6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x38, 0x4C, 0x41, 0x4D,
    0x45, 0x34, 0x2E, 0x30, 0x20, 0x00, 0x01, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x2E, 0x15, 0x00, 0x00, 0x14, 0x80, 0x24, 0x03, 0xB0, 0x22, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x18, 0xC0, 0x9A, 0x66, 0xF0, 0x95,
};

constexpr std::uint8_t lameMpeg2MonoCbr[] = {
    0xFF, 0xF3, 0x80, 0xC4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x6E, 0x66, 0x6F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x11, 0x23, 0x00, 0x0C, 0x0C, 0x0C, 0x0C, 0x19, 0x19, 0x19, 0x19, 0x19, 0x26, 0x26, 0x26, 0x26, 0x26, 0x33, 0x33, 0x33, 0x33,
    0x33, 0x40, 0x40, 0x40, 0x40, 0x40, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x59, 0x59, 0x59, 0x59, 0x59, 0x66, 0x66, 0x66, 0x66, 0x66, 0x73, 0x73, 0x73,
    0x73, 0x73, 0x80, 0x80, 0x80, 0x80, 0x80, 0x8C, 0x8C, 0x8C, 0x8C, 0x8C, 0x99, 0x99, 0x99, 0x99, 0x99, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xB3, 0xB3,
    0xB3, 0xB3, 0xB3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xD9, 0xD9, 0xD9, 0xD9, 0xD9, 0xE6, 0xE6, 0xE6, 0xE6, 0xE6, 0xF3,
    0xF3, 0xF3, 0xF3, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x38, 0x4C, 0x41, 0x4D, 0x45, 0x34, 0x2E, 0x30, 0x20, 0x00, 0x01, 0x6E,
    0x00, 0x00, 0x00, 0x00, 0x2E, 0x1E, 0x00, 0x00, 0x14, 0x40, 0x24, 0x03, 0xB0, 0x22, 0x00, 0x00, 0x40, 0x00, 0x00, 0x11, 0x23, 0xF6, 0xE8, 0xE2,
    0xEB,
};

constexpr std::uint8_t fghFrame[] = {
    0xFF, 0xFB, 0x90, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x04, 0x51, 0x00, 0x06, 0xBA, 0xA8, 0x00, 0x00, 0xA0,
};

constexpr std::uint8_t vbriFrame[] = {
    0xFF, 0xFB, 0x90, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x42, 0x52, 0x49, 0x00, 0x01, 0x02, 0x40, 0x00, 0x4B, 0x00, 0x00,
    0x10, 0x4A, 0x00, 0x00, 0x00, 0x0A,
};

constexpr std::uint8_t fghBadCrcFrame[] = {
    0xFF, 0xFB, 0x90, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x04, 0x51, 0x00, 0x06, 0xBA, 0xA8, 0x00, 0x00, 0xA1,
};

constexpr std::uint8_t at9MonoSilentSuperframe[] = {
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

struct DecodeSideband {
    std::int32_t result;
    std::int32_t internalResult;
    std::int32_t inputConsumed;
    std::int32_t outputWritten;
    std::uint64_t totalDecodedSamples;
};

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

struct ResampleInfo {
    std::int32_t result;
    std::int32_t internalResult;
    float ratio;
    std::int32_t samples;
    std::uint32_t reserved[8];
};
static_assert(sizeof(ResampleInfo) == 48);
static_assert(sizeof(AjmDecMp3ParseFrame) == 40);

constexpr std::uint64_t runGetCodecInfo = 1ull << 11;
constexpr std::uint64_t runMultipleFrames = 1ull << 12;
constexpr std::uint64_t controlReset = 1ull << 13;
constexpr std::uint64_t controlInitialize = 1ull << 14;
constexpr std::uint64_t sidebandGaplessDecode = 1ull << 45;
constexpr std::uint64_t sidebandStream = 1ull << 47;
constexpr std::uint64_t controlStart = controlReset | controlInitialize | sidebandGaplessDecode;

constexpr std::uint8_t at9Stereo48k[8] = {0xFE, 0x72, 0x1F, 0xF0};
constexpr At9Control at9Start{{2000, 100, 0}, {0xFE, 0x72, 0x1F, 0xF0}, 0xDEADBEEFu};

class AjmContext {
public:
    AjmContext() {
        RequireEqual(sceAjmInitialize(0, &id), 0, "initialize an AJM context");
        Require(id != 0, "the AJM context id is non-zero");
    }

    ~AjmContext() {
        sceAjmFinalize(id);
    }

    AjmContext(const AjmContext&) = delete;
    AjmContext& operator=(const AjmContext&) = delete;

    std::uint32_t Id() const noexcept {
        return id;
    }

private:
    std::uint32_t id = 0;
};

class AjmInstance {
public:
    AjmInstance(std::uint32_t contextId, std::uint32_t codec) : context(contextId) {
        RequireEqual(sceAjmInstanceCreate(context, codec, 0, &id), 0, "create an AJM instance of codec " + std::to_string(codec));
        alive = true;
    }

    ~AjmInstance() {
        if (alive) sceAjmInstanceDestroy(context, id);
    }

    AjmInstance(const AjmInstance&) = delete;
    AjmInstance& operator=(const AjmInstance&) = delete;

    std::uint32_t Id() const noexcept {
        return id;
    }

    int Destroy() {
        alive = false;
        return sceAjmInstanceDestroy(context, id);
    }

private:
    std::uint32_t context;
    std::uint32_t id = 0;
    bool alive = false;
};

class PendingBatch {
public:
    PendingBatch(std::uint32_t contextId, const AjmBatchInfo& info) : context(contextId) {
        AjmBatchError error{};
        RequireEqual(sceAjmBatchStart(context, &info, 0, &error, &id), 0, "start the batch");
        pending = true;
    }

    ~PendingBatch() {
        if (pending) Wait();
    }

    PendingBatch(const PendingBatch&) = delete;
    PendingBatch& operator=(const PendingBatch&) = delete;

    std::uint32_t Id() const noexcept {
        return id;
    }

    int Wait() {
        pending = false;
        AjmBatchError error{};
        return sceAjmBatchWait(context, id, 0, &error);
    }

private:
    std::uint32_t context;
    std::uint32_t id = 0;
    bool pending = false;
};

AjmBatchInfo InitializeBatch(std::vector<std::uint8_t>& buffer) {
    AjmBatchInfo info{};
    RequireEqual(sceAjmBatchInitialize(buffer.data(), buffer.size(), &info), 0, "initialize the batch");
    return info;
}

void Submit(std::uint32_t context, const AjmBatchInfo& info) {
    std::uint32_t id = 0;
    AjmBatchError error{};
    RequireEqual(sceAjmBatchStart(context, &info, 0, &error, &id), 0, "start the batch");
    RequireEqual(sceAjmBatchWait(context, id, 0, &error), 0, "wait for the batch");
}

void RequireRefused(std::uint32_t context, const AjmBatchInfo& info, std::string_view message) {
    RequireThrows<std::runtime_error>([&] {
        std::uint32_t id = 0;
        AjmBatchError error{};
        static_cast<void>(sceAjmBatchStart(context, &info, 0, &error, &id));
    }, message);
}

void RunDecode(std::uint32_t context, std::uint32_t instance, const std::uint8_t* input, std::size_t inputSize, void* output, std::size_t outputSize, DecodeSideband& sideband) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobDecode(&info, instance, input, inputSize, output, outputSize, &sideband), 0, "append the decode job");
    Submit(context, info);
}

void RunControl(std::uint32_t context, std::uint32_t instance, std::uint64_t flags, const void* input, std::size_t inputSize, std::int32_t* result) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobControl(&info, instance, flags, input, inputSize, result, 2 * sizeof(std::int32_t)), 0, "append the control job");
    Submit(context, info);
}

void RequireControlThrows(std::uint32_t instance, std::uint64_t flags, const void* input, std::size_t inputSize, std::size_t outputSize, const std::string& message) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info = InitializeBatch(batch);
    std::int32_t result[4] = {};
    RequireThrows<std::runtime_error>([&] {
        static_cast<void>(sceAjmBatchJobControl(&info, instance, flags, input, inputSize, result, outputSize));
    }, message);
    RequireEqual(info.offset, std::uint64_t{0}, message + ": batch offset after the refused job");
}

void RequireControlRefused(std::uint32_t context, std::uint32_t instance, std::uint64_t flags, const void* input, std::size_t inputSize, std::string_view message) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info = InitializeBatch(batch);
    std::int32_t result[2] = {-1, -1};
    RequireEqual(sceAjmBatchJobControl(&info, instance, flags, input, inputSize, result, sizeof(result)), 0, "append the control job");
    RequireRefused(context, info, message);
}

void InitializeOpus(std::uint32_t context, std::uint32_t instance) {
    const std::uint32_t parameters[3] = {2, 48000, 0};
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info = InitializeBatch(batch);
    std::int64_t result[2] = {-1, -1};
    RequireEqual(sceAjmBatchJobInitialize(&info, instance, parameters, sizeof(parameters), result), 0, "append the Opus initialize job");
    Submit(context, info);
    RequireEqual(result[0], std::int64_t{0}, "Opus initialize result");
}

void InitializeAt9(std::uint32_t context, std::uint32_t instance, const std::uint8_t (&config)[8]) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info = InitializeBatch(batch);
    std::int32_t result[2] = {-1, -1};
    RequireEqual(sceAjmBatchJobInitialize(&info, instance, config, sizeof(config), result), 0, "append the ATRAC9 initialize job");
    Submit(context, info);
    RequireEqual(result[0], 0, "ATRAC9 initialize result");
}

std::size_t OpusPacketBytes(std::size_t offset) {
    return opusStereo[offset] | (std::size_t{opusStereo[offset + 1]} << 8u);
}

std::int16_t Peak(const std::vector<std::int16_t>& pcm, std::int16_t peak) {
    for (const std::int16_t sample : pcm) peak = std::max<std::int16_t>(peak, static_cast<std::int16_t>(std::abs(sample)));
    return peak;
}

struct Mp3Header {
    std::uint64_t frameSize;
    std::uint32_t channels;
    std::uint32_t samples;
    std::uint32_t bitrate;
    std::uint32_t sampleRate;
};

void RequireParsesTo(const std::uint8_t* stream, std::uint32_t streamSize, const Mp3Header& expected, const std::string& label) {
    AjmDecMp3ParseFrame frame;
    std::memset(&frame, 0xFF, sizeof(frame));
    RequireEqual(sceAjmDecMp3ParseFrame(stream, streamSize, 0, &frame), 0, label + ": parse result");
    RequireEqual(frame.frame_size, expected.frameSize, label + ": frame size");
    RequireEqual(frame.num_channels, expected.channels, label + ": channels");
    RequireEqual(frame.samples_per_channel, expected.samples, label + ": samples per channel");
    RequireEqual(frame.bitrate, expected.bitrate, label + ": bitrate");
    RequireEqual(frame.sample_rate, expected.sampleRate, label + ": sample rate");
    RequireEqual(frame.encoder_delay, 0u, label + ": encoder delay");
    RequireEqual(frame.num_frames, 0u, label + ": frame count");
    RequireEqual(frame.total_samples, 0u, label + ": total samples");
    RequireEqual(frame.ofl_type, 0u, label + ": OFL type");
}

struct OflInfo {
    std::uint32_t frames;
    std::uint32_t delay;
    std::uint32_t total;
    std::uint32_t type;
};

void RequireOflParsesTo(const std::vector<std::uint8_t>& stream, std::uint32_t streamSize, const OflInfo& expected, const std::string& label) {
    AjmDecMp3ParseFrame frame;
    std::memset(&frame, 0xFF, sizeof(frame));
    RequireEqual(sceAjmDecMp3ParseFrame(stream.data(), streamSize, 1, &frame), 0, label + ": parse result");
    RequireEqual(frame.num_frames, expected.frames, label + ": frame count");
    RequireEqual(frame.encoder_delay, expected.delay, label + ": encoder delay");
    RequireEqual(frame.total_samples, expected.total, label + ": total samples");
    RequireEqual(frame.ofl_type, expected.type, label + ": OFL type");
}

std::vector<std::uint8_t> Mp3Frame(const std::uint8_t* prefix, std::size_t prefixSize, std::size_t frameSize) {
    std::vector<std::uint8_t> frame(frameSize, 0);
    std::memcpy(frame.data(), prefix, prefixSize);
    return frame;
}

ResampleInfo GetResampleInfo(std::uint32_t context, std::uint32_t instance) {
    ResampleInfo resample;
    std::memset(&resample, 0xAA, sizeof(resample));
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobGetResampleInfo(&info, instance, &resample), 0, "append the resample info job");
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
        AjmBatchInfo info = InitializeBatch(batch);
        std::int64_t setResult[2] = {-1, -1};
        DecodeSideband sideband{};
        if (ratio > 0) RequireEqual(sceAjmBatchJobSetResampleParameters(&info, instance, ratio, 1, setResult), 0, "append the resample parameters job");
        RequireEqual(sceAjmBatchJobDecode(&info, instance, stream + offset, size - offset, pcm.data(), pcm.size() * sizeof(std::int16_t), &sideband), 0, "append the decode job");
        Submit(context, info);
        if (ratio > 0) RequireEqual(setResult[0], std::int64_t{0}, "resample parameters result");
        Require(sideband.result == 0 || (offset == size && sideband.outputWritten == 0),
            "decode result at offset " + std::to_string(offset) + " is " + std::to_string(sideband.result));
        offset += static_cast<std::size_t>(sideband.inputConsumed);
        if (offset < size && static_cast<std::size_t>(sideband.outputWritten) < pcm.size() * sizeof(std::int16_t)) ++out.shortJobs;
        out.pcm.insert(out.pcm.end(), pcm.begin(), pcm.begin() + sideband.outputWritten / static_cast<std::int32_t>(sizeof(std::int16_t)));
        if (++out.jobs == 1) out.afterFirstJob = GetResampleInfo(context, instance);
        if (offset == size && sideband.outputWritten == 0) break;
    }
    RequireEqual(offset, size, "the whole stream is consumed");
    out.atEnd = GetResampleInfo(context, instance);
    return out;
}

ResampledStream StreamFreshOpus(std::uint32_t context, std::size_t frames, float ratio) {
    AjmInstance instance(context, codecOpus);
    InitializeOpus(context, instance.Id());
    auto stream = Stream(context, instance.Id(), opusStereo, sizeof(opusStereo), 2, frames, ratio);
    RequireEqual(instance.Destroy(), 0, "destroy the Opus instance");
    return stream;
}

ResampledStream StreamFreshMp3(std::uint32_t context, std::size_t frames, float ratio) {
    AjmInstance instance(context, codecMp3);
    auto stream = Stream(context, instance.Id(), mp3Mono, sizeof(mp3Mono), 1, frames, ratio);
    RequireEqual(instance.Destroy(), 0, "destroy the MP3 instance");
    return stream;
}

std::vector<std::uint8_t> SilentAt9Stream() {
    std::vector<std::uint8_t> stream(1024 * 3, 0);
    for (std::size_t superframe = 0; superframe < 3; ++superframe) {
        std::memcpy(stream.data() + superframe * 1024, at9MonoSilentSuperframe, sizeof(at9MonoSilentSuperframe));
    }
    return stream;
}

ResampledStream StreamFreshAt9(std::uint32_t context, std::size_t frames, float ratio) {
    constexpr std::uint8_t mono48k[8] = {0xFE, 0x70, 0x1F, 0xF0};
    const auto stream = SilentAt9Stream();
    AjmInstance instance(context, codecAt9);
    InitializeAt9(context, instance.Id(), mono48k);
    auto out = Stream(context, instance.Id(), stream.data(), stream.size(), 1, frames, ratio);
    RequireEqual(instance.Destroy(), 0, "destroy the ATRAC9 instance");
    return out;
}

void RequireDecimated(const std::vector<std::int16_t>& reference, const std::vector<std::int16_t>& resampled, std::size_t channels, std::size_t step) {
    const std::size_t frames = reference.size() / channels;
    RequireEqual(resampled.size() / channels, (frames - 3) / step + 1, "decimated frame count");
    for (std::size_t frame = 0; frame < resampled.size() / channels; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
            RequireEqual(resampled[frame * channels + channel], reference[frame * step * channels + channel],
                "decimated frame " + std::to_string(frame) + " channel " + std::to_string(channel));
        }
    }
}

void RequireInterpolated(const std::vector<std::int16_t>& reference, const std::vector<std::int16_t>& resampled, std::size_t channels) {
    const std::size_t frames = reference.size() / channels;
    RequireEqual(resampled.size() / channels, 2 * frames - 4, "interpolated frame count");
    const auto at = [&](std::ptrdiff_t frame, std::size_t channel) {
        return static_cast<double>(reference[static_cast<std::size_t>(std::max<std::ptrdiff_t>(frame, 0)) * channels + channel]);
    };
    for (std::size_t frame = 0; frame < resampled.size() / channels; ++frame) {
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const auto label = "interpolated frame " + std::to_string(frame) + " channel " + std::to_string(channel);
            const auto source = static_cast<std::ptrdiff_t>(frame / 2);
            const double midpoint = (-at(source - 1, channel) + 9.0 * at(source, channel) + 9.0 * at(source + 1, channel) - at(source + 2, channel)) / 16.0;
            if (frame % 2 == 0) {
                RequireEqual(resampled[frame * channels + channel], reference[frame / 2 * channels + channel], label);
            } else {
                Require(std::fabs(resampled[frame * channels + channel] - midpoint) <= 1.5,
                    label + ": " + std::to_string(resampled[frame * channels + channel]) + " is not within 1.5 of " + std::to_string(midpoint));
            }
        }
    }
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
    AjmInstance instance(context, codecAt9);
    std::vector<std::uint8_t> batch(4096);
    std::vector<std::int16_t> pcm(1024 * channels);
    std::int32_t initResult[2] = {-1, -1};
    struct {
        DecodeSideband stream;
        std::uint32_t frames;
        std::uint32_t reserved;
    } decoded{};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobInitialize(&info, instance.Id(), config, sizeof(config), initResult), 0, "append the initialize job");
    RequireEqual(sceAjmBatchJobRun(&info, instance.Id(), sidebandStream | runMultipleFrames, superframe.data(), superframe.size(), pcm.data(), pcm.size() * sizeof(std::int16_t), &decoded, sizeof(decoded)), 0, "append the run job");
    Submit(context, info);
    RequireEqual(initResult[0], 0, "initialize result");
    RequireEqual(decoded.stream.result, 0, "run result");
    RequireEqual(static_cast<std::size_t>(decoded.stream.inputConsumed), superframe.size(), "input consumed");
    RequireEqual(static_cast<std::size_t>(decoded.stream.outputWritten), pcm.size() * sizeof(std::int16_t), "output written");
    RequireEqual(decoded.stream.totalDecodedSamples, std::uint64_t{1024}, "total decoded samples");
    RequireEqual(decoded.frames, 4u, "decoded frames");
    RequireEqual(instance.Destroy(), 0, "destroy the ATRAC9 instance");
    return pcm;
}

const Case initializeNull{"Initialize_NullContextPointer_FailsWithInvalidParameter", [] {
    RequireEqual(sceAjmInitialize(0, nullptr), invalidParameter, "null context pointer");
}};

const Case initializeValid{"Initialize_ValidPointer_ReturnsNonZeroContext", [] {
    std::uint32_t context = 0;
    const int result = sceAjmInitialize(0, &context);
    sceAjmFinalize(context);
    RequireEqual(result, 0, "initialize result");
    Require(context != 0, "the context id is non-zero");
}};

const Case finalizeContext{"Finalize_InitializedContext_Succeeds", [] {
    std::uint32_t context = 0;
    RequireEqual(sceAjmInitialize(0, &context), 0, "initialize result");
    RequireEqual(sceAjmFinalize(context), 0, "finalize result");
}};

const Case at9ParseStereo{"At9ParseConfigData_Stereo48k_DescribesStream", [] {
    const std::uint8_t stereo48k[4] = {0xFE, 0x72, 0x1F, 0xF0};
    AjmDecAt9ConfigDataInfo info{};
    RequireEqual(sceAjmDecAt9ParseConfigData(stereo48k, &info), 0, "parse result");
    RequireEqual(info.channels, 2u, "channels");
    RequireEqual(info.sample_rate, 48000u, "sample rate");
    RequireEqual(info.frame_samples_per_channel, 256u, "frame samples per channel");
    RequireEqual(info.superframe_samples_per_channel, 1024u, "superframe samples per channel");
    RequireEqual(info.superframe_size, 1024u, "superframe size");
}};

const Case at9ParseBadHeader{"At9ParseConfigData_BadSyncByte_FailsWithInvalidParameter", [] {
    const std::uint8_t badHeader[4] = {0xFD, 0x72, 0x1F, 0xF0};
    AjmDecAt9ConfigDataInfo info{};
    RequireEqual(sceAjmDecAt9ParseConfigData(badHeader, &info), invalidParameter, "config FD 72 1F F0");
}};

const Case at9ParseNull{"At9ParseConfigData_NullConfig_FailsWithInvalidParameter", [] {
    AjmDecAt9ConfigDataInfo info{};
    RequireEqual(sceAjmDecAt9ParseConfigData(nullptr, &info), invalidParameter, "null config");
}};

const Case at9ParseThirdOrder{"At9ParseConfigData_ThirdOrderAmbisonic_DescribesSixteenChannels", [] {
    const std::uint8_t thirdOrder[4] = {0x30, 0x73, 0xC1, 0x7E};
    AjmDecAt9ConfigDataInfo parsed{};
    RequireEqual(sceAjmDecAt9ParseConfigData(thirdOrder, &parsed), 0, "parse result");
    RequireEqual(parsed.channels, 16u, "channels");
    RequireEqual(parsed.sample_rate, 48000u, "sample rate");
    RequireEqual(parsed.frame_samples_per_channel, 256u, "frame samples per channel");
    RequireEqual(parsed.superframe_samples_per_channel, 1024u, "superframe samples per channel");
    RequireEqual(parsed.superframe_size, 6144u, "superframe size");
}};

const Case at9ParseValidationBit{"At9ParseConfigData_ValidationBitSet_FailsWithInvalidParameter", [] {
    const std::uint8_t validationBitSet[4] = {0x30, 0x73, 0xE1, 0x7E};
    AjmDecAt9ConfigDataInfo parsed{};
    RequireEqual(sceAjmDecAt9ParseConfigData(validationBitSet, &parsed), invalidParameter, "config 30 73 E1 7E");
}};

const Case runSplitNullBuffers{"BatchJobRunSplit_NullBufferArrays_ThrowsWithoutAppending", [] {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info = InitializeBatch(batch);
    RequireThrows<std::runtime_error>([&] {
        sceAjmBatchJobRunSplit(&info, 0, 0, nullptr, 1, nullptr, 0, nullptr, 0);
    }, "one input buffer through a null array");
    RequireEqual(info.offset, std::uint64_t{0}, "batch offset after the refused job");
}};

const Case mp3ParseMonoFrames{"Mp3ParseFrame_MonoStreamFrames_DescribeEachFrame", [] {
    for (std::size_t offset = 0; offset < sizeof(mp3Mono); offset += 96) {
        RequireParsesTo(mp3Mono + offset, static_cast<std::uint32_t>(sizeof(mp3Mono) - offset), {96, 1, 1152, 32000, 48000},
            "frame at offset " + std::to_string(offset));
    }
}};

const Case mp3ParseVariants{"Mp3ParseFrame_HeaderVariants_DescribeEachHeader", [] {
    struct Variant {
        const char* name;
        std::uint8_t header[4];
        Mp3Header expected;
    };
    const Variant variants[] = {
        {"MPEG-1 stereo padded", {0xFF, 0xFB, 0x92, 0x00}, {418, 2, 1152, 128000, 44100}},
        {"MPEG-1 CRC protected", {0xFF, 0xFA, 0xE8, 0x40}, {1440, 2, 1152, 320000, 32000}},
        {"MPEG-2 mono", {0xFF, 0xF3, 0x80, 0xC0}, {208, 1, 576, 64000, 22050}},
        {"MPEG-2 dual channel", {0xFF, 0xF3, 0xE8, 0x80}, {720, 2, 576, 160000, 16000}},
        {"MPEG-2.5 mono", {0xFF, 0xE3, 0x88, 0xC0}, {576, 1, 576, 64000, 8000}},
        {"MPEG-2.5 stereo padded", {0xFF, 0xE3, 0x12, 0x00}, {53, 2, 576, 8000, 11025}},
    };
    for (const auto& variant : variants) RequireParsesTo(variant.header, 4, variant.expected, variant.name);
}};

const Case mp3ParseNullStream{"Mp3ParseFrame_NullStream_FailsWithInvalidParameter", [] {
    AjmDecMp3ParseFrame frame{};
    RequireEqual(sceAjmDecMp3ParseFrame(nullptr, 4, 0, &frame), invalidParameter, "null stream");
}};

const Case mp3ParseNullFrame{"Mp3ParseFrame_NullFrame_FailsWithInvalidParameter", [] {
    RequireEqual(sceAjmDecMp3ParseFrame(mp3Mono, 4, 0, nullptr), invalidParameter, "null frame");
}};

const Case mp3ParseShort{"Mp3ParseFrame_StreamShorterThanHeader_FailsWithInvalidParameter", [] {
    AjmDecMp3ParseFrame frame{};
    RequireEqual(sceAjmDecMp3ParseFrame(mp3Mono, 3, 0, &frame), invalidParameter, "three byte stream");
}};

const Case mp3ParseInvalid{"Mp3ParseFrame_InvalidHeaders_FailWithInvalidParameter", [] {
    struct Invalid {
        const char* name;
        std::uint8_t header[4];
    };
    const Invalid headers[] = {
        {"broken sync", {0xFF, 0xDB, 0x14, 0xC4}},
        {"reserved version", {0xFF, 0xEB, 0x14, 0xC4}},
        {"free bitrate", {0xFF, 0xFB, 0x04, 0xC4}},
        {"forbidden bitrate", {0xFF, 0xFB, 0xF4, 0xC4}},
        {"reserved sample rate", {0xFF, 0xFB, 0x1C, 0xC4}},
        {"MPEG-2.5 above 64 kbps", {0xFF, 0xE3, 0x98, 0xC0}},
    };
    for (const auto& invalid : headers) {
        AjmDecMp3ParseFrame frame{};
        RequireEqual(sceAjmDecMp3ParseFrame(invalid.header, 4, 0, &frame), invalidParameter, invalid.name);
    }
}};

const Case mp3ParseXingWithoutOfl{"Mp3ParseFrame_LameXingFrameWithoutOfl_DescribesHeaderOnly", [] {
    const auto stereo = Mp3Frame(lameMpeg1StereoVbr, sizeof(lameMpeg1StereoVbr), 417);
    RequireParsesTo(stereo.data(), 417, {417, 2, 1152, 128000, 44100}, "LAME MPEG-1 stereo VBR");
}};

const Case oflLameStereo{"Mp3ParseOfl_LameXingStereoVbr_ReportsFramesDelayAndTotal", [] {
    const auto stereo = Mp3Frame(lameMpeg1StereoVbr, sizeof(lameMpeg1StereoVbr), 417);
    RequireOflParsesTo(stereo, 417, {10, 1152 + 576 + 529, 10000, 1}, "LAME MPEG-1 stereo VBR");
}};

const Case oflXingWithoutLame{"Mp3ParseOfl_StreamEndsBeforeLameTag_ReportsFramesOnly", [] {
    const auto stereo = Mp3Frame(lameMpeg1StereoVbr, sizeof(lameMpeg1StereoVbr), 417);
    RequireOflParsesTo(stereo, 48, {10, 0, 0, 0}, "48 byte stream");
}};

const Case oflXingTruncated{"Mp3ParseOfl_StreamEndsInsideXingTag_ReportsNothing", [] {
    const auto stereo = Mp3Frame(lameMpeg1StereoVbr, sizeof(lameMpeg1StereoVbr), 417);
    RequireOflParsesTo(stereo, 46, {0, 0, 0, 0}, "46 byte stream");
}};

const Case oflLameMono{"Mp3ParseOfl_LameInfoMpeg1MonoCbr_ReportsFramesDelayAndTotal", [] {
    const auto mono = Mp3Frame(lameMpeg1MonoCbr, sizeof(lameMpeg1MonoCbr), 576);
    RequireOflParsesTo(mono, 576, {10, 1152 + 576 + 529, 10000, 1}, "LAME MPEG-1 mono CBR");
}};

const Case oflLameMpeg2{"Mp3ParseOfl_LameInfoMpeg2MonoCbr_ReportsFramesDelayAndTotal", [] {
    const auto mpeg2 = Mp3Frame(lameMpeg2MonoCbr, sizeof(lameMpeg2MonoCbr), 208);
    RequireOflParsesTo(mpeg2, 208, {20, 576 + 576 + 529, 10000, 1}, "LAME MPEG-2 mono CBR");
}};

const Case oflFgh{"Mp3ParseOfl_FghTag_ReportsDelayAndTotal", [] {
    const auto fgh = Mp3Frame(fghFrame, sizeof(fghFrame), 417);
    RequireOflParsesTo(fgh, 417, {0, 1105, 441000, 3}, "FGH frame");
}};

const Case oflFghBadCrc{"Mp3ParseOfl_FghTagWithBadCrc_ReportsNothing", [] {
    RequireOflParsesTo(Mp3Frame(fghBadCrcFrame, sizeof(fghBadCrcFrame), 417), 417, {0, 0, 0, 0}, "FGH frame with a bad CRC");
}};

const Case oflVbri{"Mp3ParseOfl_VbriTag_ReportsDelay", [] {
    const auto vbri = Mp3Frame(vbriFrame, sizeof(vbriFrame), 417);
    RequireOflParsesTo(vbri, 417, {0, 576, 0, 2}, "VBRI frame");
}};

const Case oflVbriThenFgh{"Mp3ParseOfl_VbriFollowedByFgh_CombinesDelaysAndTotal", [] {
    auto vbri = Mp3Frame(vbriFrame, sizeof(vbriFrame), 417);
    const auto fgh = Mp3Frame(fghFrame, sizeof(fghFrame), 417);
    vbri.insert(vbri.end(), fgh.begin(), fgh.end());
    RequireOflParsesTo(vbri, 834, {0, 576 + 1105, 441000, 4}, "VBRI frame followed by an FGH frame");
}};

const Case oflVbriThenTruncatedFgh{"Mp3ParseOfl_VbriFollowedByTruncatedFgh_ReportsVbriOnly", [] {
    auto vbri = Mp3Frame(vbriFrame, sizeof(vbriFrame), 417);
    const auto fgh = Mp3Frame(fghFrame, sizeof(fghFrame), 417);
    vbri.insert(vbri.end(), fgh.begin(), fgh.end());
    RequireOflParsesTo(vbri, 480, {0, 576, 0, 2}, "VBRI frame followed by a truncated FGH frame");
}};

const Case oflUnsupported{"Mp3ParseOfl_UnsupportedTagFrames_Throw", [] {
    const auto stereo = Mp3Frame(lameMpeg1StereoVbr, sizeof(lameMpeg1StereoVbr), 417);
    auto protectedFrame = stereo;
    protectedFrame[1] = 0xFA;
    auto layer2 = stereo;
    layer2[1] = 0xFD;
    auto monoVbri = Mp3Frame(vbriFrame, sizeof(vbriFrame), 417);
    monoVbri[3] = 0xC4;
    struct Unsupported {
        const char* name;
        const std::vector<std::uint8_t>* stream;
    };
    const Unsupported frames[] = {{"CRC protected Xing frame", &protectedFrame}, {"layer II Xing frame", &layer2}, {"mono VBRI frame", &monoVbri}};
    for (const auto& unsupported : frames) {
        AjmDecMp3ParseFrame frame{};
        RequireThrows<std::runtime_error>([&] {
            sceAjmDecMp3ParseFrame(unsupported.stream->data(), static_cast<std::uint32_t>(unsupported.stream->size()), 1, &frame);
        }, unsupported.name);
    }
}};

const Case decodeMp3{"BatchJobDecode_Mp3MonoStream_DecodesSixFramesInOrder", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    std::vector<std::uint8_t> batch(0x40);
    std::vector<std::int16_t> pcm(1152);
    std::size_t offset = 0;
    std::int16_t peak = 0;
    for (int frame = 0; frame < 6; ++frame) {
        const auto label = "frame " + std::to_string(frame);
        AjmBatchInfo info = InitializeBatch(batch);
        DecodeSideband sideband{};
        RequireEqual(sceAjmBatchJobDecode(&info, instance.Id(), mp3Mono + offset, sizeof(mp3Mono) - offset, pcm.data(), pcm.size() * sizeof(std::int16_t), &sideband), 0, label + ": append the decode job");
        Submit(context.Id(), info);
        RequireEqual(sideband.result, 0, label + ": result");
        RequireEqual(sideband.inputConsumed, 96, label + ": input consumed");
        RequireEqual(sideband.outputWritten, 1152 * 2, label + ": output written");
        RequireEqual(sideband.totalDecodedSamples, static_cast<std::uint64_t>(frame + 1) * 1152, label + ": total decoded samples");
        peak = Peak(pcm, peak);
        offset += static_cast<std::size_t>(sideband.inputConsumed);
    }
    RequireEqual(offset, sizeof(mp3Mono), "the whole stream is consumed");
    Require(peak > 3276 && peak < 4915, "peak " + std::to_string(peak) + " lies in (3276, 4915)");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case decodeOpus{"BatchJobDecode_OpusStereoPackets_DecodeFramesAtExpectedLevel", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecOpus);
    InitializeOpus(context.Id(), instance.Id());
    std::vector<std::int16_t> pcm(960 * 2);
    std::size_t offset = 0;
    std::int16_t peak = 0;
    for (int packet = 0; packet < 3; ++packet) {
        const auto label = "packet " + std::to_string(packet);
        const std::size_t bytes = OpusPacketBytes(offset);
        DecodeSideband sideband{};
        RunDecode(context.Id(), instance.Id(), opusStereo + offset, sizeof(opusStereo) - offset, pcm.data(), pcm.size() * sizeof(std::int16_t), sideband);
        RequireEqual(sideband.result, 0, label + ": result");
        RequireEqual(static_cast<std::size_t>(sideband.inputConsumed), 2 + bytes, label + ": input consumed");
        RequireEqual(sideband.outputWritten, 960 * 2 * 2, label + ": output written");
        RequireEqual(sideband.totalDecodedSamples, static_cast<std::uint64_t>(packet + 1) * 960, label + ": total decoded samples");
        if (packet > 0) peak = Peak(pcm, peak);
        offset += static_cast<std::size_t>(sideband.inputConsumed);
    }
    Require(peak > 900 && peak < 1200, "peak " + std::to_string(peak) + " lies in (900, 1200)");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case decodeOpusSmallOutput{"BatchJobDecode_OpusOutputSmallerThanPacket_SplitsPacketAcrossJobs", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecOpus);
    InitializeOpus(context.Id(), instance.Id());
    std::vector<std::int16_t> pcm(960 * 2);
    std::size_t offset = 0;
    for (int packet = 0; packet < 3; ++packet) {
        DecodeSideband sideband{};
        RunDecode(context.Id(), instance.Id(), opusStereo + offset, sizeof(opusStereo) - offset, pcm.data(), pcm.size() * sizeof(std::int16_t), sideband);
        RequireEqual(sideband.result, 0, "decode the leading packets");
        offset += static_cast<std::size_t>(sideband.inputConsumed);
    }
    std::vector<std::int16_t> small(512 * 2);
    DecodeSideband first{};
    RunDecode(context.Id(), instance.Id(), opusStereo + offset, sizeof(opusStereo) - offset, small.data(), small.size() * sizeof(std::int16_t), first);
    DecodeSideband rest{};
    RunDecode(context.Id(), instance.Id(), opusStereo + sizeof(opusStereo), 0, small.data(), small.size() * sizeof(std::int16_t), rest);
    RequireEqual(first.result, 0, "first job result");
    RequireEqual(static_cast<std::size_t>(first.inputConsumed), sizeof(opusStereo) - offset, "first job input consumed");
    RequireEqual(first.outputWritten, 512 * 2 * 2, "first job output written");
    RequireEqual(rest.result, 0, "second job result");
    RequireEqual(rest.inputConsumed, 0, "second job input consumed");
    RequireEqual(rest.outputWritten, (960 - 512) * 2 * 2, "second job output written");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case decodeSingle{"BatchJobDecodeSingle_Mp3MonoStream_DecodesOneFrame", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    std::vector<std::uint8_t> batch(0x40);
    std::vector<std::int16_t> pcm(1152 * 6);
    AjmBatchInfo info = InitializeBatch(batch);
    DecodeSideband sideband{};
    RequireEqual(sceAjmBatchJobDecodeSingle(&info, instance.Id(), mp3Mono, sizeof(mp3Mono), pcm.data(), pcm.size() * sizeof(std::int16_t), &sideband), 0, "append the decode job");
    Submit(context.Id(), info);
    RequireEqual(sideband.result, 0, "result");
    RequireEqual(sideband.inputConsumed, 96, "input consumed");
    RequireEqual(sideband.outputWritten, 1152 * 2, "output written");
    RequireEqual(sideband.totalDecodedSamples, std::uint64_t{1152}, "total decoded samples");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case gaplessSetGet{"GaplessDecode_SetThenGetInOneBatch_ReportsConfiguredValues", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    std::vector<std::uint8_t> batch(4096);
    const GaplessDecode gapless{2000, 100, 0};
    std::int32_t setResult[2] = {-1, -1};
    GaplessSideband before{-1, -1, 0, 0, 0xffff};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobSetGaplessDecode(&info, instance.Id(), &gapless, 1, setResult), 0, "append the set job");
    RequireEqual(sceAjmBatchJobGetGaplessDecode(&info, instance.Id(), &before), 0, "append the get job");
    Submit(context.Id(), info);
    RequireEqual(setResult[0], 0, "set result");
    RequireEqual(before.result, 0, "get result");
    RequireEqual(before.internalResult, 0, "get internal result");
    RequireEqual(before.totalSamples, 2000u, "total samples");
    RequireEqual(before.skipSamples, std::uint16_t{100}, "skip samples");
    RequireEqual(before.skippedSamples, std::uint16_t{0}, "skipped samples");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case gaplessSkip{"GaplessDecode_SkipSamples_TrimsFirstDecodedFrame", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    std::vector<std::uint8_t> batch(4096);
    const GaplessDecode gapless{2000, 100, 0};
    std::int32_t setResult[2] = {-1, -1};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobSetGaplessDecode(&info, instance.Id(), &gapless, 1, setResult), 0, "append the set job");
    Submit(context.Id(), info);
    RequireEqual(setResult[0], 0, "set result");
    std::vector<std::int16_t> pcm(1152);
    DecodeSideband decoded{};
    GaplessSideband after{-1, -1, 0, 0, 0};
    info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobDecodeSingle(&info, instance.Id(), mp3Mono, sizeof(mp3Mono), pcm.data(), pcm.size() * sizeof(std::int16_t), &decoded), 0, "append the decode job");
    RequireEqual(sceAjmBatchJobGetGaplessDecode(&info, instance.Id(), &after), 0, "append the get job");
    Submit(context.Id(), info);
    RequireEqual(decoded.result, 0, "decode result");
    RequireEqual(decoded.inputConsumed, 96, "input consumed");
    RequireEqual(decoded.outputWritten, (1152 - 100) * 2, "output written");
    RequireEqual(after.result, 0, "get result");
    RequireEqual(after.skippedSamples, std::uint16_t{100}, "skipped samples");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case codecInfoUninitialized{"GetCodecInfo_UninitializedAt9_ReportsNotInitialized", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    std::vector<std::uint8_t> batch(4096);
    At9CodecInfoSideband early{-1, -1, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobGetCodecInfo(&info, instance.Id(), &early, sizeof(early)), 0, "append the codec info job");
    Submit(context.Id(), info);
    RequireEqual(early.result, 1, "result");
    RequireEqual(early.superframeSize, 0xaaaaaaaau, "superframe size untouched");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case codecInfoInitialized{"GetCodecInfo_InitializedAt9_ReportsSuperframeLayout", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    std::vector<std::uint8_t> batch(4096);
    std::int32_t initResult[2] = {-1, -1};
    At9CodecInfoSideband codec{-1, -1, 0, 0, 0, 0};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobInitialize(&info, instance.Id(), at9Stereo48k, sizeof(at9Stereo48k), initResult), 0, "append the initialize job");
    RequireEqual(sceAjmBatchJobGetCodecInfo(&info, instance.Id(), &codec, sizeof(codec)), 0, "append the codec info job");
    Submit(context.Id(), info);
    RequireEqual(initResult[0], 0, "initialize result");
    RequireEqual(codec.result, 0, "result");
    RequireEqual(codec.internalResult, 0, "internal result");
    RequireEqual(codec.superframeSize, 1024u, "superframe size");
    RequireEqual(codec.framesInSuperframe, 4u, "frames in superframe");
    RequireEqual(codec.nextFrameSize, 1024u, "next frame size");
    RequireEqual(codec.frameSamples, 256u, "frame samples");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case codecInfoBounded{"GetCodecInfo_SidebandOnlyFitsResult_LeavesCodecFieldsUntouched", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    std::vector<std::uint8_t> batch(4096);
    std::int32_t initResult[2] = {-1, -1};
    At9CodecInfoSideband bounded{-1, -1, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobInitialize(&info, instance.Id(), at9Stereo48k, sizeof(at9Stereo48k), initResult), 0, "append the initialize job");
    RequireEqual(sceAjmBatchJobGetCodecInfo(&info, instance.Id(), &bounded, 2 * sizeof(std::int32_t)), 0, "append the codec info job");
    Submit(context.Id(), info);
    RequireEqual(initResult[0], 0, "initialize result");
    RequireEqual(bounded.result, 0, "result");
    RequireEqual(bounded.superframeSize, 0xaaaaaaaau, "superframe size untouched");
    RequireEqual(bounded.frameSamples, 0xaaaaaaaau, "frame samples untouched");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case codecInfoWithMultipleFrames{"Run_At9CodecInfoWithMultipleFrames_RefusedAtStart", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    InitializeAt9(context.Id(), instance.Id(), at9Stereo48k);
    std::vector<std::uint8_t> batch(4096);
    std::uint8_t sideband[64] = {};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobRun(&info, instance.Id(), runGetCodecInfo | runMultipleFrames, nullptr, 0, nullptr, 0, sideband, sizeof(sideband)), 0, "append the run job");
    RequireRefused(context.Id(), info, "RUN_GET_CODEC_INFO with RUN_MULTIPLE_FRAMES");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case codecInfoMp3{"GetCodecInfo_Mp3Instance_RefusedAtStart", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    std::vector<std::uint8_t> batch(4096);
    std::uint8_t sideband[64] = {};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobGetCodecInfo(&info, instance.Id(), sideband, sizeof(sideband)), 0, "append the codec info job");
    RequireRefused(context.Id(), info, "codec info of an MP3 instance");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case getInfoUninitialized{"GetInfo_UninitializedAt9_ReportsNotInitialized", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    std::vector<std::uint8_t> batch(4096);
    FormatSideband early{-1, -1, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau, 0xaaaaaaaau};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobGetInfo(&info, instance.Id(), &early), 0, "append the info job");
    Submit(context.Id(), info);
    RequireEqual(early.result, 1, "result");
    RequireEqual(early.numChannels, 0xaaaaaaaau, "channels untouched");
    RequireEqual(early.sampleRate, 0xaaaaaaaau, "sample rate untouched");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case getInfoAt9{"GetInfo_InitializedAt9_ReportsStereoFormat", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    std::vector<std::uint8_t> batch(4096);
    std::int32_t initResult[2] = {-1, -1};
    FormatSideband at9{-1, -1, 0, 0, 0, 0xaaaaaaaau, 0, 0xaaaaaaaau};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobInitialize(&info, instance.Id(), at9Stereo48k, sizeof(at9Stereo48k), initResult), 0, "append the initialize job");
    RequireEqual(sceAjmBatchJobGetInfo(&info, instance.Id(), &at9), 0, "append the info job");
    Submit(context.Id(), info);
    RequireEqual(initResult[0], 0, "initialize result");
    RequireEqual(at9.result, 0, "result");
    RequireEqual(at9.internalResult, 0, "internal result");
    RequireEqual(at9.numChannels, 2u, "channels");
    RequireEqual(at9.channelMask, 0x3u, "channel mask");
    RequireEqual(at9.sampleRate, 48000u, "sample rate");
    RequireEqual(at9.sampleEncoding, 0u, "sample encoding");
    RequireEqual(at9.reserved, 0u, "reserved");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case getInfoMp3{"GetInfo_AfterMp3Decode_ReportsMonoFormat", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    std::vector<std::uint8_t> batch(4096);
    std::vector<std::int16_t> pcm(1152);
    DecodeSideband decoded{};
    FormatSideband mp3{-1, -1, 0, 0, 0, 0xaaaaaaaau, 0, 0xaaaaaaaau};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobDecodeSingle(&info, instance.Id(), mp3Mono, sizeof(mp3Mono), pcm.data(), pcm.size() * sizeof(std::int16_t), &decoded), 0, "append the decode job");
    RequireEqual(sceAjmBatchJobGetInfo(&info, instance.Id(), &mp3), 0, "append the info job");
    Submit(context.Id(), info);
    RequireEqual(decoded.result, 0, "decode result");
    RequireEqual(decoded.inputConsumed, 96, "input consumed");
    RequireEqual(mp3.result, 0, "result");
    RequireEqual(mp3.internalResult, 0, "internal result");
    RequireEqual(mp3.numChannels, 1u, "channels");
    RequireEqual(mp3.channelMask, 0x4u, "channel mask");
    RequireEqual(mp3.sampleRate, 48000u, "sample rate");
    RequireEqual(mp3.sampleEncoding, 0u, "sample encoding");
    RequireEqual(mp3.bitrate, 32000u, "bitrate");
    RequireEqual(mp3.reserved, 0u, "reserved");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case batchStartTwice{"BatchStart_SameInfoTwice_ReturnsDistinctIds", [] {
    const AjmContext context;
    std::vector<std::uint8_t> batch(64);
    const AjmBatchInfo info = InitializeBatch(batch);
    const PendingBatch first(context.Id(), info);
    const PendingBatch second(context.Id(), info);
    Require(second.Id() != first.Id(), "batch ids " + std::to_string(first.Id()) + " and " + std::to_string(second.Id()) + " differ");
}};

const Case batchWaitReverse{"BatchWait_StartedBatchesInReverseOrder_Succeed", [] {
    const AjmContext context;
    std::vector<std::uint8_t> batch(64);
    const AjmBatchInfo info = InitializeBatch(batch);
    PendingBatch first(context.Id(), info);
    PendingBatch second(context.Id(), info);
    RequireEqual(second.Wait(), 0, "wait for the second batch");
    RequireEqual(first.Wait(), 0, "wait for the first batch");
}};

const Case batchWaitReleased{"BatchWait_ReleasedOrUnknownBatch_FailsWithInvalidBatch", [] {
    const AjmContext context;
    std::vector<std::uint8_t> batch(64);
    const AjmBatchInfo info = InitializeBatch(batch);
    PendingBatch first(context.Id(), info);
    PendingBatch second(context.Id(), info);
    RequireEqual(second.Wait(), 0, "wait for the second batch");
    RequireEqual(first.Wait(), 0, "wait for the first batch");
    AjmBatchError error{};
    RequireEqual(sceAjmBatchWait(context.Id(), first.Id(), 0, &error), invalidBatch, "first batch waited again");
    RequireEqual(sceAjmBatchWait(context.Id(), second.Id(), 0, &error), invalidBatch, "second batch waited again");
    RequireEqual(sceAjmBatchWait(context.Id(), 0, 0, &error), invalidBatch, "batch 0");
    RequireEqual(sceAjmBatchWait(context.Id(), second.Id() + 1, 0, &error), invalidBatch, "batch never started");
}};

const Case batchCancelPending{"BatchCancel_PendingBatch_SucceedsRepeatedly", [] {
    const AjmContext context;
    std::vector<std::uint8_t> batch(64);
    const AjmBatchInfo info = InitializeBatch(batch);
    PendingBatch pending(context.Id(), info);
    RequireEqual(sceAjmBatchCancel(context.Id(), pending.Id()), 0, "first cancel");
    RequireEqual(sceAjmBatchCancel(context.Id(), pending.Id()), 0, "second cancel");
    RequireEqual(pending.Wait(), 0, "wait for the cancelled batch");
}};

const Case batchCancelInvalidContext{"BatchCancel_InvalidContext_FailsWithInvalidContext", [] {
    const AjmContext context;
    std::vector<std::uint8_t> batch(64);
    const AjmBatchInfo info = InitializeBatch(batch);
    const PendingBatch pending(context.Id(), info);
    RequireEqual(sceAjmBatchCancel(0, pending.Id()), invalidContext, "context 0");
    RequireEqual(sceAjmBatchCancel(context.Id() + 1, pending.Id()), invalidContext, "context never initialized");
}};

const Case batchCancelReleased{"BatchCancel_WaitedOrUnknownBatch_FailsWithInvalidBatch", [] {
    const AjmContext context;
    std::vector<std::uint8_t> batch(64);
    const AjmBatchInfo info = InitializeBatch(batch);
    PendingBatch pending(context.Id(), info);
    RequireEqual(pending.Wait(), 0, "wait for the batch");
    RequireEqual(sceAjmBatchCancel(context.Id(), pending.Id()), invalidBatch, "waited batch");
    RequireEqual(sceAjmBatchCancel(context.Id(), 0), invalidBatch, "batch 0");
    RequireEqual(sceAjmBatchCancel(context.Id(), pending.Id() + 1), invalidBatch, "batch never started");
}};

const Case controlStartAt9{"Control_StartAt9_InitializesCodecAndGapless", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    std::vector<std::uint8_t> batch(4096);
    std::int32_t result[4] = {-1, -1, -1, -1};
    At9CodecInfoSideband codec{-1, -1, 0, 0, 0, 0};
    GaplessSideband gapless{-1, -1, 0, 0, 0xffff};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobControl(&info, instance.Id(), controlStart, &at9Start, sizeof(at9Start), result, 2 * sizeof(std::int32_t)), 0, "append the control job");
    RequireEqual(sceAjmBatchJobGetCodecInfo(&info, instance.Id(), &codec, sizeof(codec)), 0, "append the codec info job");
    RequireEqual(sceAjmBatchJobGetGaplessDecode(&info, instance.Id(), &gapless), 0, "append the gapless job");
    Submit(context.Id(), info);
    RequireEqual(result[0], 0, "control result");
    RequireEqual(result[1], 0, "control internal result");
    RequireEqual(result[2], -1, "bytes after the 8-byte result untouched");
    RequireEqual(result[3], -1, "bytes after the 8-byte result untouched");
    RequireEqual(codec.result, 0, "codec info result");
    RequireEqual(codec.superframeSize, 1024u, "superframe size");
    RequireEqual(codec.framesInSuperframe, 4u, "frames in superframe");
    RequireEqual(codec.nextFrameSize, 1024u, "next frame size");
    RequireEqual(codec.frameSamples, 256u, "frame samples");
    RequireEqual(gapless.result, 0, "gapless result");
    RequireEqual(gapless.totalSamples, 2000u, "total samples");
    RequireEqual(gapless.skipSamples, std::uint16_t{100}, "skip samples");
    RequireEqual(gapless.skippedSamples, std::uint16_t{0}, "skipped samples");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlStartAt9BadConfig{"Control_StartAt9WithBadConfig_FailsAndLeavesInstanceUninitialized", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    std::int32_t result[2] = {-1, -1};
    RunControl(context.Id(), instance.Id(), controlStart, &at9Start, sizeof(at9Start), result);
    RequireEqual(result[0], 0, "initial start result");
    std::vector<std::uint8_t> batch(4096);
    At9Control broken = at9Start;
    broken.config[0] = 0xFD;
    At9CodecInfoSideband uninitialized{-1, -1, 0, 0, 0, 0};
    AjmBatchInfo info = InitializeBatch(batch);
    RequireEqual(sceAjmBatchJobControl(&info, instance.Id(), controlStart, &broken, sizeof(broken), result, 2 * sizeof(std::int32_t)), 0, "append the control job");
    RequireEqual(sceAjmBatchJobGetCodecInfo(&info, instance.Id(), &uninitialized, sizeof(uninitialized)), 0, "append the codec info job");
    Submit(context.Id(), info);
    RequireEqual(result[0], 4, "control result");
    RequireEqual(uninitialized.result, 1, "codec info result");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlInitializeAt9{"Control_InitializeAt9WithConfigOnly_Succeeds", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    std::int32_t result[2] = {-1, -1};
    RunControl(context.Id(), instance.Id(), controlInitialize, at9Start.config, sizeof(at9Start) - sizeof(at9Start.gapless), result);
    RequireEqual(result[0], 0, "control result");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlAt9SizeMismatch{"Control_At9SidebandSizeMismatch_RefusedAtStart", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    RequireControlRefused(context.Id(), instance.Id(), controlInitialize, &at9Start, sizeof(at9Start), "INITIALIZE with the gapless block");
    RequireControlRefused(context.Id(), instance.Id(), controlReset | sidebandGaplessDecode, &at9Start, sizeof(at9Start), "RESET | GAPLESS with the initialize block");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlUnsupportedArguments{"Control_UnsupportedArguments_ThrowWithoutAppending", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    struct Unsupported {
        const char* name;
        std::uint64_t flags;
        const void* input;
        std::size_t inputSize;
        std::size_t outputSize;
    };
    const Unsupported calls[] = {
        {"INITIALIZE | GAPLESS without RESET", controlInitialize | sidebandGaplessDecode, &at9Start, sizeof(at9Start), 8},
        {"START with flag bit 15", controlStart | (1ull << 15), &at9Start, sizeof(at9Start), 8},
        {"START with flag bit 46", controlStart | (1ull << 46), &at9Start, sizeof(at9Start), 8},
        {"START with flag bit 12", controlStart | (1ull << 12), &at9Start, sizeof(at9Start), 8},
        {"no flags", 0, nullptr, 0, 8},
        {"16-byte sideband output", controlStart, &at9Start, sizeof(at9Start), 16},
        {"null sideband input", controlStart, nullptr, sizeof(at9Start), 8},
    };
    for (const auto& call : calls) RequireControlThrows(instance.Id(), call.flags, call.input, call.inputSize, call.outputSize, call.name);
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlDestroyed{"Control_DestroyedInstance_ReportsInvalidParameter", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAt9);
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
    std::int32_t result[2] = {-1, -1};
    RunControl(context.Id(), instance.Id(), controlStart, &at9Start, sizeof(at9Start), result);
    RequireEqual(result[0], 4, "control result");
}};

const Case controlAac{"Control_StartAac_RefusedAtStart", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecAac);
    const std::uint32_t aac[4] = {0, 0, 1, 0};
    RequireControlRefused(context.Id(), instance.Id(), controlStart, aac, sizeof(aac), "AAC start");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlStartMp3{"Control_StartMp3WithSkip_RestartsTotalsAndTrimsNextFrame", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    std::vector<std::int16_t> pcm(1152);
    const std::size_t pcmBytes = pcm.size() * sizeof(std::int16_t);
    DecodeSideband decoded{};
    RunDecode(context.Id(), instance.Id(), mp3Mono, sizeof(mp3Mono), pcm.data(), pcmBytes, decoded);
    RunDecode(context.Id(), instance.Id(), mp3Mono + 96, sizeof(mp3Mono) - 96, pcm.data(), pcmBytes, decoded);
    RequireEqual(decoded.result, 0, "second frame result");
    RequireEqual(decoded.totalDecodedSamples, std::uint64_t{2 * 1152}, "total after two frames");
    const GaplessDecode skip{0, 100, 0};
    std::int32_t result[2] = {-1, -1};
    RunControl(context.Id(), instance.Id(), controlStart, &skip, sizeof(skip), result);
    RunDecode(context.Id(), instance.Id(), mp3Mono, sizeof(mp3Mono), pcm.data(), pcmBytes, decoded);
    RequireEqual(result[0], 0, "control result");
    RequireEqual(decoded.result, 0, "decode result");
    RequireEqual(decoded.inputConsumed, 96, "input consumed");
    RequireEqual(decoded.outputWritten, (1152 - 100) * 2, "output written");
    RequireEqual(decoded.totalDecodedSamples, std::uint64_t{1152 - 100}, "total decoded samples");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlResetMp3{"Control_ResetMp3_KeepsGaplessSkipForNextFrame", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    std::vector<std::int16_t> pcm(1152);
    const std::size_t pcmBytes = pcm.size() * sizeof(std::int16_t);
    DecodeSideband decoded{};
    const GaplessDecode skip{0, 100, 0};
    std::int32_t result[2] = {-1, -1};
    RunControl(context.Id(), instance.Id(), controlStart, &skip, sizeof(skip), result);
    RunDecode(context.Id(), instance.Id(), mp3Mono, sizeof(mp3Mono), pcm.data(), pcmBytes, decoded);
    RequireEqual(result[0], 0, "start result");
    result[0] = -1;
    RunControl(context.Id(), instance.Id(), controlReset, nullptr, 0, result);
    RunDecode(context.Id(), instance.Id(), mp3Mono, sizeof(mp3Mono), pcm.data(), pcmBytes, decoded);
    RequireEqual(result[0], 0, "reset result");
    RequireEqual(decoded.result, 0, "decode result");
    RequireEqual(decoded.outputWritten, (1152 - 100) * 2, "output written");
    RequireEqual(decoded.totalDecodedSamples, std::uint64_t{1152 - 100}, "total decoded samples");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlGaplessWithoutReset{"Control_Mp3GaplessWithoutReset_ThrowsWithoutAppending", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    const GaplessDecode none{0, 0, 0};
    RequireControlThrows(instance.Id(), sidebandGaplessDecode, &none, sizeof(none), 8, "GAPLESS without RESET");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlResetGaplessMp3{"Control_ResetMp3WithNewGapless_AppliesNewSkip", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    std::vector<std::int16_t> pcm(1152);
    DecodeSideband decoded{};
    const GaplessDecode longer{5000, 200, 0};
    std::int32_t result[2] = {-1, -1};
    RunControl(context.Id(), instance.Id(), controlReset | sidebandGaplessDecode, &longer, sizeof(longer), result);
    RunDecode(context.Id(), instance.Id(), mp3Mono, sizeof(mp3Mono), pcm.data(), pcm.size() * sizeof(std::int16_t), decoded);
    RequireEqual(result[0], 0, "control result");
    RequireEqual(decoded.result, 0, "decode result");
    RequireEqual(decoded.outputWritten, (1152 - 200) * 2, "output written");
    RequireEqual(decoded.totalDecodedSamples, std::uint64_t{1152 - 200}, "total decoded samples");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlInitializeMp3{"Control_InitializeMp3WithParameters_RefusedAtStart", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecMp3);
    const std::uint32_t parameters[2] = {1, 0};
    RequireControlRefused(context.Id(), instance.Id(), controlInitialize, parameters, sizeof(parameters), "MP3 INITIALIZE with 8 parameter bytes");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlStartOpus{"Control_StartOpus_InitializesDecoder", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecOpus);
    const OpusControl start{{0, 0, 0}, 2, 48000, 0};
    std::int32_t result[2] = {-1, -1};
    RunControl(context.Id(), instance.Id(), controlStart, &start, sizeof(start), result);
    std::vector<std::int16_t> pcm(960 * 2);
    DecodeSideband decoded{};
    RunDecode(context.Id(), instance.Id(), opusStereo, sizeof(opusStereo), pcm.data(), pcm.size() * sizeof(std::int16_t), decoded);
    RequireEqual(result[0], 0, "control result");
    RequireEqual(decoded.result, 0, "decode result");
    RequireEqual(static_cast<std::size_t>(decoded.inputConsumed), 2 + OpusPacketBytes(0), "input consumed");
    RequireEqual(decoded.outputWritten, 960 * 2 * 2, "output written");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case controlStartOpusShort{"Control_StartOpusWithShortParameters_RefusedAtStart", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecOpus);
    const OpusControl start{{0, 0, 0}, 2, 48000, 0};
    RequireControlRefused(context.Id(), instance.Id(), controlStart, &start, sizeof(start) - sizeof(start.third), "Opus start with 8 parameter bytes");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case runAt9Ambisonic{"Run_At9StereoAmbisonicSuperframe_MatchesPerChannelMonoDecodes", [] {
    const AjmContext context;
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
        reference[channel] = DecodeAt9Superframe(context.Id(), mono, superframe, 1);
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
    RequireEqual(superframe.size(), std::size_t{768}, "superframe size");
    const auto decoded = DecodeAt9Superframe(context.Id(), stereoAmbisonic, superframe, 2);
    bool audible = false;
    for (std::size_t sample = 0; sample < 1024; ++sample) {
        RequireEqual(decoded[sample * 2], reference[0][sample], "channel 0 sample " + std::to_string(sample));
        RequireEqual(decoded[sample * 2 + 1], reference[1][sample], "channel 1 sample " + std::to_string(sample));
        audible = audible || reference[0][sample] != 0 || reference[1][sample] != 0;
    }
    Require(audible, "the reference decodes are not silent");
}};

const Case resampleOpusReference{"Resample_OpusWithoutRatio_DecodesAtLeastFourPackets", [] {
    const AjmContext context;
    const auto reference = StreamFreshOpus(context.Id(), 960, 0);
    Require(reference.pcm.size() / 2 >= 960 * 4, "decoded " + std::to_string(reference.pcm.size() / 2) + " frames");
}};

const Case resampleOpusUnity{"Resample_OpusUnityRatio_PassesDecodedPcmThrough", [] {
    const AjmContext context;
    const auto reference = StreamFreshOpus(context.Id(), 960, 0);
    const auto passthrough = StreamFreshOpus(context.Id(), 512, 1.0f);
    Require(passthrough.pcm == reference.pcm, "unity-ratio PCM equals the reference PCM");
    RequireEqual(passthrough.afterFirstJob.result, 0, "info result after the first job");
    RequireEqual(passthrough.afterFirstJob.internalResult, 0, "info internal result after the first job");
    RequireEqual(passthrough.afterFirstJob.ratio, 1.0f, "ratio after the first job");
    RequireEqual(passthrough.afterFirstJob.samples, 960 - 512, "held samples after the first job");
    for (const auto word : passthrough.afterFirstJob.reserved) RequireEqual(word, 0u, "reserved word");
    RequireEqual(passthrough.atEnd.samples, 0, "held samples at the end");
    RequireEqual(passthrough.shortJobs, std::size_t{0}, "short jobs");
}};

const Case resampleOpusDouble{"Resample_OpusDoubleRatio_KeepsEverySecondFrame", [] {
    const AjmContext context;
    const auto reference = StreamFreshOpus(context.Id(), 960, 0);
    const auto decimated = StreamFreshOpus(context.Id(), 512, 2.0f);
    RequireDecimated(reference.pcm, decimated.pcm, 2, 2);
    RequireEqual(decimated.afterFirstJob.ratio, 2.0f, "ratio after the first job");
    RequireEqual(decimated.afterFirstJob.samples, 2, "held samples after the first job");
    RequireEqual(decimated.atEnd.result, 0, "info result at the end");
    Require(decimated.atEnd.samples >= 0 && decimated.atEnd.samples <= 2, "held samples at the end " + std::to_string(decimated.atEnd.samples) + " lie in [0, 2]");
}};

const Case resampleOpusHalf{"Resample_OpusHalfRatio_InsertsCubicMidpoints", [] {
    const AjmContext context;
    const auto reference = StreamFreshOpus(context.Id(), 960, 0);
    const auto interpolated = StreamFreshOpus(context.Id(), 512, 0.5f);
    RequireInterpolated(reference.pcm, interpolated.pcm, 2);
    RequireEqual(interpolated.shortJobs, std::size_t{0}, "short jobs");
    RequireEqual(interpolated.afterFirstJob.ratio, 0.5f, "ratio after the first job");
    RequireEqual(interpolated.afterFirstJob.samples, 960 - 256, "held samples after the first job");
    Require(interpolated.atEnd.samples >= 0 && interpolated.atEnd.samples <= 2, "held samples at the end " + std::to_string(interpolated.atEnd.samples) + " lie in [0, 2]");
}};

const Case resampleOpusFractional{"Resample_OpusFractionalRatio_StretchesLength", [] {
    const AjmContext context;
    const auto reference = StreamFreshOpus(context.Id(), 960, 0);
    const auto pitched = StreamFreshOpus(context.Id(), 512, 0.890899f);
    const double expected = static_cast<double>(reference.pcm.size() / 2 - 2) / 0.890899;
    Require(std::fabs(static_cast<double>(pitched.pcm.size() / 2) - expected) <= 2.0,
        "resampled " + std::to_string(pitched.pcm.size() / 2) + " frames, expected about " + std::to_string(expected));
}};

const Case resampleInvalidRatio{"SetResampleParameters_InvalidRatio_FailsWithoutAppending", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecOpus);
    InitializeOpus(context.Id(), instance.Id());
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info = InitializeBatch(batch);
    std::int64_t result[2] = {-1, -1};
    for (const float bad : {0.0f, -1.0f, std::nanf(""), INFINITY}) {
        RequireEqual(sceAjmBatchJobSetResampleParameters(&info, instance.Id(), bad, 1, result), invalidParameter, "ratio " + std::to_string(bad));
    }
    RequireEqual(info.offset, std::uint64_t{0}, "batch offset after the refused jobs");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case clearContextResample{"ClearContext_ResamplingOpus_DropsHeldSamplesAndRepeatsOutput", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecOpus);
    InitializeOpus(context.Id(), instance.Id());
    static_cast<void>(Stream(context.Id(), instance.Id(), opusStereo, sizeof(opusStereo), 2, 512, 0.5f));
    const std::size_t firstPacket = 2 + OpusPacketBytes(0);
    std::vector<std::uint8_t> batch(4096);
    const auto clear = [&](const char* when) {
        std::int64_t result[2] = {-1, -1};
        AjmBatchInfo info = InitializeBatch(batch);
        RequireEqual(sceAjmBatchJobClearContext(&info, instance.Id(), result), 0, "append the clear context job");
        Submit(context.Id(), info);
        RequireEqual(result[0], std::int64_t{0}, std::string(when) + ": clear context result");
        RequireEqual(GetResampleInfo(context.Id(), instance.Id()).samples, 0, std::string(when) + ": held samples after clearing");
    };
    clear("first clear");
    const auto before = Stream(context.Id(), instance.Id(), opusStereo, firstPacket, 2, 512, 0.5f);
    RequireEqual(before.pcm.size() / 2, std::size_t{2 * 960 - 4}, "frames resampled from the first packet");
    clear("second clear");
    const auto after = Stream(context.Id(), instance.Id(), opusStereo, firstPacket, 2, 512, 0.5f);
    Require(after.pcm == before.pcm, "the first packet resamples identically after clearing");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case resampleOpusFirstPacket{"Resample_OpusUnityRatioOnFirstPacket_DecodesOneFrame", [] {
    const AjmContext context;
    AjmInstance instance(context.Id(), codecOpus);
    InitializeOpus(context.Id(), instance.Id());
    const auto head = Stream(context.Id(), instance.Id(), opusStereo, 2 + OpusPacketBytes(0), 2, 512, 1.0f);
    Require(head.jobs >= 1, "at least one job ran");
    RequireEqual(head.pcm.size(), std::size_t{960 * 2}, "samples decoded from the first packet");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case resampleAfterPartialDecode{"Resample_EnabledAfterPartialDecode_ResamplesHeldSamples", [] {
    const AjmContext context;
    const auto reference = StreamFreshOpus(context.Id(), 960, 0);
    AjmInstance instance(context.Id(), codecOpus);
    InitializeOpus(context.Id(), instance.Id());
    std::vector<std::int16_t> pcm(512 * 2);
    DecodeSideband first{};
    RunDecode(context.Id(), instance.Id(), opusStereo, sizeof(opusStereo), pcm.data(), pcm.size() * sizeof(std::int16_t), first);
    RequireEqual(first.result, 0, "first job result");
    RequireEqual(first.outputWritten, 512 * 2 * 2, "first job output written");
    RequireEqual(GetResampleInfo(context.Id(), instance.Id()).samples, 960 - 512, "held samples after the first job");
    const auto tail = Stream(context.Id(), instance.Id(), opusStereo + first.inputConsumed, sizeof(opusStereo) - static_cast<std::size_t>(first.inputConsumed), 2, 512, 2.0f);
    const std::vector<std::int16_t> later(reference.pcm.begin() + 512 * 2, reference.pcm.end());
    RequireDecimated(later, tail.pcm, 2, 2);
    Require(std::equal(pcm.begin(), pcm.end(), reference.pcm.begin()), "the first job matches the reference head");
    RequireEqual(instance.Destroy(), 0, "destroy the instance");
}};

const Case resampleInfoUnknown{"GetResampleInfo_UnknownInstance_ReportsInvalidParameter", [] {
    const AjmContext context;
    const auto unknown = GetResampleInfo(context.Id(), 0x3FFF);
    RequireEqual(unknown.result, 4, "result for instance 0x3FFF");
}};

const Case resampleMp3Reference{"Resample_Mp3WithoutRatio_DecodesSixFrames", [] {
    const AjmContext context;
    const auto reference = StreamFreshMp3(context.Id(), 1152, 0);
    RequireEqual(reference.pcm.size(), std::size_t{1152 * 6}, "decoded samples");
}};

const Case resampleMp3Double{"Resample_Mp3DoubleRatio_KeepsEverySecondFrame", [] {
    const AjmContext context;
    const auto reference = StreamFreshMp3(context.Id(), 1152, 0);
    const auto decimated = StreamFreshMp3(context.Id(), 512, 2.0f);
    RequireDecimated(reference.pcm, decimated.pcm, 1, 2);
}};

const Case resampleMp3Half{"Resample_Mp3HalfRatio_InsertsCubicMidpoints", [] {
    const AjmContext context;
    const auto reference = StreamFreshMp3(context.Id(), 1152, 0);
    const auto interpolated = StreamFreshMp3(context.Id(), 512, 0.5f);
    RequireInterpolated(reference.pcm, interpolated.pcm, 1);
    RequireEqual(interpolated.shortJobs, std::size_t{0}, "short jobs");
}};

const Case resampleAt9Reference{"Resample_At9WithoutRatio_DecodesThreeSuperframes", [] {
    const AjmContext context;
    const auto reference = StreamFreshAt9(context.Id(), 256, 0);
    RequireEqual(reference.pcm.size(), std::size_t{3 * 4 * 256}, "decoded samples");
}};

const Case resampleAt9Double{"Resample_At9DoubleRatio_KeepsEverySecondFrame", [] {
    const AjmContext context;
    const auto reference = StreamFreshAt9(context.Id(), 256, 0);
    const auto decimated = StreamFreshAt9(context.Id(), 512, 2.0f);
    RequireDecimated(reference.pcm, decimated.pcm, 1, 2);
    RequireEqual(decimated.atEnd.samples, 2, "held samples at the end");
}};

const Case resampleAt9Half{"Resample_At9HalfRatio_InsertsCubicMidpoints", [] {
    const AjmContext context;
    const auto reference = StreamFreshAt9(context.Id(), 256, 0);
    const auto interpolated = StreamFreshAt9(context.Id(), 256, 0.5f);
    RequireInterpolated(reference.pcm, interpolated.pcm, 1);
    RequireEqual(interpolated.shortJobs, std::size_t{0}, "short jobs");
    RequireEqual(interpolated.afterFirstJob.samples, 256 - 256 / 2, "held samples after the first job");
    RequireEqual(interpolated.atEnd.samples, 2, "held samples at the end");
}};

} // namespace
