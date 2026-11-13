#include "strcodec.h"
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(struct tagPostProcInfo) == 88 && offsetof(struct tagPostProcInfo, ucMBTexture) == 4 &&
                   offsetof(struct tagPostProcInfo, iBlockDC) == 8 && offsetof(struct tagPostProcInfo, ucBlockTexture) == 72,
               "tagPostProcInfo layout");

typedef struct tagPostProcInfo* JxrPostTable[2];

size_t JxrPostInfoBytes_nid_no_patch(void) {
    return sizeof(struct tagPostProcInfo);
}

const char* JxrSlideOneMBRow_nid_no_patch(void* value, size_t channels, size_t width, int32_t top, int32_t bottom) {
    JxrPostTable* table = (JxrPostTable*)value;
    size_t channel;
    if (table == NULL) return "null post-processing table";
    if (channels > MAX_CHANNELS) return "more than 16 channels";
    for (channel = 0; width != 0 && channel < channels; ++channel)
        if ((top && table[channel][1] == NULL) || (bottom && table[channel][0] == NULL)) return "null post-processing row";
    slideOneMBRow(table, channels, width, top, bottom);
    return NULL;
}

const char* JxrUpdatePostProcInfo_nid_no_patch(void* value, PixelI* macroblock, size_t column, size_t channel) {
    JxrPostTable* table = (JxrPostTable*)value;
    if (table == NULL) return "null post-processing table";
    if (channel >= MAX_CHANNELS) return "channel out of range";
    if (table[channel][1] == NULL) return "null post-processing row";
    if (macroblock == NULL) return "null macroblock";
    updatePostProcInfo(table, macroblock, column, channel);
    return NULL;
}

static const char* CheckNeighbours(JxrPostTable* table, PixelI* previous, PixelI* current, size_t channel) {
    if (table == NULL) return "null post-processing table";
    if (channel >= MAX_CHANNELS) return "channel out of range";
    if (table[channel][0] == NULL || table[channel][1] == NULL) return "null post-processing row";
    if (previous == NULL || current == NULL) return "null macroblock row";
    return NULL;
}

const char* JxrPostProcMB_nid_no_patch(void* value, PixelI* previous, PixelI* current, size_t column, size_t channel,
                                       int32_t threshold) {
    JxrPostTable* table = (JxrPostTable*)value;
    const char* error = CheckNeighbours(table, previous, current, channel);
    if (error) return error;
    postProcMB(table, previous, current, column, channel, threshold);
    return NULL;
}

const char* JxrPostProcBlock_nid_no_patch(void* value, PixelI* previous, PixelI* current, size_t column, size_t channel,
                                          int32_t threshold) {
    JxrPostTable* table = (JxrPostTable*)value;
    const char* error = CheckNeighbours(table, previous, current, channel);
    if (error) return error;
    postProcBlock(table, previous, current, column, channel, threshold);
    return NULL;
}
