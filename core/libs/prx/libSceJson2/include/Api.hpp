#pragma once

#include "SceTypes.hpp"
#include <algorithm>
#include <bit>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <list>
#include <locale>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace SceJson2 {

enum ValueType : std::int32_t {
    TypeNull = 0,
    TypeBoolean = 1,
    TypeInteger = 2,
    TypeUInteger = 3,
    TypeReal = 4,
    TypeString = 5,
    TypeArray = 6,
    TypeObject = 7,
};

}

namespace SceJson2 {

struct Node;

}

namespace SceJson2 {

struct Value { Node* node; };

}

namespace SceJson2 {

struct String { std::string* text; };

}

namespace SceJson2 {

struct Array { std::list<Value>* items; };

}

namespace SceJson2 {

struct Pair;

}

namespace SceJson2 {

struct Object { std::list<Pair>* items; };

}

namespace SceJson2 {

struct ArrayIteratorState {
    std::list<Value>::iterator position;
    std::list<Value>::iterator end;
};

}

namespace SceJson2 {

struct ArrayIterator { ArrayIteratorState* state; };

}

namespace SceJson2 {

struct ObjectIteratorState {
    std::list<Pair>* owner = nullptr;
    std::list<Pair>::iterator position{};
};

}

namespace SceJson2 {

struct ObjectIterator { ObjectIteratorState* state; };

}

namespace SceJson2 {

struct Pair {
    String key;
    std::uint64_t reserved;
    Value value;
};

}

namespace SceJson2 {

using TypeMismatchHandler = const Value& (APS5_VABI*)(ValueType, const Value*, const Value*, void*);

}

namespace SceJson2 {

using SpecialFloatHandler = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);

}

namespace SceJson2 {

using ElementAccessFailureHandler = void (APS5_VABI*)(int, const String*, const std::size_t*, const Value*, const Value*, void*);

}

namespace SceJson2 {

struct Handlers {
    TypeMismatchHandler typeMismatch = nullptr;
    void* typeMismatchContext = nullptr;
    SpecialFloatHandler specialFloat = nullptr;
    void* specialFloatContext = nullptr;
    ElementAccessFailureHandler elementAccessFailure = nullptr;
    void* elementAccessFailureContext = nullptr;
};

}

namespace SceJson2 {

struct Node {
    ValueType type = TypeNull;
    bool boolean = false;
    std::int64_t integer = 0;
    std::uint64_t uinteger = 0;
    double real = 0;
    String string{};
    Array array{};
    Object object{};
    Handlers handlers{};
};

}

namespace SceJson2 {

using NullAccessCallback = const Value& (APS5_VABI*)(ValueType, const Value*, void*);

}

namespace SceJson2 {

using ParserReadCallback = int (APS5_VABI*)(char&, void*);

}

namespace SceJson2 {

struct InitParameterRtti {
    void* allocParam;
    void* userData;
    std::size_t fileBufferSize;
};

}

namespace SceJson2 {

struct InitParameter2 {
    void* allocator;
    void* userData;
    std::size_t fileBufferSize;
    std::int32_t specialFloatFormatType;
    std::uint32_t reserved[3];
};

}

extern "C" {

int APS5_VABI _ZN3sce4Json11InitializerC1Ev(void* self);
int APS5_VABI _ZN3sce4Json11InitializerD1Ev(void* self);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void* self, const void* parameter);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void* self);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void* self, SceJson2::NullAccessCallback callback, void* context);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void* self, SceJson2::NullAccessCallback callback, void* context);
void APS5_VABI _ZN3sce4Json12MemAllocatorC2Ev(void* self);
void APS5_VABI _ZN3sce4Json12MemAllocatorD2Ev(void* self);
void APS5_VABI _ZN3sce4Json6StringC1Ev(SceJson2::String* self);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(SceJson2::String* self, const char* text);
void APS5_VABI _ZN3sce4Json6StringC1ERKS1_(SceJson2::String* self, const SceJson2::String* other);
void APS5_VABI _ZN3sce4Json6StringD1Ev(SceJson2::String* self);
SceJson2::String* APS5_VABI _ZN3sce4Json6StringaSERKS1_(SceJson2::String* self, const SceJson2::String* other);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const SceJson2::String* self);
bool APS5_VABI _ZNK3sce4Json6String5emptyEv(const SceJson2::String* self);
std::size_t APS5_VABI _ZNK3sce4Json6String6lengthEv(const SceJson2::String* self);
std::size_t APS5_VABI _ZNK3sce4Json6String4sizeEv(const SceJson2::String* self);
std::size_t APS5_VABI _ZNK3sce4Json6String8capacityEv(const SceJson2::String* self);
bool APS5_VABI _ZNK3sce4Json6StringeqEPKc(const SceJson2::String* self, const char* text);
bool APS5_VABI _ZNK3sce4Json6StringeqERKS1_(const SceJson2::String* self, const SceJson2::String* other);
void APS5_VABI _ZN3sce4Json6String5clearEv(SceJson2::String* self);
void APS5_VABI _ZN3sce4Json6String6resizeEm(SceJson2::String* self, std::size_t count);
void APS5_VABI _ZN3sce4Json6String7reserveEm(SceJson2::String* self, std::size_t capacity);
SceJson2::String* APS5_VABI _ZN3sce4Json6String6appendERKS1_(SceJson2::String* self, const SceJson2::String* other);
SceJson2::String* APS5_VABI _ZN3sce4Json6String6appendEPKc(SceJson2::String* self, const char* text);
SceJson2::String* APS5_VABI _ZN3sce4Json6String6appendEPKcm(SceJson2::String* self, const char* text, std::size_t length);
SceJson2::String* APS5_VABI _ZN3sce4Json6StringpLEPKc(SceJson2::String* self, const char* text);
SceJson2::String* APS5_VABI _ZN3sce4Json6StringpLEh(SceJson2::String* self, unsigned char ch);
SceJson2::String* APS5_VABI _ZNK3sce4Json6String6substrEmm(SceJson2::String* result, const SceJson2::String* self, std::size_t position, std::size_t length);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEcm(const SceJson2::String* self, char character, std::size_t position);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEcm(const SceJson2::String* self, char character, std::size_t position);
char APS5_VABI _ZNK3sce4Json6String2atEm(const SceJson2::String* self, std::size_t position);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcm(const SceJson2::String* self, const char* text, std::size_t position);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcmm(const SceJson2::String* self, const char* text, std::size_t position, std::size_t length);
std::size_t APS5_VABI _ZNK3sce4Json6String4findERKS1_m(const SceJson2::String* self, const SceJson2::String* other, std::size_t position);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcm(const SceJson2::String* self, const char* text, std::size_t position);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcmm(const SceJson2::String* self, const char* text, std::size_t position, std::size_t length);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindERKS1_m(const SceJson2::String* self, const SceJson2::String* other, std::size_t position);
int APS5_VABI _ZNK3sce4Json6String7compareEPKc(const SceJson2::String* self, const char* text);
int APS5_VABI _ZNK3sce4Json6String7compareERKS1_(const SceJson2::String* self, const SceJson2::String* other);
void APS5_VABI _ZN3sce4Json5ArrayC1Ev(SceJson2::Array* self);
void APS5_VABI _ZN3sce4Json5ArrayC1ERKS1_(SceJson2::Array* self, const SceJson2::Array* other);
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(SceJson2::Array* self);
SceJson2::Array* APS5_VABI _ZN3sce4Json5ArrayaSERKS1_(SceJson2::Array* self, const SceJson2::Array* other);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(SceJson2::Array* self, const SceJson2::Value* value);
SceJson2::ArrayIterator* APS5_VABI _ZNK3sce4Json5Array5beginEv(SceJson2::ArrayIterator* result, const SceJson2::Array* self);
SceJson2::ArrayIterator* APS5_VABI _ZNK3sce4Json5Array3endEv(SceJson2::ArrayIterator* result, const SceJson2::Array* self);
bool APS5_VABI _ZNK3sce4Json5Array5emptyEv(const SceJson2::Array* self);
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const SceJson2::Array* self);
void APS5_VABI _ZN3sce4Json5Array5clearEv(SceJson2::Array* self);
const SceJson2::Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const SceJson2::Array* self);
const SceJson2::Value* APS5_VABI _ZNK3sce4Json5Array4backEv(const SceJson2::Array* self);
void APS5_VABI _ZN3sce4Json5Array9pop_frontEv(SceJson2::Array* self);
void APS5_VABI _ZN3sce4Json5Array8iteratorD1Ev(SceJson2::ArrayIterator* self);
SceJson2::ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEv(SceJson2::ArrayIterator* self);
SceJson2::Value* APS5_VABI _ZNK3sce4Json5Array8iteratordeEv(const SceJson2::ArrayIterator* self);
SceJson2::Value* APS5_VABI _ZNK3sce4Json5Array8iteratorptEv(const SceJson2::ArrayIterator* self);
bool APS5_VABI _ZNK3sce4Json5Array8iteratorneERKS2_(const SceJson2::ArrayIterator* self, const SceJson2::ArrayIterator* other);
void APS5_VABI _ZN3sce4Json5Array8iteratorC1ERKS2_(SceJson2::ArrayIterator* self, const SceJson2::ArrayIterator* other);
SceJson2::ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iterator7advanceEm(SceJson2::ArrayIterator* self, std::size_t count);
void APS5_VABI _ZN3sce4Json6ObjectC1Ev(SceJson2::Object* self);
void APS5_VABI _ZN3sce4Json6ObjectC1ERKS1_(SceJson2::Object* self, const SceJson2::Object* other);
void APS5_VABI _ZN3sce4Json6ObjectD1Ev(SceJson2::Object* self);
SceJson2::Object* APS5_VABI _ZN3sce4Json6ObjectaSERKS1_(SceJson2::Object* self, const SceJson2::Object* other);
void APS5_VABI _ZN3sce4Json6Object5clearEv(SceJson2::Object* self);
SceJson2::Value* APS5_VABI _ZN3sce4Json6ObjectixERKNS0_6StringE(SceJson2::Object* self, const SceJson2::String* key);
SceJson2::ObjectIterator* APS5_VABI _ZNK3sce4Json6Object5beginEv(SceJson2::ObjectIterator* result, const SceJson2::Object* self);
SceJson2::ObjectIterator* APS5_VABI _ZNK3sce4Json6Object3endEv(SceJson2::ObjectIterator* result, const SceJson2::Object* self);
void APS5_VABI _ZN3sce4Json6Object8iteratorD1Ev(SceJson2::ObjectIterator* self);
SceJson2::ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEv(SceJson2::ObjectIterator* self);
SceJson2::Pair* APS5_VABI _ZNK3sce4Json6Object8iteratordeEv(const SceJson2::ObjectIterator* self);
SceJson2::Pair* APS5_VABI _ZNK3sce4Json6Object8iteratorptEv(const SceJson2::ObjectIterator* self);
SceJson2::ObjectIterator* APS5_VABI _ZNK3sce4Json6Object4findERKNS0_6StringE(SceJson2::ObjectIterator* result, const SceJson2::Object* self, const SceJson2::String* key);
bool APS5_VABI _ZNK3sce4Json6Object8iteratoreqERKS2_(const SceJson2::ObjectIterator* self, const SceJson2::ObjectIterator* other);
bool APS5_VABI _ZNK3sce4Json6Object8iteratorneERKS2_(const SceJson2::ObjectIterator* self, const SceJson2::ObjectIterator* other);
void APS5_VABI _ZN3sce4Json6Object8iteratorC1ERKS2_(SceJson2::ObjectIterator* self, const SceJson2::ObjectIterator* other);
SceJson2::ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iterator7advanceEm(SceJson2::ObjectIterator* self, std::size_t count);
bool APS5_VABI _ZNK3sce4Json6Object5emptyEv(const SceJson2::Object* self);
std::size_t APS5_VABI _ZNK3sce4Json6Object4sizeEv(const SceJson2::Object* self);
void APS5_VABI _ZN3sce4Json6Object5eraseERKNS0_6StringE(SceJson2::Object* self, const SceJson2::String* key);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(SceJson2::Value* self);
void APS5_VABI _ZN3sce4Json5ValueC1Eb(SceJson2::Value* self, bool value);
void APS5_VABI _ZN3sce4Json5ValueC1El(SceJson2::Value* self, std::int64_t value);
void APS5_VABI _ZN3sce4Json5ValueC1Em(SceJson2::Value* self, std::uint64_t value);
void APS5_VABI _ZN3sce4Json5ValueC1Ed(SceJson2::Value* self, double value);
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(SceJson2::Value* self, const char* text);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6StringE(SceJson2::Value* self, const SceJson2::String* text);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(SceJson2::Value* self, const SceJson2::Array* array);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(SceJson2::Value* self, const SceJson2::Object* object);
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(SceJson2::Value* self, const SceJson2::Value* other);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(SceJson2::Value* self);
SceJson2::Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1_(SceJson2::Value* self, const SceJson2::Value* other);
void APS5_VABI _ZN3sce4Json5Value4swapERS1_(SceJson2::Value* self, SceJson2::Value* other);
int APS5_VABI _ZN3sce4Json5Value3setENS0_9ValueTypeE(SceJson2::Value* self, SceJson2::ValueType type);
int APS5_VABI _ZN3sce4Json5Value3setEb(SceJson2::Value* self, bool value);
int APS5_VABI _ZN3sce4Json5Value3setEl(SceJson2::Value* self, std::int64_t value);
int APS5_VABI _ZN3sce4Json5Value3setEm(SceJson2::Value* self, std::uint64_t value);
int APS5_VABI _ZN3sce4Json5Value3setEd(SceJson2::Value* self, double value);
int APS5_VABI _ZN3sce4Json5Value3setEPKc(SceJson2::Value* self, const char* text);
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_6StringE(SceJson2::Value* self, const SceJson2::String* text);
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_5ArrayE(SceJson2::Value* self, const SceJson2::Array* array);
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_6ObjectE(SceJson2::Value* self, const SceJson2::Object* object);
int APS5_VABI _ZN3sce4Json5Value3setERKS1_(SceJson2::Value* self, const SceJson2::Value* other);
SceJson2::ValueType APS5_VABI _ZNK3sce4Json5Value7getTypeEv(const SceJson2::Value* self);
const bool* APS5_VABI _ZNK3sce4Json5Value10getBooleanEv(const SceJson2::Value* self);
const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv(const SceJson2::Value* self);
const std::uint64_t* APS5_VABI _ZNK3sce4Json5Value11getUIntegerEv(const SceJson2::Value* self);
const double* APS5_VABI _ZNK3sce4Json5Value7getRealEv(const SceJson2::Value* self);
const SceJson2::String* APS5_VABI _ZNK3sce4Json5Value9getStringEv(const SceJson2::Value* self);
const SceJson2::Array* APS5_VABI _ZNK3sce4Json5Value8getArrayEv(const SceJson2::Value* self);
const SceJson2::Object* APS5_VABI _ZNK3sce4Json5Value9getObjectEv(const SceJson2::Value* self);
std::size_t APS5_VABI _ZNK3sce4Json5Value5countEv(const SceJson2::Value* self);
const SceJson2::Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const SceJson2::Value* self, const char* key);
const SceJson2::Value* APS5_VABI _ZNK3sce4Json5ValueixEm(const SceJson2::Value* self, std::size_t index);
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(SceJson2::Value* self, SceJson2::ValueType type);
const SceJson2::Value* APS5_VABI _ZNK3sce4Json5Value8getValueEm(const SceJson2::Value* self, std::size_t index);
const SceJson2::Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKNS0_6StringE(const SceJson2::Value* self, const SceJson2::String* key);
SceJson2::Value* APS5_VABI _ZN3sce4Json5Value10referValueEm(SceJson2::Value* self, std::size_t index);
SceJson2::Value* APS5_VABI _ZN3sce4Json5Value10referValueERKNS0_6StringE(SceJson2::Value* self, const SceJson2::String* key);
SceJson2::Array* APS5_VABI _ZN3sce4Json5Value10referArrayEv(SceJson2::Value* self);
SceJson2::Object* APS5_VABI _ZN3sce4Json5Value11referObjectEv(SceJson2::Value* self);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(SceJson2::Value* self, SceJson2::String* out);
int APS5_VABI _ZNK3sce4Json5Value8toStringERNS0_6StringE(const SceJson2::Value* self, SceJson2::String* out);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(SceJson2::Value* out, SceJson2::ParserReadCallback read, void* context);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(SceJson2::Value* out, const char* text, std::size_t size);
void APS5_VABI _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(SceJson2::InitParameterRtti* self, void* allocParam, void* userData, std::size_t fileBufferSize);
void APS5_VABI _ZN3sce4Json14InitParameter2C1Ev(SceJson2::InitParameter2* self);
void APS5_VABI _ZN3sce4Json14InitParameter2C2Ev(SceJson2::InitParameter2* self);
void APS5_VABI _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(SceJson2::InitParameter2* self, void* allocator, void* userData);
void APS5_VABI _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(SceJson2::InitParameter2* self, void* allocParam, void* userData);
void APS5_VABI _ZN3sce4Json14InitParameter217setFileBufferSizeEm(SceJson2::InitParameter2* self, std::size_t size);
void APS5_VABI _ZN3sce4Json14InitParameter225setSpecialFloatFormatTypeENS0_22SpecialFloatFormatTypeE(SceJson2::InitParameter2* self, std::int32_t type);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void* self, const SceJson2::InitParameter2* parameter);
void APS5_VABI _ZN3sce4Json5Value5clearEv(SceJson2::Value* self);
void APS5_VABI _ZN3sce4Json12MemAllocator11notifyErrorEimPv(void* self, int error, std::size_t size, void* userData);
void APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(SceJson2::NullAccessCallback callback, void* context);
int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(SceJson2::Value* self, SceJson2::ElementAccessFailureHandler handler, void* context);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(SceJson2::Value* self, SceJson2::SpecialFloatHandler handler, void* context);
int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(SceJson2::Value* self, SceJson2::TypeMismatchHandler handler, void* context);
bool APS5_VABI _ZNK3sce4Json5ValuecvbEv(const SceJson2::Value* self);
void APS5_VABI _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(SceJson2::Pair* self, const SceJson2::String* key, const SceJson2::Value* value);
void APS5_VABI _ZN3sce4Json6Object4PairD1Ev(SceJson2::Pair* self);
SceJson2::ObjectIterator* APS5_VABI _ZN3sce4Json6Object6insertERKNS1_4PairE(SceJson2::ObjectIterator* result, SceJson2::Object* self, const SceJson2::Pair* pair);
SceJson2::ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEi(SceJson2::ArrayIterator* result, SceJson2::ArrayIterator* self, int);
SceJson2::ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratoraSERKS2_(SceJson2::ArrayIterator* self, const SceJson2::ArrayIterator* other);
void APS5_VABI _ZN3sce4Json5Array8iteratorC1Ev(SceJson2::ArrayIterator* self);
SceJson2::ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEi(SceJson2::ObjectIterator* result, SceJson2::ObjectIterator* self, int);
SceJson2::ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratoraSERKS2_(SceJson2::ObjectIterator* self, const SceJson2::ObjectIterator* other);
void APS5_VABI _ZN3sce4Json6Object8iteratorC1Ev(SceJson2::ObjectIterator* self);
void APS5_VABI _ZN3sce4Json6Object4PairC1Ev(SceJson2::Pair* self);

}
