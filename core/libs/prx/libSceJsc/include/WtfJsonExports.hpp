#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace Jsc::WtfAbi {

enum class JsonType : int {
    Null,
    Boolean,
    Double,
    Integer,
    String,
    Object,
    Array,
};

struct JsonValue {
    void* const* vtable;
    unsigned refCount;
    JsonType type;
    union {
        bool boolean;
        double number;
        StringImpl* string;
    } value;
};

struct JsonArrayBase {
    JsonValue base;
    JsonValue** buffer;
    unsigned capacity;
    unsigned size;
};

using JsonDeletingDestructor = void (APS5_VABI*)(JsonValue*);

inline constexpr std::size_t JsonDeletingDestructorSlot = 1;

static_assert(std::is_standard_layout_v<JsonValue>);
static_assert(std::is_standard_layout_v<JsonArrayBase>);
static_assert(sizeof(JsonType) == 4);
static_assert(sizeof(JsonValue) == 24);
static_assert(alignof(JsonValue) == 8);
static_assert(offsetof(JsonValue, vtable) == 0);
static_assert(offsetof(JsonValue, refCount) == 8);
static_assert(offsetof(JsonValue, type) == 12);
static_assert(offsetof(JsonValue, value) == 16);
static_assert(sizeof(JsonArrayBase) == 40);
static_assert(offsetof(JsonArrayBase, base) == 0);
static_assert(offsetof(JsonArrayBase, buffer) == 24);
static_assert(offsetof(JsonArrayBase, capacity) == 32);
static_assert(offsetof(JsonArrayBase, size) == 36);

}  // namespace Jsc::WtfAbi

extern "C" {

bool APS5_VABI _ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(Jsc::WtfAbi::JsonValue* self, Jsc::WtfAbi::JsonValue** output);
bool APS5_VABI _ZNK3WTF8JSONImpl5Value9asBooleanERb(const Jsc::WtfAbi::JsonValue* self, bool* output);
bool APS5_VABI _ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(const Jsc::WtfAbi::JsonValue* self, Jsc::WtfAbi::String* output);
unsigned APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase6lengthEv(const Jsc::WtfAbi::JsonArrayBase* self);
Jsc::WtfAbi::JsonValue* const* APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase3endEv(const Jsc::WtfAbi::JsonArrayBase* self);
void APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseD2Ev(Jsc::WtfAbi::JsonArrayBase* self);
}
