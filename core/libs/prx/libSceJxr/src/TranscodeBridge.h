#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JxrTranscodeEncoderFields {
    uintptr_t writePixels;
    uintptr_t transcode;
    uint8_t pixelFormat[16];
    uint32_t width;
    int32_t isWmp;
} JxrTranscodeEncoderFields;

typedef struct JxrTranscodeConverterFields {
    uintptr_t getPixelFormat;
    uintptr_t getSourcePixelFormat;
    uintptr_t copy;
    uintptr_t decoder;
} JxrTranscodeConverterFields;

typedef struct JxrTranscodeDecoderFields {
    size_t roiLeft;
    size_t roiTop;
    size_t roiWidth;
    size_t roiHeight;
    int32_t orientation;
    int32_t bitstreamFormat;
    uint8_t alphaMode;
    int32_t subband;
    int32_t ignoreOverlap;
} JxrTranscodeDecoderFields;

size_t JxrTranscodeEncoderSize_nid_no_patch(void);
size_t JxrTranscodeConverterSize_nid_no_patch(void);
size_t JxrTranscodeDecoderSize_nid_no_patch(void);
void JxrReadTranscodeEncoder_nid_no_patch(const void* record, JxrTranscodeEncoderFields* fields);
void JxrReadTranscodeConverter_nid_no_patch(const void* record, JxrTranscodeConverterFields* fields);
void JxrReadTranscodeDecoder_nid_no_patch(const void* record, JxrTranscodeDecoderFields* fields);
int32_t JxrIsTranscodeYuvStrideGuid_nid_no_patch(const void* guid);

#ifdef __cplusplus
}
#endif
