#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
int APS5_VABI sceNpUniversalDataSystemInitialize(const NpUniversalDataSystemInitParam* param);
int APS5_VABI sceNpUniversalDataSystemTerminate(void);
int APS5_VABI sceNpUniversalDataSystemGetMemoryStat(NpUniversalDataSystemMemoryStat* stat);
int APS5_VABI sceNpUniversalDataSystemGetStorageStat(int context, NpUniversalDataSystemStorageStat* stat);

int APS5_VABI sceNpUniversalDataSystemCreateHandle(int* handle);
int APS5_VABI sceNpUniversalDataSystemDestroyHandle(int handle);
int APS5_VABI sceNpUniversalDataSystemAbortHandle(int handle);

int APS5_VABI sceNpUniversalDataSystemCreateContext(int* context, int user_id, std::uint32_t service_label, std::uint64_t options);
int APS5_VABI sceNpUniversalDataSystemDestroyContext(int context);
int APS5_VABI sceNpUniversalDataSystemRegisterContext(int context, int handle, std::uint64_t options);

int APS5_VABI sceNpUniversalDataSystemCreateEvent(const char* event_name, const NpUniversalDataSystemEventPropertyObject* prop, NpUniversalDataSystemEvent** new_event, NpUniversalDataSystemEventPropertyObject** prop_ptr);
int APS5_VABI sceNpUniversalDataSystemDestroyEvent(NpUniversalDataSystemEvent* event);
int APS5_VABI sceNpUniversalDataSystemPostEvent(int context, int handle, const void* event, std::uint64_t options);
int APS5_VABI sceNpUniversalDataSystemEventEstimateSize(const NpUniversalDataSystemEvent* event, std::size_t* size);
int APS5_VABI sceNpUniversalDataSystemEventToString(const NpUniversalDataSystemEvent* event, char* buf, std::size_t buf_size, std::size_t* string_size);

int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyObject(NpUniversalDataSystemEventPropertyObject** new_object);
int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyObject(NpUniversalDataSystemEventPropertyObject* object);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetString(NpUniversalDataSystemEventPropertyObject* object, const char* key, const char* value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetInt32(NpUniversalDataSystemEventPropertyObject* object, const char* key, std::int32_t value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetBool(NpUniversalDataSystemEventPropertyObject* object, const char* key, bool value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetBinary(NpUniversalDataSystemEventPropertyObject* object, const char* key, const void* value, std::size_t value_size);

int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyArray(NpUniversalDataSystemEventPropertyArray** new_array);
int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyArray(NpUniversalDataSystemEventPropertyArray* array);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetString(NpUniversalDataSystemEventPropertyArray* array, const char* value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetInt32(NpUniversalDataSystemEventPropertyArray* array, std::int32_t value);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpUniversalDataSystem: %s\n", message);
        std::abort();
    }
}

template<typename TException, typename TCall>
void RequireThrows(TCall call, const char* message) {
    try {
        call();
    } catch (const TException&) {
        return;
    } catch (...) {
        Require(false, message);
    }
    Require(false, message);
}

void CheckLifecycleAndStats() {
    const NpUniversalDataSystemInitParam param{sizeof(NpUniversalDataSystemInitParam), 0};
    RequireThrows<std::invalid_argument>([] { sceNpUniversalDataSystemInitialize(nullptr); }, "initialize with a null parameter");
    Require(sceNpUniversalDataSystemInitialize(&param) == 0, "initialize failed");
    Require(sceNpUniversalDataSystemTerminate() == 0, "terminate failed");

    RequireThrows<std::invalid_argument>([] { sceNpUniversalDataSystemGetMemoryStat(nullptr); }, "memory stat with a null output");
    NpUniversalDataSystemMemoryStat memory{};
    std::memset(&memory, 0xAA, sizeof(memory));
    Require(sceNpUniversalDataSystemGetMemoryStat(&memory) == 0, "memory stat failed");
    Require(memory.pool_size == 0 && memory.max_inuse_size == 0 && memory.current_inuse_size == 0, "memory stat was not cleared");

    RequireThrows<std::invalid_argument>([] { sceNpUniversalDataSystemGetStorageStat(1, nullptr); }, "storage stat with a null output");
    NpUniversalDataSystemStorageStat storage{};
    std::memset(&storage, 0xAA, sizeof(storage));
    Require(sceNpUniversalDataSystemGetStorageStat(1, &storage) == 0, "storage stat failed");
    Require(storage.in_events == 0 && storage.out_events == 0 && storage.lost_events == 0 && storage.max_inuse_size == 0 &&
        storage.current_events == 0 && storage.current_inuse_size == 0 && storage.current_free_size == 0, "storage stat was not cleared");
}

void CheckHandle() {
    RequireThrows<std::invalid_argument>([] { sceNpUniversalDataSystemCreateHandle(nullptr); }, "handle creation with a null output");
    int handle = -1;
    Require(sceNpUniversalDataSystemCreateHandle(&handle) == 0 && handle == 1, "handle creation failed");
    Require(sceNpUniversalDataSystemDestroyHandle(handle) == 0, "handle destroy failed");
    Require(sceNpUniversalDataSystemAbortHandle(handle) == 0, "handle abort failed");
}

void CheckContext() {
    int handle = 0;
    Require(sceNpUniversalDataSystemCreateHandle(&handle) == 0, "handle for context failed");

    RequireThrows<std::invalid_argument>([] { sceNpUniversalDataSystemCreateContext(nullptr, 0, 0, 0); }, "context creation with a null output");
    int context = -1;
    Require(sceNpUniversalDataSystemCreateContext(&context, 0, 0, 0) == 0 && context == 1, "context creation failed");
    Require(sceNpUniversalDataSystemRegisterContext(context, handle, 0) == 0, "context register failed");
    Require(sceNpUniversalDataSystemDestroyContext(context) == 0, "context destroy failed");
}

void CheckEvent() {
    int context = 0;
    int handle = 0;
    Require(sceNpUniversalDataSystemCreateContext(&context, 0, 0, 0) == 0, "context for event failed");
    Require(sceNpUniversalDataSystemCreateHandle(&handle) == 0, "handle for event failed");

    RequireThrows<std::invalid_argument>([] {
        NpUniversalDataSystemEvent* event = nullptr;
        sceNpUniversalDataSystemCreateEvent(nullptr, nullptr, &event, nullptr);
    }, "event creation with a null name");
    RequireThrows<std::invalid_argument>([] {
        sceNpUniversalDataSystemCreateEvent("event", nullptr, nullptr, nullptr);
    }, "event creation with a null output");

    NpUniversalDataSystemEvent* event = nullptr;
    Require(sceNpUniversalDataSystemCreateEvent("event", nullptr, &event, nullptr) == 0 && event != nullptr, "event creation failed");

    std::size_t size = 0;
    RequireThrows<std::invalid_argument>([&] { sceNpUniversalDataSystemEventEstimateSize(nullptr, &size); }, "event estimate with a null event");
    RequireThrows<std::invalid_argument>([&] { sceNpUniversalDataSystemEventEstimateSize(event, nullptr); }, "event estimate with a null output");
    Require(sceNpUniversalDataSystemEventEstimateSize(event, &size) == 0 && size == 3, "event estimate failed");

    char buffer[8] = {};
    std::size_t length = 0;
    RequireThrows<std::invalid_argument>([&] { sceNpUniversalDataSystemEventToString(nullptr, buffer, sizeof(buffer), &length); }, "event to string with a null event");
    Require(sceNpUniversalDataSystemEventToString(event, buffer, sizeof(buffer), &length) == 0, "event to string failed");
    Require(std::strcmp(buffer, "{}") == 0 && length == 3, "event to string output is wrong");

    Require(sceNpUniversalDataSystemPostEvent(context, handle, event, 0) == 0, "post event failed");
    Require(sceNpUniversalDataSystemDestroyEvent(event) == 0, "event destroy failed");
}

void CheckEventPropertyObject() {
    RequireThrows<std::invalid_argument>([] { sceNpUniversalDataSystemCreateEventPropertyObject(nullptr); }, "property object creation with a null output");
    NpUniversalDataSystemEventPropertyObject* object = nullptr;
    Require(sceNpUniversalDataSystemCreateEventPropertyObject(&object) == 0 && object != nullptr, "property object creation failed");

    RequireThrows<std::invalid_argument>([&] { sceNpUniversalDataSystemEventPropertyObjectSetString(nullptr, "key", "value"); }, "property object string with a null object");
    RequireThrows<std::invalid_argument>([&] { sceNpUniversalDataSystemEventPropertyObjectSetString(object, nullptr, "value"); }, "property object string with a null key");
    RequireThrows<std::invalid_argument>([&] { sceNpUniversalDataSystemEventPropertyObjectSetString(object, "key", nullptr); }, "property object string with a null value");
    RequireThrows<std::invalid_argument>([&] { sceNpUniversalDataSystemEventPropertyObjectSetInt32(object, nullptr, 1); }, "property object int32 with a null key");
    RequireThrows<std::invalid_argument>([&] { sceNpUniversalDataSystemEventPropertyObjectSetBinary(object, "key", nullptr, 4); }, "property object binary with a null value");

    const char binary[] = {1, 2, 3, 4};
    Require(sceNpUniversalDataSystemEventPropertyObjectSetString(object, "key", "value") == 0, "property object string failed");
    Require(sceNpUniversalDataSystemEventPropertyObjectSetInt32(object, "key", 1) == 0, "property object int32 failed");
    Require(sceNpUniversalDataSystemEventPropertyObjectSetBool(object, "key", true) == 0, "property object bool failed");
    Require(sceNpUniversalDataSystemEventPropertyObjectSetBinary(object, "key", binary, sizeof(binary)) == 0, "property object binary failed");
    Require(sceNpUniversalDataSystemDestroyEventPropertyObject(object) == 0, "property object destroy failed");
}

void CheckEventPropertyArray() {
    RequireThrows<std::invalid_argument>([] { sceNpUniversalDataSystemCreateEventPropertyArray(nullptr); }, "property array creation with a null output");
    NpUniversalDataSystemEventPropertyArray* array = nullptr;
    Require(sceNpUniversalDataSystemCreateEventPropertyArray(&array) == 0 && array != nullptr, "property array creation failed");

    RequireThrows<std::invalid_argument>([&] { sceNpUniversalDataSystemEventPropertyArraySetString(array, nullptr); }, "property array string with a null value");
    Require(sceNpUniversalDataSystemEventPropertyArraySetInt32(array, 1) == 0, "property array int32 failed");
    Require(sceNpUniversalDataSystemEventPropertyArraySetString(array, "v") == 0, "property array string failed");
    Require(sceNpUniversalDataSystemDestroyEventPropertyArray(array) == 0, "property array destroy failed");
}

}

int main() {
    CheckLifecycleAndStats();
    CheckHandle();
    CheckContext();
    CheckEvent();
    CheckEventPropertyObject();
    CheckEventPropertyArray();
    return 0;
}
