#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
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
using SetNullAccessCallback = int (APS5_VABI*)(void*, NullAccessCallback, void*);

extern "C" {
int APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11InitializerD1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void*, const void*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void*, NullAccessCallback, void*);
int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void*, NullAccessCallback, void*);
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
void APS5_VABI _ZN3sce4Json5Value5clearEv(Value*);
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
bool APS5_VABI _ZNK3sce4Json6Object5emptyEv(const Object*);
std::size_t APS5_VABI _ZNK3sce4Json6Object4sizeEv(const Object*);
const Object* APS5_VABI _ZNK3sce4Json5Value9getObjectEv(const Value*);
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(Value*, std::int32_t);
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueEm(const Value*, std::size_t);
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKNS0_6StringE(const Value*, const String*);
Value* APS5_VABI _ZN3sce4Json5Value10referValueEm(Value*, std::size_t);
void APS5_VABI _ZN3sce4Json14InitParameter2C1Ev(void*);
void APS5_VABI _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(void*, void*, void*);
void APS5_VABI _ZN3sce4Json14InitParameter217setFileBufferSizeEm(void*, std::size_t);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const void*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

enum : std::int32_t { TypeNull, TypeBoolean, TypeInteger, TypeUInteger, TypeReal, TypeString, TypeArray, TypeObject };

const std::string documentText =
    "{ \"name\" : \"x\\u00e9\\n\", \"count\": 3, \"big\": 18446744073709551615, \"neg\": -5,"
    " \"pi\": 1.5, \"ok\": true, \"none\": null, \"list\": [1, \"two\", [3]] }";
const std::string serializedDocument =
    "{\"name\":\"x\xc3\xa9\\n\",\"count\":3,\"big\":18446744073709551615,\"neg\":-5,"
    "\"pi\":1.5,\"ok\":true,\"none\":null,\"list\":[1,\"two\",[3]]}";
const std::string listText = "{\"list\":[10,20,30],\"name\":\"value\",\"flag\":true}";

class ValueGuard {
public:
    explicit ValueGuard(Value* value) : value(value) {}
    ~ValueGuard() { _ZN3sce4Json5ValueD1Ev(value); }
    ValueGuard(const ValueGuard&) = delete;
    ValueGuard& operator=(const ValueGuard&) = delete;

private:
    Value* value;
};

class StringGuard {
public:
    explicit StringGuard(String* text) : text(text) {}
    ~StringGuard() { _ZN3sce4Json6StringD1Ev(text); }
    StringGuard(const StringGuard&) = delete;
    StringGuard& operator=(const StringGuard&) = delete;

private:
    String* text;
};

class ObjectGuard {
public:
    explicit ObjectGuard(Object* object) : object(object) {}
    ~ObjectGuard() { _ZN3sce4Json6ObjectD1Ev(object); }
    ObjectGuard(const ObjectGuard&) = delete;
    ObjectGuard& operator=(const ObjectGuard&) = delete;

private:
    Object* object;
};

class ArrayGuard {
public:
    explicit ArrayGuard(Array* array) : array(array) {}
    ~ArrayGuard() { _ZN3sce4Json5ArrayD1Ev(array); }
    ArrayGuard(const ArrayGuard&) = delete;
    ArrayGuard& operator=(const ArrayGuard&) = delete;

private:
    Array* array;
};

class ArrayRange {
public:
    explicit ArrayRange(const Array* array) {
        _ZNK3sce4Json5Array5beginEv(&current, array);
        _ZNK3sce4Json5Array3endEv(&end, array);
    }
    ~ArrayRange() {
        _ZN3sce4Json5Array8iteratorD1Ev(&current);
        _ZN3sce4Json5Array8iteratorD1Ev(&end);
    }
    ArrayRange(const ArrayRange&) = delete;
    ArrayRange& operator=(const ArrayRange&) = delete;

    bool HasCurrent() const { return _ZNK3sce4Json5Array8iteratorneERKS2_(&current, &end); }
    void Advance() { _ZN3sce4Json5Array8iteratorppEv(&current); }
    Value* Current() const { return _ZNK3sce4Json5Array8iteratordeEv(&current); }

private:
    Iterator current{};
    Iterator end{};
};

class ObjectRange {
public:
    explicit ObjectRange(const Object* object) {
        _ZNK3sce4Json6Object5beginEv(&current, object);
        _ZNK3sce4Json6Object3endEv(&end, object);
    }
    ~ObjectRange() {
        _ZN3sce4Json6Object8iteratorD1Ev(&current);
        _ZN3sce4Json6Object8iteratorD1Ev(&end);
    }
    ObjectRange(const ObjectRange&) = delete;
    ObjectRange& operator=(const ObjectRange&) = delete;

    bool HasCurrent() const { return _ZNK3sce4Json6Object8iteratorneERKS2_(&current, &end); }
    void Advance() { _ZN3sce4Json6Object8iteratorppEv(&current); }
    Pair* Current() const { return _ZNK3sce4Json6Object8iteratordeEv(&current); }

private:
    Iterator current{};
    Iterator end{};
};

class ParsedValue {
public:
    explicit ParsedValue(const std::string& text) {
        _ZN3sce4Json5ValueC1Ev(&root);
        result = _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text.c_str(), text.size());
    }
    ~ParsedValue() { _ZN3sce4Json5ValueD1Ev(&root); }
    ParsedValue(const ParsedValue&) = delete;
    ParsedValue& operator=(const ParsedValue&) = delete;

    Value& Root() {
        RequireEqual(result, 0, "parse the fixture document");
        return root;
    }

    int Result() const { return result; }

private:
    Value root{};
    int result = -1;
};

int Parse(Value& value, const std::string& text) {
    return _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, text.c_str(), text.size());
}

std::string Serialize(Value& value) {
    String out{};
    _ZN3sce4Json6StringC1Ev(&out);
    const StringGuard outGuard(&out);
    RequireEqual(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &out), 0, "serialize result");
    return std::string(_ZNK3sce4Json6String5c_strEv(&out), _ZNK3sce4Json6String6lengthEv(&out));
}

const Value& Member(const Value& value, const char* key) {
    return *_ZNK3sce4Json5ValueixEPKc(&value, key);
}

std::int32_t TypeOf(const Value* value) {
    return _ZNK3sce4Json5Value7getTypeEv(value);
}

void RequireUnchangedDocument(const Value& root, const std::string& input) {
    RequireEqual(TypeOf(&root), TypeObject, input + " kept the previous root type");
    RequireEqual(_ZNK3sce4Json5Value5countEv(&root), std::size_t{8}, input + " kept the previous member count");
}

std::string Nested(std::size_t depth, bool objects) {
    std::string text;
    for (std::size_t i = 0; i < depth; ++i) text += objects ? "{\"a\":" : "[";
    text += "1";
    for (std::size_t i = 0; i < depth; ++i) text += objects ? "}" : "]";
    return text;
}

struct NullAccessRecorder {
    Value fallback{};
    int calls = 0;
    std::int32_t lastRequested = -1;
    const Value* lastParent = nullptr;
    void* lastContext = nullptr;
};

const Value& APS5_VABI OnNullAccess(std::int32_t requested, const Value* parent, void* context) {
    auto* recorder = static_cast<NullAccessRecorder*>(context);
    ++recorder->calls;
    recorder->lastRequested = requested;
    recorder->lastParent = parent;
    recorder->lastContext = context;
    return recorder->fallback;
}

struct CallbackSetter {
    const char* name;
    SetNullAccessCallback set;
};

const CallbackSetter callbackSetters[] = {
    {"setGlobalNullAccessCallBack", _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_},
    {"setGlobalNullAccessCallback", _ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_},
};

class NullAccessFixture {
public:
    explicit NullAccessFixture(const CallbackSetter& setter) : name(setter.name) {
        _ZN3sce4Json5ValueC1Ev(&recorder.fallback);
        constructResult = _ZN3sce4Json11InitializerC1Ev(initializer);
        initializeResult = _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(initializer, parameter);
        setResult = setter.set(initializer, OnNullAccess, &recorder);
        fallbackResult = _ZN3sce4Json5Value3setEl(&recorder.fallback, 99);
    }

    ~NullAccessFixture() {
        if (!terminated) _ZN3sce4Json11Initializer9terminateEv(initializer);
        _ZN3sce4Json5ValueD1Ev(&recorder.fallback);
        if (!destroyed) _ZN3sce4Json11InitializerD1Ev(initializer);
    }

    NullAccessFixture(const NullAccessFixture&) = delete;
    NullAccessFixture& operator=(const NullAccessFixture&) = delete;

    void RequireReady() const {
        RequireEqual(constructResult, 0, name + " Initializer constructor");
        RequireEqual(initializeResult, 0, name + " Initializer::initialize");
        RequireEqual(setResult, 0, name + " result");
        RequireEqual(fallbackResult, 0, name + " fallback set");
    }

    int Terminate() {
        terminated = true;
        return _ZN3sce4Json11Initializer9terminateEv(initializer);
    }

    int Destroy() {
        destroyed = true;
        return _ZN3sce4Json11InitializerD1Ev(initializer);
    }

    const std::string name;
    NullAccessRecorder recorder;

private:
    alignas(16) std::uint8_t initializer[16]{};
    alignas(16) std::uint8_t parameter[64]{};
    int constructResult = -1;
    int initializeResult = -1;
    int setResult = -1;
    int fallbackResult = -1;
    bool terminated = false;
    bool destroyed = false;
};

const std::string presentText = "{\"present\":\"text\"}";

const Case parseDocument{"Json_Parse_Document_ReturnsObjectWithEightMembers", [] {
    ParsedValue document(documentText);
    RequireEqual(document.Result(), 0, "parse result");
    RequireEqual(TypeOf(&document.Root()), TypeObject, "root type");
    RequireEqual(_ZNK3sce4Json5Value5countEv(&document.Root()), std::size_t{8}, "member count");
}};

const Case parseScalars{"Json_Parse_Document_DecodesScalarMembers", [] {
    ParsedValue document(documentText);
    const Value& root = document.Root();
    RequireEqual(std::string(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&Member(root, "name")))),
        std::string("x\xc3\xa9\n"), "name decodes the escapes");
    RequireEqual(TypeOf(&Member(root, "count")), TypeInteger, "count type");
    RequireEqual(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "count")), std::int64_t{3}, "count as integer");
    RequireEqual(*_ZNK3sce4Json5Value11getUIntegerEv(&Member(root, "count")), std::uint64_t{3}, "count as unsigned integer");
    RequireEqual(*_ZNK3sce4Json5Value7getRealEv(&Member(root, "count")), 3.0, "count as real");
    RequireEqual(TypeOf(&Member(root, "big")), TypeUInteger, "big type");
    RequireEqual(*_ZNK3sce4Json5Value11getUIntegerEv(&Member(root, "big")), std::numeric_limits<std::uint64_t>::max(), "big value");
    RequireEqual(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "neg")), std::int64_t{-5}, "neg value");
    RequireEqual(*_ZNK3sce4Json5Value7getRealEv(&Member(root, "pi")), 1.5, "pi value");
    RequireEqual(*_ZNK3sce4Json5Value10getBooleanEv(&Member(root, "ok")), true, "ok value");
    RequireEqual(TypeOf(&Member(root, "none")), TypeNull, "none type");
}};

const Case referValueByKey{"Json_ReferValue_ExistingAndMissingKeys_ReturnsMemberOrNullWithoutInserting", [] {
    ParsedValue document(documentText);
    Value& root = document.Root();
    String present{};
    String absent{};
    _ZN3sce4Json6StringC1EPKc(&present, "ok");
    const StringGuard presentGuard(&present);
    _ZN3sce4Json6StringC1EPKc(&absent, "applyToAll");
    const StringGuard absentGuard(&absent);
    Require(_ZN3sce4Json5Value10referValueERKNS0_6StringE(&root, &present) == &Member(root, "ok"), "existing key refers to the member");
    Require(_ZN3sce4Json5Value10referValueERKNS0_6StringE(&root, &absent) == nullptr, "missing key returns null");
    RequireEqual(_ZNK3sce4Json5Value5countEv(&root), std::size_t{8}, "member count after lookups");
}};

const Case parseNestedArray{"Json_Parse_NestedArray_IteratesElementTypes", [] {
    ParsedValue document(documentText);
    const Value& list = Member(document.Root(), "list");
    const Array* array = _ZNK3sce4Json5Value8getArrayEv(&list);
    RequireEqual(_ZNK3sce4Json5Array4sizeEv(array), std::size_t{3}, "list size");
    std::vector<std::int32_t> types;
    for (ArrayRange range(array); range.HasCurrent(); range.Advance()) types.push_back(TypeOf(range.Current()));
    Require((types == std::vector<std::int32_t>{TypeInteger, TypeString, TypeArray}), "list element types are integer, string, array");
    RequireEqual(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5ValueixEm(_ZNK3sce4Json5ValueixEm(&list, 2), 0)), std::int64_t{3},
        "list[2][0]");
}};

const Case serializeDocument{"Json_Serialize_ParsedDocument_ProducesCompactText", [] {
    ParsedValue document(documentText);
    RequireEqual(Serialize(document.Root()), serializedDocument, "serialized text");
}};

const Case reparseSerialized{"Json_Parse_SerializedDocument_RoundTrips", [] {
    ParsedValue reparsed(serializedDocument);
    RequireEqual(reparsed.Result(), 0, "parse result");
    RequireEqual(Serialize(reparsed.Root()), serializedDocument, "serialized again");
}};

const Case parseInvalid{"Json_Parse_InvalidText_FailsAndKeepsPreviousValue", [] {
    ParsedValue document(documentText);
    Value& root = document.Root();
    for (const char* invalid : {"{\"a\":}", "[1,]", "\"open", "tru", "{} x", "01"}) {
        const std::string input = std::string("input ") + invalid;
        Require(Parse(root, invalid) < 0, input + " fails");
        RequireUnchangedDocument(root, input);
    }
}};

const Case parseOverflow{"Json_Parse_OverflowingNumber_ThrowsUnverifiedAndKeepsPreviousValue", [] {
    ParsedValue document(documentText);
    Value& root = document.Root();
    for (const char* overflow : {"1e309", "[-1e309]", "{\"a\":[1,1e400]}"}) {
        const std::string input = std::string("input ") + overflow;
        const auto error = Testing::RequireThrows<std::runtime_error>([&] { Parse(root, overflow); }, input + " throws");
        Require(std::string(error.what()).find("unverified") != std::string::npos, input + " reports unverified behaviour: " + error.what());
        RequireUnchangedDocument(root, input);
    }
}};

const Case nestingAccepted{"Json_Parse_Nesting512_Succeeds", [] {
    for (const bool objects : {false, true}) {
        ParsedValue parsed(Nested(512, objects));
        RequireEqual(parsed.Result(), 0, objects ? "512 nested objects" : "512 nested arrays");
    }
}};

const Case nestingRejected{"Json_Parse_NestingBeyond512_Fails", [] {
    for (const bool objects : {false, true}) {
        for (const std::size_t depth : {std::size_t{513}, std::size_t{100000}}) {
            ParsedValue parsed(Nested(depth, objects));
            Require(parsed.Result() < 0, std::to_string(depth) + (objects ? " nested objects fail" : " nested arrays fail"));
        }
    }
}};

const Case objectInsertsNull{"Json_ObjectSubscript_NewKey_InsertsNullMember", [] {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    const ObjectGuard objectGuard(&object);
    String alpha{};
    _ZN3sce4Json6StringC1EPKc(&alpha, "alpha");
    const StringGuard alphaGuard(&alpha);
    Require(_ZNK3sce4Json6Object5emptyEv(&object), "new object is empty");
    Value* first = _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &alpha);
    Require(!_ZNK3sce4Json6Object5emptyEv(&object), "object is not empty after subscript");
    RequireEqual(TypeOf(first), TypeNull, "inserted member type");
}};

Value* PopulateAlphaBeta(Object& object, const String& alpha, const String& beta) {
    Value* first = _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &alpha);
    RequireEqual(_ZN3sce4Json5Value3setEl(first, 1), 0, "set alpha to 1");
    RequireEqual(_ZN3sce4Json5Value3setEPKc(_ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &beta), "b"), 0, "set beta to \"b\"");
    return first;
}

const Case objectExistingKey{"Json_ObjectSubscript_ExistingKey_ReturnsSameMember", [] {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    const ObjectGuard objectGuard(&object);
    String alpha{};
    String beta{};
    _ZN3sce4Json6StringC1EPKc(&alpha, "alpha");
    const StringGuard alphaGuard(&alpha);
    _ZN3sce4Json6StringC1EPKc(&beta, "beta");
    const StringGuard betaGuard(&beta);
    Value* first = PopulateAlphaBeta(object, alpha, beta);
    Require(_ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &alpha) == first, "second subscript returns the same member");
}};

const Case objectIteration{"Json_ObjectIterator_TwoMembers_VisitsInInsertionOrder", [] {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    const ObjectGuard objectGuard(&object);
    String alpha{};
    String beta{};
    _ZN3sce4Json6StringC1EPKc(&alpha, "alpha");
    const StringGuard alphaGuard(&alpha);
    _ZN3sce4Json6StringC1EPKc(&beta, "beta");
    const StringGuard betaGuard(&beta);
    Value* first = PopulateAlphaBeta(object, alpha, beta);
    std::vector<std::string> keys;
    for (ObjectRange range(&object); range.HasCurrent(); range.Advance()) {
        const Pair* pair = range.Current();
        keys.emplace_back(_ZNK3sce4Json6String5c_strEv(&pair->key));
        if (keys.size() == 1) Require(&pair->value == first, "first pair holds the alpha member");
    }
    Require((keys == std::vector<std::string>{"alpha", "beta"}), "keys are alpha, beta");
}};

const Case objectSerialize{"Json_Serialize_ValueFromObject_ProducesMembersInOrder", [] {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    const ObjectGuard objectGuard(&object);
    String alpha{};
    String beta{};
    _ZN3sce4Json6StringC1EPKc(&alpha, "alpha");
    const StringGuard alphaGuard(&alpha);
    _ZN3sce4Json6StringC1EPKc(&beta, "beta");
    const StringGuard betaGuard(&beta);
    PopulateAlphaBeta(object, alpha, beta);
    Value wrapped{};
    _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&wrapped, &object);
    const ValueGuard wrappedGuard(&wrapped);
    RequireEqual(Serialize(wrapped), std::string("{\"alpha\":1,\"beta\":\"b\"}"), "serialized object");
}};

const Case arrayPushBack{"Json_ArrayPushBack_TwoValues_StoresCopiesInOrder", [] {
    Array array{};
    _ZN3sce4Json5ArrayC1Ev(&array);
    const ArrayGuard arrayGuard(&array);
    {
        Value flag{};
        Value real{};
        _ZN3sce4Json5ValueC1Eb(&flag, true);
        const ValueGuard flagGuard(&flag);
        _ZN3sce4Json5ValueC1Ed(&real, 2.5);
        const ValueGuard realGuard(&real);
        _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &flag);
        _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &real);
    }
    RequireEqual(_ZNK3sce4Json5Array4sizeEv(&array), std::size_t{2}, "array size");
    RequireEqual(*_ZNK3sce4Json5Value7getRealEv(_ZNK3sce4Json5Array4backEv(&array)), 2.5, "back value");
    int visited = 0;
    for (ArrayRange range(&array); range.HasCurrent(); range.Advance()) ++visited;
    RequireEqual(visited, 2, "iterated elements");
}};

const Case nullAccessMissingMember{"Json_NullAccessCallback_MissingMember_ReturnsCallbackValue", [] {
    for (const auto& setter : callbackSetters) {
        NullAccessFixture fixture(setter);
        fixture.RequireReady();
        ParsedValue document(presentText);
        const Value& root = document.Root();
        Require(&Member(root, "missing") == &fixture.recorder.fallback, fixture.name + " missing member returns the callback value");
        RequireEqual(fixture.recorder.calls, 1, fixture.name + " callback calls");
        RequireEqual(fixture.recorder.lastRequested, TypeNull, fixture.name + " requested type");
        Require(fixture.recorder.lastParent == &root, fixture.name + " parent is the root");
        Require(fixture.recorder.lastContext == &fixture.recorder, fixture.name + " context is passed through");
    }
}};

const Case nullAccessInteger{"Json_NullAccessCallback_IntegerOfString_ReturnsCallbackInteger", [] {
    for (const auto& setter : callbackSetters) {
        NullAccessFixture fixture(setter);
        fixture.RequireReady();
        ParsedValue document(presentText);
        const Value& root = document.Root();
        RequireEqual(*_ZNK3sce4Json5Value10getIntegerEv(&Member(root, "present")), std::int64_t{99}, fixture.name + " integer value");
        RequireEqual(fixture.recorder.calls, 1, fixture.name + " callback calls");
        RequireEqual(fixture.recorder.lastRequested, TypeInteger, fixture.name + " requested type");
        Require(fixture.recorder.lastParent == &Member(root, "present"), fixture.name + " parent is the string member");
    }
}};

const Case nullAccessBoolean{"Json_NullAccessCallback_BooleanOfStringWithIntegerFallback_ReturnsFalse", [] {
    for (const auto& setter : callbackSetters) {
        NullAccessFixture fixture(setter);
        fixture.RequireReady();
        ParsedValue document(presentText);
        RequireEqual(*_ZNK3sce4Json5Value10getBooleanEv(&Member(document.Root(), "present")), false, fixture.name + " boolean value");
        RequireEqual(fixture.recorder.calls, 1, fixture.name + " callback calls");
        RequireEqual(fixture.recorder.lastRequested, TypeBoolean, fixture.name + " requested type");
    }
}};

const Case nullAccessIndexOnObject{"Json_NullAccessCallback_IndexOnObject_ReturnsCallbackValue", [] {
    for (const auto& setter : callbackSetters) {
        NullAccessFixture fixture(setter);
        fixture.RequireReady();
        ParsedValue document(presentText);
        RequireEqual(TypeOf(_ZNK3sce4Json5ValueixEm(&document.Root(), 0)), TypeInteger, fixture.name + " element type");
        RequireEqual(fixture.recorder.calls, 1, fixture.name + " callback calls");
    }
}};

const Case nullAccessAfterTerminate{"Json_NullAccessCallback_AfterTerminate_ReturnsDefaultNullWithoutCallback", [] {
    for (const auto& setter : callbackSetters) {
        NullAccessFixture fixture(setter);
        fixture.RequireReady();
        ParsedValue document(presentText);
        const Value& root = document.Root();
        Require(&Member(root, "missing") == &fixture.recorder.fallback, fixture.name + " callback active before terminate");
        RequireEqual(fixture.Terminate(), 0, fixture.name + " terminate");
        const Value& unhandled = Member(root, "missing");
        Require(&unhandled != &fixture.recorder.fallback, fixture.name + " missing member no longer returns the callback value");
        RequireEqual(TypeOf(&unhandled), TypeNull, fixture.name + " default value type");
        RequireEqual(fixture.recorder.calls, 1, fixture.name + " callback calls");
        RequireEqual(fixture.Destroy(), 0, fixture.name + " Initializer destructor");
    }
}};

const Case typedConstructor{"Json_ValueTypeConstructor_EachType_CreatesEmptyValueOfType", [] {
    for (std::int32_t type = TypeNull; type <= TypeObject; ++type) {
        Value typed{};
        _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&typed, type);
        const ValueGuard typedGuard(&typed);
        RequireEqual(TypeOf(&typed), type, "type " + std::to_string(type));
        RequireEqual(_ZNK3sce4Json5Value5countEv(&typed), std::size_t{0}, "count of type " + std::to_string(type));
    }
}};

const Case objectSize{"Json_GetObject_ThreeMemberDocument_SizeIsThree", [] {
    ParsedValue document(listText);
    RequireEqual(_ZNK3sce4Json6Object4sizeEv(_ZNK3sce4Json5Value9getObjectEv(&document.Root())), std::size_t{3}, "object size");
}};

const Case getValueByKey{"Json_GetValueByKey_ExistingKey_ReturnsMember", [] {
    ParsedValue document(listText);
    String key{};
    _ZN3sce4Json6StringC1EPKc(&key, "list");
    const StringGuard keyGuard(&key);
    RequireEqual(TypeOf(_ZNK3sce4Json5Value8getValueERKNS0_6StringE(&document.Root(), &key)), TypeArray, "list type");
}};

const Case getValueByIndex{"Json_GetValueByIndex_InRange_ReturnsElement", [] {
    ParsedValue document(listText);
    const Value* list = &Member(document.Root(), "list");
    RequireEqual(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Value8getValueEm(list, 1)), std::int64_t{20}, "list[1]");
    Require(_ZNK3sce4Json5Value8getValueEm(list, 2) == _ZNK3sce4Json5ValueixEm(list, 2), "getValue(2) matches operator[](2)");
}};

const Case getValueByIndexOutOfRange{"Json_GetValueByIndex_OutOfRange_ReturnsNull", [] {
    ParsedValue document(listText);
    const Value* list = &Member(document.Root(), "list");
    RequireEqual(TypeOf(_ZNK3sce4Json5Value8getValueEm(list, 3)), TypeNull, "list[3] type");
}};

const Case referValueByIndex{"Json_ReferValueByIndex_Element_AllowsMutation", [] {
    ParsedValue document(listText);
    Value& root = document.Root();
    String key{};
    _ZN3sce4Json6StringC1EPKc(&key, "list");
    const StringGuard keyGuard(&key);
    const Value* list = _ZNK3sce4Json5Value8getValueERKNS0_6StringE(&root, &key);
    Value* mutableList = _ZN3sce4Json5Value10referValueERKNS0_6StringE(&root, &key);
    Value* element = _ZN3sce4Json5Value10referValueEm(mutableList, 0);
    Require(element != nullptr, "list[0] is referable");
    RequireEqual(_ZN3sce4Json5Value3setEl(element, 15), 0, "set list[0]");
    RequireEqual(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Value8getValueEm(list, 0)), std::int64_t{15}, "list[0] after set");
}};

const Case referValueByIndexInvalid{"Json_ReferValueByIndex_OutOfRangeOrNonArray_ReturnsNull", [] {
    ParsedValue document(listText);
    Value& root = document.Root();
    String key{};
    _ZN3sce4Json6StringC1EPKc(&key, "list");
    const StringGuard keyGuard(&key);
    Value* mutableList = _ZN3sce4Json5Value10referValueERKNS0_6StringE(&root, &key);
    Require(_ZN3sce4Json5Value10referValueEm(mutableList, 3) == nullptr, "list[3] is not referable");
    Require(_ZN3sce4Json5Value10referValueEm(&root, 0) == nullptr, "object index 0 is not referable");
}};

const Case getValueMissingKey{"Json_GetValueByKey_MissingKey_ReturnsNull", [] {
    ParsedValue document(listText);
    String missing{};
    _ZN3sce4Json6StringC1EPKc(&missing, "missing");
    const StringGuard missingGuard(&missing);
    RequireEqual(TypeOf(_ZNK3sce4Json5Value8getValueERKNS0_6StringE(&document.Root(), &missing)), TypeNull, "missing member type");
}};

const Case initParameter2Setters{"Json_InitParameter2_Setters_StoreAllocatorUserDataAndBufferSize", [] {
    alignas(16) std::uint8_t parameter[40];
    std::memset(parameter, 0xff, sizeof(parameter));
    _ZN3sce4Json14InitParameter2C1Ev(parameter);
    int allocator = 0;
    int userData = 0;
    _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(parameter, &allocator, &userData);
    _ZN3sce4Json14InitParameter217setFileBufferSizeEm(parameter, 4096);
    void* stored[3]{};
    std::memcpy(stored, parameter, sizeof(stored));
    Require(stored[0] == &allocator, "allocator is stored first");
    Require(stored[1] == &userData, "user data is stored second");
    RequireEqual(reinterpret_cast<std::uintptr_t>(stored[2]), std::uintptr_t{4096}, "file buffer size");
}};

const Case initializeWithParameter2{"Json_Initializer_InitParameter2_InitializesAndTerminates", [] {
    alignas(16) std::uint8_t parameter[40];
    std::memset(parameter, 0xff, sizeof(parameter));
    _ZN3sce4Json14InitParameter2C1Ev(parameter);
    int allocator = 0;
    int userData = 0;
    _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(parameter, &allocator, &userData);
    _ZN3sce4Json14InitParameter217setFileBufferSizeEm(parameter, 4096);
    alignas(16) std::uint8_t initializer[16]{};
    RequireEqual(_ZN3sce4Json11InitializerC1Ev(initializer), 0, "Initializer constructor");
    RequireEqual(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(initializer, parameter), 0, "initialize");
    RequireEqual(_ZN3sce4Json11Initializer9terminateEv(initializer), 0, "terminate");
    RequireEqual(_ZN3sce4Json11InitializerD1Ev(initializer), 0, "Initializer destructor");
}};

const Case clearNull{"Json_ValueClear_Null_StaysNull", [] {
    Value value{};
    _ZN3sce4Json5ValueC1Ev(&value);
    const ValueGuard valueGuard(&value);
    RequireEqual(TypeOf(&value), TypeNull, "type before clear");
    _ZN3sce4Json5Value5clearEv(&value);
    RequireEqual(TypeOf(&value), TypeNull, "type after clear");
}};

const Case clearInteger{"Json_ValueClear_Integer_BecomesNull", [] {
    Value value{};
    _ZN3sce4Json5ValueC1Ev(&value);
    const ValueGuard valueGuard(&value);
    RequireEqual(_ZN3sce4Json5Value3setEl(&value, 12345), 0, "set integer");
    RequireEqual(TypeOf(&value), TypeInteger, "type before clear");
    _ZN3sce4Json5Value5clearEv(&value);
    RequireEqual(TypeOf(&value), TypeNull, "type after clear");
}};

const Case clearString{"Json_ValueClear_String_BecomesNull", [] {
    Value value{};
    _ZN3sce4Json5ValueC1Ev(&value);
    const ValueGuard valueGuard(&value);
    RequireEqual(_ZN3sce4Json5Value3setEPKc(&value, "test string"), 0, "set string");
    RequireEqual(TypeOf(&value), TypeString, "type before clear");
    _ZN3sce4Json5Value5clearEv(&value);
    RequireEqual(TypeOf(&value), TypeNull, "type after clear");
}};

void ConstructObjectValue(Value& value) {
    Object object{};
    _ZN3sce4Json6ObjectC1Ev(&object);
    const ObjectGuard objectGuard(&object);
    String key{};
    _ZN3sce4Json6StringC1EPKc(&key, "prop");
    const StringGuard keyGuard(&key);
    _ZN3sce4Json5Value3setEl(_ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &key), 999);
    _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&value, &object);
}

const Case clearObject{"Json_ValueClear_Object_BecomesNull", [] {
    Value value{};
    ConstructObjectValue(value);
    const ValueGuard valueGuard(&value);
    RequireEqual(TypeOf(&value), TypeObject, "type before clear");
    _ZN3sce4Json5Value5clearEv(&value);
    RequireEqual(TypeOf(&value), TypeNull, "type after clear");
}};

const Case setAfterClear{"Json_ValueSet_AfterClearingObject_StoresInteger", [] {
    Value value{};
    ConstructObjectValue(value);
    const ValueGuard valueGuard(&value);
    _ZN3sce4Json5Value5clearEv(&value);
    RequireEqual(_ZN3sce4Json5Value3setEl(&value, 777), 0, "set integer");
    RequireEqual(TypeOf(&value), TypeInteger, "type");
    RequireEqual(*_ZNK3sce4Json5Value10getIntegerEv(&value), std::int64_t{777}, "value");
}};

} // namespace
