#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <initializer_list>

struct Value { alignas(8) std::uint64_t storage[4]{}; };
struct String { void* text; };
struct Array { void* items; };
struct Object { void* items; };
struct Iterator { void* state; };
struct Pair { String key; std::uint64_t reserved; Value value; };

extern "C" {
Iterator* APS5_VABI _ZNK3sce4Json5Array5beginEv(Iterator*, const Array*);
Iterator* APS5_VABI _ZNK3sce4Json6Object5beginEv(Iterator*, const Object*);
void APS5_VABI _ZN3sce4Json5Array8iteratorC1ERKS2_(Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json6Object8iteratorC1ERKS2_(Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json5Array8iteratorD1Ev(Iterator*);
void APS5_VABI _ZN3sce4Json6Object8iteratorD1Ev(Iterator*);
void APS5_VABI _ZN3sce4Json5Value4swapERS1_(Value*, Value*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value*, const char*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6StringE(Value*, const String*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(Value*, const Array*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(Value*, const Object*);
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(Value*, std::int32_t);
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(Value*, const Value*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1_(Value*, const Value*);
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_5ArrayE(Value*, const Array*);
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_6ObjectE(Value*, const Object*);
int APS5_VABI _ZN3sce4Json5Value3setENS0_9ValueTypeE(Value*, std::int32_t);
std::int32_t APS5_VABI _ZNK3sce4Json5Value7getTypeEv(const Value*);
const Array* APS5_VABI _ZNK3sce4Json5Value8getArrayEv(const Value*);
const Object* APS5_VABI _ZNK3sce4Json5Value9getObjectEv(const Value*);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value*, String*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
void APS5_VABI _ZN3sce4Json6StringC1Ev(String*);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array*);
void APS5_VABI _ZN3sce4Json5ArrayC1ERKS1_(Array*, const Array*);
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array*);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array*, const Value*);
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array*);
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array*);
void APS5_VABI _ZN3sce4Json6ObjectC1Ev(Object*);
void APS5_VABI _ZN3sce4Json6ObjectC1ERKS1_(Object*, const Object*);
void APS5_VABI _ZN3sce4Json6ObjectD1Ev(Object*);
Object* APS5_VABI _ZN3sce4Json6ObjectaSERKS1_(Object*, const Object*);
std::size_t APS5_VABI _ZNK3sce4Json6Object4sizeEv(const Object*);
void APS5_VABI _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(Pair*, const String*, const Value*);
void APS5_VABI _ZN3sce4Json6Object4PairD1Ev(Pair*);
}

static std::ptrdiff_t allocationBudget = -1;
static std::size_t liveAllocations = 0;
static const char* activeCase = nullptr;
static unsigned activeBudget = 0;

#ifndef _WIN32
extern "C" void NotImplemented_nid_no_patch(const char* name) {
    std::fprintf(stderr, "unexpected unimplemented JSON export: %s\n", name);
    std::abort();
}
#endif

void* operator new(std::size_t bytes) {
    if (allocationBudget == 0) throw std::bad_alloc();
    if (allocationBudget > 0) --allocationBudget;
    if (void* pointer = std::malloc(bytes ? bytes : 1)) {
        ++liveAllocations;
        return pointer;
    }
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept {
    if (pointer) --liveAllocations;
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s budget=%u: %s (live=%zu)\n", activeCase, activeBudget, message, liveAllocations);
        std::abort();
    }
}

static void CheckValue(const Value& value, const char* expected) {
    String output{};
    _ZN3sce4Json6StringC1Ev(&output);
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(const_cast<Value*>(&value), &output) == 0, "serialization failed");
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&output), expected) == 0, "value changed");
    _ZN3sce4Json6StringD1Ev(&output);
}

enum class Operation {
    ValueCopy, ArrayCopy, ObjectCopy, ValueAssign, ObjectAssign, ArrayAppend,
    ValueArray, ValueObject, ValueString, ValueText, ValueTypeString, ValueTypeArray,
    ValueTypeObject, SetArray, SetObject, SetTypeArray, SetTypeObject, SetTypeString, PairCopy
};

static bool Run(Operation operation, const Value& source, const char* expected, const Array& array, const Object& object, const String& text) {
    Value result{};
    Array arrayResult{};
    Object objectResult{};
    Pair pair{};
    bool valueReady = false, arrayReady = false, objectReady = false, pairReady = false;
    const bool typeSetter = operation == Operation::SetTypeArray || operation == Operation::SetTypeObject || operation == Operation::SetTypeString;
    if (operation == Operation::ValueAssign || operation == Operation::SetArray || operation == Operation::SetObject || typeSetter) {
        _ZN3sce4Json5ValueC1EPKc(&result, "previous");
        valueReady = true;
    }
    if (operation == Operation::ArrayAppend) { _ZN3sce4Json5ArrayC1Ev(&arrayResult); arrayReady = true; }
    if (operation == Operation::ObjectAssign) { _ZN3sce4Json6ObjectC1Ev(&objectResult); objectReady = true; }
    bool succeeded = false;
    allocationBudget = activeBudget;
    try {
        switch (operation) {
            case Operation::ValueCopy: _ZN3sce4Json5ValueC1ERKS1_(&result, &source); valueReady = true; break;
            case Operation::ArrayCopy: _ZN3sce4Json5ArrayC1ERKS1_(&arrayResult, &array); arrayReady = true; break;
            case Operation::ObjectCopy: _ZN3sce4Json6ObjectC1ERKS1_(&objectResult, &object); objectReady = true; break;
            case Operation::ValueAssign: _ZN3sce4Json5ValueaSERKS1_(&result, &source); break;
            case Operation::ObjectAssign: _ZN3sce4Json6ObjectaSERKS1_(&objectResult, &object); break;
            case Operation::ArrayAppend: _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&arrayResult, &source); break;
            case Operation::ValueArray: _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&result, &array); valueReady = true; break;
            case Operation::ValueObject: _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&result, &object); valueReady = true; break;
            case Operation::ValueString: _ZN3sce4Json5ValueC1ERKNS0_6StringE(&result, &text); valueReady = true; break;
            case Operation::ValueText: _ZN3sce4Json5ValueC1EPKc(&result, _ZNK3sce4Json6String5c_strEv(&text)); valueReady = true; break;
            case Operation::ValueTypeString: _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&result, 5); valueReady = true; break;
            case Operation::ValueTypeArray: _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&result, 6); valueReady = true; break;
            case Operation::ValueTypeObject: _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&result, 7); valueReady = true; break;
            case Operation::SetArray: _ZN3sce4Json5Value3setERKNS0_5ArrayE(&result, &array); break;
            case Operation::SetObject: _ZN3sce4Json5Value3setERKNS0_6ObjectE(&result, &object); break;
            case Operation::SetTypeArray: _ZN3sce4Json5Value3setENS0_9ValueTypeE(&result, 6); break;
            case Operation::SetTypeObject: _ZN3sce4Json5Value3setENS0_9ValueTypeE(&result, 7); break;
            case Operation::SetTypeString: _ZN3sce4Json5Value3setENS0_9ValueTypeE(&result, 5); break;
            case Operation::PairCopy: _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pair, &text, &source); pairReady = true; break;
        }
        succeeded = true;
    } catch (const std::bad_alloc&) {
    }
    allocationBudget = -1;
    if (valueReady) {
        if (succeeded) CheckValue(result, expected);
        else {
            Require(_ZNK3sce4Json5Value7getTypeEv(&result) == 5, "failed setter changed the destination type");
            CheckValue(result, "\"previous\"");
        }
        _ZN3sce4Json5ValueD1Ev(&result);
    }
    if (arrayReady) {
        const auto count = _ZNK3sce4Json5Array4sizeEv(&arrayResult);
        Require(count == (succeeded ? (operation == Operation::ArrayAppend ? 1 : 3) : 0), "array contents changed");
        if (succeeded && operation == Operation::ArrayAppend) CheckValue(*_ZNK3sce4Json5Array5frontEv(&arrayResult), expected);
        _ZN3sce4Json5ArrayD1Ev(&arrayResult);
    }
    if (objectReady) {
        Require(_ZNK3sce4Json6Object4sizeEv(&objectResult) == (succeeded ? 2 : 0), "object contents changed");
        _ZN3sce4Json6ObjectD1Ev(&objectResult);
    }
    if (pairReady) { CheckValue(pair.value, expected); _ZN3sce4Json6Object4PairD1Ev(&pair); }
    return succeeded;
}

int main() {
    constexpr const char* objectJson = "{\"long\":\"a sufficiently long string to require its own allocation\",\"nested\":[{\"leaf\":\"another sufficiently long string\"},[true,null,17]]}";
    constexpr const char* arrayJson = "[1,{\"long\":\"a sufficiently long string to require its own allocation\"},[false,null]]";
    constexpr const char* stringJson = "\"a sufficiently long string to require its own allocation\"";
    const auto initial = liveAllocations;
    Value objectValue{}, arrayValue{};
    String text{};
    _ZN3sce4Json5ValueC1Ev(&objectValue);
    _ZN3sce4Json5ValueC1Ev(&arrayValue);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&objectValue, objectJson, std::strlen(objectJson)) == 0, "prepare object");
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&arrayValue, arrayJson, std::strlen(arrayJson)) == 0, "prepare array");
    _ZN3sce4Json6StringC1EPKc(&text, "a sufficiently long string to require its own allocation");
    const auto* array = _ZNK3sce4Json5Value8getArrayEv(&arrayValue);
    const auto* object = _ZNK3sce4Json5Value9getObjectEv(&objectValue);
    struct Case { const char* name; Operation operation; const Value* source; const char* expected; };
    const Case cases[] = {
        {"value object copy", Operation::ValueCopy, &objectValue, objectJson},
        {"value array copy", Operation::ValueCopy, &arrayValue, arrayJson},
        {"array copy", Operation::ArrayCopy, &arrayValue, arrayJson},
        {"object copy", Operation::ObjectCopy, &objectValue, objectJson},
        {"value assignment", Operation::ValueAssign, &objectValue, objectJson},
        {"object assignment", Operation::ObjectAssign, &objectValue, objectJson},
        {"array append", Operation::ArrayAppend, &objectValue, objectJson},
        {"value from array", Operation::ValueArray, &arrayValue, arrayJson},
        {"value from object", Operation::ValueObject, &objectValue, objectJson},
        {"value from string", Operation::ValueString, &objectValue, stringJson},
        {"value from text", Operation::ValueText, &objectValue, stringJson},
        {"empty string value", Operation::ValueTypeString, &objectValue, "\"\""},
        {"empty array value", Operation::ValueTypeArray, &objectValue, "[]"},
        {"empty object value", Operation::ValueTypeObject, &objectValue, "{}"},
        {"set array", Operation::SetArray, &arrayValue, arrayJson},
        {"set object", Operation::SetObject, &objectValue, objectJson},
        {"set empty array", Operation::SetTypeArray, &objectValue, "[]"},
        {"set empty object", Operation::SetTypeObject, &objectValue, "{}"},
        {"retain string type", Operation::SetTypeString, &objectValue, "\"previous\""},
        {"pair copy", Operation::PairCopy, &objectValue, objectJson},
    };
    unsigned failures = 0;
    for (const auto& test : cases) {
        activeCase = test.name;
        bool succeeded = false;
        for (activeBudget = 0; activeBudget < 256 && !succeeded; ++activeBudget) {
            const auto before = liveAllocations;
            succeeded = Run(test.operation, *test.source, test.expected, *array, *object, text);
            if (liveAllocations != before) std::fprintf(stderr, "expected %zu live allocations, got %zu\n", before, liveAllocations);
            Require(liveAllocations == before, "copy leaked allocations");
            failures += !succeeded;
            CheckValue(objectValue, objectJson);
            CheckValue(arrayValue, arrayJson);
        }
        Require(succeeded, "copy never succeeded");
    }
    for (bool objectIterator : {false, true}) {
        activeCase = objectIterator ? "object iterator copy" : "array iterator copy";
        const auto before = liveAllocations;
        Iterator source{};
        if (objectIterator) _ZNK3sce4Json6Object5beginEv(&source, object);
        else _ZNK3sce4Json5Array5beginEv(&source, array);
        for (activeBudget = 0; activeBudget <= 1; ++activeBudget) {
            Iterator copy{};
            bool succeeded = false;
            allocationBudget = activeBudget;
            try {
                if (objectIterator) _ZN3sce4Json6Object8iteratorC1ERKS2_(&copy, &source);
                else _ZN3sce4Json5Array8iteratorC1ERKS2_(&copy, &source);
                succeeded = true;
            } catch (const std::bad_alloc&) {
                ++failures;
            }
            allocationBudget = -1;
            Require(succeeded == (activeBudget == 1), "iterator allocation failure ignored");
            if (succeeded) Require(copy.state != source.state, "iterator copy shares owned storage");
            if (objectIterator) _ZN3sce4Json6Object8iteratorD1Ev(&copy);
            else _ZN3sce4Json5Array8iteratorD1Ev(&copy);
        }
        if (objectIterator) _ZN3sce4Json6Object8iteratorD1Ev(&source);
        else _ZN3sce4Json5Array8iteratorD1Ev(&source);
        Require(liveAllocations == before, "iterator copy leaked allocations");
    }
    activeCase = "value swap";
    bool swapSucceeded = false;
    for (activeBudget = 0; activeBudget < 256 && !swapSucceeded; ++activeBudget) {
        const auto before = liveAllocations;
        Value first{}, second{};
        _ZN3sce4Json5ValueC1ERKS1_(&first, &objectValue);
        _ZN3sce4Json5ValueC1ERKS1_(&second, &arrayValue);
        allocationBudget = activeBudget;
        try {
            _ZN3sce4Json5Value4swapERS1_(&first, &second);
            swapSucceeded = true;
        } catch (const std::bad_alloc&) {
            ++failures;
        }
        allocationBudget = -1;
        if (swapSucceeded) {
            CheckValue(first, arrayJson);
            CheckValue(second, objectJson);
        }
        _ZN3sce4Json5ValueD1Ev(&first);
        _ZN3sce4Json5ValueD1Ev(&second);
        Require(liveAllocations == before, "swap leaked allocations");
        CheckValue(objectValue, objectJson);
        CheckValue(arrayValue, arrayJson);
    }
    Require(swapSucceeded, "swap never succeeded");
    _ZN3sce4Json6StringD1Ev(&text);
    _ZN3sce4Json5ValueD1Ev(&arrayValue);
    _ZN3sce4Json5ValueD1Ev(&objectValue);
    Require(liveAllocations == initial, "source cleanup leaked allocations");
    std::printf("JSON copy cleanup: %zu operations, %u allocation failures, no leaks\n", sizeof(cases) / sizeof(cases[0]) + 3, failures);
}
