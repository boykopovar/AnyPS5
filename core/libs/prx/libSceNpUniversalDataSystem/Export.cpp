#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemAbortHandle(int handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemCreateContext(int* context, int user_id, uint32_t service_label, uint64_t options) {
 (void)context;
 (void)user_id;
 (void)service_label;
 (void)options;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemCreateEvent(const char* event_name, const NpUniversalDataSystemEventPropertyObject* prop, NpUniversalDataSystemEvent** new_event, NpUniversalDataSystemEventPropertyObject** prop_ptr) {
 (void)event_name;
 (void)prop;
 (void)new_event;
 (void)prop_ptr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyArray(NpUniversalDataSystemEventPropertyArray** new_array) {
 (void)new_array;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyObject(NpUniversalDataSystemEventPropertyObject** new_object) {
 (void)new_object;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemCreateHandle(int* handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemDestroyContext(int context) {
 (void)context;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEvent(NpUniversalDataSystemEvent* event) {
 (void)event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyArray(NpUniversalDataSystemEventPropertyArray* array) {
 (void)array;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyObject(NpUniversalDataSystemEventPropertyObject* object) {
 (void)object;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemDestroyHandle(int handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventEstimateSize(const NpUniversalDataSystemEvent* event, size_t* size) {
 (void)event;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetArray(NpUniversalDataSystemEventPropertyArray* array, const NpUniversalDataSystemEventPropertyArray* value, NpUniversalDataSystemEventPropertyArray** value_ptr) {
 (void)array;
 (void)value;
 (void)value_ptr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetBinary(NpUniversalDataSystemEventPropertyArray* array, const void* value, size_t value_size) {
 (void)array;
 (void)value;
 (void)value_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetBool(NpUniversalDataSystemEventPropertyArray* array, bool value) {
 (void)array;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetFloat32(NpUniversalDataSystemEventPropertyArray* array, float value) {
 (void)array;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetFloat64(NpUniversalDataSystemEventPropertyArray* array, double value) {
 (void)array;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetInt32(NpUniversalDataSystemEventPropertyArray* array, int32_t value) {
 (void)array;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetInt64(NpUniversalDataSystemEventPropertyArray* array, int64_t value) {
 (void)array;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetObject(NpUniversalDataSystemEventPropertyArray* array, const NpUniversalDataSystemEventPropertyObject* value, NpUniversalDataSystemEventPropertyObject** value_ptr) {
 (void)array;
 (void)value;
 (void)value_ptr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetString(NpUniversalDataSystemEventPropertyArray* array, const char* value) {
 (void)array;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetUInt32(NpUniversalDataSystemEventPropertyArray* array, uint32_t value) {
 (void)array;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetUInt64(NpUniversalDataSystemEventPropertyArray* array, uint64_t value) {
 (void)array;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetArray(NpUniversalDataSystemEventPropertyObject* object, const char* key, const NpUniversalDataSystemEventPropertyArray* value, NpUniversalDataSystemEventPropertyArray** value_ptr) {
 (void)object;
 (void)key;
 (void)value;
 (void)value_ptr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetBinary(NpUniversalDataSystemEventPropertyObject* object, const char* key, const void* value, size_t value_size) {
 (void)object;
 (void)key;
 (void)value;
 (void)value_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetBool(NpUniversalDataSystemEventPropertyObject* object, const char* key, bool value) {
 (void)object;
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetFloat32(NpUniversalDataSystemEventPropertyObject* object, const char* key, float value) {
 (void)object;
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetFloat64(NpUniversalDataSystemEventPropertyObject* object, const char* key, double value) {
 (void)object;
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetInt32(NpUniversalDataSystemEventPropertyObject* object, const char* key, int32_t value) {
 (void)object;
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetInt64(NpUniversalDataSystemEventPropertyObject* object, const char* key, int64_t value) {
 (void)object;
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetObject(NpUniversalDataSystemEventPropertyObject* object, const char* key, const NpUniversalDataSystemEventPropertyObject* value, NpUniversalDataSystemEventPropertyObject** value_ptr) {
 (void)object;
 (void)key;
 (void)value;
 (void)value_ptr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetString(NpUniversalDataSystemEventPropertyObject* object, const char* key, const char* value) {
 (void)object;
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetUInt32(NpUniversalDataSystemEventPropertyObject* object, const char* key, uint32_t value) {
 (void)object;
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetUInt64(NpUniversalDataSystemEventPropertyObject* object, const char* key, uint64_t value) {
 (void)object;
 (void)key;
 (void)value;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventToString(const NpUniversalDataSystemEvent* event, char* buf, size_t buf_size, size_t* string_size) {
 (void)event;
 (void)buf;
 (void)buf_size;
 (void)string_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemGetMemoryStat(NpUniversalDataSystemMemoryStat* stat) {
 (void)stat;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemGetStorageStat(int context, NpUniversalDataSystemStorageStat* stat) {
 (void)context;
 (void)stat;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemInitialize(const NpUniversalDataSystemInitParam* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemPostEvent(int context, int handle, const void* event, uint64_t options) {
 (void)context;
 (void)handle;
 (void)event;
 (void)options;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemRegisterContext(int context, int handle, uint64_t options) {
 (void)context;
 (void)handle;
 (void)options;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUniversalDataSystemTerminate(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
