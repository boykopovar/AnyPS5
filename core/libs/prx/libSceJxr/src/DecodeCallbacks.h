#ifndef CORE_LIBS_PRX_LIBSCEJXR_SRC_DECODECALLBACKS_H
#define CORE_LIBS_PRX_LIBSCEJXR_SRC_DECODECALLBACKS_H

#include "prx/libc/include/general/VabiMacros.hpp"
#include <stdint.h>

typedef struct JxrDecodeCallbacks {
    int64_t (APS5_VABI *initialize)(void*, void*);
    int64_t (APS5_VABI *getPixelFormat)(void*, void*);
    int64_t (APS5_VABI *getSize)(void*, int32_t*, int32_t*);
    int64_t (APS5_VABI *getResolution)(void*, float*, float*);
    int64_t (APS5_VABI *getColorContext)(void*, uint8_t*, uint32_t*);
    int64_t (APS5_VABI *getDescriptiveMetadata)(void*, void*);
    int64_t (APS5_VABI *copy)(void*, const void*, uint8_t*, uint32_t);
    int64_t (APS5_VABI *getFrameCount)(void*, uint32_t*);
    int64_t (APS5_VABI *selectFrame)(void*, uint32_t);
    int64_t (APS5_VABI *release)(void**);
} JxrDecodeCallbacks;

#endif
