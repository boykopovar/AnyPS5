#include "strcodec.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { JxrPostWidth = 12, JxrPostPixels = 768, JxrPostOrigin = 256 };

typedef struct JxrPostCase {
    struct tagPostProcInfo* table[MAX_CHANNELS][2];
    struct tagPostProcInfo rows[MAX_CHANNELS][2][JxrPostWidth + 2];
    PixelI pixels[2][JxrPostPixels];
} JxrPostCase;

static uint32_t Next(uint32_t* seed) {
    *seed = *seed * 1664525u + 1013904223u;
    return *seed >> 8;
}

static int32_t Value(uint32_t* seed, int32_t extreme, int32_t small) {
    uint32_t bits = Next(seed) ^ (Next(seed) << 24);
    if (extreme) return (int32_t)bits;
    return (int32_t)(bits % (2u * (uint32_t)small + 1u)) - small;
}

static uint8_t Texture(uint32_t* seed, int32_t flat) {
    return flat || Next(seed) % 2 ? 0 : (uint8_t)(Next(seed) % 4);
}

static void Rebase(JxrPostCase* to, const JxrPostCase* from) {
    size_t channel, row;
    for (channel = 0; channel < MAX_CHANNELS; ++channel)
        for (row = 0; row < 2; ++row) {
            uintptr_t pointer = (uintptr_t)from->table[channel][row];
            if (pointer >= (uintptr_t)from && pointer < (uintptr_t)from + sizeof(JxrPostCase))
                to->table[channel][row] = (struct tagPostProcInfo*)((uintptr_t)to + (pointer - (uintptr_t)from));
        }
}

void* JxrPostMake_nid_no_patch(uint32_t seed, int32_t zeros, int32_t extreme, int32_t flat) {
    JxrPostCase* value = (JxrPostCase*)malloc(sizeof(JxrPostCase));
    unsigned char* bytes = (unsigned char*)value;
    size_t i, channel, row, entry, j, k;
    if (value == NULL) return NULL;
    for (i = 0; i < sizeof(JxrPostCase); ++i) bytes[i] = (unsigned char)Next(&seed);
    for (channel = 0; channel < MAX_CHANNELS; ++channel)
        for (row = 0; row < 2; ++row) {
            value->table[channel][row] = value->rows[channel][row] + 1;
            for (entry = 0; entry < JxrPostWidth + 2; ++entry) {
                struct tagPostProcInfo* info = &value->rows[channel][row][entry];
                info->iMBDC = Value(&seed, extreme, 8);
                info->ucMBTexture = Texture(&seed, flat);
                for (j = 0; j < 4; ++j)
                    for (k = 0; k < 4; ++k) {
                        info->iBlockDC[j][k] = Value(&seed, extreme, 8);
                        info->ucBlockTexture[j][k] = Texture(&seed, flat);
                    }
            }
        }
    for (row = 0; row < 2; ++row)
        for (i = 0; i < JxrPostPixels; ++i)
            value->pixels[row][i] = (int32_t)(Next(&seed) % 100) < zeros ? 0 : Value(&seed, extreme, 1 << 20);
    return value;
}

void* JxrPostClone_nid_no_patch(const void* source) {
    JxrPostCase* value = (JxrPostCase*)malloc(sizeof(JxrPostCase));
    if (value == NULL) return NULL;
    memcpy(value, source, sizeof(JxrPostCase));
    Rebase(value, (const JxrPostCase*)source);
    return value;
}

size_t JxrPostCaseBytes_nid_no_patch(void) {
    return sizeof(JxrPostCase);
}

void JxrPostCopyTo_nid_no_patch(void* target, const void* source) {
    memcpy(target, source, sizeof(JxrPostCase));
    Rebase((JxrPostCase*)target, (const JxrPostCase*)source);
}

static uintptr_t Offset(const JxrPostCase* value, size_t channel, size_t row) {
    uintptr_t pointer = (uintptr_t)value->table[channel][row];
    if (pointer >= (uintptr_t)value && pointer < (uintptr_t)value + sizeof(JxrPostCase)) return pointer - (uintptr_t)value;
    return pointer == 0 ? SIZE_MAX : pointer;
}

int JxrPostEqual_nid_no_patch(const void* left, const void* right) {
    const JxrPostCase* a = (const JxrPostCase*)left;
    const JxrPostCase* b = (const JxrPostCase*)right;
    size_t channel, row;
    for (channel = 0; channel < MAX_CHANNELS; ++channel)
        for (row = 0; row < 2; ++row)
            if (Offset(a, channel, row) != Offset(b, channel, row)) return 0;
    return memcmp(a->rows, b->rows, sizeof(a->rows)) == 0 && memcmp(a->pixels, b->pixels, sizeof(a->pixels)) == 0;
}

void* JxrPostTable_nid_no_patch(void* value) {
    return ((JxrPostCase*)value)->table;
}

int32_t* JxrPostPixels_nid_no_patch(void* value, size_t which) {
    return ((JxrPostCase*)value)->pixels[which] + JxrPostOrigin;
}

size_t JxrPostPixelBytes_nid_no_patch(void) {
    return sizeof(((JxrPostCase*)NULL)->pixels);
}

void JxrPostSetRow_nid_no_patch(void* value, size_t channel, size_t row, void* pointer) {
    ((JxrPostCase*)value)->table[channel][row] = (struct tagPostProcInfo*)pointer;
}

void JxrPostReferenceSlide_nid_no_patch(void* value, size_t channels, size_t width, int32_t top, int32_t bottom) {
    slideOneMBRow(((JxrPostCase*)value)->table, channels, width, top, bottom);
}

void JxrPostReferenceUpdate_nid_no_patch(void* value, size_t column, size_t channel) {
    JxrPostCase* post = (JxrPostCase*)value;
    updatePostProcInfo(post->table, post->pixels[1] + JxrPostOrigin, column, channel);
}

void JxrPostReferenceMB_nid_no_patch(void* value, size_t column, size_t channel, int32_t threshold) {
    JxrPostCase* post = (JxrPostCase*)value;
    postProcMB(post->table, post->pixels[0] + JxrPostOrigin, post->pixels[1] + JxrPostOrigin, column, channel, threshold);
}

void JxrPostReferenceBlock_nid_no_patch(void* value, size_t column, size_t channel, int32_t threshold) {
    JxrPostCase* post = (JxrPostCase*)value;
    postProcBlock(post->table, post->pixels[0] + JxrPostOrigin, post->pixels[1] + JxrPostOrigin, column, channel,
                  threshold);
}
