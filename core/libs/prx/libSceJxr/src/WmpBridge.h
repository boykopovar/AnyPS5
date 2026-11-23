#ifndef CORE_LIBS_PRX_LIBSCEJXR_SRC_WMPBRIDGE_H
#define CORE_LIBS_PRX_LIBSCEJXR_SRC_WMPBRIDGE_H

#include "prx/libc/include/general/VabiMacros.hpp"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    JXR_WMP_ACCEPTED = 0,
    JXR_WMP_REFUSE_STREAM = 1,
    JXR_WMP_REFUSE_FORMAT = 2,
    JXR_WMP_REFUSE_WIDE_TEXT = 3,
    JXR_WMP_REFUSE_STATE = 4,
    JXR_WMP_REFUSE_CALLBACK = 5,
    JXR_WMP_REFUSE_ALPHA_MODE = 6
};

typedef struct JxrWmpDecodeSlots {
    int64_t (APS5_VABI *initialize)(void*, void*);
    int64_t (APS5_VABI *getSize)(void*, int32_t*, int32_t*);
    int64_t (APS5_VABI *getRawStream)(void*, void**);
    int64_t (APS5_VABI *copy)(void*, const void*, uint8_t*, uint32_t);
    int64_t (APS5_VABI *getColorContext)(void*, uint8_t*, uint32_t*);
    int64_t (APS5_VABI *getDescriptiveMetadata)(void*, void*);
    int64_t (APS5_VABI *release)(void**);
} JxrWmpDecodeSlots;

void* JxrWmpRetainProxy_nid_no_patch(void* record, size_t size);
void JxrWmpMissingCallback_nid_no_patch(const char* name);

size_t JxrWmpEncodeParameterSize_nid_no_patch(void);
int32_t JxrWmpEncodePixelSpan_nid_no_patch(const void* encoder, uint32_t lines, uint32_t stride, size_t* bytes,
                                           int32_t* overflow);
int32_t JxrWmpDecodePixelSpan_nid_no_patch(const void* decoder, int32_t height, uint32_t stride,
                                           size_t* bytes, int32_t* overflow);
int64_t JxrWmpEncodeInitialize_nid_no_patch(void* encoder, void* stream, void* parameters, size_t size);
int64_t JxrWmpEncodeTerminate_nid_no_patch(void* encoder);
int64_t JxrWmpEncodeSetDescriptiveMetadata_nid_no_patch(void* encoder, const void* metadata, int32_t* refusal);
int64_t JxrWmpEncodeWritePixels_nid_no_patch(void* encoder, uint32_t lines, uint8_t* pixels, uint32_t stride,
                                             int32_t* refusal);
int64_t JxrWmpEncodeBandedBegin_nid_no_patch(void* encoder, void* temporary, int32_t* refusal);
int64_t JxrWmpEncodeBanded_nid_no_patch(void* encoder, uint32_t lines, uint8_t* pixels, uint32_t stride,
                                        int32_t last, int32_t* refusal);
int64_t JxrWmpEncodeBandedEnd_nid_no_patch(void* encoder, int32_t* refusal);
int64_t JxrWmpEncodeTranscode_nid_no_patch(void* encoder, void* decoder, void* parameters, int32_t* refusal);
int64_t JxrWmpEncodeCreateNewFrame_nid_no_patch(void* encoder, void* parameters, size_t size);
int64_t JxrWmpEncodeRelease_nid_no_patch(void** encoder, int32_t* refusal);

void JxrWmpDecodeInstall_nid_no_patch(void* decoder, const JxrWmpDecodeSlots* slots);
int64_t JxrWmpDecodeInitialize_nid_no_patch(void* decoder, void* stream);
int64_t JxrWmpDecodeGetSize_nid_no_patch(void* decoder, int32_t* width, int32_t* height);
int64_t JxrWmpDecodeGetRawStream_nid_no_patch(void* decoder, void** stream, int32_t* refusal);
int64_t JxrWmpDecodeCopy_nid_no_patch(void* decoder, const void* rectangle, uint8_t* pixels, uint32_t stride,
                                      int32_t* refusal);
int64_t JxrWmpDecodeGetColorContext_nid_no_patch(void* decoder, uint8_t* buffer, uint32_t* size, int32_t* refusal);
int64_t JxrWmpDecodeGetDescriptiveMetadata_nid_no_patch(void* decoder, void* metadata);
int64_t JxrWmpDecodeRelease_nid_no_patch(void** decoder, int32_t* refusal);

#ifdef __cplusplus
}
#endif

#endif
