#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

namespace SceJson {

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
struct RootParam;
struct String;
struct Array;
struct Object;

using NullAccessCallback = const Value& (APS5_VABI*)(ValueType, const Value*, void*);
using ParserReadCallback = int (APS5_VABI*)(char&, void*);
using SerializeCallback = int (APS5_VABI*)(String&, void*);

struct MemAllocator {
    void* const* vtable;
};

struct InitParameter {
    MemAllocator* allocator;
    void* userData;
    std::size_t fileBufferSize;
};

struct Initializer {
    bool initialized;
};

struct String {
    std::uint64_t allocator;
    union {
        char buffer[16];
        char* pointer;
    } storage;
    std::size_t size;
    std::size_t capacity;
};

struct Value {
    const Value* parent;
    RootParam* root;
    union {
        bool boolean;
        std::int64_t integer;
        std::uint64_t uinteger;
        double real;
        String* string;
        Array* array;
        Object* object;
    } payload;
    ValueType type;
    std::uint32_t reserved;
};

struct RootParam {
    NullAccessCallback callback;
    void* context;
    Value* nullValue;
};

struct ListNode {
    ListNode* next;
    ListNode* previous;
    Value value;
};

struct Array {
    std::uint64_t allocator;
    ListNode* head;
    std::size_t size;
};

struct TreeNode {
    TreeNode* left;
    TreeNode* parent;
    TreeNode* right;
    char color;
    char isNil;
    char reserved[6];
    String key;
    Value value;
};

struct Object {
    std::uint64_t comparator;
    TreeNode* head;
    std::size_t size;
};

struct InputStream {
    ParserReadCallback read;
    void* context;
    std::int32_t character;
    std::int32_t status;
    bool pushedBack;
};

static_assert(sizeof(InitParameter) == 0x18);
static_assert(sizeof(String) == 0x28 && offsetof(String, size) == 0x18 && offsetof(String, capacity) == 0x20);
static_assert(sizeof(Value) == 0x20 && offsetof(Value, payload) == 0x10 && offsetof(Value, type) == 0x18);
static_assert(sizeof(RootParam) == 0x18);
static_assert(sizeof(ListNode) == 0x30 && offsetof(ListNode, value) == 0x10);
static_assert(sizeof(Array) == 0x18 && offsetof(Array, head) == 0x8);
static_assert(sizeof(TreeNode) == 0x68 && offsetof(TreeNode, color) == 0x18 && offsetof(TreeNode, isNil) == 0x19);
static_assert(offsetof(TreeNode, key) == 0x20 && offsetof(TreeNode, value) == 0x48);
static_assert(sizeof(Object) == 0x18 && offsetof(Object, head) == 0x8);
static_assert(offsetof(InputStream, character) == 0x10 && offsetof(InputStream, status) == 0x14 && offsetof(InputStream, pushedBack) == 0x18);

}

extern "C" {

extern const std::int64_t _ZN3sce4Json5Value13s_nullintegerE;
extern const std::uint64_t _ZN3sce4Json5Value14s_nulluintegerE;
extern const double _ZN3sce4Json5Value10s_nullrealE;
extern const bool _ZN3sce4Json5Value10s_nullboolE;
extern SceJson::String* _ZN3sce4Json5Value12s_nullstringE;
extern SceJson::Array* _ZN3sce4Json5Value11s_nullarrayE;
extern SceJson::Object* _ZN3sce4Json5Value12s_nullobjectE;
extern SceJson::InitParameter* _ZN3sce4Json11s_initparamE;

void* APS5_VABI _ZN3sce4Json6MallocEm_nid_no_patch(std::size_t size);
void APS5_VABI _ZN3sce4Json4FreeEPv_nid_no_patch(void* pointer);

void APS5_VABI _ZN3sce4Json9RootParamC1Ev_nid_no_patch(SceJson::RootParam* self);
void APS5_VABI _ZN3sce4Json9RootParamC2Ev_nid_no_patch(SceJson::RootParam* self);
void APS5_VABI _ZN3sce4Json9RootParamD1Ev_nid_no_patch(SceJson::RootParam* self);
void APS5_VABI _ZN3sce4Json9RootParamD2Ev_nid_no_patch(SceJson::RootParam* self);

void APS5_VABI _ZN3sce4Json11InitializerC1Ev_nid_no_patch(SceJson::Initializer* self);
void APS5_VABI _ZN3sce4Json11InitializerC2Ev_nid_no_patch(SceJson::Initializer* self);
void APS5_VABI _ZN3sce4Json11InitializerD1Ev_nid_no_patch(SceJson::Initializer* self);
void APS5_VABI _ZN3sce4Json11InitializerD2Ev_nid_no_patch(SceJson::Initializer* self);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv_nid_no_patch(SceJson::Initializer* self);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE_nid_no_patch(SceJson::Initializer* self, const SceJson::InitParameter* parameter);

void APS5_VABI _ZN3sce4Json5ValueD1Ev_nid_no_patch(SceJson::Value* self);
void APS5_VABI _ZN3sce4Json5ValueD2Ev_nid_no_patch(SceJson::Value* self);
void APS5_VABI _ZN3sce4Json5ValueC1Ev_nid_no_patch(SceJson::Value* self);
void APS5_VABI _ZN3sce4Json5ValueC2Ev_nid_no_patch(SceJson::Value* self);
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE_nid_no_patch(SceJson::Value* self, SceJson::ValueType type);
void APS5_VABI _ZN3sce4Json5ValueC2ENS0_9ValueTypeE_nid_no_patch(SceJson::Value* self, SceJson::ValueType type);
void APS5_VABI _ZN3sce4Json5ValueC1Eb_nid_no_patch(SceJson::Value* self, bool value);
void APS5_VABI _ZN3sce4Json5ValueC2Eb_nid_no_patch(SceJson::Value* self, bool value);
void APS5_VABI _ZN3sce4Json5ValueC1El_nid_no_patch(SceJson::Value* self, std::int64_t value);
void APS5_VABI _ZN3sce4Json5ValueC2El_nid_no_patch(SceJson::Value* self, std::int64_t value);
void APS5_VABI _ZN3sce4Json5ValueC1Em_nid_no_patch(SceJson::Value* self, std::uint64_t value);
void APS5_VABI _ZN3sce4Json5ValueC2Em_nid_no_patch(SceJson::Value* self, std::uint64_t value);
void APS5_VABI _ZN3sce4Json5ValueC1Ed_nid_no_patch(SceJson::Value* self, double value);
void APS5_VABI _ZN3sce4Json5ValueC2Ed_nid_no_patch(SceJson::Value* self, double value);
void APS5_VABI _ZN3sce4Json5ValueC1ERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(SceJson::Value* self, const SceJson::String* value);
void APS5_VABI _ZN3sce4Json5ValueC2ERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(SceJson::Value* self, const SceJson::String* value);
void APS5_VABI _ZN3sce4Json5ValueC1ERKSt4listIS1_NS0_8StlAllocIS1_EEE_nid_no_patch(SceJson::Value* self, const SceJson::Array* value);
void APS5_VABI _ZN3sce4Json5ValueC2ERKSt4listIS1_NS0_8StlAllocIS1_EEE_nid_no_patch(SceJson::Value* self, const SceJson::Array* value);
void APS5_VABI _ZN3sce4Json5ValueC1ERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE_nid_no_patch(SceJson::Value* self, const SceJson::Object* value);
void APS5_VABI _ZN3sce4Json5ValueC2ERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE_nid_no_patch(SceJson::Value* self, const SceJson::Object* value);
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1__nid_no_patch(SceJson::Value* self, const SceJson::Value* value);
void APS5_VABI _ZN3sce4Json5ValueC2ERKS1__nid_no_patch(SceJson::Value* self, const SceJson::Value* value);

void APS5_VABI _ZN3sce4Json5Value3setENS0_9ValueTypeE_nid_no_patch(SceJson::Value* self, SceJson::ValueType type);
void APS5_VABI _ZN3sce4Json5Value3setEb_nid_no_patch(SceJson::Value* self, bool value);
void APS5_VABI _ZN3sce4Json5Value3setEl_nid_no_patch(SceJson::Value* self, std::int64_t value);
void APS5_VABI _ZN3sce4Json5Value3setEm_nid_no_patch(SceJson::Value* self, std::uint64_t value);
void APS5_VABI _ZN3sce4Json5Value3setEd_nid_no_patch(SceJson::Value* self, double value);
void APS5_VABI _ZN3sce4Json5Value3setERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(SceJson::Value* self, const SceJson::String* value);
void APS5_VABI _ZN3sce4Json5Value3setERKSt4listIS1_NS0_8StlAllocIS1_EEE_nid_no_patch(SceJson::Value* self, const SceJson::Array* value);
void APS5_VABI _ZN3sce4Json5Value3setERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE_nid_no_patch(SceJson::Value* self, const SceJson::Object* value);
void APS5_VABI _ZN3sce4Json5Value3setERKS1__nid_no_patch(SceJson::Value* self, const SceJson::Value* value);
SceJson::Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1__nid_no_patch(SceJson::Value* self, const SceJson::Value* value);
bool APS5_VABI _ZNK3sce4Json5ValuecvbEv_nid_no_patch(const SceJson::Value* self);
void APS5_VABI _ZN3sce4Json5Value4swapERS1__nid_no_patch(SceJson::Value* self, SceJson::Value* other);
void APS5_VABI _ZN3sce4Json5Value5clearEv_nid_no_patch(SceJson::Value* self);
int APS5_VABI _ZNK3sce4Json5Value5countEv_nid_no_patch(const SceJson::Value* self);
SceJson::ValueType APS5_VABI _ZNK3sce4Json5Value7getTypeEv_nid_no_patch(const SceJson::Value* self);
const SceJson::Value* APS5_VABI _ZNK3sce4Json5Value7getRootEv_nid_no_patch(const SceJson::Value* self);
void APS5_VABI _ZN3sce4Json5Value9setParentEPKS1__nid_no_patch(SceJson::Value* self, const SceJson::Value* parent);
int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6__nid_no_patch(SceJson::Value* self, SceJson::NullAccessCallback callback, void* context);

void APS5_VABI _ZNK3sce4Json5Value8toStringERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(const SceJson::Value* self, SceJson::String* out);
int APS5_VABI _ZN3sce4Json5Value18serialize_internalERSbIcSt11char_traitsIcENS0_8StlAllocIcEEEPFiS7_PvES8_PS1__nid_no_patch(SceJson::Value* self, SceJson::String* out, SceJson::SerializeCallback callback, void* context, SceJson::Value* parent);
int APS5_VABI _ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEEPFiS7_PvES8__nid_no_patch(SceJson::Value* self, SceJson::String* out, SceJson::SerializeCallback callback, void* context);
int APS5_VABI _ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(SceJson::Value* self, SceJson::String* out);

SceJson::String* APS5_VABI _ZN3sce4Json5Value11referStringEv_nid_no_patch(SceJson::Value* self);
SceJson::Array* APS5_VABI _ZN3sce4Json5Value10referArrayEv_nid_no_patch(SceJson::Value* self);
SceJson::Object* APS5_VABI _ZN3sce4Json5Value11referObjectEv_nid_no_patch(SceJson::Value* self);
std::int64_t* APS5_VABI _ZN3sce4Json5Value12referIntegerEv_nid_no_patch(SceJson::Value* self);
std::uint64_t* APS5_VABI _ZN3sce4Json5Value13referUIntegerEv_nid_no_patch(SceJson::Value* self);
double* APS5_VABI _ZN3sce4Json5Value9referRealEv_nid_no_patch(SceJson::Value* self);
bool* APS5_VABI _ZN3sce4Json5Value12referBooleanEv_nid_no_patch(SceJson::Value* self);
SceJson::Value* APS5_VABI _ZN3sce4Json5Value10referValueEm_nid_no_patch(SceJson::Value* self, std::size_t index);
SceJson::Value* APS5_VABI _ZN3sce4Json5Value10referValueERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(SceJson::Value* self, const SceJson::String* key);

const SceJson::String* APS5_VABI _ZNK3sce4Json5Value9getStringEv_nid_no_patch(const SceJson::Value* self);
const SceJson::Array* APS5_VABI _ZNK3sce4Json5Value8getArrayEv_nid_no_patch(const SceJson::Value* self);
const SceJson::Object* APS5_VABI _ZNK3sce4Json5Value9getObjectEv_nid_no_patch(const SceJson::Value* self);
const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv_nid_no_patch(const SceJson::Value* self);
const std::uint64_t* APS5_VABI _ZNK3sce4Json5Value11getUIntegerEv_nid_no_patch(const SceJson::Value* self);
const double* APS5_VABI _ZNK3sce4Json5Value7getRealEv_nid_no_patch(const SceJson::Value* self);
const bool* APS5_VABI _ZNK3sce4Json5Value10getBooleanEv_nid_no_patch(const SceJson::Value* self);
const SceJson::Value* APS5_VABI _ZNK3sce4Json5Value8getValueEm_nid_no_patch(const SceJson::Value* self, std::size_t index);
const SceJson::Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(const SceJson::Value* self, const SceJson::String* key);
const SceJson::Value* APS5_VABI _ZNK3sce4Json5ValueixEm_nid_no_patch(const SceJson::Value* self, std::size_t index);
const SceJson::Value* APS5_VABI _ZNK3sce4Json5ValueixERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(const SceJson::Value* self, const SceJson::String* key);
const SceJson::Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc_nid_no_patch(const SceJson::Value* self, const char* key);

int APS5_VABI _ZN3sce4Json6Parser12parseQuadHexERNS0_11InputStreamE_nid_no_patch(SceJson::InputStream* stream);
bool APS5_VABI _ZN3sce4Json6Parser14parseCodePointERSbIcSt11char_traitsIcENS0_8StlAllocIcEEERNS0_11InputStreamE_nid_no_patch(SceJson::String* out, SceJson::InputStream* stream);
bool APS5_VABI _ZN3sce4Json6Parser11parseStringERSbIcSt11char_traitsIcENS0_8StlAllocIcEEERNS0_11InputStreamE_nid_no_patch(SceJson::String* out, SceJson::InputStream* stream);
bool APS5_VABI _ZN3sce4Json6Parser11parseStringERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch(SceJson::Value* value, SceJson::InputStream* stream, SceJson::Value* parent);
bool APS5_VABI _ZN3sce4Json6Parser10parseArrayERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch(SceJson::Value* value, SceJson::InputStream* stream, SceJson::Value* parent);
bool APS5_VABI _ZN3sce4Json6Parser10parseValueERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch(SceJson::Value* value, SceJson::InputStream* stream, SceJson::Value* parent);
bool APS5_VABI _ZN3sce4Json6Parser11parseObjectERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch(SceJson::Value* value, SceJson::InputStream* stream, SceJson::Value* parent);
bool APS5_VABI _ZN3sce4Json6Parser11parseNumberERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch(SceJson::Value* value, SceJson::InputStream* stream, SceJson::Value* parent);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5__nid_no_patch(SceJson::Value* value, SceJson::ParserReadCallback read, void* context);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm_nid_no_patch(SceJson::Value* value, const char* text, std::size_t size);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKc_nid_no_patch(SceJson::Value* value, const char* path);

}
