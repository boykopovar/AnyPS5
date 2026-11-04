#include "prx/libSceMetadataReaderWriter/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceMetadataReaderWriter/include/InventoryExports.hpp"
#include "prx/libSceMetadataReaderWriter/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

using SceMetadataReaderWriter::Metadata;
using SceMetadataReaderWriter::Value;

constexpr std::int64_t InitialMarker = -1;
constexpr std::int64_t DeserializedMarker = 0;
constexpr std::int32_t LastValueType = 3;
constexpr std::int64_t SerializedUnsetIdentifier = 0xFFFFFFFF;
constexpr const char* FieldListKey = "field_list";

struct SerializedInteger {
    const char* key;
    std::int64_t value;
};

constexpr SerializedInteger g_serializedIntegers[] {
    {"mimeType", SerializedUnsetIdentifier},
    {"mimeTypeFormatVersion", 0},
    {"parserId", SerializedUnsetIdentifier},
    {"promoterFormatVersion", 0},
};

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

void RequireConstructedState(const Metadata* metadata, const char* function) {
    RequireNoFields(metadata, function);
    if (metadata->marker != InitialMarker || metadata->reserved != 0)
        throw std::runtime_error(std::string(function) + ": metadata that is not in its constructed state is not supported");
}

class JsonString {
public:
    explicit JsonString(const char* text) { _ZN3sce4Json6StringC1EPKc(&string, text); }
    ~JsonString() { _ZN3sce4Json6StringD1Ev(&string); }
    JsonString(const JsonString&) = delete;
    JsonString& operator=(const JsonString&) = delete;
    const SceJson2::String* Get() const { return &string; }

private:
    SceJson2::String string{};
};

class JsonValue {
public:
    explicit JsonValue(SceJson2::ValueType type) { _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&value, type); }
    ~JsonValue() { _ZN3sce4Json5ValueD1Ev(&value); }
    JsonValue(const JsonValue&) = delete;
    JsonValue& operator=(const JsonValue&) = delete;
    SceJson2::Value* Get() { return &value; }

private:
    SceJson2::Value value{};
};

SceJson2::Value* Entry(SceJson2::Object* object, const char* key) {
    const JsonString name(key);
    return _ZN3sce4Json6ObjectixERKNS0_6StringE(object, name.Get());
}

SceJson2::Value* Member(SceJson2::Value* object, const char* key) {
    const JsonString name(key);
    return _ZN3sce4Json5Value10referValueERKNS0_6StringE(object, name.Get());
}

void RequireReplaceable(const SceJson2::Value* value) {
    switch (_ZNK3sce4Json5Value7getTypeEv(value)) {
        case SceJson2::TypeNull:
        case SceJson2::TypeInteger:
        case SceJson2::TypeString:
        case SceJson2::TypeObject:
            return;
        default:
            throw std::runtime_error("Metadata::Serialize: a destination value of this type is not supported");
    }
}

bool HasSerializedConstructedFields(SceJson2::Value* value) {
    if (_ZNK3sce4Json5Value5countEv(value) != std::size(g_serializedIntegers) + 1) return false;
    SceJson2::Value* fieldList = Member(value, FieldListKey);
    if (fieldList == nullptr || _ZNK3sce4Json5Value7getTypeEv(fieldList) != SceJson2::TypeArray || _ZNK3sce4Json5Value5countEv(fieldList) != 0) return false;
    for (const auto& field : g_serializedIntegers) {
        SceJson2::Value* member = Member(value, field.key);
        if (member == nullptr || _ZNK3sce4Json5Value7getTypeEv(member) != SceJson2::TypeInteger) return false;
        if (*_ZNK3sce4Json5Value10getIntegerEv(member) != field.value) return false;
    }
    return true;
}

bool IsSerializedConstructedState(SceJson2::Value* value) {
    switch (_ZNK3sce4Json5Value7getTypeEv(value)) {
        case SceJson2::TypeNull:
        case SceJson2::TypeInteger:
        case SceJson2::TypeString:
            return false;
        case SceJson2::TypeObject:
            break;
        default:
            throw std::runtime_error("Metadata::Deserialize: a value of this type is not supported");
    }
    if (_ZNK3sce4Json5Value5countEv(value) == 0) return false;
    if (HasSerializedConstructedFields(value)) return true;
    throw std::runtime_error("Metadata::Deserialize: an object other than the serialized constructed state is not supported");
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

int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata11DeserializeERN3sce4Json5ValueERS0_(SceJson2::Value* value, SceMetadataReaderWriter::Metadata* metadata) {
    RequireObject(value, "Metadata::Deserialize");
    RequireObject(metadata, "Metadata::Deserialize");
    RequireConstructedState(metadata, "Metadata::Deserialize");
    metadata->marker = IsSerializedConstructedState(value) ? InitialMarker : DeserializedMarker;
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

int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata9SerializeERS0_RN3sce4Json5ValueE(SceMetadataReaderWriter::Metadata* metadata, SceJson2::Value* value) {
    RequireObject(metadata, "Metadata::Serialize");
    RequireObject(value, "Metadata::Serialize");
    RequireConstructedState(metadata, "Metadata::Serialize");
    RequireReplaceable(value);
    JsonValue serialized(SceJson2::TypeObject);
    SceJson2::Object* object = _ZN3sce4Json5Value11referObjectEv(serialized.Get());
    _ZN3sce4Json5Value3setENS0_9ValueTypeE(Entry(object, FieldListKey), SceJson2::TypeArray);
    for (const auto& field : g_serializedIntegers) _ZN3sce4Json5Value3setEl(Entry(object, field.key), field.value);
    _ZN3sce4Json5ValueaSERKS1_(value, serialized.Get());
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
