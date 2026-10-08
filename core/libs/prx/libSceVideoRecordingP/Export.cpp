#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::mutex g_infoMutex;
std::map<int32_t, std::vector<unsigned char>> g_info;
}

extern "C" {

int APS5_VABI sceVideoRecordingClose(int discard) {
 (void)discard;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingGetInfo(int32_t info, void* data, size_t size) {
    if (data == nullptr) APS5_INVALID_ARG_EX;
    std::lock_guard lock(g_infoMutex);
    const auto found = g_info.find(info);
    if (found == g_info.end()) throw std::logic_error(std::string(__func__) + ": info " + std::to_string(info) + " was never set");
    if (size < found->second.size()) APS5_INVALID_ARG_EX;
    std::memcpy(data, found->second.data(), found->second.size());
    return 0;
}

int APS5_VABI sceVideoRecordingGetStatus(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingOpen(const char* path, const void* param, void* heap, int heapSize) {
 (void)path;
 (void)param;
 (void)heap;
 (void)heapSize;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingQueryMemSize(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingSetInfo(int32_t info, const void* data, size_t size) {
    if (data == nullptr && size != 0) APS5_INVALID_ARG_EX;
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::lock_guard lock(g_infoMutex);
    g_info[info].assign(bytes, bytes + size);
    return 0;
}

int APS5_VABI sceVideoRecordingStart(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVideoRecordingStop(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("iQS6DUtLybE", videoRecordingUnknown_iQS6DUtLybE);
int APS5_VABI videoRecordingUnknown_iQS6DUtLybE(void) {
 NotImplemented_nid_no_patch("iQS6DUtLybE");
 return 0;
}

}
