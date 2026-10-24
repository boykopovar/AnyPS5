#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

Uds::Owned<NpUniversalDataSystemEventPropertyObject>& objects() {
    static Uds::Owned<NpUniversalDataSystemEventPropertyObject> registry;
    return registry;
}

}

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyObject(NpUniversalDataSystemEventPropertyObject** new_object) {
    if (new_object == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    *new_object = objects().Adopt(std::make_unique<NpUniversalDataSystemEventPropertyObject>());
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyObject(NpUniversalDataSystemEventPropertyObject* object) {
    objects().Release(object);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetString(
    NpUniversalDataSystemEventPropertyObject* object, const char* key, const char* value)
{
    if (object == nullptr || key == nullptr || value == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    object->Set(key, std::string(value));
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetInt32(
    NpUniversalDataSystemEventPropertyObject* object, const char* key, int32_t value)
{
    if (object == nullptr || key == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    object->Set(key, value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetUInt32(
    NpUniversalDataSystemEventPropertyObject* object, const char* key, uint32_t value)
{
    if (object == nullptr || key == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    object->Set(key, value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetInt64(
    NpUniversalDataSystemEventPropertyObject* object, const char* key, int64_t value)
{
    if (object == nullptr || key == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    object->Set(key, value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetUInt64(
    NpUniversalDataSystemEventPropertyObject* object, const char* key, uint64_t value)
{
    if (object == nullptr || key == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    object->Set(key, value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetFloat32(
    NpUniversalDataSystemEventPropertyObject* object, const char* key, float value)
{
    if (object == nullptr || key == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    object->Set(key, value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetFloat64(
    NpUniversalDataSystemEventPropertyObject* object, const char* key, double value)
{
    if (object == nullptr || key == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    object->Set(key, value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetBool(
    NpUniversalDataSystemEventPropertyObject* object, const char* key, bool value)
{
    if (object == nullptr || key == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    object->Set(key, value);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetBinary(
    NpUniversalDataSystemEventPropertyObject* object, const char* key, const void* value, size_t value_size)
{
    if (object == nullptr || key == nullptr || value == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(value);
    object->Set(key, Uds::Binary(bytes, bytes + value_size));
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetObject(
    NpUniversalDataSystemEventPropertyObject* object,
    const char* key,
    const NpUniversalDataSystemEventPropertyObject* value,
    NpUniversalDataSystemEventPropertyObject** value_ptr)
{
    if (object == nullptr || key == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyObject>(value != nullptr ? value->Clone() : NpUniversalDataSystemEventPropertyObject{});
    if (value_ptr != nullptr) {
        *value_ptr = child.get();
    }
    object->Set(key, std::move(child));
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetArray(
    NpUniversalDataSystemEventPropertyObject* object,
    const char* key,
    const NpUniversalDataSystemEventPropertyArray* value,
    NpUniversalDataSystemEventPropertyArray** value_ptr)
{
    if (object == nullptr || key == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyArray>(value != nullptr ? value->Clone() : NpUniversalDataSystemEventPropertyArray{});
    if (value_ptr != nullptr) {
        *value_ptr = child.get();
    }
    object->Set(key, std::move(child));
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}
