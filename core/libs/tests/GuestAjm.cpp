#include "prx/libSceAjm.native/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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

const std::uint8_t AAC_STEREO[] = {
    0xFF, 0xF1, 0x4C, 0x80, 0x15, 0x1F, 0xFC, 0x21, 0x28, 0x08, 0x03, 0x00, 0x00, 0x05, 0x25, 0x24, 0x9D, 0xA6, 0x20, 0xE8, 0x87, 0x32, 0xAE, 0x49,
    0xA7, 0x13, 0x8A, 0x5A, 0x44, 0x74, 0x44, 0x93, 0xF1, 0x70, 0x36, 0x6B, 0x1D, 0x1B, 0x5A, 0xB4, 0xD5, 0xF2, 0x8E, 0xD7, 0xBF, 0x7D, 0x45, 0x62,
    0x0E, 0xC7, 0x8B, 0x8F, 0x6E, 0xAF, 0xE6, 0x65, 0xCD, 0x36, 0x5E, 0x35, 0x55, 0xF2, 0xF9, 0x65, 0xCB, 0xF2, 0xE3, 0xC7, 0x8F, 0xCB, 0x2C, 0xD9,
    0x62, 0xE3, 0x57, 0x1F, 0x96, 0x59, 0xA2, 0x8A, 0x2A, 0xAA, 0x88, 0xB8, 0xF1, 0x8B, 0x2E, 0x58, 0xA2, 0x8A, 0x75, 0xAF, 0xA6, 0x40, 0xE8, 0x87,
    0x80, 0x35, 0xBA, 0xA9, 0xCF, 0x75, 0x7E, 0xDA, 0xD5, 0x3F, 0xAE, 0x49, 0x3F, 0x7B, 0xB1, 0x4C, 0x8C, 0xED, 0x5B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B,
    0x3C, 0x23, 0xB3, 0xB3, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3A, 0x56, 0x84, 0x84, 0x85, 0x1B, 0xFC, 0xBF, 0xFB, 0x47, 0xF7, 0xDF, 0x4D, 0x6C,
    0xEC, 0xE9, 0x23, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3A, 0x3A, 0x6F, 0x40, 0xC6, 0xCD, 0x9B, 0x96, 0x59, 0x78,
    0xFF, 0xF1, 0x4C, 0x80, 0x22, 0x3F, 0xFC, 0x21, 0x4A, 0x6C, 0x80, 0x00, 0x00, 0x16, 0x00, 0xD0, 0xD9, 0x2B, 0xB2, 0x48, 0xF5, 0x64, 0x7A, 0xB2,
    0x5D, 0x3B, 0xFD, 0x7C, 0xF1, 0xA7, 0x16, 0xAF, 0x5F, 0xF1, 0xFE, 0xBF, 0xF7, 0xF8, 0xEB, 0x5C, 0x6B, 0x5A, 0xFD, 0x3F, 0xFE, 0xA7, 0xFD, 0x7E,
    0xBA, 0xE3, 0xAB, 0xD6, 0x9F, 0xFF, 0x87, 0xF9, 0xFF, 0x4F, 0x6E, 0x3A, 0xE3, 0x5C, 0x04, 0x5D, 0xF4, 0xC6, 0xFE, 0x94, 0x51, 0x43, 0xF1, 0xEC,
    0x7C, 0xF9, 0x28, 0xCD, 0x03, 0x51, 0xE7, 0x1F, 0xCF, 0x50, 0x7F, 0x3D, 0x41, 0xFC, 0xFC, 0xE0, 0xFE, 0x7F, 0xCF, 0xF9, 0x81, 0xE7, 0xE7, 0x51,
    0x98, 0xB2, 0xA9, 0x5E, 0xAC, 0x39, 0xB7, 0xBB, 0xAF, 0xA5, 0x15, 0xFD, 0x4D, 0xFF, 0x93, 0xF0, 0xFE, 0x5E, 0xA3, 0x7D, 0xF5, 0xCC, 0xDF, 0x7D,
    0x9B, 0x0D, 0x5A, 0xB5, 0xEB, 0xD6, 0x37, 0xEF, 0xDF, 0xAF, 0x5E, 0xB1, 0xBF, 0x7E, 0xFF, 0x0D, 0xAC, 0x96, 0xB2, 0x2E, 0x77, 0x99, 0x14, 0x51,
    0x45, 0x14, 0x44, 0xF3, 0x1E, 0x8A, 0x59, 0xEB, 0x65, 0x91, 0xE7, 0xCD, 0x91, 0x7E, 0x45, 0x73, 0xB2, 0x4F, 0xD2, 0x47, 0xF5, 0xFF, 0xC6, 0x7C,
    0x76, 0xF9, 0xE3, 0x5A, 0xFF, 0xFF, 0x9F, 0xFD, 0x7F, 0xF1, 0xF1, 0xFF, 0x3E, 0x7D, 0xB8, 0xD5, 0x79, 0xFF, 0xFB, 0x1F, 0xDB, 0xFD, 0xEF, 0xFD,
    0xBD, 0xBD, 0x55, 0x79, 0xFF, 0xFB, 0xBF, 0xA7, 0xFB, 0xFD, 0xDF, 0xF5, 0xE3, 0x5E, 0xC3, 0xB3, 0xDA, 0x95, 0xC3, 0xCE, 0xBD, 0xB5, 0xDB, 0x12,
    0xED, 0x5F, 0xB7, 0x49, 0x4A, 0x56, 0xD1, 0x26, 0xBE, 0xB1, 0x76, 0xBD, 0x67, 0xB9, 0xAF, 0x59, 0xFC, 0xEF, 0xF0, 0xD7, 0xAC, 0x7F, 0x3D, 0xE8,
    0xA7, 0xD4, 0xCE, 0xAB, 0x51, 0xF7, 0x40, 0x4A, 0x27, 0x3C, 0xC8, 0xF8, 0x07, 0xB1, 0xB3, 0x61, 0xA9, 0xEB, 0x5F, 0xC1, 0x4C, 0x66, 0x09, 0x09,
    0x09, 0x09, 0x09, 0x09, 0x09, 0x06, 0x06, 0x06, 0xE0, 0xFF, 0xF1, 0x4C, 0x80, 0x0E, 0xDF, 0xFC, 0x21, 0x78, 0xCF, 0xA2, 0x63, 0xF0, 0x00, 0x8D,
    0x2B, 0xEB, 0x1C, 0xE3, 0x9D, 0x7C, 0xE2, 0x7E, 0xDF, 0x3D, 0x9D, 0x6B, 0xFE, 0xBF, 0x1A, 0xD4, 0xB7, 0xB5, 0xD6, 0x97, 0x2E, 0x45, 0x87, 0xFE,
    0x3E, 0x3E, 0x1B, 0x75, 0xDF, 0x7F, 0x70, 0x3E, 0x07, 0xC3, 0x1D, 0x64, 0x3D, 0xDE, 0x1D, 0xDB, 0xFC, 0x05, 0xE0, 0x7C, 0x66, 0x1E, 0xFE, 0xFA,
    0x0F, 0xF1, 0xF1, 0x98, 0x7B, 0xFB, 0xE8, 0x3F, 0xC7, 0xC4, 0x68, 0xDF, 0x58, 0xE7, 0x17, 0xBD, 0xD5, 0x77, 0xFF, 0x6E, 0xCF, 0xC7, 0xE2, 0xFA,
    0xF8, 0xD6, 0xA5, 0xBE, 0x6D, 0xA5, 0xCB, 0x92, 0x40, 0x18, 0x5E, 0x6F, 0xE1, 0xD6, 0x8C, 0xFF, 0xE3, 0xE3, 0x58, 0x42, 0xB0, 0x61, 0x97, 0xF0,
    0xFE, 0x1E, 0xA3, 0xC4, 0x00, 0x01, 0xC0, 0xFF, 0xF1, 0x4C, 0x80, 0x0F, 0xFF, 0xFC, 0x21, 0x18, 0xC8, 0x00, 0x00, 0x10, 0x00, 0x7C, 0x2B, 0xEB,
    0x1E, 0xDD, 0xCC, 0x93, 0x6C, 0xCE, 0xFF, 0x4F, 0xEB, 0xFD, 0x3D, 0xD5, 0x7F, 0x1F, 0xF9, 0xFF, 0xBF, 0xDD, 0xEF, 0x09, 0x82, 0xD4, 0x13, 0x04,
    0x6B, 0x35, 0x7D, 0x2B, 0xAF, 0xD6, 0x79, 0xCF, 0x38, 0x38, 0x58, 0x99, 0xFF, 0x86, 0xF9, 0xA8, 0xE9, 0xEC, 0xC3, 0xEC, 0x23, 0x00, 0xC0, 0x50,
    0x60, 0x1E, 0x7A, 0xFF, 0x38, 0x92, 0xBE, 0xB1, 0xDF, 0xAF, 0x9F, 0xB7, 0xCC, 0xDF, 0x83, 0xE2, 0x7F, 0xE5, 0xFE, 0x7C, 0xDD, 0xDA, 0xD1, 0xD3,
    0x28, 0x4C, 0xB6, 0x41, 0xCE, 0x94, 0xA7, 0x09, 0x2B, 0xD3, 0xBF, 0xF7, 0xCB, 0x46, 0x47, 0x48, 0x36, 0x1E, 0x1F, 0x4F, 0xA7, 0xD2, 0x8D, 0x74,
    0x6B, 0xFA, 0x6B, 0xE7, 0xDD, 0xCE, 0x6D, 0xCF, 0x5E, 0xB5, 0x87, 0x82, 0x4C, 0xCE, 0xFF, 0xF1, 0x4C, 0x80, 0x1A, 0x5F, 0xFC, 0x21, 0x18, 0xC8,
    0x00, 0x3C, 0x1E, 0x00, 0x96, 0x47, 0x09, 0x91, 0xEA, 0xCA, 0x55, 0x29, 0x4A, 0xAA, 0xCA, 0xFE, 0x3C, 0x57, 0xE3, 0xCF, 0xB4, 0x4C, 0x92, 0x44,
    0x01, 0xB9, 0x65, 0x96, 0x59, 0x65, 0x96, 0x59, 0x65, 0x96, 0x59, 0x65, 0x46, 0x19, 0x78, 0x02, 0x49, 0x7F, 0x0C, 0x78, 0x01, 0x87, 0x9C, 0x30,
    0x30, 0x32, 0x60, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0xC9, 0xA6, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x98, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x32, 0x9D, 0x2C, 0x24, 0xF4, 0xB7, 0xBE,
    0x7C, 0x6E, 0x4E, 0x3E, 0x35, 0x27, 0xB5, 0x65, 0x44, 0x44, 0x4C, 0x80, 0x81, 0x6D, 0xD1, 0x72, 0xA2, 0xCC, 0x28, 0x29, 0x77, 0x83, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x00, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC3, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x36, 0x44, 0x00, 0x00, 0xE0,
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

void TestOpus(std::uint32_t context, std::uint32_t mappingFamily = 0) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 24, 0, &instance) == 0);
    const std::uint32_t parameters[3] = {2, 48000, mappingFamily};
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

void TestOpusSurround(std::uint32_t context, std::uint32_t channels, std::size_t activeStream) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 24, 0, &instance) == 0);
    const std::uint32_t parameters[3] = {channels, 48000, 1};
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int64_t initResult[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobInitialize(&info, instance, parameters, sizeof(parameters), initResult) == 0);
    std::uint32_t id = 0;
    AjmBatchError error{};
    Require(sceAjmBatchStart(context, &info, 0, &error, &id) == 0 && sceAjmBatchWait(context, id, 0, &error) == 0);
    Require(initResult[0] == 0);
    const std::size_t streamCount = channels == 3 || channels == 4 ? 2 : channels == 5 ? 3 : channels == 8 ? 5 : 4;
    const std::size_t coupledCount = channels == 3 ? 1 : channels >= 7 ? 3 : 2;
    std::size_t offset = 0;
    for (std::size_t packetIndex = 0; packetIndex < 3; ++packetIndex) {
        const std::size_t bytes = OPUS_STEREO[offset] | (std::size_t{OPUS_STEREO[offset + 1]} << 8u);
        Require((OPUS_STEREO[offset + 2] & 3u) == 0 && bytes < 252);
        std::vector<std::uint8_t> packet{0, 0};
        for (std::size_t stream = 0; stream < streamCount; ++stream) {
            if (stream == activeStream) {
                packet.push_back(OPUS_STEREO[offset + 2]);
                if (stream + 1 < streamCount) packet.push_back(static_cast<std::uint8_t>(bytes - 1));
                packet.insert(packet.end(), OPUS_STEREO + offset + 3, OPUS_STEREO + offset + 2 + bytes);
            } else {
                packet.push_back(stream < coupledCount ? 0xfc : 0xf8);
                if (stream + 1 < streamCount) packet.push_back(2);
                packet.push_back(0xff);
                packet.push_back(0xfe);
            }
        }
        const std::size_t payload = packet.size() - 2;
        packet[0] = static_cast<std::uint8_t>(payload);
        packet[1] = static_cast<std::uint8_t>(payload >> 8u);
        std::vector<std::int16_t> pcm(512 * channels, -12345);
        DecodeSideband first{};
        RunDecode(context, instance, packet.data(), packet.size(), pcm.data(), pcm.size() * sizeof(std::int16_t), first);
        Require(first.result == 0 && static_cast<std::size_t>(first.inputConsumed) == packet.size() && first.outputWritten == static_cast<std::int32_t>(pcm.size() * sizeof(std::int16_t)));
        std::vector<std::int16_t> rest(448 * channels, -12345);
        DecodeSideband remaining{};
        RunDecode(context, instance, packet.data() + packet.size(), 0, rest.data(), rest.size() * sizeof(std::int16_t), remaining);
        Require(remaining.result == 0 && remaining.inputConsumed == 0 && remaining.outputWritten == static_cast<std::int32_t>(rest.size() * sizeof(std::int16_t)));
        Require(remaining.totalDecodedSamples == (packetIndex + 1) * 960);
        pcm.insert(pcm.end(), rest.begin(), rest.end());
        for (std::size_t channel = 0; channel < channels; ++channel) {
            int peak = 0;
            for (std::size_t sample = channel; sample < pcm.size(); sample += channels) peak = std::max(peak, std::abs(static_cast<int>(pcm[sample])));
            const std::size_t activeChannel = activeStream == 0 ? 0 : channels == 4 ? 2 : channels == 5 ? 3 : channels == 6 ? 4 : channels == 7 ? activeStream == 1 ? 5 : 2 : activeStream == 1 ? 6 : 4;
            const std::size_t secondChannel = channels == 7 && activeStream == 2 ? 4 : activeChannel + 1;
            const bool active = channel == activeChannel || channel == secondChannel;
            Require(active ? packetIndex == 0 || peak > 900 : peak <= 2);
        }
        offset += 2 + bytes;
    }
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void TestOpusFamilyZeroChannels(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 24, 0, &instance) == 0);
    const std::uint32_t parameters[3] = {4, 48000, 0};
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int32_t result[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobInitialize(&info, instance, parameters, sizeof(parameters), result) == 0);
    std::uint32_t id = 0;
    AjmBatchError error{};
    Require(sceAjmBatchStart(context, &info, 0, &error, &id) == 0 && sceAjmBatchWait(context, id, 0, &error) == 0);
    Require(result[0] == 4 && result[1] == 0);
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void TestOpusInputBeyondStatedData(std::uint32_t context, bool multipleFrames) {
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const std::size_t pageSize = system.dwPageSize;
    auto* pages = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr);
    DWORD oldProtection = 0;
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &oldProtection) != 0);
#else
    const long hostPageSize = sysconf(_SC_PAGESIZE);
    Require(hostPageSize > 0);
    const std::size_t pageSize = static_cast<std::size_t>(hostPageSize);
    auto* pages = static_cast<std::uint8_t*>(mmap(nullptr, 2 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(pages != MAP_FAILED);
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0);
#endif
    const std::size_t packetBytes = 2 + (OPUS_STEREO[0] | (std::size_t{OPUS_STEREO[1]} << 8u));
    auto* packet = pages + pageSize - packetBytes;
    std::memcpy(packet, OPUS_STEREO, packetBytes);

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
    if (multipleFrames) {
        constexpr std::uint64_t runMultipleFrames = 1ull << 12;
        constexpr std::uint64_t sidebandStream = 1ull << 47;
        struct {
            DecodeSideband stream;
            std::uint32_t frames;
            std::uint32_t reserved;
        } decoded{{-1, -1, 0, 0, 0}, 0, 0};
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        Require(sceAjmBatchJobRun(&info, instance, sidebandStream | runMultipleFrames, packet, 0xFFFFFE74u, pcm.data(), pcm.size() * sizeof(std::int16_t), &decoded, sizeof(decoded)) == 0);
        Require(sceAjmBatchStart(context, &info, 0, &error, &id) == 0 && sceAjmBatchWait(context, id, 0, &error) == 0);
        Require(decoded.stream.result == 0 && static_cast<std::size_t>(decoded.stream.inputConsumed) == packetBytes && decoded.stream.outputWritten == 960 * 2 * 2 && decoded.frames == 1);
    } else {
        DecodeSideband sideband{};
        RunDecode(context, instance, packet, 0xFFFFFE74u, pcm.data(), pcm.size() * sizeof(std::int16_t), sideband);
        Require(sideband.result == 0 && static_cast<std::size_t>(sideband.inputConsumed) == packetBytes && sideband.outputWritten == 960 * 2 * 2);
    }
    Require(sceAjmInstanceDestroy(context, instance) == 0);
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 2 * pageSize) == 0);
#endif
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
    for (const std::uint8_t layer : {0u, 2u, 3u}) {
        const std::uint8_t header[4] = {0xFF, static_cast<std::uint8_t>(0xF9u | (layer << 1u)), 0x90, 0x04};
        if (layer == 0) {
            std::memset(&frame, 0xA5, sizeof(frame));
            const auto previous = frame;
            Require(sceAjmDecMp3ParseFrame(header, sizeof(header), 0, &frame) == invalidParameter);
            Require(std::memcmp(&frame, &previous, sizeof(frame)) == 0);
        } else {
            Require(ParsesTo(header, sizeof(header), 417, 2, 1152, 128000, 44100));
        }
        std::memset(&frame, 0xA5, sizeof(frame));
        const auto previous = frame;
        Require(sceAjmDecMp3ParseFrame(header, sizeof(header), 1, &frame) == invalidParameter);
        Require(std::memcmp(&frame, &previous, sizeof(frame)) == 0);
    }
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

std::vector<std::uint8_t> ProtectedMp3Frame(std::vector<std::uint8_t> frame, bool tagAfterCrc) {
    frame[1] &= 0xFE;
    if (tagAfterCrc) {
        const auto size = frame.size();
        frame.insert(frame.begin() + 4, {0xA5, 0x5A});
        frame.resize(size);
    } else {
        frame[4] = 0xA5;
        frame[5] = 0x5A;
    }
    return frame;
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
    const auto marker = std::find(fgh.begin() + 36, fgh.end(), 0xB4u);
    Require(marker != fgh.end() && static_cast<std::size_t>(fgh.end() - marker) >= 10);
    std::vector<std::uint8_t> misleadingFgh(417, 0);
    std::copy_n(fgh.begin(), 4, misleadingFgh.begin());
    const auto markerOffset = static_cast<std::size_t>(marker - fgh.begin());
    std::copy_n(fgh.begin() + static_cast<std::ptrdiff_t>(markerOffset), 10, misleadingFgh.begin() + 36);
    misleadingFgh[45] ^= 0x01u;
    std::copy_n(fgh.begin() + static_cast<std::ptrdiff_t>(markerOffset), 10, misleadingFgh.begin() + 48);
    Require(OflParsesTo(misleadingFgh, 417, 0, 0, 0, 0));
    auto vbri = Mp3Frame(VBRI_FRAME, sizeof(VBRI_FRAME), 417);
    Require(OflParsesTo(vbri, 417, 0, 576, 0, 2));
    vbri.insert(vbri.end(), fgh.begin(), fgh.end());
    Require(OflParsesTo(vbri, 834, 0, 576 + 1105, 441000, 4));
    Require(OflParsesTo(vbri, 480, 0, 576, 0, 2));

    for (const bool tagAfterCrc : {false, true}) {
        const auto protectedStereo = ProtectedMp3Frame(stereo, tagAfterCrc);
        Require(OflParsesTo(protectedStereo, 417, 10, 1152 + 576 + 529, 10000, 1));
        const auto protectedMono = ProtectedMp3Frame(mono, tagAfterCrc);
        Require(OflParsesTo(protectedMono, 576, 10, 1152 + 576 + 529, 10000, 1));
        const auto protectedMpeg2 = ProtectedMp3Frame(mpeg2, tagAfterCrc);
        Require(OflParsesTo(protectedMpeg2, 208, 20, 576 + 576 + 529, 10000, 1));
    }
    auto protectedNonzeroSideInfo = ProtectedMp3Frame(stereo, true);
    protectedNonzeroSideInfo[8] = 0x00;
    protectedNonzeroSideInfo[9] = 0x10;
    std::move_backward(protectedNonzeroSideInfo.begin() + 38, protectedNonzeroSideInfo.end() - 2, protectedNonzeroSideInfo.end());
    protectedNonzeroSideInfo[38] = 0;
    protectedNonzeroSideInfo[39] = 0;
    Require(OflParsesTo(protectedNonzeroSideInfo, 417, 10, 1152 + 576 + 529, 10000, 1));
    const auto protectedFgh = ProtectedMp3Frame(fgh, true);
    Require(OflParsesTo(protectedFgh, 417, 0, 1105, 441000, 3));
    auto monoVbri = Mp3Frame(VBRI_FRAME, sizeof(VBRI_FRAME), 417);
    monoVbri[3] = 0xC4;
    Require(OflParsesTo(monoVbri, 417, 0, 576, 0, 2));
    for (const std::uint8_t version : {0xF3u, 0xE3u}) {
        for (const std::uint8_t mode : {0x04u, 0xC4u}) {
            auto otherVbri = Mp3Frame(VBRI_FRAME, sizeof(VBRI_FRAME), version == 0xF3 ? 208 : 417);
            otherVbri[1] = version;
            otherVbri[2] = 0x80;
            otherVbri[3] = mode;
            Require(OflParsesTo(otherVbri, static_cast<std::uint32_t>(otherVbri.size()), 0, 576, 0, 2));
        }
    }
    const auto protectedVbri = ProtectedMp3Frame(monoVbri, false);
    Require(OflParsesTo(protectedVbri, 417, 0, 576, 0, 2));
    auto monoVbriAndFgh = monoVbri;
    monoVbriAndFgh.insert(monoVbriAndFgh.end(), protectedFgh.begin(), protectedFgh.end());
    Require(OflParsesTo(monoVbriAndFgh, 834, 0, 576 + 1105, 441000, 4));
}

void TestMp3MetadataBounds() {
    const auto stereo = Mp3Frame(LAME_MPEG1_STEREO_VBR, sizeof(LAME_MPEG1_STEREO_VBR), 417);
    const auto protectedStereo = ProtectedMp3Frame(stereo, true);
    const auto protectedLame = ProtectedMp3Frame(stereo, false);
    auto vbri = Mp3Frame(VBRI_FRAME, sizeof(VBRI_FRAME), 417);
    vbri[3] = 0xC4;
    for (const auto& complete : {stereo, protectedStereo, protectedLame, vbri}) {
        for (std::size_t length = 0; length <= complete.size(); ++length) {
            const std::vector<std::uint8_t> prefix(complete.begin(), complete.begin() + length);
            AjmDecMp3ParseFrame frame{};
            const int result = sceAjmDecMp3ParseFrame(prefix.data(), static_cast<std::uint32_t>(prefix.size()), 1, &frame);
            Require(result == (length < 4 ? static_cast<int>(0x80930005) : 0));
            if (length >= 4) Require(frame.frame_size == 417);
        }
    }
    std::vector<std::uint8_t> chain;
    for (std::size_t i = 0; i < 4096; ++i) chain.insert(chain.end(), vbri.begin(), vbri.end());
    Require(OflParsesTo(chain, static_cast<std::uint32_t>(chain.size()), 0, 576, 0, 2));
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

void AppendOpusLength(std::vector<std::uint8_t>& out, std::size_t length) {
    if (length < 252) {
        out.push_back(static_cast<std::uint8_t>(length));
        return;
    }
    out.push_back(static_cast<std::uint8_t>(252 + (length - 252) % 4));
    out.push_back(static_cast<std::uint8_t>((length - 252) / 4));
}

std::vector<std::uint8_t> SelfDelimited(const std::uint8_t* packet, std::size_t bytes) {
    const std::uint32_t code = packet[0] & 3u;
    Require(code <= 1);
    std::vector<std::uint8_t> out{packet[0]};
    AppendOpusLength(out, code == 0 ? bytes - 1 : (bytes - 1) / 2);
    out.insert(out.end(), packet + 1, packet + bytes);
    return out;
}

void TestOpusMultistreamFamily255(std::uint32_t context) {
    const bool stereo = (OPUS_STEREO[2] & 0x04u) != 0;
    const std::uint32_t channels = stereo ? 4 : 2;
    std::uint32_t mono = 0;
    std::uint32_t multi = 0;
    const std::uint32_t parameters[2][3] = {{stereo ? 2u : 1u, 48000, 0}, {channels, 48000, 255}};
    std::uint32_t* instances[2] = {&mono, &multi};
    for (std::size_t index = 0; index < 2; ++index) {
        Require(sceAjmInstanceCreate(context, 24, 0, instances[index]) == 0);
        std::vector<std::uint8_t> batch(4096);
        AjmBatchInfo info{};
        std::int64_t initResult[2] = {-1, -1};
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        Require(sceAjmBatchJobInitialize(&info, *instances[index], parameters[index], sizeof(parameters[index]), initResult) == 0);
        Submit(context, info);
        Require(initResult[0] == 0);
    }
    const std::uint32_t streamChannels = channels / 2;
    std::size_t offset = 0;
    for (int packet = 0; packet < 3; ++packet) {
        const std::size_t bytes = OPUS_STEREO[offset] | (std::size_t{OPUS_STEREO[offset + 1]} << 8u);
        const std::uint8_t* data = OPUS_STEREO + offset + 2;
        std::vector<std::uint8_t> joined = SelfDelimited(data, bytes);
        joined.insert(joined.end(), data, data + bytes);
        std::vector<std::uint8_t> framed{static_cast<std::uint8_t>(joined.size()), static_cast<std::uint8_t>(joined.size() >> 8u)};
        framed.insert(framed.end(), joined.begin(), joined.end());
        std::vector<std::int16_t> single(960 * streamChannels, -12345);
        std::vector<std::int16_t> both(960 * channels, -12345);
        DecodeSideband sideband{};
        RunDecode(context, mono, OPUS_STEREO + offset, 2 + bytes, single.data(), single.size() * sizeof(std::int16_t), sideband);
        Require(sideband.result == 0);
        RunDecode(context, multi, framed.data(), framed.size(), both.data(), both.size() * sizeof(std::int16_t), sideband);
        Require(sideband.result == 0 && static_cast<std::size_t>(sideband.inputConsumed) == framed.size() && sideband.outputWritten == 960 * channels * 2);
        for (std::size_t sample = 0; sample < 960; ++sample) {
            for (std::uint32_t channel = 0; channel < streamChannels; ++channel) {
                Require(both[sample * channels + channel] == single[sample * streamChannels + channel]);
                Require(both[sample * channels + streamChannels + channel] == single[sample * streamChannels + channel]);
            }
        }
        offset += 2 + bytes;
    }
    Require(sceAjmInstanceDestroy(context, mono) == 0);
    Require(sceAjmInstanceDestroy(context, multi) == 0);
}

void TestOpusMonoFamily255(std::uint32_t context) {
    std::uint32_t instances[2] = {};
    for (std::size_t index = 0; index < 2; ++index) {
        Require(sceAjmInstanceCreate(context, 24, 0, &instances[index]) == 0);
        const std::uint32_t parameters[3] = {1, 48000, index == 0 ? 0u : 255u};
        std::vector<std::uint8_t> batch(4096);
        AjmBatchInfo info{};
        std::int64_t initResult[2] = {-1, -1};
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        Require(sceAjmBatchJobInitialize(&info, instances[index], parameters, sizeof(parameters), initResult) == 0);
        Submit(context, info);
        Require(initResult[0] == 0);
    }
    std::size_t offset = 0;
    int peak = 0;
    for (int packet = 0; packet < 3; ++packet) {
        const std::size_t bytes = OPUS_STEREO[offset] | (std::size_t{OPUS_STEREO[offset + 1]} << 8u);
        std::vector<std::int16_t> pcm[2] = {std::vector<std::int16_t>(960, -12345), std::vector<std::int16_t>(960, -12345)};
        for (std::size_t index = 0; index < 2; ++index) {
            DecodeSideband sideband{};
            RunDecode(context, instances[index], OPUS_STEREO + offset, 2 + bytes, pcm[index].data(), pcm[index].size() * sizeof(std::int16_t), sideband);
            Require(sideband.result == 0 && static_cast<std::size_t>(sideband.inputConsumed) == 2 + bytes && sideband.outputWritten == 960 * 2);
            Require(sideband.totalDecodedSamples == static_cast<std::uint64_t>(packet + 1) * 960);
        }
        Require(pcm[0] == pcm[1]);
        if (packet > 0) {
            for (const std::int16_t sample : pcm[1]) peak = std::max(peak, std::abs(static_cast<int>(sample)));
        }
        offset += 2 + bytes;
    }
    Require(peak > 900 && peak < 1200);

    const std::uint32_t refusedParameters[1][3] = {{1, 48000, 2}};
    for (const auto& parameters : refusedParameters) {
        std::uint32_t instance = 0;
        Require(sceAjmInstanceCreate(context, 24, 0, &instance) == 0);
        std::vector<std::uint8_t> batch(4096);
        AjmBatchInfo info{};
        std::int64_t initResult[2] = {-1, -1};
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        Require(sceAjmBatchJobInitialize(&info, instance, parameters, sizeof(parameters), initResult) == 0);
        Require(Refused(context, info));
        Require(sceAjmInstanceDestroy(context, instance) == 0);
    }
    for (const std::uint32_t instance : instances) Require(sceAjmInstanceDestroy(context, instance) == 0);
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

void TestDecodeSplit(std::uint32_t context) {
    std::uint32_t whole = 0;
    std::uint32_t split = 0;
    Require(sceAjmInstanceCreate(context, 0, 0, &whole) == 0 && sceAjmInstanceCreate(context, 0, 0, &split) == 0);
    std::vector<std::int16_t> reference(1152);
    DecodeSideband expected{};
    RunDecode(context, whole, MP3_MONO, sizeof(MP3_MONO), reference.data(), reference.size() * sizeof(std::int16_t), expected);
    std::vector<std::int16_t> first(512);
    std::vector<std::int16_t> second(640);
    const AjmBuffer input{const_cast<std::uint8_t*>(MP3_MONO), sizeof(MP3_MONO)};
    const AjmBuffer outputs[2] = {{first.data(), first.size() * sizeof(std::int16_t)}, {second.data(), second.size() * sizeof(std::int16_t)}};
    std::vector<std::uint8_t> batch(0x100);
    AjmBatchInfo info{};
    DecodeSideband sideband{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobDecodeSplit(&info, split, &input, 1, outputs, 2, &sideband) == 0);
    Submit(context, info);
    Require(expected.result == 0 && sideband.result == 0 && sideband.inputConsumed == expected.inputConsumed && sideband.outputWritten == expected.outputWritten && sideband.totalDecodedSamples == expected.totalDecodedSamples);
    Require(std::equal(first.begin(), first.end(), reference.begin()) && std::equal(second.begin(), second.end(), reference.begin() + 512));
    Require(sceAjmInstanceDestroy(context, whole) == 0 && sceAjmInstanceDestroy(context, split) == 0);
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

void TestBatchContextOwnership(std::uint32_t context) {
    constexpr int invalidContext = static_cast<int>(0x80930002);
    constexpr int invalidBatch = static_cast<int>(0x80930004);
    std::uint32_t other = 0;
    Require(sceAjmInitialize(0, &other) == 0 && other != context);
    std::vector<std::uint8_t> storage(256);
    AjmBatchInfo info{};
    Require(sceAjmBatchInitialize(storage.data(), storage.size(), &info) == 0);
    AjmBatchError error{};
    std::uint32_t first = 0;
    std::uint32_t second = 0;
    Require(sceAjmBatchStart(context, &info, 0, &error, &first) == 0);
    Require(sceAjmBatchStart(other, &info, 0, &error, &second) == 0);
    Require(sceAjmBatchCancel(other, first) == invalidBatch);
    Require(sceAjmBatchWait(other, first, 0, &error) == invalidBatch);
    Require(sceAjmBatchCancel(context, second) == invalidBatch);
    Require(sceAjmBatchWait(context, second, 0, &error) == invalidBatch);
    Require(sceAjmBatchWait(0, first, 0, &error) == invalidContext);
    Require(sceAjmBatchWait(other + 1, first, 0, &error) == invalidContext);
    Require(sceAjmBatchCancel(context, first) == 0);
    Require(sceAjmBatchWait(context, first, 0, &error) == 0);
    Require(sceAjmBatchWait(other, second, 0, &error) == 0);
    Require(sceAjmBatchWait(context, first, 0, &error) == invalidBatch);
    Require(sceAjmBatchWait(other, second, 0, &error) == invalidBatch);
    std::uint32_t untouched = 0xfeedu;
    Require(sceAjmBatchStart(0, &info, 0, &error, &untouched) == invalidContext);
    Require(sceAjmBatchStart(other + 1, &info, 0, &error, &untouched) == invalidContext);
    Require(untouched == 0xfeedu);
    Require(sceAjmFinalize(other) == 0);
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

constexpr std::uint32_t CODEC_M4AAC = 2;
constexpr std::uint64_t M4AAC_STEREO = 2ull << 3;
constexpr std::uint64_t M4AAC_FLOAT = 2ull << 7;
constexpr std::uint64_t M4AAC_NONDELAY = 1ull << 33;
constexpr std::uint64_t RUN_MULTIPLE_FRAMES = 1ull << 12;
constexpr std::uint64_t SIDEBAND_STREAM = 1ull << 47;
constexpr std::size_t AAC_FRAMES = 5;
constexpr std::size_t AAC_FRAME_SAMPLES = 1024;
constexpr std::size_t AAC_PRIMING = 1024;
constexpr std::size_t AAC_INPUT_SAMPLES = 4096;

struct AacInitialize {
    std::uint32_t configType;
    std::uint32_t frequencyIndex;
};

struct AacControl {
    GaplessDecode gapless;
    AacInitialize initialize;
};

struct AacCodecInfoSideband {
    std::int32_t result;
    std::int32_t internalResult;
    std::uint32_t heaac;
    std::uint32_t reserved;
};

struct MultipleFramesSideband {
    std::int32_t result;
    std::int32_t internalResult;
    std::int32_t inputConsumed;
    std::int32_t outputWritten;
    std::uint64_t totalDecodedSamples;
    std::uint32_t numFrames;
    std::uint32_t reserved;
};

std::vector<std::size_t> AdtsFrameOffsets() {
    std::vector<std::size_t> offsets;
    for (std::size_t offset = 0; offset < sizeof(AAC_STEREO);) {
        offsets.push_back(offset);
        offset += ((std::size_t{AAC_STEREO[offset + 3]} & 3u) << 11u) | (std::size_t{AAC_STEREO[offset + 4]} << 3u) | (AAC_STEREO[offset + 5] >> 5u);
    }
    Require(offsets.size() == AAC_FRAMES);
    offsets.push_back(sizeof(AAC_STEREO));
    return offsets;
}

double AacReference(std::size_t index, std::size_t channel) {
    const double duration = static_cast<double>(AAC_INPUT_SAMPLES) / 48000.0;
    const double t = static_cast<double>(index) / 48000.0;
    const double start = channel == 0 ? 300.0 : 2500.0;
    const double end = channel == 0 ? 3000.0 : 400.0;
    return std::sin(2.0 * 3.14159265358979323846 * (start * t + (end - start) * t * t / (2.0 * duration)));
}

double AacCorrelation(const std::vector<std::int16_t>& pcm, std::size_t channel, std::size_t referenceChannel, std::size_t lag) {
    double cross = 0;
    double decodedEnergy = 0;
    double referenceEnergy = 0;
    for (std::size_t sample = 0; sample < pcm.size() / 2; ++sample) {
        const double decoded = pcm[sample * 2 + channel];
        const double reference = AacReference(sample + lag, referenceChannel);
        cross += decoded * reference;
        decodedEnergy += decoded * decoded;
        referenceEnergy += reference * reference;
    }
    return cross / std::sqrt(decodedEnergy * referenceEnergy);
}

std::uint32_t CreateAac(std::uint32_t context, std::uint64_t flags, std::uint32_t configType, std::uint32_t frequencyIndex) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, CODEC_M4AAC, flags, &instance) == 0);
    const AacInitialize parameters{configType, frequencyIndex};
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int32_t result[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobInitialize(&info, instance, &parameters, sizeof(parameters), result) == 0);
    Submit(context, info);
    Require(result[0] == 0 && result[1] == 0);
    return instance;
}

MultipleFramesSideband RunAacFrames(std::uint32_t context, std::uint32_t instance, std::uint64_t flags, const std::uint8_t* input, std::size_t inputSize, void* output, std::size_t outputSize) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    MultipleFramesSideband sideband{-1, -1, -1, -1, 0, 0, 0};
    const std::size_t sidebandSize = (flags & RUN_MULTIPLE_FRAMES) ? sizeof(sideband) : sizeof(DecodeSideband);
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobRun(&info, instance, flags | SIDEBAND_STREAM, input, inputSize, output, outputSize, &sideband, sidebandSize) == 0);
    Submit(context, info);
    return sideband;
}

std::int32_t InitializeAacResult(std::uint32_t context, std::uint32_t instance, const void* parameters, std::size_t size) {
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int32_t result[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobInitialize(&info, instance, parameters, size, result) == 0);
    Submit(context, info);
    return result[0];
}

void TestM4aacAdts(std::uint32_t context, std::vector<std::int16_t>& delayed) {
    const auto offsets = AdtsFrameOffsets();
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, CODEC_M4AAC, M4AAC_STEREO, &instance) == 0);
    const AacControl start{{0, 0, 0}, {1, 3}};
    std::int32_t result[2] = {-1, -1};
    RunControl(context, instance, CONTROL_START, &start, sizeof(start), result);
    Require(result[0] == 0 && result[1] == 0);

    std::vector<std::int16_t> pcm(AAC_FRAME_SAMPLES * 2);
    for (std::size_t frame = 0; frame < AAC_FRAMES; ++frame) {
        const auto decoded = RunAacFrames(context, instance, 0, AAC_STEREO + offsets[frame], sizeof(AAC_STEREO) - offsets[frame], pcm.data(), pcm.size() * sizeof(std::int16_t));
        const std::size_t produced = frame < 2 ? 0 : AAC_FRAME_SAMPLES;
        Require(decoded.result == 0 && static_cast<std::size_t>(decoded.inputConsumed) == offsets[frame + 1] - offsets[frame]);
        Require(static_cast<std::size_t>(decoded.outputWritten) == produced * 2 * sizeof(std::int16_t) && decoded.totalDecodedSamples == (frame < 2 ? 0 : (frame - 1) * AAC_FRAME_SAMPLES));
        delayed.insert(delayed.end(), pcm.begin(), pcm.begin() + static_cast<std::ptrdiff_t>(produced * 2));
    }
    Require(delayed.size() == (AAC_FRAMES - 2) * AAC_FRAME_SAMPLES * 2);
    const auto empty = RunAacFrames(context, instance, 0, AAC_STEREO + sizeof(AAC_STEREO), 0, pcm.data(), pcm.size() * sizeof(std::int16_t));
    Require(empty.result == 8 && empty.inputConsumed == 0 && empty.outputWritten == 0);

    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    FormatSideband format{-1, -1, 0, 0, 0, 0xaaaaaaaau, 0, 0xaaaaaaaau};
    AacCodecInfoSideband codec{-1, -1, 0xaaaaaaaau, 0xaaaaaaaau};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobGetInfo(&info, instance, &format) == 0);
    Require(sceAjmBatchJobGetCodecInfo(&info, instance, &codec, sizeof(codec)) == 0);
    Submit(context, info);
    const std::size_t lastFrameBytes = offsets[AAC_FRAMES] - offsets[AAC_FRAMES - 1];
    Require(format.result == 0 && format.numChannels == 2 && format.channelMask == 0x3 && format.sampleRate == 48000 && format.sampleEncoding == 0 && format.bitrate == lastFrameBytes * 8 * 48000 / AAC_FRAME_SAMPLES && format.reserved == 0);
    Require(codec.result == 0 && codec.internalResult == 0 && codec.heaac == 0 && codec.reserved == 0);

    const std::size_t lag = AAC_FRAME_SAMPLES * 2 - AAC_PRIMING;
    for (std::size_t channel = 0; channel < 2; ++channel) {
        const double aligned = AacCorrelation(delayed, channel, channel, lag);
        Require(aligned > 0.85 && AacCorrelation(delayed, channel, 1 - channel, lag) < 0.2);
        for (std::size_t shifted = lag - 64; shifted < lag; shifted += 8) Require(AacCorrelation(delayed, channel, channel, shifted) < aligned - 0.3);
    }

    result[0] = -1;
    RunControl(context, instance, CONTROL_RESET, nullptr, 0, result);
    const auto reset = RunAacFrames(context, instance, 0, AAC_STEREO, sizeof(AAC_STEREO), pcm.data(), pcm.size() * sizeof(std::int16_t));
    Require(result[0] == 0 && reset.result == 0 && static_cast<std::size_t>(reset.inputConsumed) == offsets[1] && reset.outputWritten == 0 && reset.totalDecodedSamples == 0);

    const AacControl gapless{{2000, 100, 0}, {1, 3}};
    result[0] = -1;
    RunControl(context, instance, CONTROL_START, &gapless, sizeof(gapless), result);
    std::vector<std::int16_t> all(AAC_FRAMES * AAC_FRAME_SAMPLES * 2);
    const auto trimmed = RunAacFrames(context, instance, RUN_MULTIPLE_FRAMES, AAC_STEREO, sizeof(AAC_STEREO), all.data(), all.size() * sizeof(std::int16_t));
    Require(result[0] == 0 && trimmed.result == 0 && trimmed.inputConsumed == static_cast<std::int32_t>(sizeof(AAC_STEREO)) && trimmed.numFrames == AAC_FRAMES);
    Require(trimmed.outputWritten == 2000 * 2 * static_cast<std::int32_t>(sizeof(std::int16_t)) && trimmed.totalDecodedSamples == 2000);
    Require(std::equal(all.begin(), all.begin() + 2000 * 2, delayed.begin() + 100 * 2));
    Require(sceAjmInstanceDestroy(context, instance) == 0);
}

void TestM4aacNondelay(std::uint32_t context, const std::vector<std::int16_t>& delayed) {
    const auto offsets = AdtsFrameOffsets();
    const std::uint32_t adts = CreateAac(context, M4AAC_STEREO | M4AAC_NONDELAY, 1, 0);
    std::vector<std::int16_t> whole(AAC_FRAMES * AAC_FRAME_SAMPLES * 2);
    const auto decoded = RunAacFrames(context, adts, RUN_MULTIPLE_FRAMES, AAC_STEREO, sizeof(AAC_STEREO), whole.data(), whole.size() * sizeof(std::int16_t));
    Require(decoded.result == 0 && decoded.inputConsumed == static_cast<std::int32_t>(sizeof(AAC_STEREO)) && decoded.numFrames == AAC_FRAMES);
    Require(static_cast<std::size_t>(decoded.outputWritten) == whole.size() * sizeof(std::int16_t) && decoded.totalDecodedSamples == AAC_FRAMES * AAC_FRAME_SAMPLES);
    Require(std::equal(delayed.begin(), delayed.end(), whole.begin() + 2 * AAC_FRAME_SAMPLES * 2));

    std::vector<std::int16_t> small(1000 * 2);
    const auto noRoom = RunAacFrames(context, adts, 0, AAC_STEREO, sizeof(AAC_STEREO), small.data(), small.size() * sizeof(std::int16_t));
    Require(noRoom.result == 0x10 && noRoom.inputConsumed == 0 && noRoom.outputWritten == 0);
    const auto partial = RunAacFrames(context, adts, 0, AAC_STEREO, offsets[1] - 1, whole.data(), whole.size() * sizeof(std::int16_t));
    Require(partial.result == 8 && partial.inputConsumed == 0 && partial.outputWritten == 0);
    const auto header = RunAacFrames(context, adts, 0, AAC_STEREO, 6, whole.data(), whole.size() * sizeof(std::int16_t));
    Require(header.result == 8 && header.inputConsumed == 0 && header.outputWritten == 0);
    Require(sceAjmInstanceDestroy(context, adts) == 0);

    const std::uint32_t floating = CreateAac(context, M4AAC_STEREO | M4AAC_NONDELAY | M4AAC_FLOAT, 1, 0);
    std::vector<float> samples(AAC_FRAMES * AAC_FRAME_SAMPLES * 2);
    const auto decodedFloat = RunAacFrames(context, floating, RUN_MULTIPLE_FRAMES, AAC_STEREO, sizeof(AAC_STEREO), samples.data(), samples.size() * sizeof(float));
    Require(decodedFloat.result == 0 && static_cast<std::size_t>(decodedFloat.outputWritten) == samples.size() * sizeof(float));
    for (std::size_t index = 0; index < samples.size(); ++index) Require(std::fabs(samples[index] * 32768.0f - static_cast<float>(whole[index])) <= 1.0f);
    Require(sceAjmInstanceDestroy(context, floating) == 0);

    const std::uint32_t raw = CreateAac(context, M4AAC_STEREO | M4AAC_NONDELAY, 2, 3);
    std::vector<std::int16_t> pcm(AAC_FRAME_SAMPLES * 2);
    for (std::size_t frame = 0; frame < AAC_FRAMES; ++frame) {
        const std::size_t payload = offsets[frame] + 7;
        const auto access = RunAacFrames(context, raw, 0, AAC_STEREO + payload, offsets[frame + 1] - payload, pcm.data(), pcm.size() * sizeof(std::int16_t));
        Require(access.result == 0 && static_cast<std::size_t>(access.inputConsumed) == offsets[frame + 1] - payload && static_cast<std::size_t>(access.outputWritten) == pcm.size() * sizeof(std::int16_t));
        Require(std::equal(pcm.begin(), pcm.end(), whole.begin() + static_cast<std::ptrdiff_t>(frame * pcm.size())));
    }
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    MultipleFramesSideband sideband{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobRun(&info, raw, RUN_MULTIPLE_FRAMES | SIDEBAND_STREAM, AAC_STEREO + 7, offsets[1] - 7, pcm.data(), pcm.size() * sizeof(std::int16_t), &sideband, sizeof(sideband)) == 0);
    Require(Refused(context, info));
    Require(sceAjmInstanceDestroy(context, raw) == 0);
}

void TestM4aacInitialize(std::uint32_t context) {
    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, CODEC_M4AAC, M4AAC_STEREO, &instance) == 0);
    std::vector<std::int16_t> pcm(AAC_FRAME_SAMPLES * 2);
    const auto early = RunAacFrames(context, instance, 0, AAC_STEREO, sizeof(AAC_STEREO), pcm.data(), pcm.size() * sizeof(std::int16_t));
    Require(early.result == 1);
    const AacInitialize noType{0, 3};
    const AacInitialize badType{3, 3};
    const AacInitialize badIndex{2, 12};
    Require(InitializeAacResult(context, instance, &noType, sizeof(noType)) == 4);
    Require(InitializeAacResult(context, instance, &badType, sizeof(badType)) == 4);
    Require(InitializeAacResult(context, instance, &badIndex, sizeof(badIndex)) == 4);
    Require(InitializeAacResult(context, instance, &badIndex, 4) == 4);
    const auto stillEarly = RunAacFrames(context, instance, 0, AAC_STEREO, sizeof(AAC_STEREO), pcm.data(), pcm.size() * sizeof(std::int16_t));
    Require(stillEarly.result == 1);

    const AacControl invalid{{0, 0, 0}, {0, 3}};
    std::int32_t result[2] = {-1, -1};
    RunControl(context, instance, CONTROL_START, &invalid, sizeof(invalid), result);
    Require(result[0] == 4);
    Require(ControlRefused(context, instance, CONTROL_START, &invalid, sizeof(invalid) - 4));
    Require(ControlRefused(context, instance, CONTROL_INITIALIZE, &invalid, sizeof(invalid)));
    const AacInitialize adts{1, 15};
    result[0] = -1;
    RunControl(context, instance, CONTROL_INITIALIZE, &adts, sizeof(adts), result);
    Require(result[0] == 0);

    std::vector<std::uint8_t> broken(AAC_STEREO, AAC_STEREO + sizeof(AAC_STEREO));
    broken[1] = 0x00;
    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    DecodeSideband sideband{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobDecode(&info, instance, broken.data(), broken.size(), pcm.data(), pcm.size() * sizeof(std::int16_t), &sideband) == 0);
    Require(Refused(context, info));
    Require(sceAjmInstanceDestroy(context, instance) == 0);

    Require(sceAjmInstanceCreate(context, CODEC_M4AAC, 1ull << 3, &instance) == 0);
    Require(InitializeAacResult(context, instance, &adts, sizeof(adts)) == 0);
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobDecode(&info, instance, AAC_STEREO, sizeof(AAC_STEREO), pcm.data(), pcm.size() * sizeof(std::int16_t), &sideband) == 0);
    Require(Refused(context, info));
    Require(sceAjmInstanceDestroy(context, instance) == 0);

    Require(sceAjmInstanceCreate(context, CODEC_M4AAC, 3ull << 3, &instance) == 0);
    const AacInitialize raw{2, 3};
    std::int32_t rawResult[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    Require(sceAjmBatchJobInitialize(&info, instance, &raw, sizeof(raw), rawResult) == 0);
    Require(Refused(context, info));
    Require(sceAjmInstanceDestroy(context, instance) == 0);

    bool unknownFlagThrows = false;
    try {
        static_cast<void>(sceAjmInstanceCreate(context, CODEC_M4AAC, M4AAC_STEREO | (1ull << 36), &instance));
    } catch (const std::runtime_error&) {
        unknownFlagThrows = true;
    }
    Require(unknownFlagThrows);
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

void TestInstanceHandleLifetime(std::uint32_t context) {
    const auto live = CreateOpus(context);
    const auto reference = CreateOpus(context);
    const std::size_t packetBytes = 2 + (OPUS_STEREO[0] | (std::size_t{OPUS_STEREO[1]} << 8u));
    std::vector<std::int16_t> pcm(128 * 2);
    std::vector<std::int16_t> expected(pcm.size());
    DecodeSideband decoded{};
    DecodeSideband baseline{};
    RunDecode(context, live, OPUS_STEREO, packetBytes, pcm.data(), pcm.size() * sizeof(std::int16_t), decoded);
    RunDecode(context, reference, OPUS_STEREO, packetBytes, expected.data(), expected.size() * sizeof(std::int16_t), baseline);
    Require(decoded.result == 0 && decoded.outputWritten == 128 * 2 * 2 && decoded.totalDecodedSamples == 960);
    Require(baseline.result == 0 && pcm == expected);
    for (std::size_t index = 0; index < 0x8000; ++index) {
        std::uint32_t temporary = 0;
        Require(sceAjmInstanceCreate(context, 24, 0, &temporary) == 0);
        if (temporary == live || temporary == reference) throw std::runtime_error("AJM creation replaced an existing live decoder handle");
        Require(temporary >> 14 == 24);
        Require(sceAjmInstanceDestroy(context, temporary) == 0);
    }
    std::vector<bool> occupied(0x4000);
    occupied[live & 0x3FFF] = true;
    occupied[reference & 0x3FFF] = true;
    std::vector<std::uint32_t> instances;
    instances.reserve(0x4000 - 2);
    for (std::size_t index = 0; index < 0x4000 - 2; ++index) {
        std::uint32_t instance = 0;
        Require(sceAjmInstanceCreate(context, 24, 0, &instance) == 0 && instance >> 14 == 24);
        Require(!occupied[instance & 0x3FFF]);
        occupied[instance & 0x3FFF] = true;
        instances.push_back(instance);
    }
    Require(std::all_of(occupied.begin(), occupied.end(), [](bool used) { return used; }));
    constexpr int outOfResources = static_cast<int>(0x80930007);
    std::uint32_t refused = 0xDEADBEEF;
    Require(sceAjmInstanceCreate(context, 24, 0, &refused) == outOfResources && refused == 0xDEADBEEF);
    std::uint32_t otherCodec = 0;
    Require(sceAjmInstanceCreate(context, 1, 0, &otherCodec) == 0 && otherCodec >> 14 == 1);
    const auto released = instances.back();
    Require(sceAjmInstanceDestroy(context, released) == 0);
    Require(sceAjmInstanceCreate(context, 24, 0, &instances.back()) == 0 && instances.back() == released);
    Require(sceAjmInstanceCreate(context, 24, 0, &refused) == outOfResources && refused == 0xDEADBEEF);
    pcm.assign(832 * 2, -12345);
    expected.assign(pcm.size(), -12345);
    RunDecode(context, live, OPUS_STEREO, 0, pcm.data(), pcm.size() * sizeof(std::int16_t), decoded);
    RunDecode(context, reference, OPUS_STEREO, 0, expected.data(), expected.size() * sizeof(std::int16_t), baseline);
    Require(decoded.result == 0 && decoded.inputConsumed == 0 && decoded.outputWritten == 832 * 2 * 2 && decoded.totalDecodedSamples == 960);
    Require(baseline.result == 0 && baseline.outputWritten == decoded.outputWritten && pcm == expected);
    const std::size_t nextPacketBytes = 2 + (OPUS_STEREO[packetBytes] | (std::size_t{OPUS_STEREO[packetBytes + 1]} << 8u));
    pcm.assign(960 * 2, -12345);
    expected.assign(pcm.size(), -12345);
    RunDecode(context, live, OPUS_STEREO + packetBytes, nextPacketBytes, pcm.data(), pcm.size() * sizeof(std::int16_t), decoded);
    RunDecode(context, reference, OPUS_STEREO + packetBytes, nextPacketBytes, expected.data(), expected.size() * sizeof(std::int16_t), baseline);
    Require(decoded.result == 0 && static_cast<std::size_t>(decoded.inputConsumed) == nextPacketBytes && decoded.outputWritten == 960 * 2 * 2 && decoded.totalDecodedSamples == 1920);
    Require(baseline.result == 0 && baseline.outputWritten == decoded.outputWritten && pcm == expected);
    for (const auto instance : instances) Require(sceAjmInstanceDestroy(context, instance) == 0);
    Require(sceAjmInstanceDestroy(context, otherCodec) == 0);
    Require(sceAjmInstanceDestroy(context, live) == 0);
    Require(sceAjmInstanceDestroy(context, reference) == 0);
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

void TestResampleParametersEx(std::uint32_t context) {
    std::uint32_t plain = 0;
    std::uint32_t extended = 0;
    Require(sceAjmInstanceCreate(context, 0, 0, &plain) == 0 && sceAjmInstanceCreate(context, 0, 0, &extended) == 0);
    std::vector<std::int16_t> plainPcm(512);
    std::vector<std::int16_t> extendedPcm(512);
    for (const bool ex : {false, true}) {
        std::vector<std::uint8_t> batch(4096);
        AjmBatchInfo info{};
        std::int64_t setResult[2] = {-1, -1};
        DecodeSideband sideband{};
        auto& pcm = ex ? extendedPcm : plainPcm;
        const auto instance = ex ? extended : plain;
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        if (ex) Require(sceAjmBatchJobSetResampleParametersEx(&info, instance, 2.0f, 0.0f, 1, setResult) == 0);
        else Require(sceAjmBatchJobSetResampleParameters(&info, instance, 2.0f, 1, setResult) == 0);
        Require(sceAjmBatchJobDecode(&info, instance, MP3_MONO, sizeof(MP3_MONO), pcm.data(), pcm.size() * sizeof(std::int16_t), &sideband) == 0);
        Submit(context, info);
        Require(setResult[0] == 0 && sideband.result == 0);
    }
    Require(extendedPcm == plainPcm);
    Require(GetResampleInfo(context, extended).ratio == 2.0f && GetResampleInfo(context, extended).samples == GetResampleInfo(context, plain).samples);

    std::vector<std::uint8_t> batch(4096);
    AjmBatchInfo info{};
    std::int64_t result[2] = {-1, -1};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    for (const float bad : {0.0f, -1.0f, std::nanf(""), INFINITY}) Require(sceAjmBatchJobSetResampleParametersEx(&info, extended, bad, 0.0f, 1, result) == static_cast<int>(0x80930005));
    for (const float bad : {std::nanf(""), INFINITY, -INFINITY}) Require(sceAjmBatchJobSetResampleParametersEx(&info, extended, 1.0f, bad, 1, result) == static_cast<int>(0x80930005));

    std::uint32_t ramped = 0;
    Require(sceAjmInstanceCreate(context, 0, 0, &ramped) == 0);
    std::vector<std::int16_t> rampedPcm(512);
    {
        std::vector<std::uint8_t> rampBatch(4096);
        AjmBatchInfo rampInfo{};
        std::int64_t setResult[2] = {-1, -1};
        DecodeSideband sideband{};
        Require(sceAjmBatchInitialize(rampBatch.data(), rampBatch.size(), &rampInfo) == 0);
        Require(sceAjmBatchJobSetResampleParametersEx(&rampInfo, ramped, 1.0f, 0.001f, 1, setResult) == 0);
        Require(sceAjmBatchJobDecode(&rampInfo, ramped, MP3_MONO, sizeof(MP3_MONO), rampedPcm.data(), rampedPcm.size() * sizeof(std::int16_t), &sideband) == 0);
        Submit(context, rampInfo);
        Require(setResult[0] == 0 && sideband.result == 0 && sideband.outputWritten == 512 * 2);
    }
    const auto resampleInfo = GetResampleInfo(context, ramped);
    Require(std::abs(resampleInfo.ratio - 1.512f) < 0.001f && rampedPcm != plainPcm);
    Require(sceAjmInstanceDestroy(context, plain) == 0 && sceAjmInstanceDestroy(context, extended) == 0 && sceAjmInstanceDestroy(context, ramped) == 0);
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

void TestOpusGaplessReset(std::uint32_t context, std::size_t outputFrames, float ratio) {
    const auto instance = CreateOpus(context);
    const auto reference = CreateOpus(context);
    std::vector<std::uint8_t> batch(4096);
    std::vector<std::int16_t> pcm(outputFrames * 2 + 4, -12345);
    std::vector<std::int16_t> expected(960 * 2);
    const std::uint32_t totalSamples = ratio > 0 ? 860 : 500;
    const GaplessDecode gapless{totalSamples, 100, 0};
    const GaplessDecode unlimited{0, 100, 0};
    AjmBatchInfo info{};
    if (ratio > 0) {
        std::int32_t result[2][2] = {{-1, -1}, {-1, -1}};
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        Require(sceAjmBatchJobSetResampleParameters(&info, instance, ratio, 1, result[0]) == 0);
        Require(sceAjmBatchJobSetResampleParameters(&info, reference, ratio, 1, result[1]) == 0);
        Submit(context, info);
        Require(result[0][0] == 0 && result[1][0] == 0);
    }
    std::size_t offset = 0;
    for (int segment = 0; segment < 3; ++segment) {
        std::int32_t result[2][2] = {{-1, -1}, {-1, -1}};
        GaplessSideband state{-1, -1, 0, 0, 0xffff};
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        Require(sceAjmBatchJobSetGaplessDecode(&info, instance, &gapless, 1, result[0]) == 0);
        Require(sceAjmBatchJobSetGaplessDecode(&info, reference, &unlimited, 1, result[1]) == 0);
        Require(sceAjmBatchJobGetGaplessDecode(&info, instance, &state) == 0);
        Submit(context, info);
        Require(result[0][0] == 0 && result[1][0] == 0);
        Require(state.result == 0 && state.totalSamples == totalSamples && state.skipSamples == 100 && state.skippedSamples == 0);
        const std::size_t packetBytes = 2 + (OPUS_STEREO[offset] | (std::size_t{OPUS_STEREO[offset + 1]} << 8u));
        DecodeSideband baseline{};
        RunDecode(context, reference, OPUS_STEREO + offset, packetBytes, expected.data(), expected.size() * sizeof(std::int16_t), baseline);
        Require(baseline.result == 0 && static_cast<std::size_t>(baseline.inputConsumed) == packetBytes);
        Require(baseline.totalDecodedSamples == static_cast<std::uint64_t>(segment + 1) * 860 + (ratio == 0 ? segment * 960 : 0));
        const std::size_t expectedBytes = ratio > 0 ? static_cast<std::size_t>(baseline.outputWritten) : totalSamples * 2 * sizeof(std::int16_t);
        Require(expectedBytes > 0);
        std::size_t produced = 0;
        for (int drain = 0; produced < expectedBytes && drain < 16; ++drain) {
            std::fill(pcm.begin(), pcm.end(), -12345);
            DecodeSideband decoded{};
            RunDecode(context, instance, OPUS_STEREO + offset, drain == 0 ? packetBytes : 0, pcm.data(), outputFrames * 2 * sizeof(std::int16_t), decoded);
            if (decoded.outputWritten <= 0) throw std::runtime_error("Opus gapless segment " + std::to_string(segment) + " wrote " + std::to_string(decoded.outputWritten) + " bytes with " + std::to_string(expectedBytes - produced) + " still expected");
            Require(decoded.result == 0 && static_cast<std::size_t>(decoded.inputConsumed) == (drain == 0 ? packetBytes : 0));
            Require(decoded.totalDecodedSamples == static_cast<std::uint64_t>(segment + 1) * totalSamples);
            const auto samples = static_cast<std::size_t>(decoded.outputWritten) / sizeof(std::int16_t);
            Require(samples <= outputFrames * 2 && produced + static_cast<std::size_t>(decoded.outputWritten) <= expectedBytes);
            Require(std::equal(pcm.begin(), pcm.begin() + samples, expected.begin() + produced / sizeof(std::int16_t)));
            Require(std::all_of(pcm.begin() + samples, pcm.end(), [](auto sample) { return sample == -12345; }));
            produced += static_cast<std::size_t>(decoded.outputWritten);
        }
        Require(produced == expectedBytes);
        if (ratio == 0) {
            Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
            Require(sceAjmBatchJobSetGaplessDecode(&info, instance, &gapless, 0, result[0]) == 0);
            Require(sceAjmBatchJobGetGaplessDecode(&info, instance, &state) == 0);
            Submit(context, info);
            Require(result[0][0] == 0 && state.result == 0 && state.skippedSamples == 100);
            std::fill(pcm.begin(), pcm.end(), -12345);
            DecodeSideband decoded{};
            RunDecode(context, instance, OPUS_STEREO + offset, packetBytes, pcm.data(), outputFrames * 2 * sizeof(std::int16_t), decoded);
            RunDecode(context, reference, OPUS_STEREO + offset, packetBytes, expected.data(), expected.size() * sizeof(std::int16_t), baseline);
            Require(decoded.result == 0 && static_cast<std::size_t>(decoded.inputConsumed) == packetBytes && decoded.outputWritten == 0);
            Require(decoded.totalDecodedSamples == static_cast<std::uint64_t>(segment + 1) * totalSamples);
            Require(std::all_of(pcm.begin(), pcm.end(), [](auto sample) { return sample == -12345; }));
        }
        offset = (offset + packetBytes) % sizeof(OPUS_STEREO);
    }
    Require(sceAjmInstanceDestroy(context, instance) == 0);
    Require(sceAjmInstanceDestroy(context, reference) == 0);
}

struct PcmResults {
    std::int64_t initialize[2] = {-1, -1};
    std::int64_t resample[2] = {-1, -1};
    DecodeSideband decode{};
};

AjmBatchInfo PcmJobs(std::vector<std::uint8_t>& batch, std::uint32_t instance, const std::uint32_t* parameters, float ratio, const std::vector<std::int16_t>& input, std::vector<std::int16_t>& output, PcmResults& results) {
    AjmBatchInfo info{};
    Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
    if (parameters) Require(sceAjmBatchJobInitialize(&info, instance, parameters, 3 * sizeof(std::uint32_t), results.initialize) == 0);
    if (ratio > 0) Require(sceAjmBatchJobSetResampleParameters(&info, instance, ratio, 1, results.resample) == 0);
    Require(sceAjmBatchJobDecode(&info, instance, input.data(), input.size() * sizeof(std::int16_t), output.data(), output.size() * sizeof(std::int16_t), &results.decode) == 0);
    return info;
}

void TestPcmResample(std::uint32_t context) {
    constexpr std::uint64_t flags = 0x1002;
    constexpr std::int16_t untouched = 0x7FFF;
    const std::uint32_t parameters[3] = {0, 2, 44100};
    constexpr std::size_t inputFrames = 235;
    constexpr float ratio = 0.91875f;
    std::vector<std::int16_t> input(inputFrames * 2);
    for (std::size_t frame = 0; frame < inputFrames; ++frame) {
        input[frame * 2] = static_cast<std::int16_t>(frame * 100);
        input[frame * 2 + 1] = static_cast<std::int16_t>((static_cast<int>(frame * 7919 % 2001) - 1000) * 20);
    }
    const auto at = [&](std::ptrdiff_t frame, std::size_t channel) {
        const auto clamped = std::clamp<std::ptrdiff_t>(frame, 0, static_cast<std::ptrdiff_t>(inputFrames) - 1);
        return static_cast<double>(input[static_cast<std::size_t>(clamped) * 2 + channel]);
    };
    std::vector<std::uint8_t> batch(4096);

    std::uint32_t instance = 0;
    Require(sceAjmInstanceCreate(context, 23, flags, &instance) == 0);
    for (int block = 0; block < 2; ++block) {
        std::vector<std::int16_t> output(300 * 2, untouched);
        PcmResults results;
        Submit(context, PcmJobs(batch, instance, parameters, ratio, input, output, results));
        Require(results.initialize[0] == 0 && results.resample[0] == 0 && results.decode.result == 0);
        Require(results.decode.inputConsumed == 940 && results.decode.outputWritten == 1024 && results.decode.totalDecodedSamples == 235);
        for (std::size_t frame = 0; frame < 256; ++frame) {
            const double position = static_cast<double>(frame) * static_cast<double>(ratio);
            const auto index = static_cast<std::ptrdiff_t>(position);
            const double t = position - static_cast<double>(index);
            for (std::size_t channel = 0; channel < 2; ++channel) {
                const double p0 = at(index - 1, channel), p1 = at(index, channel), p2 = at(index + 1, channel), p3 = at(index + 2, channel);
                const double expected = std::clamp(p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0))), -32768.0, 32767.0);
                Require(std::fabs(output[frame * 2 + channel] - expected) <= 1.0);
            }
        }
        Require(std::all_of(output.begin() + 256 * 2, output.end(), [&](std::int16_t value) { return value == untouched; }));
    }
    {
        std::vector<std::int16_t> output(256 * 2);
        PcmResults results;
        Require(Refused(context, PcmJobs(batch, instance, nullptr, ratio, input, output, results)));
    }
    {
        std::vector<std::int16_t> output(255 * 2);
        PcmResults results;
        Require(Refused(context, PcmJobs(batch, instance, parameters, ratio, input, output, results)));
    }
    {
        const GaplessDecode gapless{100, 0, 0};
        std::int32_t gaplessResult[2] = {-1, -1};
        std::vector<std::int16_t> output(256 * 2);
        PcmResults results;
        AjmBatchInfo info{};
        Require(sceAjmBatchInitialize(batch.data(), batch.size(), &info) == 0);
        Require(sceAjmBatchJobInitialize(&info, instance, parameters, sizeof(parameters), results.initialize) == 0);
        Require(sceAjmBatchJobSetGaplessDecode(&info, instance, &gapless, 1, gaplessResult) == 0);
        Require(sceAjmBatchJobDecode(&info, instance, input.data(), input.size() * sizeof(std::int16_t), output.data(), output.size() * sizeof(std::int16_t), &results.decode) == 0);
        Require(Refused(context, info));
    }
    Require(sceAjmInstanceDestroy(context, instance) == 0);

    std::uint32_t plain = 0;
    Require(sceAjmInstanceCreate(context, 23, flags, &plain) == 0);
    for (int run = 0; run < 2; ++run) {
        std::vector<std::int16_t> output(input.size() + 4, untouched);
        PcmResults results;
        Submit(context, PcmJobs(batch, plain, run == 0 ? parameters : nullptr, 0, input, output, results));
        Require(results.decode.result == 0 && results.decode.inputConsumed == 940 && results.decode.outputWritten == 940);
        Require(results.decode.totalDecodedSamples == 235u * static_cast<std::uint64_t>(run + 1));
        Require(std::equal(input.begin(), input.end(), output.begin()) && output[input.size()] == untouched);
    }
    Require(sceAjmInstanceDestroy(context, plain) == 0);

    std::uint32_t invalid = 0;
    Require(sceAjmInstanceCreate(context, 23, flags, &invalid) == 0);
    {
        const std::uint32_t noChannels[3] = {0, 0, 44100};
        std::vector<std::int16_t> output(256 * 2);
        PcmResults results;
        Submit(context, PcmJobs(batch, invalid, noChannels, 0, input, output, results));
        Require(results.initialize[0] == 4 && results.decode.result == 1);
    }
    {
        const std::uint32_t otherFormat[3] = {1, 2, 44100};
        std::vector<std::int16_t> output(256 * 2);
        PcmResults results;
        Require(Refused(context, PcmJobs(batch, invalid, otherFormat, 0, input, output, results)));
    }
    Require(sceAjmInstanceDestroy(context, invalid) == 0);
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
    TestMp3MetadataBounds();
    TestMp3(context);
    TestOpus(context);
    TestOpus(context, 1);
    TestOpusMonoFamily255(context);
    TestOpusMultistreamFamily255(context);
    TestOpusFamilyZeroChannels(context);
    TestOpusSurround(context, 3, 0);
    for (std::size_t stream = 0; stream < 2; ++stream) {
        TestOpusSurround(context, 4, stream);
        TestOpusSurround(context, 5, stream);
        TestOpusSurround(context, 6, stream);
    }
    for (std::size_t stream = 0; stream < 3; ++stream) {
        TestOpusSurround(context, 7, stream);
        TestOpusSurround(context, 8, stream);
    }
    TestOpusInputBeyondStatedData(context, false);
    TestOpusInputBeyondStatedData(context, true);
    TestDecodeSingle(context);
    TestDecodeSplit(context);
    TestGaplessDecode(context);
    TestCodecInfo(context);
    TestGetInfo(context);
    TestBatchWaitRelease(context);
    TestInstanceHandleLifetime(context);
    TestBatchCancel(context);
    TestBatchContextOwnership(context);
    TestControlAt9(context);
    TestMultichannelAt9(context);
    TestControlMp3(context);
    TestControlOpus(context);
    TestOpusGaplessReset(context, 960, 0);
    TestOpusGaplessReset(context, 128, 0);
    TestOpusGaplessReset(context, 960, 2.0f);
    std::vector<std::int16_t> delayedAac;
    TestM4aacAdts(context, delayedAac);
    TestM4aacNondelay(context, delayedAac);
    TestM4aacInitialize(context);
    TestResampleOpus(context);
    TestResampleMp3(context);
    TestResampleParametersEx(context);
    TestResampleAt9(context);
    TestAt9GaplessSegments(context);
    TestAt9RunDecodesWholeInput(context);
    TestPcmResample(context);
    Require(sceAjmFinalize(context) == 0);
}
