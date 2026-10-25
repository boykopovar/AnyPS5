#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

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

struct Value;
struct Text;
struct Auxiliary;
struct ArrayStorage;
struct ObjectStorage;
struct ArrayIteratorState;
struct ObjectIteratorState;

struct Text { char* characters; std::uint32_t length; std::uint32_t capacity; std::uint64_t reserved; };

struct String { Text* text; };

struct Array { ArrayStorage* items; };

struct Object { ObjectStorage* items; };

struct ArrayIterator { ArrayIteratorState* state; };

struct ObjectIterator { ObjectIteratorState* state; };

struct Value {
    Value* parent = nullptr;
    Auxiliary* auxiliary = nullptr;
    union {
        std::uint64_t payload = 0;
        bool boolean;
        std::int64_t integer;
        std::uint64_t uinteger;
        double real;
        String* string;
        Array* array;
        Object* object;
    };
    std::uint32_t reserved = 0;
    ValueType type = TypeNull;
};

struct Pair {
    String key;
    std::uint64_t reserved;
    Value value;
};

struct InitParameterRtti {
    void* allocParam;
    void* userData;
    std::size_t fileBufferSize;
};

struct InitParameter2 {
    void* allocator;
    void* userData;
    std::size_t fileBufferSize;
    std::int32_t specialFloatFormatType;
    std::uint32_t reserved[3];
};

using TypeMismatchHandler = const Value& (APS5_VABI*)(ValueType, const Value*, const Value*, void*);

using SpecialFloatHandler = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);

using ElementAccessFailureHandler = void (APS5_VABI*)(int, const String*, const std::size_t*, const Value*, const Value*, void*);

using NullAccessCallback = const Value& (APS5_VABI*)(ValueType, const Value*, void*);

using ParserReadCallback = int (APS5_VABI*)(char&, void*);

using SerializationCallback = int (APS5_VABI*)(String&, void*);


extern "C" {

int APS5_VABI _ZN3sce4Json12MemAllocatorD0Ev(void);
extern std::size_t _ZN3sce4Json6String4nposE;

void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void* self);
void APS5_VABI _ZN3sce4Json11InitializerD1Ev(void* self);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void* self, const void* parameter);
void APS5_VABI _ZN3sce4Json19InternalInitializerC1Ev(void* self);
void APS5_VABI _ZN3sce4Json19InternalInitializerC2Ev(void* self);
void APS5_VABI _ZN3sce4Json19InternalInitializerD1Ev(void* self);
void APS5_VABI _ZN3sce4Json19InternalInitializerD2Ev(void* self);
int APS5_VABI _ZN3sce4Json19InternalInitializer19terminate_staticlibEv(void* self);
int APS5_VABI _ZN3sce4Json19InternalInitializer20initialize_staticlibEPKNS0_13InitParameterENS1_10InitOptionE(void* self, const void* parameter, std::int32_t option);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void* self);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void* self, NullAccessCallback callback, void* context);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void* self, NullAccessCallback callback, void* context);
int APS5_VABI _ZN3sce4Json11Initializer28setGlobalTypeMismatchHandlerEPFRKNS0_5ValueENS0_9ValueTypeEPS3_S6_PvES7_(void* self, TypeMismatchHandler callback, void* context);
int APS5_VABI _ZN3sce4Json11Initializer28setGlobalSpecialFloatHandlerEPFKNS0_5ValueENS0_12FunctionTypeEdPS3_PvPbES6_(void* self, SpecialFloatHandler callback, void* context);
int APS5_VABI _ZN3sce4Json11Initializer36setGlobalElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKNS0_5ValueES9_PvESA_(void* self, ElementAccessFailureHandler callback, void* context);
void APS5_VABI _ZN3sce4Json12MemAllocatorC2Ev(void* self);
void APS5_VABI _ZN3sce4Json12MemAllocatorD2Ev(void*);
void APS5_VABI _ZN3sce4Json12MemAllocatorD1Ev(void*);
void* APS5_VABI _ZN3sce4Json6MallocEm(std::size_t size);
void APS5_VABI _ZN3sce4Json4FreeEPv(void* memory);
int APS5_VABI _ZN3sce4Json11Initializer24setAllocatorInfoCallBackEPFviNS0_9ValueTypeEPvES3_(void*, void (APS5_VABI*)(int, ValueType, void*), void*);
void APS5_VABI _ZN3sce4Json6StringC1Ev(String* self);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String* self, const char* text);
void APS5_VABI _ZN3sce4Json6StringC1ERKS1_(String* self, const String* other);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String* self);
String* APS5_VABI _ZN3sce4Json6StringaSERKS1_(String* self, const String* other);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String* self);
bool APS5_VABI _ZNK3sce4Json6String5emptyEv(const String* self);
std::size_t APS5_VABI _ZNK3sce4Json6String6lengthEv(const String* self);
std::size_t APS5_VABI _ZNK3sce4Json6String4sizeEv(const String* self);
std::size_t APS5_VABI _ZNK3sce4Json6String8capacityEv(const String* self);
bool APS5_VABI _ZNK3sce4Json6StringeqEPKc(const String* self, const char* text);
bool APS5_VABI _ZNK3sce4Json6StringeqERKS1_(const String* self, const String* other);
void APS5_VABI _ZN3sce4Json6String5clearEv(String* self);
void APS5_VABI _ZN3sce4Json6String6resizeEm(String* self, std::size_t count);
void APS5_VABI _ZN3sce4Json6String7reserveEm(String* self, std::size_t capacity);
String* APS5_VABI _ZN3sce4Json6String6appendERKS1_(String* self, const String* other);
String* APS5_VABI _ZN3sce4Json6String6appendEPKc(String* self, const char* text);
String* APS5_VABI _ZN3sce4Json6String6appendEPKcm(String* self, const char* text, std::size_t length);
String* APS5_VABI _ZN3sce4Json6StringpLEPKc(String* self, const char* text);
String* APS5_VABI _ZN3sce4Json6StringpLEh(String* self, unsigned char ch);
String* APS5_VABI _ZNK3sce4Json6String6substrEmm(String* result, const String* self, std::size_t position, std::size_t length);
char APS5_VABI _ZNK3sce4Json6String2atEm(const String* self, std::size_t index);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcm(const String* self, const char* needle, std::size_t position);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcmm(const String* self, const char* needle, std::size_t position, std::size_t count);
std::size_t APS5_VABI _ZNK3sce4Json6String4findERKS1_m(const String* self, const String* needle, std::size_t position);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcm(const String* self, const char* needle, std::size_t position);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcmm(const String* self, const char* needle, std::size_t position, std::size_t count);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindERKS1_m(const String* self, const String* needle, std::size_t position);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEcm(const String* self, char character, std::size_t position);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEcm(const String* self, char character, std::size_t position);
int APS5_VABI _ZNK3sce4Json6String7compareEPKc(const String* self, const char* text);
int APS5_VABI _ZNK3sce4Json6String7compareERKS1_(const String* self, const String* other);
void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array* self);
void APS5_VABI _ZN3sce4Json5ArrayC1ERKS1_(Array* self, const Array* other);
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array* self);
Array* APS5_VABI _ZN3sce4Json5ArrayaSERKS1_(Array* self, const Array* other);
void APS5_VABI _ZN3sce4Json5Array10push_frontERKNS0_5ValueE(Array* self, const Value* value);
ArrayIterator* APS5_VABI _ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(ArrayIterator* result, Array* self, const ArrayIterator* position, const Value* value);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array* self, const Value* value);
ArrayIterator* APS5_VABI _ZNK3sce4Json5Array5beginEv(ArrayIterator* result, const Array* self);
ArrayIterator* APS5_VABI _ZNK3sce4Json5Array3endEv(ArrayIterator* result, const Array* self);
bool APS5_VABI _ZNK3sce4Json5Array5emptyEv(const Array* self);
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array* self);
void APS5_VABI _ZN3sce4Json5Array5clearEv(Array* self);
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array* self);
const Value* APS5_VABI _ZNK3sce4Json5Array4backEv(const Array* self);
void APS5_VABI _ZN3sce4Json5Array8pop_backEv(Array* self);
ArrayIterator* APS5_VABI _ZN3sce4Json5Array5eraseERKNS1_8iteratorE(ArrayIterator* result, Array* self, const ArrayIterator* position);
void APS5_VABI _ZN3sce4Json5Array9pop_frontEv(Array* self);
void APS5_VABI _ZN3sce4Json5Array8iteratorD1Ev(ArrayIterator* self);
ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEv(ArrayIterator* self);
Value* APS5_VABI _ZNK3sce4Json5Array8iteratordeEv(const ArrayIterator* self);
Value* APS5_VABI _ZNK3sce4Json5Array8iteratorptEv(const ArrayIterator* self);
bool APS5_VABI _ZNK3sce4Json5Array8iteratorneERKS2_(const ArrayIterator* self, const ArrayIterator* other);
void APS5_VABI _ZN3sce4Json5Array8iteratorC1ERKS2_(ArrayIterator* self, const ArrayIterator* other);
void APS5_VABI _ZN3sce4Json5Array8iterator7advanceEm(ArrayIterator* self, std::size_t count);
void APS5_VABI _ZN3sce4Json6ObjectC1Ev(Object* self);
void APS5_VABI _ZN3sce4Json6ObjectC1ERKS1_(Object* self, const Object* other);
void APS5_VABI _ZN3sce4Json6ObjectD1Ev(Object* self);
Object* APS5_VABI _ZN3sce4Json6ObjectaSERKS1_(Object* self, const Object* other);
void APS5_VABI _ZN3sce4Json6Object5clearEv(Object* self);
Value* APS5_VABI _ZN3sce4Json6ObjectixERKNS0_6StringE(Object* self, const String* key);
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object5beginEv(ObjectIterator* result, const Object* self);
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object3endEv(ObjectIterator* result, const Object* self);
void APS5_VABI _ZN3sce4Json6Object8iteratorD1Ev(ObjectIterator* self);
ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEv(ObjectIterator* self);
Pair* APS5_VABI _ZNK3sce4Json6Object8iteratordeEv(const ObjectIterator* self);
Pair* APS5_VABI _ZNK3sce4Json6Object8iteratorptEv(const ObjectIterator* self);
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object4findERKNS0_6StringE(ObjectIterator* result, const Object* self, const String* key);
bool APS5_VABI _ZNK3sce4Json6Object8iteratoreqERKS2_(const ObjectIterator* self, const ObjectIterator* other);
bool APS5_VABI _ZNK3sce4Json6Object8iteratorneERKS2_(const ObjectIterator* self, const ObjectIterator* other);
void APS5_VABI _ZN3sce4Json6Object8iteratorC1ERKS2_(ObjectIterator* self, const ObjectIterator* other);
void APS5_VABI _ZN3sce4Json6Object8iterator7advanceEm(ObjectIterator* self, std::size_t count);
bool APS5_VABI _ZNK3sce4Json6Object5emptyEv(const Object* self);
std::size_t APS5_VABI _ZNK3sce4Json6Object4sizeEv(const Object* self);
void APS5_VABI _ZN3sce4Json6Object5eraseERKNS0_6StringE(Object* self, const String* key);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value* self);
void APS5_VABI _ZN3sce4Json5ValueC1Eb(Value* self, bool value);
void APS5_VABI _ZN3sce4Json5ValueC1El(Value* self, std::int64_t value);
void APS5_VABI _ZN3sce4Json5ValueC1Em(Value* self, std::uint64_t value);
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value* self, double value);
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value* self, const char* text);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6StringE(Value* self, const String* text);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(Value* self, const Array* array);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(Value* self, const Object* object);
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(Value* self, const Value* other);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value* self);
Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1_(Value* self, const Value* other);
void APS5_VABI _ZN3sce4Json5Value4swapERS1_(Value* self, Value* other);
void APS5_VABI _ZN3sce4Json5Value3setENS0_9ValueTypeE(Value* self, ValueType type);
void APS5_VABI _ZN3sce4Json5Value3setEb(Value* self, bool value);
void APS5_VABI _ZN3sce4Json5Value3setEl(Value* self, std::int64_t value);
void APS5_VABI _ZN3sce4Json5Value3setEm(Value* self, std::uint64_t value);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value* self, double value);
void APS5_VABI _ZN3sce4Json5Value3setEPKc(Value* self, const char* text);
void APS5_VABI _ZN3sce4Json5Value3setERKNS0_6StringE(Value* self, const String* text);
void APS5_VABI _ZN3sce4Json5Value3setERKNS0_5ArrayE(Value* self, const Array* array);
void APS5_VABI _ZN3sce4Json5Value3setERKNS0_6ObjectE(Value* self, const Object* object);
void APS5_VABI _ZN3sce4Json5Value3setERKS1_(Value* self, const Value* other);
ValueType APS5_VABI _ZNK3sce4Json5Value7getTypeEv(const Value* self);
const bool* APS5_VABI _ZNK3sce4Json5Value10getBooleanEv(const Value* self);
const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv(const Value* self);
const std::uint64_t* APS5_VABI _ZNK3sce4Json5Value11getUIntegerEv(const Value* self);
const double* APS5_VABI _ZNK3sce4Json5Value7getRealEv(const Value* self);
const String* APS5_VABI _ZNK3sce4Json5Value9getStringEv(const Value* self);
const Array* APS5_VABI _ZNK3sce4Json5Value8getArrayEv(const Value* self);
const Object* APS5_VABI _ZNK3sce4Json5Value9getObjectEv(const Value* self);
std::size_t APS5_VABI _ZNK3sce4Json5Value5countEv(const Value* self);
const Value* APS5_VABI _ZNK3sce4Json5ValueixERKNS0_6StringE(const Value* self, const String* key);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value* self, const char* key);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEm(const Value* self, std::size_t index);
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(Value* self, ValueType type);
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueEm(const Value* self, std::size_t index);
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKNS0_6StringE(const Value* self, const String* key);
Value* APS5_VABI _ZN3sce4Json5Value10referValueEm(Value* self, std::size_t index);
Value* APS5_VABI _ZN3sce4Json5Value10referValueERKNS0_6StringE(Value* self, const String* key);
Array* APS5_VABI _ZN3sce4Json5Value10referArrayEv(Value* self);
Object* APS5_VABI _ZN3sce4Json5Value11referObjectEv(Value* self);
int APS5_VABI _ZN3sce4Json5Value9serializeEPFiRNS0_6StringEPvES4_(Value* self, SerializationCallback callback, void* context);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(Value* self, String* output, SerializationCallback callback, void* context);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value* self, String* out);
void APS5_VABI _ZNK3sce4Json5Value8toStringERNS0_6StringE(const Value* self, String* out);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(Value* out, ParserReadCallback read, void* context);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value* out, const char* text, std::size_t size);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(Value* out, const char* path);
void APS5_VABI _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(InitParameterRtti* self, void* allocParam, void* userData, std::size_t fileBufferSize);
void APS5_VABI _ZN3sce4Json14InitParameter2C1Ev(InitParameter2* self);
void APS5_VABI _ZN3sce4Json14InitParameter2C2Ev(InitParameter2* self);
void APS5_VABI _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(InitParameter2* self, void* allocator, void* userData);
void APS5_VABI _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(InitParameter2* self, void* allocParam, void* userData);
void APS5_VABI _ZN3sce4Json14InitParameter217setFileBufferSizeEm(InitParameter2* self, std::size_t size);
void APS5_VABI _ZN3sce4Json14InitParameter225setSpecialFloatFormatTypeENS0_22SpecialFloatFormatTypeE(InitParameter2* self, std::int32_t type);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void* self, const InitParameter2* parameter);
void APS5_VABI _ZN3sce4Json5Value5clearEv(Value* self);
void APS5_VABI _ZN3sce4Json12MemAllocator11notifyErrorEimPv(void* self, int error, std::size_t size, void* userData);
int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(Value* self, NullAccessCallback callback, void* context);
int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(Value* self, ElementAccessFailureHandler handler, void* context);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value* self, SpecialFloatHandler handler, void* context);
int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value* self, TypeMismatchHandler handler, void* context);
bool APS5_VABI _ZNK3sce4Json5ValuecvbEv(const Value* self);
void APS5_VABI _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(Pair* self, const String* key, const Value* value);
void APS5_VABI _ZN3sce4Json6Object4PairD1Ev(Pair* self);
ObjectIterator* APS5_VABI _ZN3sce4Json6Object6insertERKNS1_4PairE(ObjectIterator* result, Object* self, const Pair* pair);
ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEi(ArrayIterator* self, int);
ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratoraSERKS2_(ArrayIterator* self, const ArrayIterator* other);
void APS5_VABI _ZN3sce4Json5Array8iteratorC1Ev(ArrayIterator* self);
ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEi(ObjectIterator* self, int);
ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratoraSERKS2_(ObjectIterator* self, const ObjectIterator* other);
void APS5_VABI _ZN3sce4Json6Object8iteratorC1Ev(ObjectIterator* self);
void APS5_VABI _ZN3sce4Json6Object4PairC1Ev(Pair* self);
void APS5_VABI _ZN3sce4Json11InitializerC2Ev(void* self);
void APS5_VABI _ZN3sce4Json11InitializerD2Ev(void* self);
void APS5_VABI _ZN3sce4Json17InitParameterRttiC2EPNS0_14AllocParamRttiEPvm(InitParameterRtti* self, void* allocParam, void* userData, std::size_t fileBufferSize);
void APS5_VABI _ZN3sce4Json5Array8iteratorC2ERKS2_(ArrayIterator* self, const ArrayIterator* other);
void APS5_VABI _ZN3sce4Json5Array8iteratorC2Ev(ArrayIterator* self);
void APS5_VABI _ZN3sce4Json5Array8iteratorD2Ev(ArrayIterator* self);
void APS5_VABI _ZN3sce4Json5ArrayC2ERKS1_(Array* self, const Array* other);
void APS5_VABI _ZN3sce4Json5ArrayC2Ev(Array* self);
void APS5_VABI _ZN3sce4Json5ArrayD2Ev(Array* self);
void APS5_VABI _ZN3sce4Json5ValueC2ENS0_9ValueTypeE(Value* self, ValueType type);
void APS5_VABI _ZN3sce4Json5ValueC2EPKc(Value* self, const char* text);
void APS5_VABI _ZN3sce4Json5ValueC2ERKNS0_5ArrayE(Value* self, const Array* array);
void APS5_VABI _ZN3sce4Json5ValueC2ERKNS0_6ObjectE(Value* self, const Object* object);
void APS5_VABI _ZN3sce4Json5ValueC2ERKNS0_6StringE(Value* self, const String* text);
void APS5_VABI _ZN3sce4Json5ValueC2ERKS1_(Value* self, const Value* other);
void APS5_VABI _ZN3sce4Json5ValueC2Eb(Value* self, bool value);
void APS5_VABI _ZN3sce4Json5ValueC2Ed(Value* self, double value);
void APS5_VABI _ZN3sce4Json5ValueC2El(Value* self, std::int64_t value);
void APS5_VABI _ZN3sce4Json5ValueC2Em(Value* self, std::uint64_t value);
void APS5_VABI _ZN3sce4Json5ValueC2Ev(Value* self);
void APS5_VABI _ZN3sce4Json5ValueD2Ev(Value* self);
void APS5_VABI _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(Pair* self, const String* key, const Value* value);
void APS5_VABI _ZN3sce4Json6Object4PairC2Ev(Pair* self);
void APS5_VABI _ZN3sce4Json6Object4PairD2Ev(Pair* self);
void APS5_VABI _ZN3sce4Json6Object8iteratorC2ERKS2_(ObjectIterator* self, const ObjectIterator* other);
void APS5_VABI _ZN3sce4Json6Object8iteratorC2Ev(ObjectIterator* self);
void APS5_VABI _ZN3sce4Json6Object8iteratorD2Ev(ObjectIterator* self);
void APS5_VABI _ZN3sce4Json6ObjectC2ERKS1_(Object* self, const Object* other);
void APS5_VABI _ZN3sce4Json6ObjectC2Ev(Object* self);
void APS5_VABI _ZN3sce4Json6ObjectD2Ev(Object* self);
void APS5_VABI _ZN3sce4Json6StringC2EPKc(String* self, const char* text);
void APS5_VABI _ZN3sce4Json6StringC2ERKS1_(String* self, const String* other);
void APS5_VABI _ZN3sce4Json6StringC2Ev(String* self);
void APS5_VABI _ZN3sce4Json6StringD2Ev(String* self);
String* APS5_VABI _ZN3sce4Json5Value11referStringEv(Value* self);
bool* APS5_VABI _ZN3sce4Json5Value12referBooleanEv(Value* self);
std::int64_t* APS5_VABI _ZN3sce4Json5Value12referIntegerEv(Value* self);
std::uint64_t* APS5_VABI _ZN3sce4Json5Value13referUIntegerEv(Value* self);
double* APS5_VABI _ZN3sce4Json5Value9referRealEv(Value* self);
void* APS5_VABI _ZNK3sce4Json14InitParameter211getUserDataEv(const InitParameter2* self);
void* APS5_VABI _ZNK3sce4Json14InitParameter212getAllocatorEv(const InitParameter2* self);
std::size_t APS5_VABI _ZNK3sce4Json14InitParameter217getFileBufferSizeEv(const InitParameter2* self);
std::int32_t APS5_VABI _ZNK3sce4Json14InitParameter225getSpecialFloatFormatTypeEv(const InitParameter2* self);

}

}
