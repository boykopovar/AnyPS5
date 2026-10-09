#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_PSML_ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x8A810001);

}

extern "C" {

std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement1100(void* requirement, const void* param) {
 (void)requirement;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrCreateContext1100(void** context, const void* param) {
 (void)context;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1100(void* context, void* commandBuffer, const void* param) {
 (void)context;
 (void)commandBuffer;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrGetSharedResourcesInitRequirement(void* requirement, const void* param) {
 (void)requirement;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePsmlMfsrInit() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(const void* context, std::uint32_t* sizeInDwords) {
 (void)context;
 (void)sizeInDwords;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement800M3_2(void* requirement, const void* param) {
 (void)requirement;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePsmlMfsrSelectConfig() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePsmlMfsrGetMipmapBias() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrRequestCapture(const void* object) {
 (void)object;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrReleaseContext(void* context) {
 (void)context;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePsmlMfsrIsCaptureInProgress() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket900(void* context, void* commandBuffer, const void* param) {
 (void)context;
 (void)commandBuffer;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrCreateSharedResources(void* sharedResources, const void* param) {
 (void)sharedResources;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrCreateContext800M3_2(void* context, const void* param) {
 (void)context;
 (void)param;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

std::int32_t APS5_VABI scePsmlMfsrReleaseSharedResources(void* sharedResources) {
 (void)sharedResources;
 return SCE_PSML_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePsmlMfsr2ReleaseSharedResources() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2GetSharedResourcesInitRequirement() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2GetContextInitRequirement() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2ReleaseContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2GetDispatchPacketsSizeInDwords() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2CreateSharedResources() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2GetDispatchPackets() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2CreateContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePsmlMfsr2Init() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
