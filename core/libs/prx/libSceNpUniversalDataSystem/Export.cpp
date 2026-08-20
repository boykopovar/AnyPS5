#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceNpUniversalDataSystemAbortHandle(int handle) {
 (void)handle;
 return 0;
}

int sceNpUniversalDataSystemCreateContext(int* context, int user_id, uint32_t service_label, uint64_t options) {
 (void)context;
 (void)user_id;
 (void)service_label;
 (void)options;
 return 0;
}

int sceNpUniversalDataSystemCreateEvent(const char* event_name, const NpUniversalDataSystemEventPropertyObject* prop, NpUniversalDataSystemEvent** new_event, NpUniversalDataSystemEventPropertyObject** prop_ptr) {
 (void)event_name;
 (void)prop;
 (void)new_event;
 (void)prop_ptr;
 return 0;
}

int sceNpUniversalDataSystemCreateEventPropertyArray(NpUniversalDataSystemEventPropertyArray** new_array) {
 (void)new_array;
 return 0;
}

int sceNpUniversalDataSystemCreateEventPropertyObject(NpUniversalDataSystemEventPropertyObject** new_object) {
 (void)new_object;
 return 0;
}

int sceNpUniversalDataSystemCreateHandle(int* handle) {
 (void)handle;
 return 0;
}

int sceNpUniversalDataSystemDestroyContext(int context) {
 (void)context;
 return 0;
}

int sceNpUniversalDataSystemDestroyEvent(NpUniversalDataSystemEvent* event) {
 (void)event;
 return 0;
}

int sceNpUniversalDataSystemDestroyEventPropertyArray(NpUniversalDataSystemEventPropertyArray* array) {
 (void)array;
 return 0;
}

int sceNpUniversalDataSystemDestroyEventPropertyObject(NpUniversalDataSystemEventPropertyObject* object) {
 (void)object;
 return 0;
}

int sceNpUniversalDataSystemDestroyHandle(int handle) {
 (void)handle;
 return 0;
}

int sceNpUniversalDataSystemEventEstimateSize(const NpUniversalDataSystemEvent* event, size_t* size) {
 (void)event;
 (void)size;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetArray(NpUniversalDataSystemEventPropertyArray* array, const NpUniversalDataSystemEventPropertyArray* value, NpUniversalDataSystemEventPropertyArray** value_ptr) {
 (void)array;
 (void)value;
 (void)value_ptr;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetBinary(NpUniversalDataSystemEventPropertyArray* array, const void* value, size_t value_size) {
 (void)array;
 (void)value;
 (void)value_size;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetBool(NpUniversalDataSystemEventPropertyArray* array, bool value) {
 (void)array;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetFloat32(NpUniversalDataSystemEventPropertyArray* array, float value) {
 (void)array;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetFloat64(NpUniversalDataSystemEventPropertyArray* array, double value) {
 (void)array;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetInt32(NpUniversalDataSystemEventPropertyArray* array, int32_t value) {
 (void)array;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetInt64(NpUniversalDataSystemEventPropertyArray* array, int64_t value) {
 (void)array;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetObject(NpUniversalDataSystemEventPropertyArray* array, const NpUniversalDataSystemEventPropertyObject* value, NpUniversalDataSystemEventPropertyObject** value_ptr) {
 (void)array;
 (void)value;
 (void)value_ptr;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetString(NpUniversalDataSystemEventPropertyArray* array, const char* value) {
 (void)array;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetUInt32(NpUniversalDataSystemEventPropertyArray* array, uint32_t value) {
 (void)array;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyArraySetUInt64(NpUniversalDataSystemEventPropertyArray* array, uint64_t value) {
 (void)array;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetArray(NpUniversalDataSystemEventPropertyObject* object, const char* key, const NpUniversalDataSystemEventPropertyArray* value, NpUniversalDataSystemEventPropertyArray** value_ptr) {
 (void)object;
 (void)key;
 (void)value;
 (void)value_ptr;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetBinary(NpUniversalDataSystemEventPropertyObject* object, const char* key, const void* value, size_t value_size) {
 (void)object;
 (void)key;
 (void)value;
 (void)value_size;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetBool(NpUniversalDataSystemEventPropertyObject* object, const char* key, bool value) {
 (void)object;
 (void)key;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetFloat32(NpUniversalDataSystemEventPropertyObject* object, const char* key, float value) {
 (void)object;
 (void)key;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetFloat64(NpUniversalDataSystemEventPropertyObject* object, const char* key, double value) {
 (void)object;
 (void)key;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetInt32(NpUniversalDataSystemEventPropertyObject* object, const char* key, int32_t value) {
 (void)object;
 (void)key;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetInt64(NpUniversalDataSystemEventPropertyObject* object, const char* key, int64_t value) {
 (void)object;
 (void)key;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetObject(NpUniversalDataSystemEventPropertyObject* object, const char* key, const NpUniversalDataSystemEventPropertyObject* value, NpUniversalDataSystemEventPropertyObject** value_ptr) {
 (void)object;
 (void)key;
 (void)value;
 (void)value_ptr;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetString(NpUniversalDataSystemEventPropertyObject* object, const char* key, const char* value) {
 (void)object;
 (void)key;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetUInt32(NpUniversalDataSystemEventPropertyObject* object, const char* key, uint32_t value) {
 (void)object;
 (void)key;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventPropertyObjectSetUInt64(NpUniversalDataSystemEventPropertyObject* object, const char* key, uint64_t value) {
 (void)object;
 (void)key;
 (void)value;
 return 0;
}

int sceNpUniversalDataSystemEventToString(const NpUniversalDataSystemEvent* event, char* buf, size_t buf_size, size_t* string_size) {
 (void)event;
 (void)buf;
 (void)buf_size;
 (void)string_size;
 return 0;
}

int sceNpUniversalDataSystemGetMemoryStat(NpUniversalDataSystemMemoryStat* stat) {
 (void)stat;
 return 0;
}

int sceNpUniversalDataSystemGetStorageStat(int context, NpUniversalDataSystemStorageStat* stat) {
 (void)context;
 (void)stat;
 return 0;
}

int sceNpUniversalDataSystemInitialize(const NpUniversalDataSystemInitParam* param) {
 (void)param;
 return 0;
}

int sceNpUniversalDataSystemPostEvent(int context, int handle, const void* event, uint64_t options) {
 (void)context;
 (void)handle;
 (void)event;
 (void)options;
 return 0;
}

int sceNpUniversalDataSystemRegisterContext(int context, int handle, uint64_t options) {
 (void)context;
 (void)handle;
 (void)options;
 return 0;
}

int sceNpUniversalDataSystemTerminate(void) {
 return 0;
}

}
