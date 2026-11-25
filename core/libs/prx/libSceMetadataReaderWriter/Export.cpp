#include "prx/libSceMetadataReaderWriter/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceMetadataReaderWriter/include/InventoryExports.hpp"
#include "prx/libSceMetadataReaderWriter/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"

namespace {

void CopyParserInterface(void* dst, const void* src, const char* caller) {
    if (dst == nullptr || src == nullptr) throw std::invalid_argument(std::string(caller) + ": null object");
    std::array<unsigned char, 56> bytes{};
    const int savedErrno = errno;
    const bool readOk = GuestCopyIn_nid_no_patch(bytes.data(), src, bytes.size());
    errno = savedErrno;
    if (!readOk) throw std::invalid_argument(std::string(caller) + ": unmapped source");
    const bool writeOk = GuestCopyOut_nid_no_patch(dst, bytes.data(), bytes.size());
    errno = savedErrno;
    if (!writeOk) throw std::invalid_argument(std::string(caller) + ": unmapped destination");
}

void ZeroThumbnail(void* thumbnail, const char* nid) {
    if (thumbnail == nullptr) throw std::invalid_argument(std::string(nid) + ": null object");
    constexpr std::array<unsigned char, 24> zeros{};
    const int savedErrno = errno;
    const bool wrote = GuestCopyOut_nid_no_patch(thumbnail, zeros.data(), zeros.size());
    errno = savedErrno;
    if (!wrote) throw std::invalid_argument(std::string(nid) + ": unmapped object");
}

void ReleaseThumbnail(void* thumbnail, const char* nid) {
    if (thumbnail == nullptr) throw std::invalid_argument(std::string(nid) + ": null object");
    std::uint64_t size = 0;
    std::uint64_t addr = 0;
    auto* bytes = static_cast<unsigned char*>(thumbnail);
    const int savedErrno = errno;
    const bool readSize = GuestCopyIn_nid_no_patch(&size, bytes, sizeof(size));
    errno = savedErrno;
    if (!readSize) throw std::invalid_argument(std::string(nid) + ": unmapped object");
    const bool readAddr = GuestCopyIn_nid_no_patch(&addr, bytes + 0x10, sizeof(addr));
    errno = savedErrno;
    if (!readAddr) throw std::invalid_argument(std::string(nid) + ": unmapped object");
    if (addr != 0) {
        sceKernelMunmap(addr, static_cast<std::size_t>(size));
    }
    ZeroThumbnail(thumbnail, nid);
}

}

extern "C" {

int APS5_VABI _ZN23sceMetadataReaderWriter10initializeEv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter11getMimeTypeERKSsRSt6vectorIjSaIjEE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter12getMimeTypesERKSsiPi() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter12readMetadataERKSsRKNS_8MetadataEjRS2_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter13writeMetadataERKSsRKNS_8MetadataEj() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter13writeMetadataERKSsRKNS_8MetadataEjPS2_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI _ZN23sceMetadataReaderWriter15ParserInterfaceC1ERKS0_(void* dst, const void* src) {
    CopyParserInterface(dst, src, __func__);
}

void APS5_VABI _ZN23sceMetadataReaderWriter15ParserInterfaceC2ERKS0_(void* dst, const void* src) {
    CopyParserInterface(dst, src, __func__);
}

int APS5_VABI _ZN23sceMetadataReaderWriter16extractThumbnailERKSsjRNS_9ThumbnailE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter16getNumOfMimeTypeERKSsPi() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter17getParserInfoListERSt6vectorINS_10ParserInfoESaIS1_EE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter18registerParserInfoERKSt6vectorINS_10ParserInfoESaIS1_EE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter20unregisterParserInfoEv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter23registerParserInterfaceEjRKNS_15ParserInterfaceE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void* APS5_VABI _ZN23sceMetadataReaderWriter5ValueaSERS0_(void* dst, const void* src) {
    if (dst == nullptr || src == nullptr) throw std::invalid_argument("_ZN23sceMetadataReaderWriter5ValueaSERS0_: null object");
    const auto* from = static_cast<const unsigned char*>(src);
    auto* to = static_cast<unsigned char*>(dst);
    std::uint32_t kind = 0;
    std::uint64_t size = 0;
    std::uint64_t payload = 0;
    const int savedErrno = errno;
    const bool readKind = GuestCopyIn_nid_no_patch(&kind, from, sizeof(kind));
    errno = savedErrno;
    if (!readKind) throw std::invalid_argument("_ZN23sceMetadataReaderWriter5ValueaSERS0_: unmapped source");
    const bool readSize = GuestCopyIn_nid_no_patch(&size, from + 8, sizeof(size));
    errno = savedErrno;
    if (!readSize) throw std::invalid_argument("_ZN23sceMetadataReaderWriter5ValueaSERS0_: unmapped source");
    if (kind <= 3u) {
        const bool readPayload = GuestCopyIn_nid_no_patch(&payload, from + 0x10, sizeof(payload));
        errno = savedErrno;
        if (!readPayload) throw std::invalid_argument("_ZN23sceMetadataReaderWriter5ValueaSERS0_: unmapped source");
    }
    const bool writeKind = GuestCopyOut_nid_no_patch(to, &kind, sizeof(kind));
    errno = savedErrno;
    if (!writeKind) throw std::invalid_argument("_ZN23sceMetadataReaderWriter5ValueaSERS0_: unmapped destination");
    const bool writeSize = GuestCopyOut_nid_no_patch(to + 8, &size, sizeof(size));
    errno = savedErrno;
    if (!writeSize) throw std::invalid_argument("_ZN23sceMetadataReaderWriter5ValueaSERS0_: unmapped destination");
    if (kind <= 3u) {
        const bool writePayload = GuestCopyOut_nid_no_patch(to + 0x10, &payload, sizeof(payload));
        errno = savedErrno;
        if (!writePayload) throw std::invalid_argument("_ZN23sceMetadataReaderWriter5ValueaSERS0_: unmapped destination");
    }
    return dst;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata11DeserializeERN3sce4Json5ValueERS0_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata11deleteFieldERKSs() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata15iterateAllFieldEPFiRKSsRKNS_5ValueEbE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata8getFieldEiRSsRNS_5ValueERb() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata8setFieldERKSs() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata8setFieldERKSsRKNS_5ValueE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata9SerializeERS0_RN3sce4Json5ValueE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1EPFPvmEPFvS1_E() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_PFPvmEPFvS3_E() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1Ev() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2EPFPvmEPFvS1_E() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2ERKS0_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2ERKS0_PFPvmEPFvS3_E() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2Ev() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataD0Ev() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataD1Ev() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataD2Ev() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8MetadataaSERKS0_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8finalizeEv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(const void* metadata) {
    if (metadata == nullptr) throw std::invalid_argument("_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv: null object");
    std::uint64_t implAddr = 0;
    const auto* base = static_cast<const unsigned char*>(metadata);
    const int savedErrno = errno;
    const bool readImpl = GuestCopyIn_nid_no_patch(&implAddr, base + 0x28, sizeof(implAddr));
    errno = savedErrno;
    if (!readImpl) throw std::invalid_argument("_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv: unmapped object");
    if (implAddr == 0) return 0;
    std::int32_t count = 0;
    const auto* impl = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(implAddr));
    const bool readCount = GuestCopyIn_nid_no_patch(&count, impl + 4, sizeof(count));
    errno = savedErrno;
    if (!readCount) throw std::invalid_argument("_ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv: unmapped impl");
    return count;
}

int APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata8getFieldEiRSsRNS_5ValueE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata8getValueERKSsRNS_5ValueE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata9dumpValueEv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

void APS5_VABI libSceMetadataReaderWriterUnknown_2oFr2hbrkRs_nid_no_patch(void* thumbnail) {
    ReleaseThumbnail(thumbnail, "2oFr2hbrkRs");
}
APS5_EXPORT("2oFr2hbrkRs", libSceMetadataReaderWriterUnknown_2oFr2hbrkRs_nid_no_patch);

int APS5_VABI libSceMetadataReaderWriterUnknown_ElB1bNKymPg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ElB1bNKymPg", libSceMetadataReaderWriterUnknown_ElB1bNKymPg_nid_no_patch);

int APS5_VABI libSceMetadataReaderWriterUnknown_GCOzTsybYr0_nid_no_patch(void* thumbnail) {
    ReleaseThumbnail(thumbnail, "GCOzTsybYr0");
    return 0;
}
APS5_EXPORT("GCOzTsybYr0", libSceMetadataReaderWriterUnknown_GCOzTsybYr0_nid_no_patch);

void APS5_VABI libSceMetadataReaderWriterUnknown_Ly4V1dsyKCA_nid_no_patch(void* thumbnail) {
    ZeroThumbnail(thumbnail, "Ly4V1dsyKCA");
}
APS5_EXPORT("Ly4V1dsyKCA", libSceMetadataReaderWriterUnknown_Ly4V1dsyKCA_nid_no_patch);

void APS5_VABI libSceMetadataReaderWriterUnknown_YeIbFGmyUVM_nid_no_patch(void* thumbnail) {
    ReleaseThumbnail(thumbnail, "YeIbFGmyUVM");
}
APS5_EXPORT("YeIbFGmyUVM", libSceMetadataReaderWriterUnknown_YeIbFGmyUVM_nid_no_patch);

void APS5_VABI libSceMetadataReaderWriterUnknown_ckVz4iaNj4E_nid_no_patch(void* thumbnail) {
    ZeroThumbnail(thumbnail, "ckVz4iaNj4E");
}
APS5_EXPORT("ckVz4iaNj4E", libSceMetadataReaderWriterUnknown_ckVz4iaNj4E_nid_no_patch);

}

extern "C" {

int APS5_VABI metadataReaderLegacyUnknown_2n7W08UL4yA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2n7W08UL4yA", metadataReaderLegacyUnknown_2n7W08UL4yA_nid_no_patch);

int APS5_VABI metadataReaderLegacyUnknown_4SkB_plus_X3N5F0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4SkB+X3N5F0", metadataReaderLegacyUnknown_4SkB_plus_X3N5F0_nid_no_patch);

}
