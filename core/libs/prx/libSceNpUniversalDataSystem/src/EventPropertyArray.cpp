#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyArray(NpUniversalDataSystemEventPropertyArray** new_array) {
    (void)new_array;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyArray(NpUniversalDataSystemEventPropertyArray* array) {
    (void)array;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetString(NpUniversalDataSystemEventPropertyArray* array, const char* value) {
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

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetUInt32(NpUniversalDataSystemEventPropertyArray* array, uint32_t value) {
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

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetUInt64(NpUniversalDataSystemEventPropertyArray* array, uint64_t value) {
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

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetBool(NpUniversalDataSystemEventPropertyArray* array, bool value) {
    (void)array;
    (void)value;
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

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetObject(NpUniversalDataSystemEventPropertyArray* array, const NpUniversalDataSystemEventPropertyObject* value, NpUniversalDataSystemEventPropertyObject** value_ptr) {
    (void)array;
    (void)value;
    (void)value_ptr;
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

}
