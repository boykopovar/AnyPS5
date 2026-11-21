#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

using Jsc::WtfAbi::JsonArrayBase;
using Jsc::WtfAbi::JsonType;
using Jsc::WtfAbi::JsonValue;
using Jsc::WtfAbi::String;
using Jsc::WtfAbi::StringImpl;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF JSON check failed at line %d\n", line);
        std::abort();
    }
}

template<typename Exception, typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

std::vector<const JsonValue*> deleted;

void APS5_VABI JsonCompleteDestructor(JsonValue*) {}

void APS5_VABI JsonDeletingDestructorStub(JsonValue* value) {
    deleted.push_back(value);
}

void* const ValueVtable[] = {reinterpret_cast<void*>(&JsonCompleteDestructor), reinterpret_cast<void*>(&JsonDeletingDestructorStub)};

JsonValue MakeValue(JsonType type) {
    JsonValue value{};
    value.vtable = ValueVtable;
    value.refCount = 1;
    value.type = type;
    return value;
}

String Make8(const char* text) {
    return _ZN3WTF10StringImpl6createEPKh(reinterpret_cast<const unsigned char*>(text));
}

void TestAsBoolean() {
    JsonValue value = MakeValue(JsonType::Boolean);
    value.value.boolean = true;
    bool output = false;
    Check(_ZNK3WTF8JSONImpl5Value9asBooleanERb(&value, &output) && output, __LINE__);
    value.value.boolean = false;
    Check(_ZNK3WTF8JSONImpl5Value9asBooleanERb(&value, &output) && !output, __LINE__);

    JsonValue integer = MakeValue(JsonType::Integer);
    integer.value.number = 1;
    output = true;
    Check(!_ZNK3WTF8JSONImpl5Value9asBooleanERb(&integer, &output) && output, __LINE__);
    JsonValue null = MakeValue(JsonType::Null);
    Check(!_ZNK3WTF8JSONImpl5Value9asBooleanERb(&null, &output) && output, __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZNK3WTF8JSONImpl5Value9asBooleanERb(nullptr, nullptr); }), __LINE__);
}

void TestAsString() {
    String text = Make8("json");
    StringImpl* impl = text.impl;
    const unsigned before = impl->refCount;
    JsonValue value = MakeValue(JsonType::String);
    value.value.string = impl;

    String output = Make8("old");
    Check(_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&value, &output), __LINE__);
    Check(output.impl == impl && impl->refCount == before + 2, __LINE__);
    Check(_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&value, &output), __LINE__);
    Check(output.impl == impl && impl->refCount == before + 2, __LINE__);

    JsonValue nullString = MakeValue(JsonType::String);
    nullString.value.string = nullptr;
    Check(_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&nullString, &output), __LINE__);
    Check(!output.impl && impl->refCount == before, __LINE__);

    JsonValue boolean = MakeValue(JsonType::Boolean);
    String untouched = Make8("keep");
    StringImpl* kept = untouched.impl;
    Check(!_ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(&boolean, &untouched) && untouched.impl == kept, __LINE__);
}

void TestAsValue() {
    deleted.clear();
    JsonValue first = MakeValue(JsonType::Null);
    JsonValue second = MakeValue(JsonType::Double);
    JsonValue* output = nullptr;
    Check(_ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(&first, &output), __LINE__);
    Check(output == &first && first.refCount == 2, __LINE__);
    Check(_ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(&first, &output), __LINE__);
    Check(output == &first && first.refCount == 2 && deleted.empty(), __LINE__);

    first.refCount = 1;
    Check(_ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(&second, &output), __LINE__);
    Check(output == &second && second.refCount == 2, __LINE__);
    Check(deleted.size() == 1 && deleted[0] == &first && first.refCount == 1, __LINE__);

    JsonValue third = MakeValue(JsonType::Integer);
    third.refCount = 3;
    output = &third;
    Check(_ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(&second, &output), __LINE__);
    Check(third.refCount == 2 && second.refCount == 3 && deleted.size() == 1, __LINE__);
}

void TestArrayBase() {
    deleted.clear();
    JsonArrayBase array{};
    array.base = MakeValue(JsonType::Array);
    Check(_ZNK3WTF8JSONImpl9ArrayBase6lengthEv(&array) == 0, __LINE__);
    Check(_ZNK3WTF8JSONImpl9ArrayBase3endEv(&array) == nullptr, __LINE__);
    _ZN3WTF8JSONImpl9ArrayBaseD2Ev(&array);
    Check(!array.buffer && deleted.empty(), __LINE__);

    JsonValue shared = MakeValue(JsonType::Boolean);
    shared.refCount = 2;
    JsonValue owned = MakeValue(JsonType::Integer);
    JsonArrayBase filled{};
    filled.base = MakeValue(JsonType::Array);
    filled.buffer = static_cast<JsonValue**>(Jsc::WtfAbi::AllocateBlock(4 * sizeof(JsonValue*)));
    filled.capacity = 4;
    filled.size = 3;
    filled.buffer[0] = &shared;
    filled.buffer[1] = nullptr;
    filled.buffer[2] = &owned;
    Check(_ZNK3WTF8JSONImpl9ArrayBase6lengthEv(&filled) == 3, __LINE__);
    Check(_ZNK3WTF8JSONImpl9ArrayBase3endEv(&filled) == filled.buffer + 3, __LINE__);
    _ZN3WTF8JSONImpl9ArrayBaseD2Ev(&filled);
    Check(shared.refCount == 1 && deleted.size() == 1 && deleted[0] == &owned, __LINE__);
    Check(!filled.buffer && !filled.capacity, __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZNK3WTF8JSONImpl9ArrayBase6lengthEv(nullptr); }), __LINE__);
}

}  // namespace

int main() {
    TestAsBoolean();
    TestAsString();
    TestAsValue();
    TestArrayBase();
    std::puts("WTF JSON tests passed");
    return 0;
}
