#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include "prx/libSceNpUniversalDataSystem/include/UdsEvent.hpp"
#include "NpUniversalDataSystem.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>

namespace {

constexpr int kOutOfMemory = static_cast<int>(0x80553101u);
constexpr int kInvalidArgument = static_cast<int>(0x80553102u);
constexpr int kInvalidName = static_cast<int>(0x8055310Du);
constexpr int kInvalidParamSize = static_cast<int>(0x80553112u);
constexpr int kInvalidKey = static_cast<int>(0x80553114u);
constexpr int kNullValue = static_cast<int>(0x80553115u);
constexpr int kNullRecord = static_cast<int>(0x80553116u);
constexpr int kNotInitialized = static_cast<int>(0x80553117u);
constexpr int kAlreadyInitialized = static_cast<int>(0x80553118u);
constexpr int kNullObject = static_cast<int>(0x80553119u);
constexpr int kNullArray = static_cast<int>(0x8055311Au);

std::atomic<bool> g_initialized{false};
Uds::Owned<NpUniversalDataSystemEventPropertyArray> g_arrays;
Uds::Owned<NpUniversalDataSystemEventPropertyObject> g_objects;

NpUniversalDataSystemEventPropertyArray* FindArray(void* value) {
    auto* array = static_cast<NpUniversalDataSystemEventPropertyArray*>(value);
    return g_arrays.Contains(array) ? array : nullptr;
}

NpUniversalDataSystemEventPropertyObject* FindObject(void* value) {
    auto* object = static_cast<NpUniversalDataSystemEventPropertyObject*>(value);
    return g_objects.Contains(object) ? object : nullptr;
}

bool ValidKey(const char* key) {
    return key != nullptr && key[0] != '\0';
}

}

extern "C" {

int APS5_VABI sceNpUniversalDataSystemIntInitialize(const NpUniversalDataSystemIntInitParam* param) {
    if (g_initialized.load()) return kAlreadyInitialized;
    if (param == nullptr) return kInvalidArgument;
    if (param->size != sizeof(NpUniversalDataSystemIntInitParam)) return kInvalidParamSize;
    if (param->memory == nullptr) return kInvalidArgument;
    g_initialized.store(true);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntTerminate(void) {
    if (!g_initialized.exchange(false)) return kNotInitialized;
    g_arrays.Clear();
    g_objects.Clear();
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntDeleteAllRecord(void) {
    if (!g_initialized.load()) return kNotInitialized;
    g_arrays.Clear();
    g_objects.Clear();
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntGetMemoryStat(NpUniversalDataSystemMemoryStat* stat) {
    if (!g_initialized.load()) return kNotInitialized;
    if (stat == nullptr) return kInvalidArgument;
    *stat = {};
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntCreateRecordArray(void** record) {
    if (!g_initialized.load()) return kNotInitialized;
    if (record == nullptr) return kInvalidArgument;
    try {
        *record = g_arrays.Adopt(std::make_unique<NpUniversalDataSystemEventPropertyArray>());
    } catch (const std::bad_alloc&) {
        return kOutOfMemory;
    }
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntCreateRecordObject(void** record) {
    if (!g_initialized.load()) return kNotInitialized;
    if (record == nullptr) return kInvalidArgument;
    try {
        *record = g_objects.Adopt(std::make_unique<NpUniversalDataSystemEventPropertyObject>());
    } catch (const std::bad_alloc&) {
        return kOutOfMemory;
    }
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntCreateRecordData(const char* recordType, void** record) {
    if (!g_initialized.load()) return kNotInitialized;
    if (!ValidKey(recordType)) return kInvalidName;
    if (record == nullptr) return kInvalidArgument;
    try {
        auto* data = g_objects.Adopt(std::make_unique<NpUniversalDataSystemEventPropertyObject>());
        data->Set("recordType", std::string(recordType));
        data->Set("context", std::make_shared<NpUniversalDataSystemEventPropertyObject>());
        *record = data;
    } catch (const std::bad_alloc&) {
        return kOutOfMemory;
    }
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntDestroyRecordArray(void* record) {
    if (!g_initialized.load()) return kNotInitialized;
    if (record != nullptr) g_arrays.Release(static_cast<NpUniversalDataSystemEventPropertyArray*>(record));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntDestroyRecordObject(void* record) {
    if (!g_initialized.load()) return kNotInitialized;
    if (record != nullptr) g_objects.Release(static_cast<NpUniversalDataSystemEventPropertyObject*>(record));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntDestroyRecordData(void* record) {
    return sceNpUniversalDataSystemIntDestroyRecordObject(record);
}

int APS5_VABI sceNpUniversalDataSystemIntRecordDataEstimateSize(const void* record, std::size_t* size) {
    if (!g_initialized.load()) return kNotInitialized;
    if (record == nullptr) return kNullRecord;
    if (size == nullptr) return kInvalidArgument;
    if (const auto* object = FindObject(const_cast<void*>(record)); object != nullptr) {
        *size = Uds::MessagePackSize(*object);
        return 0;
    }
    if (const auto* array = FindArray(const_cast<void*>(record)); array != nullptr) {
        *size = Uds::MessagePackSize(*array);
        return 0;
    }
    return kNullRecord;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetBool(void* record, bool value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetInt32(void* record, std::int32_t value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetInt64(void* record, std::int64_t value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetUInt32(void* record, std::uint32_t value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetUInt64(void* record, std::uint64_t value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetFloat32(void* record, float value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetFloat64(void* record, double value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetString(void* record, const char* value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    if (value == nullptr) return kNullValue;
    array->Add(std::string(value));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetBinary(void* record, const void* value, std::size_t size) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    if (value == nullptr) return kNullValue;
    const auto* bytes = static_cast<const std::uint8_t*>(value);
    array->Add(Uds::Binary(bytes, bytes + size));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetObject(void* record, const void* value, void** valuePtr) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    if (value == nullptr) return kNullValue;
    const auto* source = FindObject(const_cast<void*>(value));
    if (source == nullptr) return kNullObject;
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyObject>(source->Clone());
    if (valuePtr != nullptr) *valuePtr = child.get();
    array->Add(std::move(child));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetArray(void* record, const void* value, void** valuePtr) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    if (value == nullptr) return kNullValue;
    const auto* source = FindArray(const_cast<void*>(value));
    if (source == nullptr) return kNullArray;
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyArray>(source->Clone());
    if (valuePtr != nullptr) *valuePtr = child.get();
    array->Add(std::move(child));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetBool(void* record, const char* key, bool value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetInt32(void* record, const char* key, std::int32_t value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetInt64(void* record, const char* key, std::int64_t value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetUInt32(void* record, const char* key, std::uint32_t value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetUInt64(void* record, const char* key, std::uint64_t value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetFloat32(void* record, const char* key, float value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetFloat64(void* record, const char* key, double value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetString(void* record, const char* key, const char* value) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    if (value == nullptr) return kNullValue;
    object->Set(key, std::string(value));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetBinary(void* record, const char* key, const void* value, std::size_t size) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    if (value == nullptr) return kNullValue;
    const auto* bytes = static_cast<const std::uint8_t*>(value);
    object->Set(key, Uds::Binary(bytes, bytes + size));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetObject(void* record, const char* key, const void* value, void** valuePtr) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    if (value == nullptr) return kNullValue;
    const auto* source = FindObject(const_cast<void*>(value));
    if (source == nullptr) return kNullObject;
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyObject>(source->Clone());
    if (valuePtr != nullptr) *valuePtr = child.get();
    object->Set(key, std::move(child));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetArray(void* record, const char* key, const void* value, void** valuePtr) {
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    if (value == nullptr) return kNullValue;
    const auto* source = FindArray(const_cast<void*>(value));
    if (source == nullptr) return kNullArray;
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyArray>(source->Clone());
    if (valuePtr != nullptr) *valuePtr = child.get();
    object->Set(key, std::move(child));
    return 0;
}

}
