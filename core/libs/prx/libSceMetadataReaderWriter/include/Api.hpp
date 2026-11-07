#pragma once

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

namespace SceJson2 {

struct Value;

}

namespace SceMetadataReaderWriter {

using Allocate = void* (APS5_VABI*)(std::size_t);
using Release = void (APS5_VABI*)(void*);

struct Metadata {
    const void* const* vtable;
    std::int64_t marker;
    std::uint64_t reserved;
    Allocate allocate;
    Release release;
    void* fields;
};

struct Value {
    std::int32_t type;
    std::uint32_t padding;
    std::uint64_t first;
    std::uint64_t second;
};

static_assert(sizeof(Metadata) == 48);
static_assert(sizeof(Value) == 24);

}

extern "C" {

int APS5_VABI _ZN23sceMetadataReaderWriter10initializeEv(void);
int APS5_VABI _ZN23sceMetadataReaderWriter11getMimeTypeERKSsRSt6vectorIjSaIjEE(void);
int APS5_VABI _ZN23sceMetadataReaderWriter12getMimeTypesERKSsiPi(void);
int APS5_VABI _ZN23sceMetadataReaderWriter12readMetadataERKSsRKNS_8MetadataEjRS2_(void);
int APS5_VABI _ZN23sceMetadataReaderWriter13writeMetadataERKSsRKNS_8MetadataEj(void);
int APS5_VABI _ZN23sceMetadataReaderWriter13writeMetadataERKSsRKNS_8MetadataEjPS2_(void);
int APS5_VABI _ZN23sceMetadataReaderWriter15ParserInterfaceC1ERKS0_(void);
int APS5_VABI _ZN23sceMetadataReaderWriter15ParserInterfaceC2ERKS0_(void);
int APS5_VABI _ZN23sceMetadataReaderWriter16extractThumbnailERKSsjRNS_9ThumbnailE(void);
int APS5_VABI _ZN23sceMetadataReaderWriter16getNumOfMimeTypeERKSsPi(void);
int APS5_VABI _ZN23sceMetadataReaderWriter17getParserInfoListERSt6vectorINS_10ParserInfoESaIS1_EE(void);
int APS5_VABI _ZN23sceMetadataReaderWriter18registerParserInfoERKSt6vectorINS_10ParserInfoESaIS1_EE(void);
int APS5_VABI _ZN23sceMetadataReaderWriter20unregisterParserInfoEv(void);
int APS5_VABI _ZN23sceMetadataReaderWriter23registerParserInterfaceEjRKNS_15ParserInterfaceE(void);
SceMetadataReaderWriter::Value* APS5_VABI _ZN23sceMetadataReaderWriter5ValueaSERS0_(SceMetadataReaderWriter::Value* self, SceMetadataReaderWriter::Value* other);
int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata11DeserializeERN3sce4Json5ValueERS0_(SceJson2::Value* value, SceMetadataReaderWriter::Metadata* metadata);
int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata11deleteFieldERKSs(void);
int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata15iterateAllFieldEPFiRKSsRKNS_5ValueEbE(void);
int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata8getFieldEiRSsRNS_5ValueERb(void);
int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata8setFieldERKSs(void);
int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata8setFieldERKSsRKNS_5ValueE(void);
int APS5_VABI _ZN23sceMetadataReaderWriter8Metadata9SerializeERS0_RN3sce4Json5ValueE(SceMetadataReaderWriter::Metadata* metadata, SceJson2::Value* value);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1EPFPvmEPFvS1_E(SceMetadataReaderWriter::Metadata* self, SceMetadataReaderWriter::Allocate allocate, SceMetadataReaderWriter::Release release);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_(SceMetadataReaderWriter::Metadata* self, const SceMetadataReaderWriter::Metadata* other);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1ERKS0_PFPvmEPFvS3_E(SceMetadataReaderWriter::Metadata* self, const SceMetadataReaderWriter::Metadata* other, SceMetadataReaderWriter::Allocate allocate, SceMetadataReaderWriter::Release release);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC1Ev(SceMetadataReaderWriter::Metadata* self);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2EPFPvmEPFvS1_E(SceMetadataReaderWriter::Metadata* self, SceMetadataReaderWriter::Allocate allocate, SceMetadataReaderWriter::Release release);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2ERKS0_(SceMetadataReaderWriter::Metadata* self, const SceMetadataReaderWriter::Metadata* other);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2ERKS0_PFPvmEPFvS3_E(SceMetadataReaderWriter::Metadata* self, const SceMetadataReaderWriter::Metadata* other, SceMetadataReaderWriter::Allocate allocate, SceMetadataReaderWriter::Release release);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataC2Ev(SceMetadataReaderWriter::Metadata* self);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataD0Ev(SceMetadataReaderWriter::Metadata* self);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataD1Ev(SceMetadataReaderWriter::Metadata* self);
void APS5_VABI _ZN23sceMetadataReaderWriter8MetadataD2Ev(SceMetadataReaderWriter::Metadata* self);
SceMetadataReaderWriter::Metadata* APS5_VABI _ZN23sceMetadataReaderWriter8MetadataaSERKS0_(SceMetadataReaderWriter::Metadata* self, const SceMetadataReaderWriter::Metadata* other);
int APS5_VABI _ZN23sceMetadataReaderWriter8finalizeEv(void);
int APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata13getFieldCountEv(const SceMetadataReaderWriter::Metadata* self);
int APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata8getFieldEiRSsRNS_5ValueE(void);
int APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata8getValueERKSsRNS_5ValueE(void);
void APS5_VABI _ZNK23sceMetadataReaderWriter8Metadata9dumpValueEv(const SceMetadataReaderWriter::Metadata* self);

}
