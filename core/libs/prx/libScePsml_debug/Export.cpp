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

int APS5_VABI scePsmlMfsrGetSharedResourcesInitRequirement() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrInit() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrGetDispatchMfsrPacketSizeInDwords() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrGetContextBufferRequirement800M3_2() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrSelectConfig() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrGetMipmapBias() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrRequestCapture() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrReleaseContext() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrIsCaptureInProgress() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrGetDispatchMfsrPacket900() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrCreateSharedResources() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrCreateContext800M3_2() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsrReleaseSharedResources() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsr2ReleaseSharedResources() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsr2GetSharedResourcesInitRequirement() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsr2GetContextInitRequirement() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsr2ReleaseContext() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsr2GetDispatchPacketsSizeInDwords() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsr2CreateSharedResources() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsr2GetDispatchPackets() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsr2CreateContext() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI scePsmlMfsr2Init() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

}

