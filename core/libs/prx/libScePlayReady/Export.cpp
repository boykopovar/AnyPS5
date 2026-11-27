#include "prx/libScePlayReady/include/Api.hpp"
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iterator>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int PlayReadyInvalidArgument = static_cast<int>(0x80070057);
constexpr int PlayReadyNotStarted = static_cast<int>(0x8004c002);
constexpr std::size_t BufferSize = 0x18;
constexpr std::size_t InitParamsSize = 0x138;
constexpr uint32_t InitParamsMagic = 0x50524459;
constexpr uint32_t InitParamsVersion = 1;
constexpr uint64_t ApiVersions[2][4] = {{2, 5, 0, 1788}, {1, 3, 3, 2021111201}};

char versionText[32];

void InitializeInitParams(void* params) {
    if (params == nullptr) return;
    std::array<unsigned char, InitParamsSize> bytes{};
    std::memcpy(bytes.data(), &InitParamsMagic, sizeof(InitParamsMagic));
    std::memcpy(bytes.data() + sizeof(InitParamsMagic), &InitParamsVersion, sizeof(InitParamsVersion));
    const auto savedErrno = errno;
    GuestCopyOut_nid_no_patch(params, bytes.data(), bytes.size());
    errno = savedErrno;
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
    if (buffer == nullptr) return;
    const std::array<unsigned char, BufferSize> zeros{};
    const auto savedErrno = errno;
    GuestCopyOut_nid_no_patch(buffer, zeros.data(), zeros.size());
    errno = savedErrno;
}

int APS5_VABI sceBufferRealloc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI scePlayReadyApiInitializeInitParams(void* params) {
    InitializeInitParams(params);
}

int APS5_VABI scePlayReadyApiIsStarted() {
    return 0;
}

int APS5_VABI scePlayReadyApiStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyApiStop() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyApiVersion(uint32_t index, uint64_t* version) {
    if (index >= std::size(ApiVersions) || version == nullptr) return PlayReadyInvalidArgument;
    const auto savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(version, ApiVersions[index], sizeof(ApiVersions[index]))) {
        errno = savedErrno;
        return PlayReadyInvalidArgument;
    }
    errno = savedErrno;
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
    const std::array<unsigned char, BufferSize> zeros{};
    const auto savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(buffer, zeros.data(), zeros.size())) {
        errno = savedErrno;
        return PlayReadyInvalidArgument;
    }
    errno = savedErrno;
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
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyContentSetProperty() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyDomainCertEnumFini() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyDomainCertEnumInit() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyDomainCertEnumNext() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyDomainCertFind() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyDomainJoinGenerateChallenge() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyDomainJoinProcessResponse() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyDomainLeaveGenerateChallenge() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyDomainLeaveProcessResponse() {
    return PlayReadyNotStarted;
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
    return PlayReadyNotStarted;
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

int APS5_VABI scePlayReadyLicenseAcqGenerateAck(void* argument1, const void* argument2, void* argument3) {
    if (argument2 == nullptr || argument3 == nullptr) return PlayReadyInvalidArgument;
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyLicenseAcqGenerateChallenge() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyLicenseAcqGetAdditionalResponseData() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyLicenseAcqProcessAckResponse(void* argument1, const void* argument2) {
    if (argument2 == nullptr) return PlayReadyInvalidArgument;
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyLicenseAcqProcessResponse(void* argument1, std::uint32_t argument2, const void* argument3, void* argument4) {
    if (argument3 == nullptr || argument4 == nullptr) return PlayReadyInvalidArgument;
    return PlayReadyNotStarted;
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
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyReaderCommit() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyReaderDecrypt() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyReset() {
    return PlayReadyNotStarted;
}

int APS5_VABI scePlayReadyStoreCleanup() {
    return PlayReadyNotStarted;
}

}
