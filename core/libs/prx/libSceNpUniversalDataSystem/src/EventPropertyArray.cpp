#include <cstddef>
#include <cstdint>
#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyArray(NpUniversalDataSystemEventPropertyArray** new_array) {
    if (new_array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    *new_array = new NpUniversalDataSystemEventPropertyArray;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyArray(NpUniversalDataSystemEventPropertyArray* array) {
    delete array;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetString(
    NpUniversalDataSystemEventPropertyArray* array, const char* value)
{
    if (array == nullptr || value == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetInt32(
    NpUniversalDataSystemEventPropertyArray* array, int32_t value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetUInt32(
    NpUniversalDataSystemEventPropertyArray* array, uint32_t value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetInt64(
    NpUniversalDataSystemEventPropertyArray* array, int64_t value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetUInt64(
    NpUniversalDataSystemEventPropertyArray* array, uint64_t value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetFloat32(
    NpUniversalDataSystemEventPropertyArray* array, float value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetFloat64(
NpUniversalDataSystemEventPropertyArray* array, double value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetBool(
    NpUniversalDataSystemEventPropertyArray* array, bool value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetBinary(
    NpUniversalDataSystemEventPropertyArray* array, const void* value, size_t value_size)
{
    if (array == nullptr || value == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetObject(
    NpUniversalDataSystemEventPropertyArray* array,
    const NpUniversalDataSystemEventPropertyObject* value,
    NpUniversalDataSystemEventPropertyObject** value_ptr)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    if (value_ptr != nullptr) {
        *value_ptr = (value != nullptr
            ? const_cast<NpUniversalDataSystemEventPropertyObject*>(value)
            : new NpUniversalDataSystemEventPropertyObject);
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetArray(
    NpUniversalDataSystemEventPropertyArray* array,
    const NpUniversalDataSystemEventPropertyArray* value,
    NpUniversalDataSystemEventPropertyArray** value_ptr)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    if (value_ptr != nullptr) {
        *value_ptr = (value != nullptr
            ? const_cast<NpUniversalDataSystemEventPropertyArray*>(value)
            : new NpUniversalDataSystemEventPropertyArray);
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}
