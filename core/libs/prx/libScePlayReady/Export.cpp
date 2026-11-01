#include "prx/libScePlayReady/include/Api.hpp"
#include <charconv>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iterator>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int PlayReadyInvalidArgument = static_cast<int>(0x80070057);
constexpr std::size_t BufferSize = 0x18;
constexpr std::size_t InitParamsSize = 0x138;
constexpr uint32_t InitParamsMagic = 0x50524459;
constexpr uint32_t InitParamsVersion = 1;
constexpr uint64_t ApiVersions[2][4] = {{2, 5, 0, 1788}, {1, 3, 3, 2021111201}};

char versionText[32];

void InitializeInitParams(void* params) {
    if (params == nullptr) return;
    auto* bytes = static_cast<unsigned char*>(params);
    std::memset(bytes, 0, InitParamsSize);
    std::memcpy(bytes, &InitParamsMagic, sizeof(InitParamsMagic));
    std::memcpy(bytes + sizeof(InitParamsMagic), &InitParamsVersion, sizeof(InitParamsVersion));
}

} // namespace

extern "C" {

int APS5_VABI sceBufferAlloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBufferDup() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceBufferFree() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceBufferInit(void* buffer) {
    if (buffer != nullptr) std::memset(buffer, 0, BufferSize);
}

int APS5_VABI sceBufferRealloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI scePlayReadyApiInitializeInitParams(void* params) {
    InitializeInitParams(params);
}

int APS5_VABI scePlayReadyApiIsStarted() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyApiStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyApiStop() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyApiVersion(uint32_t index, uint64_t* version) {
    if (index >= std::size(ApiVersions) || version == nullptr) return PlayReadyInvalidArgument;
    std::memcpy(version, ApiVersions[index], sizeof(ApiVersions[index]));
    return 0;
}

const char* APS5_VABI scePlayReadyApiVersionString(uint32_t index) {
    if (index >= std::size(ApiVersions)) {
        std::memset(versionText, 0, sizeof(versionText));
        return versionText;
    }
    char text[sizeof(versionText)];
    char* end = text;
    for (std::size_t part = 0; part < std::size(ApiVersions[index]); ++part) {
        if (part != 0) *end++ = '.';
        end = std::to_chars(end, text + sizeof(text) - 1, ApiVersions[index][part]).ptr;
    }
    *end++ = '\0';
    std::memcpy(versionText, text, static_cast<std::size_t>(end - text));
    return versionText;
}

int APS5_VABI scePlayReadyBufferFree() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyBufferInit(void* buffer) {
    if (buffer == nullptr) return PlayReadyInvalidArgument;
    std::memset(buffer, 0, BufferSize);
    return 0;
}

int APS5_VABI scePlayReadyBufferMalloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyBufferRealloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyContentGetProperty() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyContentSetProperty() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyDomainCertEnumFini() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyDomainCertEnumInit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyDomainCertEnumNext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyDomainCertFind() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyDomainJoinGenerateChallenge() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyDomainJoinProcessResponse() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyDomainLeaveGenerateChallenge() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyDomainLeaveProcessResponse() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyEnvelopeClose() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyEnvelopeGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyEnvelopeInitializeRead() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyEnvelopeOpen() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyEnvelopeRead() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyEnvelopeSeek() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyFreeDecryptContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI scePlayReadyInitializeInitParams(void* params) {
    InitializeInitParams(params);
}

int APS5_VABI scePlayReadyLicenseAcqGenerateAck() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyLicenseAcqGenerateChallenge() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyLicenseAcqGetAdditionalResponseData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyLicenseAcqProcessAckResponse() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyLicenseAcqProcessResponse() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyMemAlloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyMemFree() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyMemGetStats() {
    return static_cast<int>(0x80004001);
}

int APS5_VABI scePlayReadyMemRealloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyNewDecryptContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReaderBind() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReaderCommit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReaderDecrypt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReset() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyStoreCleanup() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
