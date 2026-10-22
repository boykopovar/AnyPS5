#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Value { alignas(8) std::uint64_t storage[4]{}; };
struct String { void* text; };
struct Array { void* items; };
struct Object { void* items; };
struct Iterator { void* position; };
struct Pair {
    String key;
    std::uint64_t reserved;
    Value value;
};
using NullAccessCallback = const Value& (APS5_VABI*)(std::int32_t, const Value*, void*);
using SetNullAccessCallback = int (APS5_VABI*)(void*, NullAccessCallback, void*);
using TypeMismatchHandler = const Value& (APS5_VABI*)(std::int32_t, const Value*, const Value*, void*);
using SpecialFloatHandler = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
using ElementAccessFailureHandler = void (APS5_VABI*)(std::int32_t, const String*, const std::size_t*, const Value*, const Value*, void*);

extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
void APS5_VABI _ZN3sce4Json11InitializerD1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void*, const void*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void*, NullAccessCallback, void*);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void*, NullAccessCallback, void*);
int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(Value*, NullAccessCallback, void*);
int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value*, TypeMismatchHandler, void*);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value*, SpecialFloatHandler, void*);
int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(Value*, ElementAccessFailureHandler, void*);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
int APS5_VABI _ZN3sce4Json5Value3setERKS1_(Value*, const Value*);
Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1_(Value*, const Value*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(Value*, const Value*);
void APS5_VABI _ZN3sce4Json6StringC1Ev(String*);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
std::size_t APS5_VABI _ZNK3sce4Json6String6lengthEv(const String*);
bool APS5_VABI _ZNK3sce4Json6String5emptyEv(const String*);
void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array*);
void APS5_VABI _ZN3sce4Json5ArrayC1ERKS1_(Array*, const Array*);
void APS5_VABI _ZN3sce4Json5Array5clearEv(Array*);
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array*);
Value* APS5_VABI _ZNK3sce4Json5Array8iteratorptEv(const Iterator*);
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array*);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array*, const Value*);
Iterator* APS5_VABI _ZNK3sce4Json5Array5beginEv(Iterator*, const Array*);
Iterator* APS5_VABI _ZNK3sce4Json5Array3endEv(Iterator*, const Array*);
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array*);
const Value* APS5_VABI _ZNK3sce4Json5Array4backEv(const Array*);
void APS5_VABI _ZN3sce4Json5Array9pop_frontEv(Array*);
bool APS5_VABI _ZNK3sce4Json5Array5emptyEv(const Array*);
void APS5_VABI _ZN3sce4Json5Array8iteratorD1Ev(Iterator*);
Iterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEv(Iterator*);
Value* APS5_VABI _ZNK3sce4Json5Array8iteratordeEv(const Iterator*);
bool APS5_VABI _ZNK3sce4Json5Array8iteratorneERKS2_(const Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json6ObjectC1Ev(Object*);
void APS5_VABI _ZN3sce4Json6ObjectD1Ev(Object*);
Value* APS5_VABI _ZN3sce4Json6ObjectixERKNS0_6StringE(Object*, const String*);
Iterator* APS5_VABI _ZNK3sce4Json6Object5beginEv(Iterator*, const Object*);
Iterator* APS5_VABI _ZNK3sce4Json6Object3endEv(Iterator*, const Object*);
void APS5_VABI _ZN3sce4Json6Object8iteratorD1Ev(Iterator*);
Iterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEv(Iterator*);
Pair* APS5_VABI _ZNK3sce4Json6Object8iteratordeEv(const Iterator*);
Pair* APS5_VABI _ZNK3sce4Json6Object8iteratorptEv(const Iterator*);
Iterator* APS5_VABI _ZNK3sce4Json6Object4findERKNS0_6StringE(Iterator*, const Object*, const String*);
int APS5_VABI _ZNK3sce4Json6String7compareEPKc(const String*, const char*);
int APS5_VABI _ZNK3sce4Json6String7compareERKS1_(const String*, const String*);
std::size_t APS5_VABI _ZNK3sce4Json6String4sizeEv(const String*);
std::size_t APS5_VABI _ZNK3sce4Json6String8capacityEv(const String*);
bool APS5_VABI _ZNK3sce4Json6StringeqEPKc(const String*, const char*);
bool APS5_VABI _ZNK3sce4Json6StringeqERKS1_(const String*, const String*);
extern std::size_t _ZN3sce4Json6String4nposE;
void APS5_VABI _ZN3sce4Json6String5clearEv(String*);
void APS5_VABI _ZN3sce4Json6String6resizeEm(String*, std::size_t);
void APS5_VABI _ZN3sce4Json6String7reserveEm(String*, std::size_t);
String* APS5_VABI _ZN3sce4Json6String6appendERKS1_(String*, const String*);
String* APS5_VABI _ZN3sce4Json6String6appendEPKc(String*, const char*);
String* APS5_VABI _ZN3sce4Json6String6appendEPKcm(String*, const char*, std::size_t);
String* APS5_VABI _ZN3sce4Json6StringpLEPKc(String*, const char*);
String* APS5_VABI _ZN3sce4Json6StringpLEh(String*, unsigned char);
String* APS5_VABI _ZNK3sce4Json6String6substrEmm(String*, const String*, std::size_t, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEcm(const String*, char, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEcm(const String*, char, std::size_t);
bool APS5_VABI _ZNK3sce4Json6Object8iteratorneERKS2_(const Iterator*, const Iterator*);
bool APS5_VABI _ZNK3sce4Json6Object8iteratoreqERKS2_(const Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1Eb(Value*, bool);
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value*, double);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(Value*, const Object*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value5clearEv(Value*);
int APS5_VABI _ZN3sce4Json5Value3setEl(Value*, std::int64_t);
int APS5_VABI _ZN3sce4Json5Value3setEm(Value*, std::uint64_t);
int APS5_VABI _ZN3sce4Json5Value3setEPKc(Value*, const char*);
std::int32_t APS5_VABI _ZNK3sce4Json5Value7getTypeEv(const Value*);
const bool* APS5_VABI _ZNK3sce4Json5Value10getBooleanEv(const Value*);
const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv(const Value*);
const std::uint64_t* APS5_VABI _ZNK3sce4Json5Value11getUIntegerEv(const Value*);
const double* APS5_VABI _ZNK3sce4Json5Value7getRealEv(const Value*);
const String* APS5_VABI _ZNK3sce4Json5Value9getStringEv(const Value*);
const Array* APS5_VABI _ZNK3sce4Json5Value8getArrayEv(const Value*);
std::size_t APS5_VABI _ZNK3sce4Json5Value5countEv(const Value*);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value*, const char*);
Value* APS5_VABI _ZN3sce4Json5Value10referValueERKNS0_6StringE(Value*, const String*);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEm(const Value*, std::size_t);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value*, String*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(Value*, int (APS5_VABI*)(char&, void*), void*);
bool APS5_VABI _ZNK3sce4Json6Object5emptyEv(const Object*);
std::size_t APS5_VABI _ZNK3sce4Json6Object4sizeEv(const Object*);
const Object* APS5_VABI _ZNK3sce4Json5Value9getObjectEv(const Value*);
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(Value*, std::int32_t);
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueEm(const Value*, std::size_t);
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKNS0_6StringE(const Value*, const String*);
Value* APS5_VABI _ZN3sce4Json5Value10referValueEm(Value*, std::size_t);
void APS5_VABI _ZN3sce4Json14InitParameter2C1Ev(void*);
void APS5_VABI _ZN3sce4Json14InitParameter2C2Ev(void*);
void APS5_VABI _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(void*, void*, void*);
void APS5_VABI _ZN3sce4Json14InitParameter217setFileBufferSizeEm(void*, std::size_t);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const void*);
void APS5_VABI _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(void*, void*, void*);
void APS5_VABI _ZN3sce4Json14InitParameter225setSpecialFloatFormatTypeENS0_22SpecialFloatFormatTypeE(void*, std::int32_t);
void APS5_VABI _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(void*, void*, void*, std::size_t);
void APS5_VABI _ZN3sce4Json6Object4PairC1Ev(Pair*);
void APS5_VABI _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(Pair*, const String*, const Value*);
void APS5_VABI _ZN3sce4Json6Object4PairD1Ev(Pair*);
Iterator* APS5_VABI _ZN3sce4Json6Object6insertERKNS1_4PairE(Iterator*, Object*, const Pair*);
void APS5_VABI _ZN3sce4Json6Object8iteratorC1Ev(Iterator*);
Iterator* APS5_VABI _ZN3sce4Json6Object8iteratoraSERKS2_(Iterator*, const Iterator*);
Iterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEi(Iterator*, int);
void APS5_VABI _ZN3sce4Json5Array8iteratorC1Ev(Iterator*);
Iterator* APS5_VABI _ZN3sce4Json5Array8iteratoraSERKS2_(Iterator*, const Iterator*);
Iterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEi(Iterator*, int);
void APS5_VABI _ZN3sce4Json12MemAllocator11notifyErrorEimPv(void*, int, std::size_t, void*);
bool APS5_VABI _ZNK3sce4Json5ValuecvbEv(const Value*);
void APS5_VABI _ZN3sce4Json5Value4swapERS1_(Value*, Value*);
void APS5_VABI _ZN3sce4Json5Array8iteratorC1ERKS2_(Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json5Array8iterator7advanceEm(Iterator*, std::size_t);
void APS5_VABI _ZN3sce4Json6Object8iteratorC1ERKS2_(Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json6Object8iterator7advanceEm(Iterator*, std::size_t);
void APS5_VABI _ZN3sce4Json6Object5eraseERKNS0_6StringE(Object*, const String*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Json check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

enum : std::int32_t { TypeNull, TypeBoolean, TypeInteger, TypeUInteger, TypeReal, TypeString, TypeArray, TypeObject };
constexpr int JsonErrorParse = static_cast<int>(0x80848101);

static Value fallback{};
static int callbackCalls = 0;
static std::int32_t lastRequested = -1;
static const Value* lastParent = nullptr;
static void* lastContext = nullptr;

static const Value& APS5_VABI OnNullAccess(std::int32_t requested, const Value* parent, void* context) {
    ++callbackCalls;
    lastRequested = requested;
    lastParent = parent;
    lastContext = context;
    return fallback;
}

static std::string Serialize(Value& value) {
    String out{};
    _ZN3sce4Json6StringC1Ev(&out);
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &out) == 0);
    std::string text(_ZNK3sce4Json6String5c_strEv(&out), _ZNK3sce4Json6String6lengthEv(&out));
    _ZN3sce4Json6StringD1Ev(&out);
    return text;
}

static const Value& Member(const Value& value, const char* key) {
    return *_ZNK3sce4Json5ValueixEPKc(&value, key);
}

static void ParseAndRoundTrip() {
    const std::string text =
        "{ \"name\" : \"x\\u00e9\\n\", \"count\": 3, \"big\": 18446744073709551615, \"neg\": -5,"
        " \"pi\": 1.5, \"ok\": true, \"none\": null, \"list\": [1, \"two\", [3]] }";
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text.c_str(), text.size()) == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&root) == TypeObject);
    Require(_ZNK3sce4Json5Value5countEv(&root) == 8);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&Member(root, "name"))), "x\xc3\xa9\n") == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&Member(root, "count")) == TypeUInteger);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "count")) == 3);
    Require(*_ZNK3sce4Json5Value11getUIntegerEv(&Member(root, "count")) == 3);
    Require(*_ZNK3sce4Json5Value7getRealEv(&Member(root, "count")) == 0.0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&Member(root, "big")) == TypeUInteger);
    Require(*_ZNK3sce4Json5Value11getUIntegerEv(&Member(root, "big")) == std::numeric_limits<std::uint64_t>::max());
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "neg")) == -5);
    Require(*_ZNK3sce4Json5Value7getRealEv(&Member(root, "pi")) == 1.5);
    Require(*_ZNK3sce4Json5Value10getBooleanEv(&Member(root, "ok")));
    Require(_ZNK3sce4Json5Value7getTypeEv(&Member(root, "none")) == TypeNull);
    String present{};
    String absent{};
    _ZN3sce4Json6StringC1EPKc(&present, "ok");
    _ZN3sce4Json6StringC1EPKc(&absent, "applyToAll");
    Require(_ZN3sce4Json5Value10referValueERKNS0_6StringE(&root, &present) == &Member(root, "ok"));
    Require(_ZN3sce4Json5Value10referValueERKNS0_6StringE(&root, &absent) == nullptr);
    Require(_ZNK3sce4Json5Value5countEv(&root) == 8);
    _ZN3sce4Json6StringD1Ev(&present);
    _ZN3sce4Json6StringD1Ev(&absent);

    const Value& list = Member(root, "list");
    const Array* array = _ZNK3sce4Json5Value8getArrayEv(&list);
    Require(_ZNK3sce4Json5Array4sizeEv(array) == 3);
    std::vector<std::int32_t> types;
    Iterator it{}, end{};
    _ZNK3sce4Json5Array5beginEv(&it, array);
    _ZNK3sce4Json5Array3endEv(&end, array);
    for (; _ZNK3sce4Json5Array8iteratorneERKS2_(&it, &end); _ZN3sce4Json5Array8iteratorppEv(&it))
        types.push_back(_ZNK3sce4Json5Value7getTypeEv(_ZNK3sce4Json5Array8iteratordeEv(&it)));
    _ZN3sce4Json5Array8iteratorD1Ev(&it);
    _ZN3sce4Json5Array8iteratorD1Ev(&end);
    Require((types == std::vector<std::int32_t>{TypeUInteger, TypeString, TypeArray}));
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5ValueixEm(_ZNK3sce4Json5ValueixEm(&list, 2), 0)) == 3);

    const std::string serialized = Serialize(root);
    Require(serialized == "{\"big\":18446744073709551615,\"count\":3,\"list\":[1,\"two\",[3]],"
                          "\"name\":\"x\xc3\xa9\\n\",\"neg\":-5,\"none\":null,\"ok\":true,\"pi\":1.5}");
    Value reparsed{};
    _ZN3sce4Json5ValueC1Ev(&reparsed);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&reparsed, serialized.c_str(), serialized.size()) == 0);
    Require(Serialize(reparsed) == serialized);
    _ZN3sce4Json5ValueD1Ev(&reparsed);

    for (const char* invalid : {"{\"a\":}", "[1,]", "\"open", "tru"}) {
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, invalid, std::strlen(invalid)) == JsonErrorParse);
        Require(_ZNK3sce4Json5Value7getTypeEv(&root) == TypeObject && _ZNK3sce4Json5Value5countEv(&root) == 8);
    }
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, nullptr, 0) == JsonErrorParse);
    bool nullTextRejected = false;
    try {
        _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, nullptr, 10);
    } catch (const std::invalid_argument&) {
        nullTextRejected = true;
    }
    Require(nullTextRejected);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, "", 0) == JsonErrorParse);
    Require(_ZNK3sce4Json5Value7getTypeEv(&root) == TypeObject && _ZNK3sce4Json5Value5countEv(&root) == 8);
    for (const char* overflow : {"1e309", "[-1e309]", "{\"a\":[1,1e400]}"}) {
        bool unverified = false;
        try {
            _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, overflow, std::strlen(overflow));
        } catch (const std::runtime_error& error) {
            unverified = std::string(error.what()).find("unverified") != std::string::npos;
        }
        Require(unverified);
        Require(_ZNK3sce4Json5Value7getTypeEv(&root) == TypeObject && _ZNK3sce4Json5Value5countEv(&root) == 8);
    }
    _ZN3sce4Json5ValueD1Ev(&root);
}

static std::string Nested(std::size_t depth, bool objects) {
    std::string text;
    for (std::size_t i = 0; i < depth; ++i) text += objects ? "{\"a\":" : "[";
    text += "1";
    for (std::size_t i = 0; i < depth; ++i) text += objects ? "}" : "]";
    return text;
}

static void NestingDepth() {
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    for (const bool objects : {false, true}) {
        const std::string accepted = Nested(512, objects);
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, accepted.c_str(), accepted.size()) == 0);
        for (const std::size_t depth : {std::size_t{513}, std::size_t{100000}}) {
            const std::string rejected = Nested(depth, objects);
            Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, rejected.c_str(), rejected.size()) < 0);
        }
    }
    _ZN3sce4Json5ValueD1Ev(&root);
}

static void ObjectsAndArrays() {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    String alpha{}, beta{};
    _ZN3sce4Json6StringC1EPKc(&alpha, "alpha");
    _ZN3sce4Json6StringC1EPKc(&beta, "beta");
    Require(_ZNK3sce4Json6Object5emptyEv(&object));
    Value* first = _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &alpha);
    Require(!_ZNK3sce4Json6Object5emptyEv(&object));
    Require(_ZNK3sce4Json5Value7getTypeEv(first) == TypeNull);
    Require(_ZN3sce4Json5Value3setEl(first, 1) == 0);
    Require(_ZN3sce4Json5Value3setEPKc(_ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &beta), "b") == 0);
    Require(_ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &alpha) == first);

    std::vector<std::string> keys;
    Iterator it{}, end{};
    _ZNK3sce4Json6Object5beginEv(&it, &object);
    _ZNK3sce4Json6Object3endEv(&end, &object);
    for (; _ZNK3sce4Json6Object8iteratorneERKS2_(&it, &end); _ZN3sce4Json6Object8iteratorppEv(&it)) {
        const Pair* pair = _ZNK3sce4Json6Object8iteratordeEv(&it);
        keys.emplace_back(_ZNK3sce4Json6String5c_strEv(&pair->key));
        if (keys.size() == 1) Require(&pair->value == first);
    }
    _ZN3sce4Json6Object8iteratorD1Ev(&it);
    _ZN3sce4Json6Object8iteratorD1Ev(&end);
    Require((keys == std::vector<std::string>{"alpha", "beta"}));

    Value wrapped{};
    _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&wrapped, &object);
    Require(Serialize(wrapped) == "{\"alpha\":1,\"beta\":\"b\"}");
    _ZN3sce4Json5ValueD1Ev(&wrapped);
    _ZN3sce4Json6StringD1Ev(&alpha);
    _ZN3sce4Json6StringD1Ev(&beta);
    _ZN3sce4Json6ObjectD1Ev(&object);

    Array array{};
    _ZN3sce4Json5ArrayC1Ev(&array);
    Value flag{}, real{};
    _ZN3sce4Json5ValueC1Eb(&flag, true);
    _ZN3sce4Json5ValueC1Ed(&real, 2.5);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &flag);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &real);
    _ZN3sce4Json5ValueD1Ev(&flag);
    _ZN3sce4Json5ValueD1Ev(&real);
    Require(_ZNK3sce4Json5Array4sizeEv(&array) == 2);
    Require(*_ZNK3sce4Json5Value7getRealEv(_ZNK3sce4Json5Array4backEv(&array)) == 2.5);
    int visited = 0;
    _ZNK3sce4Json5Array5beginEv(&it, &array);
    _ZNK3sce4Json5Array3endEv(&end, &array);
    for (; _ZNK3sce4Json5Array8iteratorneERKS2_(&it, &end); _ZN3sce4Json5Array8iteratorppEv(&it)) ++visited;
    Require(visited == 2);
    Require(*_ZNK3sce4Json5Value10getBooleanEv(_ZNK3sce4Json5Array5frontEv(&array)));
    _ZN3sce4Json5Array8iteratorD1Ev(&it);
    _ZNK3sce4Json5Array5beginEv(&it, &array);
    Require(_ZNK3sce4Json5Array8iteratorptEv(&it) == _ZNK3sce4Json5Array8iteratordeEv(&it));
    Array copy{};
    _ZN3sce4Json5ArrayC1ERKS1_(&copy, &array);
    _ZN3sce4Json5Array5clearEv(&array);
    Require(_ZNK3sce4Json5Array4sizeEv(&array) == 0);
    Require(_ZNK3sce4Json5Array5frontEv(&array) == _ZNK3sce4Json5Array4backEv(&array));
    Require(_ZNK3sce4Json5Array4sizeEv(&copy) == 2);
    Require(*_ZNK3sce4Json5Value10getBooleanEv(_ZNK3sce4Json5Array5frontEv(&copy)));
    Require(*_ZNK3sce4Json5Value7getRealEv(_ZNK3sce4Json5Array4backEv(&copy)) == 2.5);
    _ZN3sce4Json5Array9pop_frontEv(&copy);
    Require(_ZNK3sce4Json5Array4sizeEv(&copy) == 1);
    Require(*_ZNK3sce4Json5Value7getRealEv(_ZNK3sce4Json5Array5frontEv(&copy)) == 2.5);
    _ZN3sce4Json5Array9pop_frontEv(&copy);
    Require(_ZNK3sce4Json5Array5emptyEv(&copy));
    bool threw = false;
    try {
        _ZN3sce4Json5Array9pop_frontEv(&copy);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    Require(threw && _ZNK3sce4Json5Array5emptyEv(&copy));
    _ZN3sce4Json5Array8iteratorD1Ev(&it);
    _ZN3sce4Json5Array8iteratorD1Ev(&end);
    _ZN3sce4Json5ArrayD1Ev(&copy);
    _ZN3sce4Json5ArrayD1Ev(&array);
}

static void ObjectPair() {
    String key{};
    _ZN3sce4Json6StringC1EPKc(&key, "answer");
    Value value{};
    _ZN3sce4Json5ValueC1Ev(&value);
    Require(_ZN3sce4Json5Value3setEPKc(&value, "forty-two") == 0);

    Pair pair{};
    std::memset(&pair, 0x5a, sizeof(pair));
    _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &value);
    Require(pair.reserved == 0);
    Require(pair.key.text != key.text && _ZNK3sce4Json5Value9getStringEv(&pair.value) != _ZNK3sce4Json5Value9getStringEv(&value));
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&pair.key), "answer") == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&pair.value) == TypeString);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&pair.value)), "forty-two") == 0);

    Require(_ZN3sce4Json5Value3setEl(&value, 7) == 0);
    _ZN3sce4Json6StringD1Ev(&key);
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&pair.key), "answer") == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&pair.value) == TypeString);
    _ZN3sce4Json6Object4PairD1Ev(&pair);
    Require(pair.key.text == nullptr && _ZNK3sce4Json5Value7getTypeEv(&pair.value) == TypeNull);
}

static void ArrayIteratorCopies() {
    Array array{};
    _ZN3sce4Json5ArrayC1Ev(&array);
    for (std::int64_t i = 1; i <= 3; ++i) {
        Value item{};
        _ZN3sce4Json5ValueC1Ev(&item);
        Require(_ZN3sce4Json5Value3setEl(&item, i) == 0);
        _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &item);
        _ZN3sce4Json5ValueD1Ev(&item);
    }

    Iterator it{};
    std::memset(&it, 0x5a, sizeof(it));
    _ZN3sce4Json5Array8iteratorC1Ev(&it);
    Require(it.position != nullptr);

    Iterator begin{}, end{};
    _ZNK3sce4Json5Array5beginEv(&begin, &array);
    _ZNK3sce4Json5Array3endEv(&end, &array);
    Require(_ZN3sce4Json5Array8iteratoraSERKS2_(&it, &begin) == &it);
    Require(!_ZNK3sce4Json5Array8iteratorneERKS2_(&it, &begin));

    std::vector<std::int64_t> seen;
    Iterator previous{};
    while (_ZNK3sce4Json5Array8iteratorneERKS2_(&it, &end)) {
        _ZN3sce4Json5Array8iteratorD1Ev(&previous);
        _ZN3sce4Json5Array8iteratorC1ERKS2_(&previous, &it);
        Require(_ZN3sce4Json5Array8iteratorppEi(&it, 0) == &it);
        seen.push_back(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array8iteratordeEv(&previous)));
        Require(_ZNK3sce4Json5Array8iteratorneERKS2_(&previous, &it));
    }
    Require((seen == std::vector<std::int64_t>{1, 2, 3}));
    Require(!_ZNK3sce4Json5Array8iteratorneERKS2_(&it, &end));
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array8iteratordeEv(&previous)) == 3);
    Require(_ZN3sce4Json5Array8iteratoraSERKS2_(&it, &end) == &it);
    Require(it.position != nullptr && !_ZNK3sce4Json5Array8iteratorneERKS2_(&it, &end));
    Require(_ZN3sce4Json5Array8iteratoraSERKS2_(&it, &begin) == &it);
    Require(it.position != nullptr);
    Iterator empty{};
    _ZN3sce4Json5Array8iteratorC1Ev(&empty);
    Require(_ZN3sce4Json5Array8iteratoraSERKS2_(&it, &empty) == &it && it.position == nullptr);
    Require(_ZN3sce4Json5Array8iteratoraSERKS2_(&it, &begin) == &it && it.position == nullptr);
    _ZN3sce4Json5Array8iteratorD1Ev(&empty);
    Require(!_ZNK3sce4Json5Array8iteratorneERKS2_(&it, &begin));

    _ZN3sce4Json5Array8iteratorD1Ev(&previous);
    _ZN3sce4Json5Array8iteratorD1Ev(&it);
    _ZN3sce4Json5Array8iteratorD1Ev(&begin);
    _ZN3sce4Json5Array8iteratorD1Ev(&end);
    _ZN3sce4Json5ArrayD1Ev(&array);
}

static void AllocatorNotifyError() {
    int allocator = 0x1234;
    int userData = 0x5678;
    _ZN3sce4Json12MemAllocator11notifyErrorEimPv(&allocator, -1, 64, &userData);
    Require(allocator == 0x1234 && userData == 0x5678);
}

static void ObjectFind() {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    String alpha{}, beta{}, gamma{};
    _ZN3sce4Json6StringC1EPKc(&alpha, "alpha");
    _ZN3sce4Json6StringC1EPKc(&beta, "beta");
    _ZN3sce4Json6StringC1EPKc(&gamma, "gamma");
    Iterator found{}, end{};
    _ZNK3sce4Json6Object3endEv(&end, &object);
    _ZNK3sce4Json6Object4findERKNS0_6StringE(&found, &object, &alpha);
    Require(!_ZNK3sce4Json6Object8iteratorneERKS2_(&found, &end));
    _ZN3sce4Json6Object8iteratorD1Ev(&found);
    _ZN3sce4Json6Object8iteratorD1Ev(&end);

    Require(_ZN3sce4Json5Value3setEl(_ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &alpha), 1) == 0);
    Value* second = _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &beta);
    Require(_ZN3sce4Json5Value3setEPKc(second, "b") == 0);
    _ZNK3sce4Json6Object3endEv(&end, &object);
    _ZN3sce4Json6Object8iteratorD1Ev(&found);
    Iterator* returned = _ZNK3sce4Json6Object4findERKNS0_6StringE(&found, &object, &beta);
    Require(returned == &found);
    Require(_ZNK3sce4Json6Object8iteratorneERKS2_(&found, &end));
    const Pair* pair = _ZNK3sce4Json6Object8iteratorptEv(&found);
    Require(pair == _ZNK3sce4Json6Object8iteratordeEv(&found));
    Require(&pair->value == second);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&pair->key), "beta") == 0);
    _ZN3sce4Json6Object8iteratorppEv(&found);
    Require(!_ZNK3sce4Json6Object8iteratorneERKS2_(&found, &end));
    _ZN3sce4Json6Object8iteratorD1Ev(&found);
    _ZNK3sce4Json6Object4findERKNS0_6StringE(&found, &object, &gamma);
    Require(!_ZNK3sce4Json6Object8iteratorneERKS2_(&found, &end));
    Require(_ZNK3sce4Json6Object4sizeEv(&object) == 2);
    _ZN3sce4Json6Object8iteratorD1Ev(&found);
    _ZN3sce4Json6Object8iteratorD1Ev(&end);

    Require(_ZNK3sce4Json6String7compareEPKc(&beta, "beta") == 0);
    Require(_ZNK3sce4Json6String7compareEPKc(&beta, "betb") < 0);
    Require(_ZNK3sce4Json6String7compareEPKc(&beta, "bet") > 0);
    Require(_ZNK3sce4Json6String7compareEPKc(&beta, "alpha") > 0);
    bool threw = false;
    try {
        static_cast<void>(_ZNK3sce4Json6String7compareEPKc(&beta, nullptr));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    _ZN3sce4Json6StringD1Ev(&alpha);
    _ZN3sce4Json6StringD1Ev(&beta);
    _ZN3sce4Json6StringD1Ev(&gamma);
    _ZN3sce4Json6ObjectD1Ev(&object);
}

static void ObjectIteratorCopies() {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    String key1{}, key2{};
    _ZN3sce4Json6StringC1EPKc(&key1, "one");
    _ZN3sce4Json6StringC1EPKc(&key2, "two");
    Value val1{}, val2{};
    _ZN3sce4Json5ValueC1Ev(&val1);
    _ZN3sce4Json5ValueC1Ev(&val2);
    Require(_ZN3sce4Json5Value3setEl(&val1, 10) == 0);
    Require(_ZN3sce4Json5Value3setEl(&val2, 20) == 0);
    Pair p1{}, p2{};
    _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&p1, &key1, &val1);
    _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&p2, &key2, &val2);
    Iterator ins1{}, ins2{};
    Require(_ZN3sce4Json6Object6insertERKNS1_4PairE(&ins1, &object, &p1) == &ins1);
    Require(_ZN3sce4Json6Object6insertERKNS1_4PairE(&ins2, &object, &p2) == &ins2);
    _ZN3sce4Json6Object8iteratorD1Ev(&ins1);
    _ZN3sce4Json6Object8iteratorD1Ev(&ins2);

    Iterator it{};
    std::memset(&it, 0x5a, sizeof(it));
    _ZN3sce4Json6Object8iteratorC1Ev(&it);
    Require(it.position != nullptr);
    _ZN3sce4Json6Object8iteratorD1Ev(&it);

    Iterator begin{}, end{};
    _ZNK3sce4Json6Object5beginEv(&begin, &object);
    _ZNK3sce4Json6Object3endEv(&end, &object);

    Require(!_ZNK3sce4Json6Object8iteratoreqERKS2_(&begin, &end));
    Require(_ZNK3sce4Json6Object8iteratorneERKS2_(&begin, &end));

    Require(_ZN3sce4Json6Object8iteratorppEi(&begin, 0) == &begin);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&_ZNK3sce4Json6Object8iteratordeEv(&begin)->key), "two") == 0);

    Iterator assigned{};
    _ZN3sce4Json6Object8iteratorC1Ev(&assigned);
    Require(_ZN3sce4Json6Object8iteratoraSERKS2_(&assigned, &begin) == &assigned);
    Require(_ZNK3sce4Json6Object8iteratoreqERKS2_(&assigned, &begin));
    Require(!_ZNK3sce4Json6Object8iteratorneERKS2_(&assigned, &begin));
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&_ZNK3sce4Json6Object8iteratordeEv(&assigned)->key), "two") == 0);
    _ZN3sce4Json6Object8iteratorD1Ev(&assigned);

    _ZN3sce4Json6Object8iteratorD1Ev(&begin);
    _ZN3sce4Json6Object8iteratorD1Ev(&end);
    _ZN3sce4Json6Object4PairD1Ev(&p1);
    _ZN3sce4Json6Object4PairD1Ev(&p2);
    _ZN3sce4Json5ValueD1Ev(&val1);
    _ZN3sce4Json5ValueD1Ev(&val2);
    _ZN3sce4Json6StringD1Ev(&key1);
    _ZN3sce4Json6StringD1Ev(&key2);
    _ZN3sce4Json6ObjectD1Ev(&object);
}

static void ObjectInsert() {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    String key{};
    _ZN3sce4Json6StringC1EPKc(&key, "score");
    Value value{};
    _ZN3sce4Json5ValueC1Ev(&value);
    Require(_ZN3sce4Json5Value3setEl(&value, 100) == 0);
    Pair pair{};
    _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &value);

    Iterator inserted{}, end{};
    _ZNK3sce4Json6Object3endEv(&end, &object);
    Require(_ZN3sce4Json6Object6insertERKNS1_4PairE(&inserted, &object, &pair) == &inserted);
    Require(_ZNK3sce4Json6Object8iteratorneERKS2_(&inserted, &end));
    Require(_ZNK3sce4Json6Object4sizeEv(&object) == 1);
    const Pair* p = _ZNK3sce4Json6Object8iteratordeEv(&inserted);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&p->key), "score") == 0);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&p->value) == 100);

    Require(_ZN3sce4Json5Value3setEl(&pair.value, 200) == 0);
    Iterator duplicate{};
    Require(_ZN3sce4Json6Object6insertERKNS1_4PairE(&duplicate, &object, &pair) == &duplicate);
    Require(_ZNK3sce4Json6Object8iteratoreqERKS2_(&duplicate, &inserted));
    Require(_ZNK3sce4Json6Object4sizeEv(&object) == 1);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&_ZNK3sce4Json6Object8iteratordeEv(&duplicate)->value) == 100);
    _ZN3sce4Json6Object8iteratorD1Ev(&duplicate);

    String otherKey{};
    _ZN3sce4Json6StringC1EPKc(&otherKey, "other");
    Value otherValue{};
    _ZN3sce4Json5ValueC1Ev(&otherValue);
    Require(_ZN3sce4Json5Value3setEPKc(&otherValue, "str") == 0);
    Pair otherPair{};
    _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&otherPair, &otherKey, &otherValue);
    Iterator otherInserted{};
    Require(_ZN3sce4Json6Object6insertERKNS1_4PairE(&otherInserted, &object, &otherPair) == &otherInserted);
    Require(_ZNK3sce4Json6Object4sizeEv(&object) == 2);
    Iterator first{};
    _ZNK3sce4Json6Object5beginEv(&first, &object);
    Require(_ZNK3sce4Json6Object8iteratoreqERKS2_(&first, &otherInserted));
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&_ZNK3sce4Json6Object8iteratordeEv(&inserted)->value) == 100);
    _ZN3sce4Json6Object8iteratorD1Ev(&first);

    _ZN3sce4Json6Object8iteratorD1Ev(&otherInserted);
    _ZN3sce4Json6Object4PairD1Ev(&otherPair);
    _ZN3sce4Json5ValueD1Ev(&otherValue);
    _ZN3sce4Json6StringD1Ev(&otherKey);
    _ZN3sce4Json6Object8iteratorD1Ev(&inserted);
    _ZN3sce4Json6Object8iteratorD1Ev(&end);
    _ZN3sce4Json6Object4PairD1Ev(&pair);
    _ZN3sce4Json5ValueD1Ev(&value);
    _ZN3sce4Json6StringD1Ev(&key);
    _ZN3sce4Json6ObjectD1Ev(&object);
}

static void ObjectKeyOrder() {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    for (const char* name : {"z", "\xc3\xa9", "a", "A", ""}) {
        String key{};
        _ZN3sce4Json6StringC1EPKc(&key, name);
        _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &key);
        _ZN3sce4Json6StringD1Ev(&key);
    }
    Iterator member{};
    _ZNK3sce4Json6Object5beginEv(&member, &object);
    for (const char* expected : {"", "A", "a", "z", "\xc3\xa9"}) {
        Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&_ZNK3sce4Json6Object8iteratordeEv(&member)->key), expected) == 0);
        _ZN3sce4Json6Object8iteratorppEv(&member);
    }
    _ZN3sce4Json6Object8iteratorD1Ev(&member);
    String key{};
    _ZN3sce4Json6StringC1EPKc(&key, "a");
    Value* existing = _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &key);
    _ZN3sce4Json6String6appendEPKcm(&key, "\0suffix", 7);
    Require(_ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &key) == existing);
    _ZNK3sce4Json6Object4findERKNS0_6StringE(&member, &object, &key);
    Require(&_ZNK3sce4Json6Object8iteratordeEv(&member)->value == existing);
    _ZN3sce4Json6Object8iteratorD1Ev(&member);
    Require(_ZNK3sce4Json6Object4sizeEv(&object) == 5);
    _ZN3sce4Json6Object5eraseERKNS0_6StringE(&object, &key);
    Require(_ZNK3sce4Json6Object4sizeEv(&object) == 4);
    _ZN3sce4Json6StringD1Ev(&key);
    _ZN3sce4Json6ObjectD1Ev(&object);
}

static void ParseDuplicateKeys() {
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    for (const char* text : {R"({"a":1,"a":2})", R"({"a":[1,2],"a":2})", R"({"a":{"old":true},"a":2})"}) {
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text, std::strlen(text)) == 0);
        Require(_ZNK3sce4Json5Value5countEv(&root) == 1);
        Require(Serialize(root) == R"({"a":2})");
    }
    _ZN3sce4Json5ValueD1Ev(&root);
}

static void StringComparison() {
    String left{}, right{};
    _ZN3sce4Json6StringC1EPKc(&left, "a");
    _ZN3sce4Json6StringC1EPKc(&right, "a");
    _ZN3sce4Json6String6appendEPKcm(&left, "\0suffix", 7);
    Require(_ZNK3sce4Json6StringeqERKS1_(&left, &right));
    Require(_ZNK3sce4Json6StringeqEPKc(&left, "a"));
    Require(_ZNK3sce4Json6String7compareERKS1_(&left, &right) == 0);
    Require(_ZNK3sce4Json6String7compareEPKc(&left, "a") == 0);
    Require(_ZNK3sce4Json6String7compareEPKc(&left, "z") == -25);
    Require(_ZNK3sce4Json6String7compareEPKc(&left, "") == 97);
    Require(_ZNK3sce4Json6String7compareEPKc(&left, "\xff") == -158);
    _ZN3sce4Json6StringD1Ev(&right);
    _ZN3sce4Json6StringD1Ev(&left);
}

static void StringMethods() {
    Require(_ZN3sce4Json6String4nposE == static_cast<std::size_t>(-1));
    const auto text = [](const String& string) { return std::string(_ZNK3sce4Json6String5c_strEv(&string), _ZNK3sce4Json6String6lengthEv(&string)); };
    String base{};
    String suffix{};
    _ZN3sce4Json6StringC1EPKc(&base, "a/b/c");
    _ZN3sce4Json6StringC1EPKc(&suffix, "-tail");
    Require(_ZNK3sce4Json6String4findEcm(&base, '/', 0) == 1);
    Require(_ZNK3sce4Json6String4findEcm(&base, '/', 2) == 3);
    Require(_ZNK3sce4Json6String4findEcm(&base, 'z', 0) == _ZN3sce4Json6String4nposE);
    Require(_ZNK3sce4Json6String5rfindEcm(&base, '/', _ZN3sce4Json6String4nposE) == 3);
    Require(_ZNK3sce4Json6String5rfindEcm(&base, '/', 2) == 1);
    String part{};
    Require(_ZNK3sce4Json6String6substrEmm(&part, &base, 2, 3) == &part);
    Require(text(part) == "b/c");
    _ZN3sce4Json6StringD1Ev(&part);
    Require(_ZNK3sce4Json6String6substrEmm(&part, &base, 5, _ZN3sce4Json6String4nposE) == &part);
    Require(text(part).empty());
    _ZN3sce4Json6StringD1Ev(&part);
    bool refused = false;
    try {
        _ZNK3sce4Json6String6substrEmm(&part, &base, 6, 1);
    } catch (const std::out_of_range&) {
        refused = true;
    }
    Require(refused);
    Require(_ZNK3sce4Json6String4sizeEv(&base) == 5);
    Require(_ZNK3sce4Json6String6lengthEv(&base) == 5);
    String match{};
    _ZN3sce4Json6StringC1EPKc(&match, "a/b/c");
    Require(_ZNK3sce4Json6StringeqERKS1_(&base, &match));
    Require(_ZNK3sce4Json6String7compareERKS1_(&base, &match) == 0);
    _ZN3sce4Json6StringD1Ev(&match);
    _ZN3sce4Json6String7reserveEm(&base, 64);
    Require(_ZNK3sce4Json6String8capacityEv(&base) >= 64);
    Require(_ZN3sce4Json6String6appendERKS1_(&base, &suffix) == &base);
    Require(_ZN3sce4Json6String6appendEPKc(&base, "!") == &base);
    Require(_ZN3sce4Json6String6appendEPKcm(&base, "12345", 2) == &base);
    Require(_ZN3sce4Json6StringpLEPKc(&base, "+") == &base);
    Require(_ZN3sce4Json6StringpLEh(&base, '?') == &base);
    Require(text(base) == "a/b/c-tail!12+?");
    _ZN3sce4Json6String6resizeEm(&base, 5);
    Require(_ZNK3sce4Json6String4sizeEv(&base) == 5);
    Require(text(base) == "a/b/c");
    _ZN3sce4Json6String6appendERKS1_(&base, &base);
    Require(text(base) == "a/b/ca/b/c");
    _ZN3sce4Json6String5clearEv(&base);
    Require(text(base).empty());
    _ZN3sce4Json6StringD1Ev(&suffix);
    _ZN3sce4Json6StringD1Ev(&base);
}

static void ValueSwap() {
    Value v1{}, v2{};
    _ZN3sce4Json5ValueC1Ev(&v1);
    _ZN3sce4Json5ValueC1Ev(&v2);
    Require(_ZN3sce4Json5Value3setEl(&v1, 123) == 0);
    Require(_ZN3sce4Json5Value3setEPKc(&v2, "hello") == 0);
    _ZN3sce4Json5Value4swapERS1_(&v1, &v2);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&v2) == 123);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&v1)), "hello") == 0);
    Require(_ZN3sce4Json5Value3setEl(&v1, -456) == 0);
    const auto* firstStorage = _ZNK3sce4Json5Value10getIntegerEv(&v1);
    const auto* secondStorage = _ZNK3sce4Json5Value10getIntegerEv(&v2);
    _ZN3sce4Json5Value4swapERS1_(&v1, &v2);
    Require(_ZNK3sce4Json5Value10getIntegerEv(&v1) == firstStorage && *firstStorage == 123);
    Require(_ZNK3sce4Json5Value10getIntegerEv(&v2) == secondStorage && *secondStorage == -456);
    _ZN3sce4Json5Value4swapERS1_(&v1, &v1);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&v1) == 123);
    _ZN3sce4Json5ValueD1Ev(&v1);
    _ZN3sce4Json5ValueD1Ev(&v2);
}

static void ArrayIteratorAdvance() {
    Array array{};
    _ZN3sce4Json5ArrayC1Ev(&array);
    for (int i = 0; i < 5; ++i) {
        Value val{};
        _ZN3sce4Json5ValueC1Ev(&val);
        _ZN3sce4Json5Value3setEl(&val, i * 10);
        _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &val);
        _ZN3sce4Json5ValueD1Ev(&val);
    }
    Iterator begin{};
    _ZNK3sce4Json5Array5beginEv(&begin, &array);
    Iterator copy{};
    _ZN3sce4Json5Array8iteratorC1ERKS2_(&copy, &begin);
    _ZN3sce4Json5Array8iterator7advanceEm(&copy, 3);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array8iteratordeEv(&copy)) == 30);
    Iterator end{};
    _ZNK3sce4Json5Array3endEv(&end, &array);
    _ZN3sce4Json5Array8iterator7advanceEm(&copy, std::numeric_limits<std::size_t>::max());
    Require(!_ZNK3sce4Json5Array8iteratorneERKS2_(&copy, &begin));
    _ZN3sce4Json5Array8iterator7advanceEm(&copy, 5);
    Require(!_ZNK3sce4Json5Array8iteratorneERKS2_(&copy, &end));
    _ZN3sce4Json5Array8iteratorppEv(&copy);
    Require(!_ZNK3sce4Json5Array8iteratorneERKS2_(&copy, &begin));
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array8iteratordeEv(&begin)) == 0);
    _ZN3sce4Json5Array8iteratorD1Ev(&end);
    _ZN3sce4Json5Array8iteratorD1Ev(&begin);
    _ZN3sce4Json5Array8iteratorD1Ev(&copy);
    _ZN3sce4Json5ArrayD1Ev(&array);
}

static void ObjectOperations() {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    String k1{}, k2{}, k3{};
    _ZN3sce4Json6StringC1EPKc(&k1, "alpha");
    _ZN3sce4Json6StringC1EPKc(&k2, "beta");
    _ZN3sce4Json6StringC1EPKc(&k3, "gamma");
    Value v1{}, v2{}, v3{};
    _ZN3sce4Json5ValueC1Ev(&v1);
    _ZN3sce4Json5ValueC1Ev(&v2);
    _ZN3sce4Json5ValueC1Ev(&v3);
    _ZN3sce4Json5Value3setEl(&v1, 1);
    _ZN3sce4Json5Value3setEl(&v2, 2);
    _ZN3sce4Json5Value3setEl(&v3, 3);
    Pair p1{}, p2{}, p3{};
    _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&p1, &k1, &v1);
    _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&p2, &k2, &v2);
    _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&p3, &k3, &v3);
    Iterator ins{};
    _ZN3sce4Json6Object6insertERKNS1_4PairE(&ins, &object, &p3);
    _ZN3sce4Json6Object8iteratorD1Ev(&ins);
    _ZN3sce4Json6Object6insertERKNS1_4PairE(&ins, &object, &p2);
    _ZN3sce4Json6Object8iteratorD1Ev(&ins);
    _ZN3sce4Json6Object6insertERKNS1_4PairE(&ins, &object, &p1);
    _ZN3sce4Json6Object8iteratorD1Ev(&ins);

    Iterator begin{};
    _ZNK3sce4Json6Object5beginEv(&begin, &object);
    Iterator copy{};
    _ZN3sce4Json6Object8iteratorC1ERKS2_(&copy, &begin);
    _ZN3sce4Json6Object8iterator7advanceEm(&copy, 2);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&_ZNK3sce4Json6Object8iteratordeEv(&copy)->key), "gamma") == 0);
    _ZN3sce4Json6Object8iterator7advanceEm(&copy, 1);
    bool endRejected = false;
    try {
        _ZN3sce4Json6Object8iterator7advanceEm(&copy, 1);
    } catch (const std::out_of_range&) {
        endRejected = true;
    }
    Require(endRejected);
    _ZN3sce4Json6Object8iteratorD1Ev(&begin);
    _ZN3sce4Json6Object8iteratorD1Ev(&copy);

    Require(_ZNK3sce4Json6Object4sizeEv(&object) == 3);
    _ZN3sce4Json6Object5eraseERKNS0_6StringE(&object, &k2);
    Require(_ZNK3sce4Json6Object4sizeEv(&object) == 2);
    _ZN3sce4Json6Object5eraseERKNS0_6StringE(&object, &k2);
    Require(_ZNK3sce4Json6Object4sizeEv(&object) == 2);

    Iterator findResult{}, end{};
    _ZNK3sce4Json6Object4findERKNS0_6StringE(&findResult, &object, &k2);
    _ZNK3sce4Json6Object3endEv(&end, &object);
    Require(_ZNK3sce4Json6Object8iteratoreqERKS2_(&findResult, &end));
    _ZN3sce4Json6Object8iteratorD1Ev(&findResult);
    _ZN3sce4Json6Object8iteratorD1Ev(&end);

    _ZN3sce4Json6Object4PairD1Ev(&p1);
    _ZN3sce4Json6Object4PairD1Ev(&p2);
    _ZN3sce4Json6Object4PairD1Ev(&p3);
    _ZN3sce4Json5ValueD1Ev(&v1);
    _ZN3sce4Json5ValueD1Ev(&v2);
    _ZN3sce4Json5ValueD1Ev(&v3);
    _ZN3sce4Json6StringD1Ev(&k1);
    _ZN3sce4Json6StringD1Ev(&k2);
    _ZN3sce4Json6StringD1Ev(&k3);
    _ZN3sce4Json6ObjectD1Ev(&object);
}

static void NullAccess(SetNullAccessCallback setCallback) {
    callbackCalls = 0;
    lastRequested = -1;
    lastParent = nullptr;
    lastContext = nullptr;
    alignas(16) std::uint8_t initializer[16]{};
    alignas(16) std::uint8_t parameter[64]{};
    JsonTest::Allocator allocator;
    void* allocatorPointer = &allocator;
    std::memcpy(parameter, &allocatorPointer, sizeof(allocatorPointer));
    _ZN3sce4Json11InitializerC1Ev(initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(initializer, parameter) == 0);
    int context = 0;
    Require(setCallback(initializer, OnNullAccess, &context) == 0);
    _ZN3sce4Json5ValueC1Ev(&fallback);
    Require(_ZN3sce4Json5Value3setEl(&fallback, 99) == 0);
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    const char text[] = "{\"present\":\"text\",\"none\":null}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text, sizeof(text) - 1) == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&Member(root, "missing")) == TypeNull && callbackCalls == 0);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "none")) == 99);
    Require(callbackCalls == 1 && lastRequested == TypeInteger && lastParent == &root && lastContext == &context);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "present")) == 0 && callbackCalls == 1);
    Require(!*_ZNK3sce4Json5Value10getBooleanEv(&Member(root, "none")));
    Require(callbackCalls == 2 && lastRequested == TypeBoolean);
    Require(_ZNK3sce4Json5Value7getTypeEv(_ZNK3sce4Json5ValueixEm(&root, 0)) == TypeNull);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "missing")) == 99 && callbackCalls == 3);
    _ZN3sce4Json5ValueD1Ev(&root);
    _ZN3sce4Json5ValueD1Ev(&fallback);
    Require(_ZN3sce4Json11Initializer9terminateEv(initializer) == 0);
    Value after{};
    _ZN3sce4Json5ValueC1Ev(&after);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&after) == 0 && callbackCalls == 3);
    _ZN3sce4Json5ValueD1Ev(&after);
    _ZN3sce4Json11InitializerD1Ev(initializer);
}

static void ValueAccess() {
    for (std::int32_t type = TypeNull; type <= TypeObject; ++type) {
        Value typed{};
        _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&typed, type);
        Require(_ZNK3sce4Json5Value7getTypeEv(&typed) == type);
        Require(_ZNK3sce4Json5Value5countEv(&typed) == 0);
        _ZN3sce4Json5ValueD1Ev(&typed);
    }

    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    const char text[] = "{\"list\":[10,20,30],\"name\":\"value\",\"flag\":true}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text, sizeof(text) - 1) == 0);
    Require(_ZNK3sce4Json6Object4sizeEv(_ZNK3sce4Json5Value9getObjectEv(&root)) == 3);
    Iterator member{};
    _ZNK3sce4Json6Object5beginEv(&member, _ZNK3sce4Json5Value9getObjectEv(&root));
    for (const char* expected : {"flag", "list", "name"}) {
        Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&_ZNK3sce4Json6Object8iteratordeEv(&member)->key), expected) == 0);
        _ZN3sce4Json6Object8iteratorppEv(&member);
    }
    _ZN3sce4Json6Object8iteratorD1Ev(&member);

    String key{};
    _ZN3sce4Json6StringC1EPKc(&key, "list");
    const Value* list = _ZNK3sce4Json5Value8getValueERKNS0_6StringE(&root, &key);
    Require(_ZNK3sce4Json5Value7getTypeEv(list) == TypeArray);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Value8getValueEm(list, 1)) == 20);
    Require(_ZNK3sce4Json5Value8getValueEm(list, 2) == _ZNK3sce4Json5ValueixEm(list, 2));
    Require(_ZNK3sce4Json5Value7getTypeEv(_ZNK3sce4Json5Value8getValueEm(list, 3)) == TypeNull);

    Value* mutableList = _ZN3sce4Json5Value10referValueERKNS0_6StringE(&root, &key);
    Value* element = _ZN3sce4Json5Value10referValueEm(mutableList, 0);
    Require(element != nullptr);
    Require(_ZN3sce4Json5Value3setEl(element, 15) == 0);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Value8getValueEm(list, 0)) == 15);
    Require(_ZN3sce4Json5Value10referValueEm(mutableList, 3) == nullptr);
    Require(_ZN3sce4Json5Value10referValueEm(&root, 0) != nullptr);
    _ZN3sce4Json6StringD1Ev(&key);

    String missing{};
    _ZN3sce4Json6StringC1EPKc(&missing, "missing");
    Require(_ZNK3sce4Json5Value7getTypeEv(_ZNK3sce4Json5Value8getValueERKNS0_6StringE(&root, &missing)) == TypeNull);
    _ZN3sce4Json6StringD1Ev(&missing);
    _ZN3sce4Json5ValueD1Ev(&root);

}

static void ValueAccessParameters() {
    alignas(16) std::uint8_t parameter[40];
    std::memset(parameter, 0xff, sizeof(parameter));
    _ZN3sce4Json14InitParameter2C1Ev(parameter);
    JsonTest::Allocator allocator;
    int userData = 0;
    _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(parameter, &allocator, &userData);
    _ZN3sce4Json14InitParameter217setFileBufferSizeEm(parameter, 4096);
    void* stored[3]{};
    std::memcpy(stored, parameter, sizeof(stored));
    Require(stored[0] == &allocator && stored[1] == &userData && reinterpret_cast<std::uintptr_t>(stored[2]) == 4096);
    alignas(16) std::uint8_t initializer[16]{};
    _ZN3sce4Json11InitializerC1Ev(initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(initializer, parameter) == 0);
    Require(_ZN3sce4Json11Initializer9terminateEv(initializer) == 0);
    _ZN3sce4Json11InitializerD1Ev(initializer);
}

static void InitParameter2BaseConstructor() {
    alignas(16) std::uint8_t parameter[40];
    std::memset(parameter, 0xff, sizeof(parameter));
    _ZN3sce4Json14InitParameter2C1Ev(parameter);
    alignas(16) std::uint8_t baseStorage[72];
    std::memset(baseStorage, 0xff, sizeof(baseStorage));
    auto* baseParameter = baseStorage + 16;
    _ZN3sce4Json14InitParameter2C2Ev(baseParameter);
    Require(std::memcmp(parameter, baseParameter, sizeof(parameter)) == 0);
    void* defaultAllocator;
    void* defaultUserData;
    std::size_t defaultFileBufferSize;
    std::memcpy(&defaultAllocator, baseParameter, sizeof(defaultAllocator));
    std::memcpy(&defaultUserData, baseParameter + sizeof(void*), sizeof(defaultUserData));
    std::memcpy(&defaultFileBufferSize, baseParameter + 2 * sizeof(void*), sizeof(defaultFileBufferSize));
    Require(defaultAllocator == nullptr && defaultUserData == nullptr && defaultFileBufferSize == 0);
    JsonTest::Allocator allocator;
    int userData = 0;
    _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(baseParameter, &allocator, &userData);
    _ZN3sce4Json14InitParameter217setFileBufferSizeEm(baseParameter, 4096);
    void* storedAllocator;
    void* storedUserData;
    std::size_t storedFileBufferSize;
    std::memcpy(&storedAllocator, baseParameter, sizeof(storedAllocator));
    std::memcpy(&storedUserData, baseParameter + sizeof(void*), sizeof(storedUserData));
    std::memcpy(&storedFileBufferSize, baseParameter + 2 * sizeof(void*), sizeof(storedFileBufferSize));
    Require(storedAllocator == &allocator && storedUserData == &userData && storedFileBufferSize == 4096);
    for (std::size_t i = 0; i < 16; ++i) Require(baseStorage[i] == 0xff);
    for (std::size_t i = 16 + sizeof(parameter); i < sizeof(baseStorage); ++i)
        Require(baseStorage[i] == 0xff);
    alignas(16) std::uint8_t initializer[16]{};
    _ZN3sce4Json11InitializerC1Ev(initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(initializer, baseParameter) == 0);
    Require(_ZN3sce4Json11Initializer9terminateEv(initializer) == 0);
    _ZN3sce4Json11InitializerD1Ev(initializer);
}

struct StoredInitParameter2 {
    void* allocator;
    void* userData;
    std::size_t fileBufferSize;
    std::int32_t specialFloatFormatType;
    std::uint32_t reserved[3];
};
static_assert(sizeof(StoredInitParameter2) == 40);

static StoredInitParameter2 Stored(const std::uint8_t* parameter) {
    StoredInitParameter2 stored{};
    std::memcpy(&stored, parameter, sizeof(stored));
    return stored;
}

static void InitParameters() {
    using Constructor = void (APS5_VABI*)(void*);
    for (Constructor construct : {_ZN3sce4Json14InitParameter2C1Ev, _ZN3sce4Json14InitParameter2C2Ev}) {
        alignas(16) std::uint8_t parameter[40];
        std::memset(parameter, 0xff, sizeof(parameter));
        construct(parameter);
        StoredInitParameter2 stored = Stored(parameter);
        Require(stored.allocator == nullptr && stored.userData == nullptr && stored.fileBufferSize == 0);
        Require(stored.specialFloatFormatType == 2);
        Require(stored.reserved[0] == 0 && stored.reserved[1] == 0 && stored.reserved[2] == 0);

        JsonTest::Allocator allocParam;
        int userData = 0;
        _ZN3sce4Json14InitParameter217setFileBufferSizeEm(parameter, 8192);
        _ZN3sce4Json14InitParameter225setSpecialFloatFormatTypeENS0_22SpecialFloatFormatTypeE(parameter, 2);
        _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(parameter, &allocParam, &userData);
        stored = Stored(parameter);
        Require(stored.allocator == &allocParam && stored.userData == &userData && stored.fileBufferSize == 8192);
        Require(stored.specialFloatFormatType == 2);
        Require(stored.reserved[0] == 0 && stored.reserved[1] == 0 && stored.reserved[2] == 0);

        _ZN3sce4Json14InitParameter225setSpecialFloatFormatTypeENS0_22SpecialFloatFormatTypeE(parameter, 0);
        Require(Stored(parameter).specialFloatFormatType == 0 && Stored(parameter).fileBufferSize == 8192);

        alignas(16) std::uint8_t initializer[16]{};
        _ZN3sce4Json11InitializerC1Ev(initializer);
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(initializer, parameter) == 0);
        Require(_ZN3sce4Json11Initializer9terminateEv(initializer) == 0);
        _ZN3sce4Json11InitializerD1Ev(initializer);
    }


}

static void ValueClear() {
    Value value{};
    _ZN3sce4Json5ValueC1Ev(&value);
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeNull);
    _ZN3sce4Json5Value5clearEv(&value);
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeNull);

    Require(_ZN3sce4Json5Value3setEl(&value, 12345) == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeInteger);
    _ZN3sce4Json5Value5clearEv(&value);
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeNull);

    Require(_ZN3sce4Json5Value3setEPKc(&value, "test string") == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeString);
    _ZN3sce4Json5Value5clearEv(&value);
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeNull);

    Object obj{};
    _ZN3sce4Json6ObjectC1Ev(&obj);
    String key{};
    _ZN3sce4Json6StringC1EPKc(&key, "prop");
    Value* item = _ZN3sce4Json6ObjectixERKNS0_6StringE(&obj, &key);
    _ZN3sce4Json5Value3setEl(item, 999);
    _ZN3sce4Json5ValueD1Ev(&value);
    _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&value, &obj);
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeObject);
    _ZN3sce4Json5Value5clearEv(&value);
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeNull);
    _ZN3sce4Json6StringD1Ev(&key);
    _ZN3sce4Json6ObjectD1Ev(&obj);

    Require(_ZN3sce4Json5Value3setEl(&value, 777) == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeInteger);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&value) == 777);

    _ZN3sce4Json5ValueD1Ev(&value);
}

struct ReadState {
    const char* text;
    std::size_t position;
    int calls;
    int successStatus = 0;
    int failureStatus = -1;
};

static int APS5_VABI ReadNext(char& ch, void* context) {
    auto* state = static_cast<ReadState*>(context);
    ++state->calls;
    if (state->text[state->position] == '\0') {
        ch = '\0';
        return state->failureStatus;
    }
    ch = state->text[state->position++];
    return state->successStatus;
}

static void IntegerRepresentation() {
    Value value{};
    _ZN3sce4Json5ValueC1Ev(&value);
    for (const char* text : {"0", "1", "9223372036854775807", "9223372036854775808", "18446744073709551615"}) {
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, text, std::strlen(text)) == 0);
        Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeUInteger);
        Require(Serialize(value) == text);
    }
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&value) == -1);
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeUInteger);
    Require(_ZN3sce4Json5Value3setEl(&value, -1) == 0);
    Require(*_ZNK3sce4Json5Value11getUIntegerEv(&value) == std::numeric_limits<std::uint64_t>::max());
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeInteger);
    for (const char* text : {"-0", "-1", "-9223372036854775808"}) {
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, text, std::strlen(text)) == 0);
        Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeInteger);
    }
    Require(*_ZNK3sce4Json5Value11getUIntegerEv(&value) == (std::uint64_t{1} << 63));
    Require(_ZNK3sce4Json5Value7getTypeEv(&value) == TypeInteger);
    _ZN3sce4Json5ValueD1Ev(&value);
}

static void StreamParse() {
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    ReadState valid{"{\"list\": [1, 2.5, true]}", 0, 0};
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&root, ReadNext, &valid) == 0);
    Require(valid.calls == static_cast<int>(std::strlen(valid.text)));
    Require(_ZNK3sce4Json5Value7getTypeEv(&root) == TypeObject);
    ReadState invalid{"{\"list\": [1, 2.5", 0, 0};
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&root, ReadNext, &invalid) < 0);
    ReadState trailing{"[1] 2", 0, 0};
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&root, ReadNext, &trailing) == 0);
    Require(trailing.position == 3);
    Require(Serialize(root) == "[1]");
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&root, ReadNext, &trailing) == 0);
    Require(Serialize(root) == "2");
    ReadState positive{"true false", 0, 0, 7};
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&root, ReadNext, &positive) == 0);
    Require(positive.position == 4 && positive.calls == 4);
    Require(Serialize(root) == "true");
    ReadState failed{"tr", 0, 0, 0, -123456};
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&root, ReadNext, &failed) == -123456);
    Require(failed.calls == 3);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, "{} trailing", 11) == 0);
    Require(Serialize(root) == "{}");
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, "01", 2) == 0);
    Require(Serialize(root) == "1");
    _ZN3sce4Json5ValueD1Ev(&root);
}

static void ValueNullAccess() {
    callbackCalls = 0;
    lastRequested = -1;
    lastParent = nullptr;
    lastContext = nullptr;
    int context = 0;
    _ZN3sce4Json5ValueC1Ev(&fallback);
    Require(_ZN3sce4Json5Value3setEl(&fallback, 99) == 0);
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    const char text[] = "{\"present\":\"text\",\"none\":null}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text, sizeof(text) - 1) == 0);
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&root, OnNullAccess, &context) == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&Member(root, "missing")) == TypeNull && callbackCalls == 0);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "none")) == 99);
    Require(callbackCalls == 1 && lastRequested == TypeInteger && lastParent == &root && lastContext == &context);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "present")) == 0 && callbackCalls == 1);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "missing")) == 99 && callbackCalls == 2);
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&root, nullptr, nullptr) == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&Member(root, "missing")) == TypeNull);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "none")) == 0 && callbackCalls == 2);
    _ZN3sce4Json5ValueD1Ev(&root);
    _ZN3sce4Json5ValueD1Ev(&fallback);
}

static Value replacement{};
static int mismatchCalls = 0;
static std::int32_t mismatchRequested = -1;
static const Value* mismatchValue = nullptr;
static const Value* mismatchSecond = nullptr;
static void* mismatchContext = nullptr;

static const Value& APS5_VABI OnTypeMismatch(std::int32_t requested, const Value* value, const Value* second, void* context) {
    ++mismatchCalls;
    mismatchRequested = requested;
    mismatchValue = value;
    mismatchSecond = second;
    mismatchContext = context;
    return replacement;
}

static void RequireMismatch(int calls, std::int32_t requested, const Value* value, void* context) {
    Require(mismatchCalls == calls && mismatchRequested == requested && mismatchValue == value);
    Require(mismatchSecond == reinterpret_cast<const Value*>(value->storage[0]) && mismatchContext == context);
}

static void ValueTypeMismatchHandler() {
    _ZN3sce4Json5ValueC1Ev(&replacement);
    Require(_ZN3sce4Json5Value3setEl(&replacement, 42) == 0);
    int context = 0;
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&root, OnTypeMismatch, &context) == 0);
    const char text[] = "{\"name\":\"text\",\"none\":null,\"list\":[true]}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text, sizeof(text) - 1) == 0);

    const Value& name = Member(root, "name");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&name) == 42);
    RequireMismatch(1, TypeInteger, &name, &context);
    Require(!*_ZNK3sce4Json5Value10getBooleanEv(&name));
    RequireMismatch(2, TypeBoolean, &name, &context);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&name)), "text") == 0);
    Require(mismatchCalls == 2);
    Require(_ZNK3sce4Json5Array4sizeEv(_ZNK3sce4Json5Value8getArrayEv(&root)) == 0);
    RequireMismatch(3, TypeArray, &root, &context);
    const Value* element = _ZNK3sce4Json5ValueixEm(&Member(root, "list"), 0);
    Require(*_ZNK3sce4Json5Value7getRealEv(element) == 0.0);
    RequireMismatch(4, TypeReal, element, &context);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "none")) == 42);
    Require(mismatchCalls == 5);

    Value later{};
    _ZN3sce4Json5ValueC1Ev(&later);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&later, text, sizeof(text) - 1) == 0);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(later, "name")) == 0);
    Require(mismatchCalls == 5);
    int laterContext = 0;
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&later, OnTypeMismatch, &laterContext) == 0);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(later, "name")) == 42);
    RequireMismatch(6, TypeInteger, &Member(later, "name"), &laterContext);

    Value other{};
    _ZN3sce4Json5ValueC1Ev(&other);
    Require(_ZN3sce4Json5Value3setEPKc(&other, "other") == 0);
    Require(_ZN3sce4Json5ValueaSERKS1_(&root, &other) == &root);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&root) == 42);
    RequireMismatch(7, TypeInteger, &root, &context);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&other) == 0);
    Require(mismatchCalls == 7);
    _ZN3sce4Json5Value3setEd(&other, 1.5);
    Require(_ZN3sce4Json5Value3setERKS1_(&root, &other) == 0);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&root) == 42);
    RequireMismatch(8, TypeInteger, &root, &context);
    _ZN3sce4Json5Value5clearEv(&root);
    Require(_ZN3sce4Json5Value3setEPKc(&root, "cleared") == 0);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&root) == 42);
    RequireMismatch(9, TypeInteger, &root, &context);
    Value copy{};
    _ZN3sce4Json5ValueC1ERKS1_(&copy, &root);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&copy) == 0);
    Require(mismatchCalls == 9);

    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&root, nullptr, nullptr) == 0);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&root) == 0);
    Require(mismatchCalls == 9);
    _ZN3sce4Json5ValueD1Ev(&copy);
    _ZN3sce4Json5ValueD1Ev(&other);
    _ZN3sce4Json5ValueD1Ev(&later);
    _ZN3sce4Json5ValueD1Ev(&root);
    _ZN3sce4Json5ValueD1Ev(&replacement);
}

static int failureCalls = 0;

static void APS5_VABI OnElementAccessFailure(std::int32_t, const String*, const std::size_t*, const Value*, const Value*, void*) {
    ++failureCalls;
}

static void ValueElementAccessFailureHandler() {
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    const char text[] = "{\"list\":[1],\"key\":2}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text, sizeof(text) - 1) == 0);
    int context = 0;
    Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, OnElementAccessFailure, &context) == 0);

    const Value& list = Member(root, "list");
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "key")) == 2);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Value8getValueEm(&list, 0)) == 1);
    String missing{};
    _ZN3sce4Json6StringC1EPKc(&missing, "missing");
    Require(_ZNK3sce4Json5Value7getTypeEv(_ZNK3sce4Json5ValueixEPKc(&root, "missing")) == TypeNull);
    Require(_ZNK3sce4Json5Value7getTypeEv(_ZNK3sce4Json5Value8getValueERKNS0_6StringE(&root, &missing)) == TypeNull);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5ValueixEm(&root, 0)) == 2);
    Require(_ZNK3sce4Json5Value7getTypeEv(_ZNK3sce4Json5Value8getValueEm(&list, 1)) == TypeNull);
    Require(failureCalls == 3);

    Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, nullptr, nullptr) == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv(&Member(root, "missing")) == TypeNull);
    Require(_ZNK3sce4Json5Value7getTypeEv(_ZNK3sce4Json5Value8getValueEm(&list, 1)) == TypeNull);
    _ZN3sce4Json6StringD1Ev(&missing);
    _ZN3sce4Json5ValueD1Ev(&root);
}

static int specialFloatCalls = 0;

static Value* APS5_VABI OnSpecialFloat(Value* result, std::int32_t, double, const Value*, void*, bool*) {
    ++specialFloatCalls;
    _ZN3sce4Json5ValueC1Ev(result);
    return result;
}

static void ValueSpecialFloatHandler() {
    Value real{};
    _ZN3sce4Json5ValueC1Ed(&real, std::numeric_limits<double>::quiet_NaN());
    int context = 0;
    Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&real, OnSpecialFloat, &context) == 0);
    String out{};
    _ZN3sce4Json6StringC1Ev(&out);
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&real, &out) == 0);
    _ZN3sce4Json5Value3setEd(&real, std::numeric_limits<double>::infinity());
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&real, &out) == 0);
    _ZN3sce4Json6StringD1Ev(&out);
    _ZN3sce4Json5Value3setEd(&real, 2.5);
    Require(Serialize(real) == "2.5");
    Require(specialFloatCalls == 2);
    _ZN3sce4Json5ValueD1Ev(&real);
}

static void ValueOperatorBool() {
    Value nullVal{};
    _ZN3sce4Json5ValueC1Ev(&nullVal);
    Require(!_ZNK3sce4Json5ValuecvbEv(&nullVal));
    _ZN3sce4Json5ValueD1Ev(&nullVal);

    Value typedNull{};
    _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&typedNull, TypeNull);
    Require(!_ZNK3sce4Json5ValuecvbEv(&typedNull));
    _ZN3sce4Json5ValueD1Ev(&typedNull);

    Value boolFalse{};
    _ZN3sce4Json5ValueC1Eb(&boolFalse, false);
    Require(!_ZNK3sce4Json5ValuecvbEv(&boolFalse));
    _ZN3sce4Json5ValueD1Ev(&boolFalse);

    Value boolTrue{};
    _ZN3sce4Json5ValueC1Eb(&boolTrue, true);
    Require(_ZNK3sce4Json5ValuecvbEv(&boolTrue));
    _ZN3sce4Json5ValueD1Ev(&boolTrue);

    Value scalar{};
    _ZN3sce4Json5ValueC1Ev(&scalar);
    for (std::int64_t value : {std::int64_t{0}, std::int64_t{1}, std::int64_t{-1}}) {
        Require(_ZN3sce4Json5Value3setEl(&scalar, value) == 0);
        Require(_ZNK3sce4Json5ValuecvbEv(&scalar) == (value != 0));
    }
    for (std::uint64_t value : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{0xffffffffffffffff}}) {
        Require(_ZN3sce4Json5Value3setEm(&scalar, value) == 0);
        Require(_ZNK3sce4Json5ValuecvbEv(&scalar) == (value != 0));
    }
    for (double value : {0.0, -0.0, 1.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        _ZN3sce4Json5Value3setEd(&scalar, value);
        Require(_ZNK3sce4Json5ValuecvbEv(&scalar) == (value != 0.0));
    }
    for (const char* text : {"", "0", "false", " "}) {
        Require(_ZN3sce4Json5Value3setEPKc(&scalar, text) == 0);
        Require(_ZNK3sce4Json5ValuecvbEv(&scalar) == (*text != 0));
    }
    _ZN3sce4Json5ValueD1Ev(&scalar);
    for (std::int32_t type : {TypeArray, TypeObject}) {
        Value container{};
        _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&container, type);
        Require(_ZNK3sce4Json5ValuecvbEv(&container));
        _ZN3sce4Json5ValueD1Ev(&container);
    }

    Value cleared{};
    _ZN3sce4Json5ValueC1Eb(&cleared, true);
    Require(_ZNK3sce4Json5ValuecvbEv(&cleared));
    _ZN3sce4Json5Value5clearEv(&cleared);
    Require(!_ZNK3sce4Json5ValuecvbEv(&cleared));
    _ZN3sce4Json5ValueD1Ev(&cleared);
}

struct ActiveModule {
    JsonTest::Allocator allocator;
    alignas(8) std::uint8_t parameter[40]{};
    std::uint8_t initializer = 0;
    ActiveModule() {
        _ZN3sce4Json14InitParameter2C1Ev(parameter);
        _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(parameter, &allocator, nullptr);
        _ZN3sce4Json11InitializerC1Ev(&initializer);
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, parameter) == 0);
    }
    ~ActiveModule() { Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0); }
};

static int NumericLocale() {
    ActiveModule module;
    const std::string previous = std::setlocale(LC_NUMERIC, nullptr);
    const char* selected = nullptr;
    for (const char* name : {"xx_COMMA", "de_DE.UTF-8", "de_DE.utf8", "de-DE", "German_Germany.1252", "fr_FR.UTF-8"}) {
        if (std::setlocale(LC_NUMERIC, name) && std::strcmp(std::localeconv()->decimal_point, ",") == 0) {
            selected = name;
            break;
        }
    }
    if (!selected) {
        std::setlocale(LC_NUMERIC, previous.c_str());
        std::fprintf(stderr, "No decimal-comma numeric locale is installed\n");
        return 77;
    }
    std::printf("JSON numeric locale: %s\n", selected);
    int failures = 0;
    const auto check = [&](bool success, const char* operation, const char* actual) {
        if (!success) {
            std::fprintf(stderr, "%s failed: %s\n", operation, actual);
            ++failures;
        }
    };
    for (const auto number : {1.5, -0.125, 1.25e30, std::numeric_limits<double>::max(),
                              std::numeric_limits<double>::min(), std::numeric_limits<double>::denorm_min()}) {
        Value value{};
        _ZN3sce4Json5ValueC1Ed(&value, number);
        const auto text = Serialize(value);
        check(text.find(',') == std::string::npos, "Decimal-point serialization", text.c_str());
        Value parsed{};
        _ZN3sce4Json5ValueC1Ev(&parsed);
        const int result = _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&parsed, text.c_str(), text.size());
        check(result == 0 && *_ZNK3sce4Json5Value7getRealEv(&parsed) == number, "Numeric round trip", text.c_str());
        _ZN3sce4Json5ValueD1Ev(&parsed);
        _ZN3sce4Json5ValueD1Ev(&value);
    }
    for (const auto [text, number] : {std::pair{"1.5", 1.5}, {"-1.25e2", -125.0},
                                    {"4.9406564584124654e-324", std::numeric_limits<double>::denorm_min()},
                                    {"2.4703282292062327e-324", 0.0},
                                    {"2.4703282292062328e-324", std::numeric_limits<double>::denorm_min()},
                                    {"18446744073709551616", 18446744073709551616.0},
                                    {"1e-999", 0.0}, {"-1e-999", -0.0}}) {
        Value value{};
        _ZN3sce4Json5ValueC1Ev(&value);
        const int result = _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, text, std::strlen(text));
        const auto parsed = *_ZNK3sce4Json5Value7getRealEv(&value);
        check(result == 0 && parsed == number && std::signbit(parsed) == std::signbit(number), "Decimal-point parsing", text);
        _ZN3sce4Json5ValueD1Ev(&value);
    }
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    constexpr char document[] = "{\"list\":[2.5,-0.125],\"value\":1.5}";
    check(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, document, sizeof(document) - 1) == 0,
          "Nested numeric parsing", document);
    const auto serialized = Serialize(root);
    check(serialized == document, "Nested numeric serialization", serialized.c_str());
    check(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, "1,5", 3) == 0 && _ZNK3sce4Json5Value7getTypeEv(&root) == TypeUInteger &&
          *_ZNK3sce4Json5Value11getUIntegerEv(&root) == 1, "Stop at a decimal comma", "1,5");
    bool overflow = false;
    try {
        _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, "1e309", 5);
    } catch (const std::runtime_error&) {
        overflow = true;
    }
    check(overflow, "Retain overflow rejection", "1e309");
    _ZN3sce4Json5ValueD1Ev(&root);
    check(std::strcmp(std::localeconv()->decimal_point, ",") == 0, "Preserve host locale", selected);
    std::setlocale(LC_NUMERIC, previous.c_str());
    return failures ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--numeric-locale") == 0) return NumericLocale();
    {
        ActiveModule module;
        ParseAndRoundTrip();
        NestingDepth();
        ObjectsAndArrays();
        ObjectFind();
        ObjectPair();
        ObjectInsert();
        ObjectKeyOrder();
        ArrayIteratorCopies();
        ObjectIteratorCopies();
        AllocatorNotifyError();
        StringMethods();
        StringComparison();
        ParseDuplicateKeys();
        ValueSwap();
        ArrayIteratorAdvance();
        ObjectOperations();
    }
    NullAccess(_ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_);
    NullAccess(_ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_);
    JsonTest::Allocator recoveryAllocator;
    alignas(8) std::uint8_t recoveryParameter[40]{};
    _ZN3sce4Json14InitParameter2C1Ev(recoveryParameter);
    _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(recoveryParameter, &recoveryAllocator, nullptr);
    std::uint8_t recoveryInitializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&recoveryInitializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&recoveryInitializer, recoveryParameter) == 0);
    ValueNullAccess();
    ValueTypeMismatchHandler();
    ValueElementAccessFailureHandler();
    ValueSpecialFloatHandler();
    Require(_ZN3sce4Json11Initializer9terminateEv(&recoveryInitializer) == 0);
    InitParameter2BaseConstructor();
    InitParameters();
    ValueAccessParameters();
    {
        ActiveModule module;
        ValueAccess();
        ValueClear();
        ValueOperatorBool();
        IntegerRepresentation();
        StreamParse();
    }
}
