#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyObject(NpUniversalDataSystemEventPropertyObject** new_object) {
    (void)new_object;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyObject(NpUniversalDataSystemEventPropertyObject* object) {
    (void)object;
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

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetInt32(NpUniversalDataSystemEventPropertyObject* object, const char* key, int32_t value) {
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

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetInt64(NpUniversalDataSystemEventPropertyObject* object, const char* key, int64_t value) {
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

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetBool(NpUniversalDataSystemEventPropertyObject* object, const char* key, bool value) {
    (void)object;
    (void)key;
    (void)value;
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

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetObject(NpUniversalDataSystemEventPropertyObject* object, const char* key, const NpUniversalDataSystemEventPropertyObject* value, NpUniversalDataSystemEventPropertyObject** value_ptr) {
    (void)object;
    (void)key;
    (void)value;
    (void)value_ptr;
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

}
