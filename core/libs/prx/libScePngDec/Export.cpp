#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int32_t APS5_VABI scePngDecCreate(const PngDecCreateParam* param, void* memory_address, uint32_t memory_size, void** handle) {
 (void)param;
 (void)memory_address;
 (void)memory_size;
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t APS5_VABI scePngDecDecode(void* handle, const PngDecDecodeParam* param, PngDecImageInfo* image_info) {
 (void)handle;
 (void)param;
 (void)image_info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t APS5_VABI scePngDecDelete(void* handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t APS5_VABI scePngDecParseHeader(const PngDecParseParam* param, PngDecImageInfo* image_info) {
 (void)param;
 (void)image_info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int32_t APS5_VABI scePngDecQueryMemorySize(const PngDecCreateParam* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
