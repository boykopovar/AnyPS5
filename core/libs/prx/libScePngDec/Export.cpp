#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int32_t scePngDecCreate(const PngDecCreateParam* param, void* memory_address, uint32_t memory_size, void** handle) {
 (void)param;
 (void)memory_address;
 (void)memory_size;
 (void)handle;
 return 0;
}

int32_t scePngDecDecode(void* handle, const PngDecDecodeParam* param, PngDecImageInfo* image_info) {
 (void)handle;
 (void)param;
 (void)image_info;
 return 0;
}

int32_t scePngDecDelete(void* handle) {
 (void)handle;
 return 0;
}

int32_t scePngDecParseHeader(const PngDecParseParam* param, PngDecImageInfo* image_info) {
 (void)param;
 (void)image_info;
 return 0;
}

int32_t scePngDecQueryMemorySize(const PngDecCreateParam* param) {
 (void)param;
 return 0;
}

}
