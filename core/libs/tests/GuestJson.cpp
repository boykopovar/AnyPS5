#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

struct Value { void* node; };
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

extern "C" {
int APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11InitializerD1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void*, const void*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void*, NullAccessCallback, void*);
void APS5_VABI _ZN3sce4Json6StringC1Ev(String*);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
std::size_t APS5_VABI _ZNK3sce4Json6String6lengthEv(const String*);
void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array*);
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array*);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array*, const Value*);
Iterator* APS5_VABI _ZNK3sce4Json5Array5beginEv(Iterator*, const Array*);
Iterator* APS5_VABI _ZNK3sce4Json5Array3endEv(Iterator*, const Array*);
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array*);
const Value* APS5_VABI _ZNK3sce4Json5Array4backEv(const Array*);
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
bool APS5_VABI _ZNK3sce4Json6Object8iteratorneERKS2_(const Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1Eb(Value*, bool);
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value*, double);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(Value*, const Object*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
int APS5_VABI _ZN3sce4Json5Value3setEl(Value*, std::int64_t);
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
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Json check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

enum : std::int32_t { TypeNull, TypeBoolean, TypeInteger, TypeUInteger, TypeReal, TypeString, TypeArray, TypeObject };

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
    Require(_ZNK3sce4Json5Value7getTypeEv(&Member(root, "count")) == TypeInteger);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "count")) == 3);
    Require(*_ZNK3sce4Json5Value11getUIntegerEv(&Member(root, "count")) == 3);
    Require(*_ZNK3sce4Json5Value7getRealEv(&Member(root, "count")) == 3.0);
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
    Require((types == std::vector<std::int32_t>{TypeInteger, TypeString, TypeArray}));
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5ValueixEm(_ZNK3sce4Json5ValueixEm(&list, 2), 0)) == 3);

    const std::string serialized = Serialize(root);
    Require(serialized == "{\"name\":\"x\xc3\xa9\\n\",\"count\":3,\"big\":18446744073709551615,\"neg\":-5,"
                          "\"pi\":1.5,\"ok\":true,\"none\":null,\"list\":[1,\"two\",[3]]}");
    Value reparsed{};
    _ZN3sce4Json5ValueC1Ev(&reparsed);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&reparsed, serialized.c_str(), serialized.size()) == 0);
    Require(Serialize(reparsed) == serialized);
    _ZN3sce4Json5ValueD1Ev(&reparsed);

    for (const char* invalid : {"{\"a\":}", "[1,]", "\"open", "tru", "{} x", "01"}) {
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, invalid, std::strlen(invalid)) < 0);
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
    Value* first = _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &alpha);
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
    _ZN3sce4Json5ArrayD1Ev(&array);
}

static void NullAccess() {
    alignas(16) std::uint8_t initializer[16]{};
    alignas(16) std::uint8_t parameter[64]{};
    Require(_ZN3sce4Json11InitializerC1Ev(initializer) == 0);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(initializer, parameter) == 0);
    int context = 0;
    Require(_ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(initializer, OnNullAccess, &context) == 0);
    _ZN3sce4Json5ValueC1Ev(&fallback);
    Require(_ZN3sce4Json5Value3setEl(&fallback, 99) == 0);

    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    const char text[] = "{\"present\":\"text\"}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text, sizeof(text) - 1) == 0);
    Require(&Member(root, "missing") == &fallback);
    Require(callbackCalls == 1 && lastRequested == TypeNull && lastParent == &root && lastContext == &context);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "present")) == 99);
    Require(callbackCalls == 2 && lastRequested == TypeInteger && lastParent == &Member(root, "present"));
    Require(!*_ZNK3sce4Json5Value10getBooleanEv(&Member(root, "present")));
    Require(callbackCalls == 3 && lastRequested == TypeBoolean);
    Require(_ZNK3sce4Json5Value7getTypeEv(_ZNK3sce4Json5ValueixEm(&root, 0)) == TypeInteger);

    Require(_ZN3sce4Json11Initializer9terminateEv(initializer) == 0);
    const Value& unhandled = Member(root, "missing");
    Require(&unhandled != &fallback && _ZNK3sce4Json5Value7getTypeEv(&unhandled) == TypeNull);
    Require(callbackCalls == 4);
    _ZN3sce4Json5ValueD1Ev(&root);
    _ZN3sce4Json5ValueD1Ev(&fallback);
    Require(_ZN3sce4Json11InitializerD1Ev(initializer) == 0);
}

int main() {
    ParseAndRoundTrip();
    NestingDepth();
    ObjectsAndArrays();
    NullAccess();
}
