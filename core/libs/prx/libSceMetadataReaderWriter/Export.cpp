#include "prx/libSceMetadataReaderWriter/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceMetadataReaderWriter/include/InventoryExports.hpp"
#include "prx/libSceMetadataReaderWriter/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::size_t ParserInterfaceBytes = 0x38;
constexpr std::size_t MetadataFieldsOffset = 0x28;
constexpr std::size_t MetadataFieldCountOffset = 0x4;
constexpr std::size_t ValueTypeOffset = 0x0;
constexpr std::size_t ValueWordOffset = 0x8;
constexpr std::size_t ValuePayloadOffset = 0x10;
constexpr std::uint32_t ValueMaxType = 3;

void ReadGuestBytes(void* destination, const void* source, const std::size_t bytes, const char* error) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(error);
}

void WriteGuestBytes(void* destination, const void* source, const std::size_t bytes, const char* error) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(error);
}

void CopyParserInterface(void* self, const void* source) {
    if (self == nullptr || source == nullptr) throw std::invalid_argument("sceMetadataReaderWriter::ParserInterface: null object");
    std::byte copy[ParserInterfaceBytes] {};
    ReadGuestBytes(copy, source, sizeof(copy), "sceMetadataReaderWriter::ParserInterface: unreadable source");
    WriteGuestBytes(self, copy, sizeof(copy), "sceMetadataReaderWriter::ParserInterface: unwritable destination");
}

} // namespace

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

void APS5_VABI _ZN23sceMetadataReaderWriter15ParserInterfaceC1ERKS0_(void* self, const void* source) {
    CopyParserInterface(self, source);
}

void APS5_VABI _ZN23sceMetadataReaderWriter15ParserInterfaceC2ERKS0_(void* self, const void* source) {
    CopyParserInterface(self, source);
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

void* APS5_VABI _ZN23sceMetadataReaderWriter5ValueaSERS0_(void* self, const void* source) {
    if (self == nullptr || source == nullptr) throw std::invalid_argument("sceMetadataReaderWriter::Value::operator=: null value");
    auto* destination = static_cast<std::byte*>(self);
    const auto* origin = static_cast<const std::byte*>(source);
    std::uint32_t type = 0;
    ReadGuestBytes(&type, origin + ValueTypeOffset, sizeof(type), "sceMetadataReaderWriter::Value::operator=: unreadable type");
    WriteGuestBytes(destination + ValueTypeOffset, &type, sizeof(type), "sceMetadataReaderWriter::Value::operator=: unwritable type");
    std::uint64_t word = 0;
    ReadGuestBytes(&word, origin + ValueWordOffset, sizeof(word), "sceMetadataReaderWriter::Value::operator=: unreadable word");
    WriteGuestBytes(destination + ValueWordOffset, &word, sizeof(word), "sceMetadataReaderWriter::Value::operator=: unwritable word");
    if (type > ValueMaxType) return self;
    std::uint64_t payload = 0;
    ReadGuestBytes(&payload, origin + ValuePayloadOffset, sizeof(payload), "sceMetadataReaderWriter::Value::operator=: unreadable payload");
    WriteGuestBytes(destination + ValuePayloadOffset, &payload, sizeof(payload), "sceMetadataReaderWriter::Value::operator=: unwritable payload");
    return self;
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

int APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(const void* self) {
    if (self == nullptr) throw std::invalid_argument("sceMetadataReaderWriter::Metadata::getFieldCount: null object");
    std::uint64_t fields = 0;
    ReadGuestBytes(&fields, static_cast<const std::byte*>(self) + MetadataFieldsOffset, sizeof(fields), "sceMetadataReaderWriter::Metadata::getFieldCount: unreadable field container");
    if (fields == 0) return 0;
    std::uint32_t count = 0;
    ReadGuestBytes(&count, reinterpret_cast<const std::byte*>(fields) + MetadataFieldCountOffset, sizeof(count), "sceMetadataReaderWriter::Metadata::getFieldCount: unreadable field count");
    return static_cast<int>(count);
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

int APS5_VABI libSceMetadataReaderWriterUnknown_2oFr2hbrkRs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2oFr2hbrkRs", libSceMetadataReaderWriterUnknown_2oFr2hbrkRs_nid_no_patch);

int APS5_VABI libSceMetadataReaderWriterUnknown_ElB1bNKymPg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ElB1bNKymPg", libSceMetadataReaderWriterUnknown_ElB1bNKymPg_nid_no_patch);

int APS5_VABI libSceMetadataReaderWriterUnknown_GCOzTsybYr0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GCOzTsybYr0", libSceMetadataReaderWriterUnknown_GCOzTsybYr0_nid_no_patch);

int APS5_VABI libSceMetadataReaderWriterUnknown_Ly4V1dsyKCA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Ly4V1dsyKCA", libSceMetadataReaderWriterUnknown_Ly4V1dsyKCA_nid_no_patch);

int APS5_VABI libSceMetadataReaderWriterUnknown_YeIbFGmyUVM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YeIbFGmyUVM", libSceMetadataReaderWriterUnknown_YeIbFGmyUVM_nid_no_patch);

int APS5_VABI libSceMetadataReaderWriterUnknown_ckVz4iaNj4E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
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
