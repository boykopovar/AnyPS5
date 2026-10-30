#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include "NpUniversalDataSystem.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <clocale>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

constexpr int InitialUser = 0x10000000;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void call(int result, const char* message) {
    check(result == 0, message);
}

template <typename TAction>
bool throwsInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

NpUniversalDataSystemEventPropertyObject* createObject() {
    NpUniversalDataSystemEventPropertyObject* object = nullptr;
    call(sceNpUniversalDataSystemCreateEventPropertyObject(&object), "property object was not created");
    check(object != nullptr, "property object is null");
    return object;
}

void testObjectSetters() {
    auto* object = createObject();
    const std::uint8_t bytes[] = {1, 2, 3};
    call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(object, "int32", -5), "SetInt32 failed");
    call(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(object, "uint32", 4000000000u), "SetUInt32 failed");
    call(sceNpUniversalDataSystemEventPropertyObjectSetInt64(object, "int64", -9000000000), "SetInt64 failed");
    call(sceNpUniversalDataSystemEventPropertyObjectSetUInt64(object, "uint64", 18000000000000000000ull), "SetUInt64 failed");
    call(sceNpUniversalDataSystemEventPropertyObjectSetFloat32(object, "float32", 0.5f), "SetFloat32 failed");
    call(sceNpUniversalDataSystemEventPropertyObjectSetFloat64(object, "float64", 0.25), "SetFloat64 failed");
    call(sceNpUniversalDataSystemEventPropertyObjectSetBool(object, "bool", true), "SetBool failed");
    call(sceNpUniversalDataSystemEventPropertyObjectSetString(object, "string", "a\"b\\c\n"), "SetString failed");
    call(sceNpUniversalDataSystemEventPropertyObjectSetBinary(object, "binary", bytes, sizeof(bytes)), "SetBinary failed");
    check(Uds::Describe(*object) == R"({"int32":-5,"uint32":4000000000,"int64":-9000000000,"uint64":18000000000000000000,"float32":0.5,"float64":0.25,"bool":true,"string":"a\"b\\c\u000a","binary":<3 bytes>})",
          "properties were not kept with their types");
    const auto* binary = std::get_if<Uds::Binary>(object->Find("binary"));
    check(binary != nullptr && *binary == Uds::Binary{1, 2, 3}, "binary bytes were not copied");
    call(sceNpUniversalDataSystemEventPropertyObjectSetString(object, "int32", "replaced"), "SetString over int32 failed");
    const auto* replaced = std::get_if<std::string>(object->Find("int32"));
    check(object->properties.size() == 9 && replaced != nullptr && *replaced == "replaced", "setting a key again did not replace its value");
    check(object->Find("missing") == nullptr, "a missing key was found");
    check(throwsInvalidArgument([] { sceNpUniversalDataSystemEventPropertyObjectSetInt32(nullptr, "key", 1); }), "a null object was accepted");
    check(throwsInvalidArgument([object] { sceNpUniversalDataSystemEventPropertyObjectSetInt32(object, nullptr, 1); }), "a null key was accepted");
    call(sceNpUniversalDataSystemDestroyEventPropertyObject(object), "property object was not destroyed");
}

void testNestedValues() {
    auto* source = createObject();
    auto* parent = createObject();
    call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(source, "x", 1), "SetInt32 on the source failed");
    NpUniversalDataSystemEventPropertyObject* child = nullptr;
    call(sceNpUniversalDataSystemEventPropertyObjectSetObject(parent, "child", source, &child), "SetObject failed");
    check(child != nullptr && child != source, "SetObject did not return its own copy");
    call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(source, "x", 2), "SetInt32 after SetObject failed");
    call(sceNpUniversalDataSystemEventPropertyObjectSetBool(child, "y", true), "SetBool on the child failed");
    call(sceNpUniversalDataSystemDestroyEventPropertyObject(source), "source object was not destroyed");
    NpUniversalDataSystemEventPropertyArray* list = nullptr;
    call(sceNpUniversalDataSystemEventPropertyObjectSetArray(parent, "list", nullptr, &list), "SetArray failed");
    check(list != nullptr, "SetArray did not return the stored array");
    const std::uint8_t bytes[] = {4, 5};
    call(sceNpUniversalDataSystemEventPropertyArraySetInt32(list, 1), "array SetInt32 failed");
    call(sceNpUniversalDataSystemEventPropertyArraySetUInt32(list, 2), "array SetUInt32 failed");
    call(sceNpUniversalDataSystemEventPropertyArraySetInt64(list, -3), "array SetInt64 failed");
    call(sceNpUniversalDataSystemEventPropertyArraySetUInt64(list, 4), "array SetUInt64 failed");
    call(sceNpUniversalDataSystemEventPropertyArraySetFloat32(list, 1.5f), "array SetFloat32 failed");
    call(sceNpUniversalDataSystemEventPropertyArraySetFloat64(list, 2.5), "array SetFloat64 failed");
    call(sceNpUniversalDataSystemEventPropertyArraySetBool(list, true), "array SetBool failed");
    call(sceNpUniversalDataSystemEventPropertyArraySetString(list, "s"), "array SetString failed");
    call(sceNpUniversalDataSystemEventPropertyArraySetBinary(list, bytes, sizeof(bytes)), "array SetBinary failed");
    NpUniversalDataSystemEventPropertyObject* element = nullptr;
    call(sceNpUniversalDataSystemEventPropertyArraySetObject(list, nullptr, &element), "array SetObject failed");
    call(sceNpUniversalDataSystemEventPropertyObjectSetUInt64(element, "z", 3), "SetUInt64 on the array element failed");
    NpUniversalDataSystemEventPropertyArray* inner = nullptr;
    call(sceNpUniversalDataSystemEventPropertyArraySetArray(list, nullptr, &inner), "array SetArray failed");
    call(sceNpUniversalDataSystemEventPropertyArraySetBool(inner, false), "SetBool on the inner array failed");
    const char* expected = R"({"child":{"x":1,"y":true},"list":[1,2,-3,4,1.5,2.5,true,"s",<2 bytes>,{"z":3},[false]]})";
    check(Uds::Describe(*parent) == expected, "nested properties were not kept");
    const auto copy = parent->Clone();
    call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(child, "x", 5), "SetInt32 on the child failed");
    call(sceNpUniversalDataSystemEventPropertyArraySetBool(inner, true), "SetBool on the inner array failed");
    check(Uds::Describe(copy) == expected, "a clone shares nested values with its source");
    call(sceNpUniversalDataSystemDestroyEventPropertyObject(child), "destroying a child failed");
    call(sceNpUniversalDataSystemDestroyEventPropertyArray(list), "destroying a child array failed");
    check(Uds::Describe(*parent) == R"({"child":{"x":5,"y":true},"list":[1,2,-3,4,1.5,2.5,true,"s",<2 bytes>,{"z":3},[false,true]]})",
          "destroying a child took it from its parent");
    NpUniversalDataSystemEventPropertyArray* array = nullptr;
    call(sceNpUniversalDataSystemCreateEventPropertyArray(&array), "property array was not created");
    call(sceNpUniversalDataSystemEventPropertyArraySetInt32(array, 9), "SetInt32 on the created array failed");
    NpUniversalDataSystemEventPropertyArray* stored = nullptr;
    call(sceNpUniversalDataSystemEventPropertyObjectSetArray(parent, "list", array, &stored), "SetArray over a key failed");
    call(sceNpUniversalDataSystemDestroyEventPropertyArray(array), "property array was not destroyed");
    check(stored != array && Uds::Describe(*parent) == R"({"child":{"x":5,"y":true},"list":[9]})", "SetArray did not store a copy under its key");
    check(throwsInvalidArgument([] { sceNpUniversalDataSystemEventPropertyArraySetInt32(nullptr, 1); }), "a null array was accepted");
    call(sceNpUniversalDataSystemDestroyEventPropertyObject(parent), "parent object was not destroyed");
}

void testEvents() {
    auto* properties = createObject();
    call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(properties, "_trophy_id", 12), "SetInt32 failed");
    NpUniversalDataSystemEvent* event = nullptr;
    NpUniversalDataSystemEventPropertyObject* eventProperties = nullptr;
    call(sceNpUniversalDataSystemCreateEvent("_UnlockTrophy", properties, &event, &eventProperties), "event was not created");
    check(event != nullptr && eventProperties == &event->properties, "CreateEvent did not return the event's property object");
    call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(properties, "_trophy_id", 13), "SetInt32 after CreateEvent failed");
    call(sceNpUniversalDataSystemDestroyEventPropertyObject(properties), "property object was not destroyed");
    check(Uds::Describe(*event) == R"(_UnlockTrophy {"_trophy_id":12})", "the event did not copy its properties");
    call(sceNpUniversalDataSystemEventPropertyObjectSetString(eventProperties, "detail", "boss"), "SetString on the event failed");
    call(sceNpUniversalDataSystemDestroyEventPropertyObject(eventProperties), "destroying the event's property object failed");
    check(Uds::Describe(*event) == R"(_UnlockTrophy {"_trophy_id":12,"detail":"boss"})", "properties set through the returned pointer were lost");
    call(sceNpUniversalDataSystemDestroyEvent(event), "event was not destroyed");
    NpUniversalDataSystemEvent* empty = nullptr;
    call(sceNpUniversalDataSystemCreateEvent("_Empty", nullptr, &empty, nullptr), "event without properties was not created");
    check(Uds::Describe(*empty) == "_Empty {}", "an event without properties was not empty");
    call(sceNpUniversalDataSystemDestroyEvent(empty), "event without properties was not destroyed");
    check(throwsInvalidArgument([] {
        NpUniversalDataSystemEvent* unnamed = nullptr;
        sceNpUniversalDataSystemCreateEvent(nullptr, nullptr, &unnamed, nullptr);

    }), "an event without a name was created");
}

void checkEventText(const NpUniversalDataSystemEvent* event, size_t expectedSize, std::string_view expected, const char* message) {
    size_t size = 0;
    call(sceNpUniversalDataSystemEventEstimateSize(event, &size), "EstimateSize failed");
    check(size == expectedSize, message);
    size_t length = 0;
    call(sceNpUniversalDataSystemEventToString(event, nullptr, 0, &length), "ToString without a buffer failed");
    check(length == expected.size(), message);
    std::string text(expected.size() + 2, '\x7f');
    length = 0;
    call(sceNpUniversalDataSystemEventToString(event, text.data(), expected.size() + 1, &length), "ToString failed");
    check(length == expected.size() && text.compare(0, expected.size(), expected) == 0 && text[expected.size()] == '\0' && text.back() == '\x7f', message);
}

void testEventSizeAndString() {
    using Fill = void (*)(NpUniversalDataSystemEventPropertyObject*);
    struct Case {
        Fill fill;
        size_t size;
        std::string_view text;
        const char* message;
    };
    const Case cases[] = {
        {[](NpUniversalDataSystemEventPropertyObject*) {}, 25,
         "{\n    \"eventName\": \"E\",\n    \"properties\": {\n    },\n}", "an empty event was not described as the console does"},
        {[](NpUniversalDataSystemEventPropertyObject* object) { call(sceNpUniversalDataSystemEventPropertyObjectSetInt64(object, "k", INT64_MIN), "SetInt64 failed"); }, 36,
         "{\n    \"eventName\": \"E\",\n    \"properties\": {\n        \"k\": -9223372036854775808 (Int64),\n    },\n}", "an int64 property was not described as the console does"},
        {[](NpUniversalDataSystemEventPropertyObject* object) { call(sceNpUniversalDataSystemEventPropertyObjectSetUInt64(object, "k", UINT64_MAX), "SetUInt64 failed"); }, 36,
         "{\n    \"eventName\": \"E\",\n    \"properties\": {\n        \"k\": 18446744073709551615 (UInt64),\n    },\n}", "a uint64 property was not described as the console does"},
        {[](NpUniversalDataSystemEventPropertyObject* object) { call(sceNpUniversalDataSystemEventPropertyObjectSetFloat32(object, "k", 0.1f), "SetFloat32 failed"); }, 32,
         "{\n    \"eventName\": \"E\",\n    \"properties\": {\n        \"k\": 0.1 (Float32),\n    },\n}", "a float32 property was not described as the console does"},
        {[](NpUniversalDataSystemEventPropertyObject* object) { call(sceNpUniversalDataSystemEventPropertyObjectSetFloat64(object, "k", 1e300), "SetFloat64 failed"); }, 36,
         "{\n    \"eventName\": \"E\",\n    \"properties\": {\n        \"k\": 1e+300 (Float64),\n    },\n}", "a float64 property was not described as the console does"},
        {[](NpUniversalDataSystemEventPropertyObject* object) { call(sceNpUniversalDataSystemEventPropertyObjectSetString(object, "s", "a\"b\\c\n\x01/z"), "SetString failed"); }, 37,
         "{\n    \"eventName\": \"E\",\n    \"properties\": {\n        \"s\": \"a\"b\\c\n\x01/z\",\n    },\n}", "a string property was escaped or resized"},
        {[](NpUniversalDataSystemEventPropertyObject* object) { call(sceNpUniversalDataSystemEventPropertyObjectSetBinary(object, "b", "\x00\x7f\xff", 3), "SetBinary failed"); }, 32,
         "{\n    \"eventName\": \"E\",\n    \"properties\": {\n        \"b\": 007FFF (Binary),\n    },\n}", "a binary property was not described as the console does"},
        {[](NpUniversalDataSystemEventPropertyObject* object) {
             call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(object, "zeta", 1), "SetInt32 failed");
             call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(object, "alpha", 2), "SetInt32 failed");
             call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(object, "mid", 3), "SetInt32 failed");
             call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(object, "alpha", 4), "SetInt32 failed");
         }, 55,
         "{\n    \"eventName\": \"E\",\n    \"properties\": {\n        \"zeta\": 1 (Int32),\n        \"alpha\": 4 (Int32),\n        \"mid\": 3 (Int32),\n    },\n}",
         "properties were not described in the order they were first set"},
        {[](NpUniversalDataSystemEventPropertyObject* object) {
             auto* child = createObject();
             call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(child, "x", 7), "SetInt32 failed");
             NpUniversalDataSystemEventPropertyObject* stored = nullptr;
             call(sceNpUniversalDataSystemEventPropertyObjectSetObject(object, "obj", child, &stored), "SetObject failed");
             call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(child, "after", 1), "SetInt32 failed");
             call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(stored, "viaPtr", 2), "SetInt32 failed");
             call(sceNpUniversalDataSystemDestroyEventPropertyObject(child), "child object was not destroyed");
         }, 49,
         "{\n    \"eventName\": \"E\",\n    \"properties\": {\n        \"obj\": {\n            \"x\": 7 (Int32),\n            \"viaPtr\": 2 (Int32),\n        },\n    },\n}",
         "a nested object was not described as the console does"},
        {[](NpUniversalDataSystemEventPropertyObject* object) {
             NpUniversalDataSystemEventPropertyArray* array = nullptr;
             call(sceNpUniversalDataSystemCreateEventPropertyArray(&array), "array was not created");
             call(sceNpUniversalDataSystemEventPropertyArraySetInt32(array, -1), "array SetInt32 failed");
             call(sceNpUniversalDataSystemEventPropertyArraySetString(array, "x"), "array SetString failed");
             call(sceNpUniversalDataSystemEventPropertyArraySetBool(array, true), "array SetBool failed");
             call(sceNpUniversalDataSystemEventPropertyArraySetUInt64(array, 9), "array SetUInt64 failed");
             call(sceNpUniversalDataSystemEventPropertyArraySetFloat32(array, 2.5f), "array SetFloat32 failed");
             call(sceNpUniversalDataSystemEventPropertyArraySetBinary(array, "\x01\x02", 2), "array SetBinary failed");
             auto* inner = createObject();
             call(sceNpUniversalDataSystemEventPropertyObjectSetInt32(inner, "i", 1), "SetInt32 failed");
             call(sceNpUniversalDataSystemEventPropertyArraySetObject(array, inner, nullptr), "array SetObject failed");
             NpUniversalDataSystemEventPropertyArray* empty = nullptr;
             call(sceNpUniversalDataSystemCreateEventPropertyArray(&empty), "empty array was not created");
             call(sceNpUniversalDataSystemEventPropertyArraySetArray(array, empty, nullptr), "array SetArray failed");
             call(sceNpUniversalDataSystemEventPropertyObjectSetArray(object, "arr", array, nullptr), "SetArray failed");
             call(sceNpUniversalDataSystemDestroyEventPropertyObject(inner), "inner object was not destroyed");
             call(sceNpUniversalDataSystemDestroyEventPropertyArray(empty), "empty array was not destroyed");
             call(sceNpUniversalDataSystemDestroyEventPropertyArray(array), "array was not destroyed");
         }, 65,
         "{\n    \"eventName\": \"E\",\n    \"properties\": {\n        \"arr\": [\n            -1 (Int32),\n            \"x\",\n            true,\n            9 (UInt64),\n"
         "            2.5 (Float32),\n            0102 (Binary),\n            {\n                \"i\": 1 (Int32),\n            },\n            [\n            ],\n        ],\n    },\n}",
         "an array was not described as the console does"},
    };
    for (const auto& item : cases) {
        NpUniversalDataSystemEvent* event = nullptr;
        NpUniversalDataSystemEventPropertyObject* properties = nullptr;
        call(sceNpUniversalDataSystemCreateEvent("E", nullptr, &event, &properties), "event was not created");
        item.fill(properties);
        checkEventText(event, item.size, item.text, item.message);
        call(sceNpUniversalDataSystemDestroyEvent(event), "event was not destroyed");
    }

    NpUniversalDataSystemEvent* event = nullptr;
    call(sceNpUniversalDataSystemCreateEvent("E", nullptr, &event, nullptr), "event was not created");
    size_t length = 0;
    check(sceNpUniversalDataSystemEventEstimateSize(nullptr, &length) == SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_NULL_EVENT, "EstimateSize accepted a null event");
    check(sceNpUniversalDataSystemEventEstimateSize(event, nullptr) == SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT, "EstimateSize accepted a null size");
    char text[8];
    check(sceNpUniversalDataSystemEventToString(nullptr, text, sizeof(text), &length) == SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_NULL_EVENT, "ToString accepted a null event");
    check(sceNpUniversalDataSystemEventToString(event, nullptr, 0, nullptr) == SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT, "ToString accepted no buffer and no length");
    length = 0;
    call(sceNpUniversalDataSystemEventToString(event, nullptr, sizeof(text), &length), "ToString without a buffer but with a capacity failed");
    check(length == 52, "ToString without a buffer did not report the full length");
    std::memset(text, 'x', sizeof(text));
    call(sceNpUniversalDataSystemEventToString(event, text, 0, &length), "ToString into an empty buffer failed");
    check(length == 52 && text[0] == 'x', "ToString wrote into an empty buffer");
    call(sceNpUniversalDataSystemEventToString(event, text, 1, nullptr), "ToString into a one-byte buffer failed");
    check(text[0] == '\0' && text[1] == 'x', "a one-byte buffer did not get only the terminator");
    call(sceNpUniversalDataSystemEventToString(event, text, 7, &length), "ToString into a short buffer failed");
    check(length == 52 && std::string_view(text, 7) == std::string_view("{\n    \0", 7) && text[7] == 'x', "a short buffer was not cut like snprintf");
    call(sceNpUniversalDataSystemDestroyEvent(event), "event was not destroyed");
}

Uds::TrophyEvent recognize(const char* name, std::optional<Uds::Value> id) {
    NpUniversalDataSystemEvent event;
    event.name = name;
    if (id) event.properties.Set(Uds::TrophyIdProperty, *id);
    return Uds::RecognizeTrophyEvent(event);
}

void testRecognition() {
    const auto unlock = recognize("_UnlockTrophy", std::int32_t{12});
    check(unlock.kind == Uds::TrophyEventKind::Unlock && unlock.trophyId == 12, "an unlock event was not recognised");
    const auto first = recognize("_UnlockTrophy", std::int32_t{0});
    check(first.kind == Uds::TrophyEventKind::Unlock && first.trophyId == 0, "an unlock of trophy 0 was not recognised");
    const auto progress = recognize("_UpdateTrophyProgress", std::int32_t{3});
    check(progress.kind == Uds::TrophyEventKind::Progress && progress.trophyId == 3, "a progress event was not recognised");
    const auto anonymous = recognize("_UpdateTrophyProgress", std::nullopt);
    check(anonymous.kind == Uds::TrophyEventKind::Progress && anonymous.trophyId == -1, "a progress event without a trophy id was not recognised");
    const auto missing = recognize("_UnlockTrophy", std::nullopt);
    check(missing.kind == Uds::TrophyEventKind::Malformed && missing.problem == "it has no _trophy_id", "an unlock without a trophy id was accepted");
    const auto widened = recognize("_UnlockTrophy", std::uint32_t{12});
    check(widened.kind == Uds::TrophyEventKind::Malformed && widened.problem == "its _trophy_id is not an int32", "an unlock with an unsigned trophy id was accepted");
    const auto negative = recognize("_UnlockTrophy", std::int32_t{-1});
    check(negative.kind == Uds::TrophyEventKind::Malformed && negative.problem == "its _trophy_id is negative", "an unlock with a negative trophy id was accepted");
    for (const char* name : {"_unlockTrophy", "UnlockTrophy", "_UnlockTrophy2", "_UnlockTrophy ", "_StartActivity", ""}) {
        check(recognize(name, std::int32_t{12}).kind == Uds::TrophyEventKind::None, "an unknown event was taken for a trophy event");
    }
}

void testStatEvent() {
    NpUniversalDataSystemEvent event;
    event.name = "STEPS";
    event.properties.Set("count", std::uint32_t{4});
    event.properties.Set("signed", std::int32_t{-2});
    event.properties.Set("wide", std::int64_t{7});
    event.properties.Set("huge", std::uint64_t{18000000000000000000ull});
    event.properties.Set("ratio", 0.5);
    event.properties.Set("name", std::string("x"));
    auto player = std::make_shared<NpUniversalDataSystemEventPropertyObject>();
    player->Set("steps", std::uint32_t{5});
    auto inner = std::make_shared<NpUniversalDataSystemEventPropertyObject>();
    inner->Set("depth", std::int32_t{1});
    player->Set("inner", inner);
    event.properties.Set("player", player);
    auto list = std::make_shared<NpUniversalDataSystemEventPropertyArray>();
    list->Add(std::int32_t{1});
    event.properties.Set("list", list);
    const auto converted = Uds::StatEvent(event);
    const std::vector<std::tuple<std::string, std::optional<std::uint64_t>, std::string>> expected{
        {"count", 4, "4"}, {"signed", std::nullopt, "-2"}, {"wide", 7, "7"}, {"huge", 18000000000000000000ull, "18000000000000000000"}, {"ratio", std::nullopt, "0.5"},
        {"name", std::nullopt, "\"x\""}, {"player.steps", 5, "5"}, {"player.inner.depth", 1, "1"}, {"list", std::nullopt, "[1]"}};
    check(converted.name == "STEPS" && converted.properties.size() == expected.size(), "the stat event does not hold every leaf property");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto& [path, value, text] = expected[i];
        const auto& property = converted.properties[i];
        check(property.path == path && property.value == value && property.text == text, "a stat event property has the wrong path, value or text");
    }
}

void testLocaleIndependentNumberFormatting() {
    const char* previous = std::setlocale(LC_NUMERIC, nullptr);
    check(previous != nullptr, "the current locale could not be read");
    const std::string saved = previous;
    struct RestoreLocale {
        std::string saved;
        ~RestoreLocale() { std::setlocale(LC_NUMERIC, saved.c_str()); }
    } restore{saved};
    bool commaDecimal = false;
    for (const auto* name : {"French", "French_France.1252", "fr_FR.UTF-8", "de_DE.UTF-8"}) {
        if (std::setlocale(LC_NUMERIC, name) != nullptr && std::string(std::localeconv()->decimal_point) == ",") {
            commaDecimal = true;
            break;
        }
    }
    if (!commaDecimal) {
#ifdef _WIN32
        throw std::runtime_error("No Windows comma-decimal locale available for regression");
#else
        std::puts("Comma-decimal locale regression unavailable on this host");
        return;
#endif
    }
    NpUniversalDataSystemEvent event;
    event.properties.Set("float", 0.1f);
    event.properties.Set("double", 0.1);
    check(Uds::Describe(event.properties) == R"({"float":0.100000001,"double":0.10000000000000001})",
          "described event properties changed with the numeric locale");
    const auto converted = Uds::StatEvent(event);
    check(converted.properties.size() == 2, "float and double event properties were lost");
    check(converted.properties[0].text == "0.100000001", "float property text changed with the numeric locale");
    check(converted.properties[1].text == "0.10000000000000001", "double property text changed with the numeric locale");
}

struct Unlocks {
    int count = 0;
};

void countUnlock(void* context, const NpTrophy2Unlock&) {
    ++static_cast<Unlocks*>(context)->count;
}

void post(int context, const char* name, std::optional<Uds::Value> id) {
    NpUniversalDataSystemEvent* event = nullptr;
    NpUniversalDataSystemEventPropertyObject* properties = nullptr;
    call(sceNpUniversalDataSystemCreateEvent(name, nullptr, &event, &properties), "event was not created");
    if (id) properties->Set(Uds::TrophyIdProperty, *id);
    call(sceNpUniversalDataSystemPostEvent(context, 1, event, 0), "event was not posted");
    call(sceNpUniversalDataSystemDestroyEvent(event), "event was not destroyed");
}

void testPostingIgnoredEvents() {
    Unlocks unlocks;
    const auto listener = NpTrophy2AddUnlockListener_nid_no_patch(countUnlock, &unlocks);
    int context = 0;
    call(sceNpUniversalDataSystemCreateContext(&context, InitialUser, 0, 0), "context was not created");
    int second = 0;
    call(sceNpUniversalDataSystemCreateContext(&second, InitialUser, 0, 0), "second context was not created");
    check(second != context, "two contexts share an id");
    post(context, "_StartActivity", std::int32_t{1});
    post(context, "_UnlockTrophy", std::string("1"));
    post(context, "_UnlockTrophy", std::nullopt);
    post(context, "_UpdateTrophyProgress", std::int32_t{2});
    post(context, "_UpdateTrophyProgress", std::int32_t{2});
    post(second + 1, "_UnlockTrophy", std::int32_t{4});
    call(sceNpUniversalDataSystemDestroyContext(context), "context was not destroyed");
    post(context, "_UnlockTrophy", std::int32_t{4});
    call(sceNpUniversalDataSystemDestroyContext(second), "second context was not destroyed");
    NpTrophy2RemoveUnlockListener_nid_no_patch(listener);
    check(unlocks.count == 0, "an event that is not a valid unlock reached the trophy store");
    check(throwsInvalidArgument([] { sceNpUniversalDataSystemPostEvent(1, 1, nullptr, 0); }), "a null event was posted");
    NpUniversalDataSystemEvent stray;
    stray.name = "_UnlockTrophy";
    check(throwsInvalidArgument([&stray] { sceNpUniversalDataSystemPostEvent(1, 1, &stray, 0); }), "an event that CreateEvent did not create was posted");
    NpUniversalDataSystemEvent* destroyed = nullptr;
    call(sceNpUniversalDataSystemCreateEvent("_UnlockTrophy", nullptr, &destroyed, nullptr), "event was not created");
    call(sceNpUniversalDataSystemDestroyEvent(destroyed), "event was not destroyed");
    check(throwsInvalidArgument([destroyed] { sceNpUniversalDataSystemPostEvent(1, 1, destroyed, 0); }), "a destroyed event was posted");
}

}

int main() {
    try {
        testObjectSetters();
        testNestedValues();
        testEvents();
        testEventSizeAndString();
        testRecognition();
        testStatEvent();
        testLocaleIndependentNumberFormatting();
        testPostingIgnoredEvents();
        std::puts("UDS event tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
