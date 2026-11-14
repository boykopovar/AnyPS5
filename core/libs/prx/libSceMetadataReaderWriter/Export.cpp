#include "prx/libSceMetadataReaderWriter/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceMetadataReaderWriter/include/InventoryExports.hpp"
#include "prx/libSceMetadataReaderWriter/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

using SceMetadataReaderWriter::Metadata;
using SceMetadataReaderWriter::Value;

constexpr std::int64_t InitialMarker = -1;
constexpr std::int32_t LastValueType = 3;

void* APS5_VABI DefaultAllocate(std::size_t) {
    NotImplemented_nid_no_patch("Metadata default allocator");
}

void APS5_VABI DefaultRelease(void*) {
    NotImplemented_nid_no_patch("Metadata default release");
}

template <int TSlot>
void APS5_VABI UnknownVirtualSlot(Metadata*) {
    NotImplemented_nid_no_patch(("Metadata vtable slot " + std::to_string(TSlot)).c_str());
}

const void* const g_metadataVtable[] {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&_ZN23sceMetadataReaderWriter8MetadataD1Ev),
    reinterpret_cast<const void*>(&_ZN23sceMetadataReaderWriter8MetadataD0Ev),
    reinterpret_cast<const void*>(&UnknownVirtualSlot<2>),
    reinterpret_cast<const void*>(&UnknownVirtualSlot<3>),
    reinterpret_cast<const void*>(&UnknownVirtualSlot<4>),
    reinterpret_cast<const void*>(&UnknownVirtualSlot<5>),
};

void RequireObject(const void* object, const char* function) {
    if (object == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
}

void RequireNoFields(const Metadata* metadata, const char* function) {
    if (metadata->fields != nullptr) throw std::runtime_error(std::string(function) + ": metadata with field storage is not supported");
}

void Construct(Metadata* self, SceMetadataReaderWriter::Allocate allocate, SceMetadataReaderWriter::Release release, const char* function) {
    RequireObject(self, function);
    self->vtable = &g_metadataVtable[2];
    self->marker = InitialMarker;
    self->reserved = 0;
    self->allocate = allocate;
    self->release = release;
    self->fields = nullptr;
}

void ConstructCopy(Metadata* self, const Metadata* other, SceMetadataReaderWriter::Allocate allocate, SceMetadataReaderWriter::Release release, const char* function) {
    Construct(self, allocate, release, function);
    RequireObject(other, function);
    RequireNoFields(other, function);
}

void Destroy(Metadata* self, const char* function) {
    RequireObject(self, function);
    RequireNoFields(self, function);
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

int APS5_VABI _ZN23sceMetadataReaderWriter15ParserInterfaceC1ERKS0_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN23sceMetadataReaderWriter15ParserInterfaceC2ERKS0_() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

SceMetadataReaderWriter::Value* APS5_VABI _ZN23sceMetadataReaderWriter5ValueaSERS0_(SceMetadataReaderWriter::Value* self, SceMetadataReaderWriter::Value* other) {
    RequireObject(self, "Value::operator=");
    RequireObject(other, "Value::operator=");
    self->type = other->type;
    self->first = other->first;
    if (self->type < 0 || self->type > LastValueType) {
        printf_nid_postfix("[MDRW] ERROR(operator= 122): m_type(%d) is wrong.\n", self->type);
        return self;
    }
    self->second = other->second;
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

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1EPFPvmEPFvS1_E(SceMetadataReaderWriter::Metadata* self, SceMetadataReaderWriter::Allocate allocate, SceMetadataReaderWriter::Release release) {
    Construct(self, allocate, release, "Metadata::Metadata");
}

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_(SceMetadataReaderWriter::Metadata* self, const SceMetadataReaderWriter::Metadata* other) {
    ConstructCopy(self, other, DefaultAllocate, DefaultRelease, "Metadata::Metadata");
}

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_PFPvmEPFvS3_E(SceMetadataReaderWriter::Metadata* self, const SceMetadataReaderWriter::Metadata* other, SceMetadataReaderWriter::Allocate allocate, SceMetadataReaderWriter::Release release) {
    ConstructCopy(self, other, allocate, release, "Metadata::Metadata");
}

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1Ev(SceMetadataReaderWriter::Metadata* self) {
    Construct(self, DefaultAllocate, DefaultRelease, "Metadata::Metadata");
}

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2EPFPvmEPFvS1_E(SceMetadataReaderWriter::Metadata* self, SceMetadataReaderWriter::Allocate allocate, SceMetadataReaderWriter::Release release) {
    Construct(self, allocate, release, "Metadata::Metadata");
}

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2ERKS0_(SceMetadataReaderWriter::Metadata* self, const SceMetadataReaderWriter::Metadata* other) {
    ConstructCopy(self, other, DefaultAllocate, DefaultRelease, "Metadata::Metadata");
}

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2ERKS0_PFPvmEPFvS3_E(SceMetadataReaderWriter::Metadata* self, const SceMetadataReaderWriter::Metadata* other, SceMetadataReaderWriter::Allocate allocate, SceMetadataReaderWriter::Release release) {
    ConstructCopy(self, other, allocate, release, "Metadata::Metadata");
}

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2Ev(SceMetadataReaderWriter::Metadata* self) {
    Construct(self, DefaultAllocate, DefaultRelease, "Metadata::Metadata");
}

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataD0Ev(SceMetadataReaderWriter::Metadata* self) {
    Destroy(self, "Metadata::~Metadata");
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataD1Ev(SceMetadataReaderWriter::Metadata* self) {
    Destroy(self, "Metadata::~Metadata");
}

void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataD2Ev(SceMetadataReaderWriter::Metadata* self) {
    Destroy(self, "Metadata::~Metadata");
}

SceMetadataReaderWriter::Metadata* APS5_VABI _ZN23sceMetadataReaderWriter8MetadataaSERKS0_(SceMetadataReaderWriter::Metadata* self, const SceMetadataReaderWriter::Metadata* other) {
    RequireObject(self, "Metadata::operator=");
    RequireObject(other, "Metadata::operator=");
    RequireNoFields(self, "Metadata::operator=");
    RequireNoFields(other, "Metadata::operator=");
    return self;
}

int APS5_VABI _ZN23sceMetadataReaderWriter8finalizeEv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(const SceMetadataReaderWriter::Metadata* self) {
    RequireObject(self, "Metadata::getFieldCount");
    RequireNoFields(self, "Metadata::getFieldCount");
    return 0;
}

int APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata8getFieldEiRSsRNS_5ValueE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata8getValueERKSsRNS_5ValueE() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata9dumpValueEv(const SceMetadataReaderWriter::Metadata* self) {
    RequireObject(self, "Metadata::dumpValue");
    RequireNoFields(self, "Metadata::dumpValue");
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
