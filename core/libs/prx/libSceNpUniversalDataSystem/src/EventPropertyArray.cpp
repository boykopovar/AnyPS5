#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

Uds::Owned<NpUniversalDataSystemEventPropertyArray>& arrays() {
    static Uds::Owned<NpUniversalDataSystemEventPropertyArray> registry;
    return registry;
}

}

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyArray(NpUniversalDataSystemEventPropertyArray** new_array) {
    if (new_array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    *new_array = arrays().Adopt(std::make_unique<NpUniversalDataSystemEventPropertyArray>());
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyArray(NpUniversalDataSystemEventPropertyArray* array) {
    arrays().Release(array);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetString(
    NpUniversalDataSystemEventPropertyArray* array, const char* value)
{
    if (array == nullptr || value == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    array->Add(std::string(value));
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetInt32(
    NpUniversalDataSystemEventPropertyArray* array, int32_t value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    array->Add(value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetUInt32(
    NpUniversalDataSystemEventPropertyArray* array, uint32_t value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    array->Add(value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetInt64(
    NpUniversalDataSystemEventPropertyArray* array, int64_t value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    array->Add(value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetUInt64(
    NpUniversalDataSystemEventPropertyArray* array, uint64_t value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    array->Add(value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetFloat32(
    NpUniversalDataSystemEventPropertyArray* array, float value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    array->Add(value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetFloat64(
NpUniversalDataSystemEventPropertyArray* array, double value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    array->Add(value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetBool(
    NpUniversalDataSystemEventPropertyArray* array, bool value)
{
    if (array == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    array->Add(value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetBinary(
    NpUniversalDataSystemEventPropertyArray* array, const void* value, size_t value_size)
{
    if (array == nullptr || value == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(value);
    array->Add(Uds::Binary(bytes, bytes + value_size));
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
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyObject>(value != nullptr ? value->Clone() : NpUniversalDataSystemEventPropertyObject{});
    if (value_ptr != nullptr) {
        *value_ptr = child.get();
    }
    array->Add(std::move(child));
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
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyArray>(value != nullptr ? value->Clone() : NpUniversalDataSystemEventPropertyArray{});
    if (value_ptr != nullptr) {
        *value_ptr = child.get();
    }
    array->Add(std::move(child));
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}
