#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include "prx/libSceNpUniversalDataSystem/include/UdsEvent.hpp"
#include "NpUniversalDataSystem.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
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

std::recursive_mutex g_recordsMutex;
std::atomic<bool> g_initialized{false};
Uds::Owned<NpUniversalDataSystemEventPropertyArray> g_arrays;
Uds::Owned<NpUniversalDataSystemEventPropertyObject> g_objects;

bool ValueContainsObject(const Uds::Value& value, const void* target);

bool ObjectContainsObject(const NpUniversalDataSystemEventPropertyObject* object, const void* target) {
    if (object == target) return true;
    for (const auto& [key, value] : object->properties) {
        if (ValueContainsObject(value, target)) return true;
    }
    return false;
}

bool ArrayContainsObject(const NpUniversalDataSystemEventPropertyArray* array, const void* target) {
    for (const auto& value : array->values) {
        if (ValueContainsObject(value, target)) return true;
    }
    return false;
}

bool ValueContainsObject(const Uds::Value& value, const void* target) {
    if (const auto* object = std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyObject>>(&value)) {
        return ObjectContainsObject(object->get(), target);
    }
    if (const auto* array = std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyArray>>(&value)) {
        return ArrayContainsObject(array->get(), target);
    }
    return false;
}

bool ValueContainsArray(const Uds::Value& value, const void* target);

bool ArrayContainsArray(const NpUniversalDataSystemEventPropertyArray* array, const void* target) {
    if (array == target) return true;
    for (const auto& value : array->values) {
        if (ValueContainsArray(value, target)) return true;
    }
    return false;
}

bool ObjectContainsArray(const NpUniversalDataSystemEventPropertyObject* object, const void* target) {
    for (const auto& [key, value] : object->properties) {
        if (ValueContainsArray(value, target)) return true;
    }
    return false;
}

bool ValueContainsArray(const Uds::Value& value, const void* target) {
    if (const auto* object = std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyObject>>(&value)) {
        return ObjectContainsArray(object->get(), target);
    }
    if (const auto* array = std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyArray>>(&value)) {
        return ArrayContainsArray(array->get(), target);
    }
    return false;
}

NpUniversalDataSystemEventPropertyArray* FindArray(void* value) {
    auto* array = static_cast<NpUniversalDataSystemEventPropertyArray*>(value);
    if (array == nullptr) return nullptr;
    bool found = false;
    g_arrays.ForEach([&](auto* root) {
        if (!found && ArrayContainsArray(root, array)) found = true;
    });
    if (!found) {
        g_objects.ForEach([&](auto* root) {
            if (!found && ObjectContainsArray(root, array)) found = true;
        });
    }
    return found ? array : nullptr;
}

NpUniversalDataSystemEventPropertyObject* FindObject(void* value) {
    auto* object = static_cast<NpUniversalDataSystemEventPropertyObject*>(value);
    if (object == nullptr) return nullptr;
    bool found = false;
    g_objects.ForEach([&](auto* root) {
        if (!found && ObjectContainsObject(root, object)) found = true;
    });
    if (!found) {
        g_arrays.ForEach([&](auto* root) {
            if (!found && ArrayContainsObject(root, object)) found = true;
        });
    }
    return found ? object : nullptr;
}

bool ValidKey(const char* key) {
    return key != nullptr && key[0] != '\0';
}

}

extern "C" {

int APS5_VABI sceNpUniversalDataSystemIntInitialize(const NpUniversalDataSystemIntInitParam* param) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (g_initialized.load()) return kAlreadyInitialized;
    if (param == nullptr) return kInvalidArgument;
    NpUniversalDataSystemIntInitParam copy{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&copy, param, sizeof(copy));
    errno = savedErrno;
    if (!copied) return kInvalidArgument;
    if (copy.size != sizeof(NpUniversalDataSystemIntInitParam)) return kInvalidParamSize;
    if (copy.memory == nullptr) return kInvalidArgument;
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) return kAlreadyInitialized;
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntTerminate(void) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.exchange(false)) return kNotInitialized;
    g_arrays.Clear();
    g_objects.Clear();
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntDeleteAllRecord(void) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    g_arrays.Clear();
    g_objects.Clear();
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntGetMemoryStat(NpUniversalDataSystemMemoryStat* stat) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    if (stat == nullptr) return kInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpUniversalDataSystemIntCreateRecordArray(void** record) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    if (record == nullptr) return kInvalidArgument;
    void* created = nullptr;
    try {
        created = g_arrays.Adopt(std::make_unique<NpUniversalDataSystemEventPropertyArray>());
    } catch (const std::bad_alloc&) {
        return kOutOfMemory;
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(record, &created, sizeof(created));
    errno = savedErrno;
    if (!copied) {
        g_arrays.Release(static_cast<NpUniversalDataSystemEventPropertyArray*>(created));
        return kInvalidArgument;
    }
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntCreateRecordObject(void** record) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    if (record == nullptr) return kInvalidArgument;
    void* created = nullptr;
    try {
        created = g_objects.Adopt(std::make_unique<NpUniversalDataSystemEventPropertyObject>());
    } catch (const std::bad_alloc&) {
        return kOutOfMemory;
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(record, &created, sizeof(created));
    errno = savedErrno;
    if (!copied) {
        g_objects.Release(static_cast<NpUniversalDataSystemEventPropertyObject*>(created));
        return kInvalidArgument;
    }
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntCreateRecordData(const char* recordType, void** record) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    if (!ValidKey(recordType)) return kInvalidName;
    if (record == nullptr) return kInvalidArgument;
    NpUniversalDataSystemEventPropertyObject* data = nullptr;
    try {
        data = g_objects.Adopt(std::make_unique<NpUniversalDataSystemEventPropertyObject>());
        data->Set("recordType", std::string(recordType));
        data->Set("context", std::make_shared<NpUniversalDataSystemEventPropertyObject>());
    } catch (const std::bad_alloc&) {
        return kOutOfMemory;
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(record, &data, sizeof(data));
    errno = savedErrno;
    if (!copied) {
        g_objects.Release(data);
        return kInvalidArgument;
    }
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntDestroyRecordArray(void* record) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    if (record != nullptr) g_arrays.Release(static_cast<NpUniversalDataSystemEventPropertyArray*>(record));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntDestroyRecordObject(void* record) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    if (record != nullptr) g_objects.Release(static_cast<NpUniversalDataSystemEventPropertyObject*>(record));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntDestroyRecordData(void* record) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    return sceNpUniversalDataSystemIntDestroyRecordObject(record);
}

int APS5_VABI sceNpUniversalDataSystemIntRecordDataEstimateSize(const void* record, std::size_t* size) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    if (record == nullptr) return kNullRecord;
    if (size == nullptr) return kInvalidArgument;
    if (FindObject(const_cast<void*>(record)) == nullptr && FindArray(const_cast<void*>(record)) == nullptr) {
        return kNullRecord;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetBool(void* record, bool value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetInt32(void* record, std::int32_t value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetInt64(void* record, std::int64_t value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetUInt32(void* record, std::uint32_t value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetUInt64(void* record, std::uint64_t value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetFloat32(void* record, float value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetFloat64(void* record, double value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    array->Add(value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetString(void* record, const char* value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    if (value == nullptr) return kNullValue;
    array->Add(std::string(value));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetBinary(void* record, const void* value, std::size_t size) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    if (value == nullptr) return kNullValue;
    const auto* bytes = static_cast<const std::uint8_t*>(value);
    array->Add(Uds::Binary(bytes, bytes + size));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetObject(void* record, const void* value, void** valuePtr) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    if (value == nullptr) return kNullValue;
    const auto* source = FindObject(const_cast<void*>(value));
    if (source == nullptr) return kNullObject;
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyObject>(source->Clone());
    if (valuePtr != nullptr) {
        auto* pointer = child.get();
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(valuePtr, &pointer, sizeof(pointer));
        errno = savedErrno;
        if (!copied) return kInvalidArgument;
    }
    array->Add(std::move(child));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetArray(void* record, const void* value, void** valuePtr) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* array = FindArray(record);
    if (array == nullptr) return kNullArray;
    if (value == nullptr) return kNullValue;
    const auto* source = FindArray(const_cast<void*>(value));
    if (source == nullptr) return kNullArray;
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyArray>(source->Clone());
    if (valuePtr != nullptr) {
        auto* pointer = child.get();
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(valuePtr, &pointer, sizeof(pointer));
        errno = savedErrno;
        if (!copied) return kInvalidArgument;
    }
    array->Add(std::move(child));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetBool(void* record, const char* key, bool value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetInt32(void* record, const char* key, std::int32_t value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetInt64(void* record, const char* key, std::int64_t value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetUInt32(void* record, const char* key, std::uint32_t value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetUInt64(void* record, const char* key, std::uint64_t value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetFloat32(void* record, const char* key, float value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetFloat64(void* record, const char* key, double value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    object->Set(key, value);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetString(void* record, const char* key, const char* value) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    if (value == nullptr) return kNullValue;
    object->Set(key, std::string(value));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetBinary(void* record, const char* key, const void* value, std::size_t size) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
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
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    if (value == nullptr) return kNullValue;
    const auto* source = FindObject(const_cast<void*>(value));
    if (source == nullptr) return kNullObject;
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyObject>(source->Clone());
    if (valuePtr != nullptr) {
        auto* pointer = child.get();
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(valuePtr, &pointer, sizeof(pointer));
        errno = savedErrno;
        if (!copied) return kInvalidArgument;
    }
    object->Set(key, std::move(child));
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetArray(void* record, const char* key, const void* value, void** valuePtr) {
    std::lock_guard<std::recursive_mutex> recordsLock(g_recordsMutex);
    if (!g_initialized.load()) return kNotInitialized;
    auto* object = FindObject(record);
    if (object == nullptr) return kNullObject;
    if (!ValidKey(key)) return kInvalidKey;
    if (value == nullptr) return kNullValue;
    const auto* source = FindArray(const_cast<void*>(value));
    if (source == nullptr) return kNullArray;
    auto child = std::make_shared<NpUniversalDataSystemEventPropertyArray>(source->Clone());
    if (valuePtr != nullptr) {
        auto* pointer = child.get();
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(valuePtr, &pointer, sizeof(pointer));
        errno = savedErrno;
        if (!copied) return kInvalidArgument;
    }
    object->Set(key, std::move(child));
    return 0;
}

}
