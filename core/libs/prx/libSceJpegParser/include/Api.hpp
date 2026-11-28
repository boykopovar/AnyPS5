#pragma once

#include "SceTypes.hpp"

struct JpegParserThumbnail {
    std::uint32_t type;
    std::uint32_t reserved04;
    std::uint64_t mappingSize;
    std::uint64_t size;
    void* data;
    std::uint32_t format;
    std::uint32_t reserved24;
    std::uint64_t width;
    std::uint64_t height;
};
static_assert(sizeof(JpegParserThumbnail) == 0x38);

extern "C" {

int APS5_VABI libSceJpegParserReadMetadata_nid_no_patch(const void* path, void* request, void* output);
int APS5_VABI libSceJpegParserUnknown_v3aOHSEXfjo_nid_no_patch(const void* config);
int APS5_VABI libSceJpegParserUnknown_minus_0oLMyS3AcA_nid_no_patch(const void* path, JpegParserThumbnail* thumbnail);

int APS5_VABI _ZN23sceMetadataReaderWriter10jpegParser18jpegParserFinalizeEv(void);
int APS5_VABI _ZN23sceMetadataReaderWriter10jpegParser20jpegParserInitializeEv(void);
int APS5_VABI _ZN23sceMetadataReaderWriter10jpegParser22jpegParserReadMetadataERKSsRKNS_8MetadataERS3_(void);
int APS5_VABI _ZN23sceMetadataReaderWriter10jpegParser23jpegParserWriteMetadataERKSsRKNS_8MetadataE(void);
int APS5_VABI _ZN23sceMetadataReaderWriter10jpegParser26jpegParserExtractThumbnailERKSsRNS_9ThumbnailE(void);

}
