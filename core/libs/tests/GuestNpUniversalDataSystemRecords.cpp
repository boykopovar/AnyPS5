#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

namespace {

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

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpUniversalDataSystem records: %s\n", message);
        std::abort();
    }
}

const std::shared_ptr<NpUniversalDataSystemEventPropertyObject>* StoredObject(void* record, const char* key) {
    const auto* object = static_cast<NpUniversalDataSystemEventPropertyObject*>(record);
    const auto* value = object->Find(key);
    return value != nullptr ? std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyObject>>(value) : nullptr;
}

const std::shared_ptr<NpUniversalDataSystemEventPropertyArray>* StoredArray(void* record, const char* key) {
    const auto* object = static_cast<NpUniversalDataSystemEventPropertyObject*>(record);
    const auto* value = object->Find(key);
    return value != nullptr ? std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyArray>>(value) : nullptr;
}

}

int main() {
    std::uint8_t memory[16] = {};
    NpUniversalDataSystemIntInitParam param{sizeof(NpUniversalDataSystemIntInitParam), memory};
    NpUniversalDataSystemIntInitParam wrongSize{0, memory};

    Require(sceNpUniversalDataSystemIntTerminate() == kNotInitialized, "terminate before initialize");
    Require(sceNpUniversalDataSystemIntDeleteAllRecord() == kNotInitialized, "delete records before initialize");
    Require(sceNpUniversalDataSystemIntGetMemoryStat(nullptr) == kNotInitialized, "memory stat before initialize");
    Require(sceNpUniversalDataSystemIntCreateRecordArray(nullptr) == kNotInitialized, "create array before initialize");
    Require(sceNpUniversalDataSystemIntInitialize(nullptr) == kInvalidArgument, "null initialize parameter");
    errno = EBUSY;
    Require(sceNpUniversalDataSystemIntInitialize(reinterpret_cast<const NpUniversalDataSystemIntInitParam*>(std::uintptr_t{0x10})) == kInvalidArgument,
            "unreadable initialize parameter");
    Require(errno == EBUSY, "unreadable initialize parameter did not keep errno");
    Require(sceNpUniversalDataSystemIntInitialize(&wrongSize) == kInvalidParamSize, "wrong parameter size");
    param.memory = nullptr;
    Require(sceNpUniversalDataSystemIntInitialize(&param) == kInvalidArgument, "null memory pool");
    param.memory = memory;
    Require(sceNpUniversalDataSystemIntInitialize(&param) == 0, "initialize failed");
    Require(sceNpUniversalDataSystemIntInitialize(&param) == kAlreadyInitialized, "second initialize");
    Require(sceNpUniversalDataSystemIntGetMemoryStat(nullptr) == kInvalidArgument, "null memory stat");

    void* array = nullptr;
    void* object = nullptr;
    void* data = nullptr;
    Require(sceNpUniversalDataSystemIntCreateRecordArray(nullptr) == kInvalidArgument, "null array output");
    errno = EBUSY;
    Require(sceNpUniversalDataSystemIntCreateRecordArray(reinterpret_cast<void**>(std::uintptr_t{0x10})) == kInvalidArgument, "unwritable array output");
    Require(errno == EBUSY, "unwritable array output did not keep errno");
    Require(sceNpUniversalDataSystemIntCreateRecordArray(&array) == 0 && array != nullptr, "create array");
    Require(sceNpUniversalDataSystemIntCreateRecordObject(nullptr) == kInvalidArgument, "null object output");
    errno = EBUSY;
    Require(sceNpUniversalDataSystemIntCreateRecordObject(reinterpret_cast<void**>(std::uintptr_t{0x10})) == kInvalidArgument, "unwritable object output");
    Require(errno == EBUSY, "unwritable object output did not keep errno");
    Require(sceNpUniversalDataSystemIntCreateRecordObject(&object) == 0 && object != nullptr, "create object");
    Require(sceNpUniversalDataSystemIntCreateRecordData(nullptr, &data) == kInvalidName, "null record type");
    Require(sceNpUniversalDataSystemIntCreateRecordData("", &data) == kInvalidName, "empty record type");
    Require(sceNpUniversalDataSystemIntCreateRecordData("type", nullptr) == kInvalidArgument, "null data output");
    errno = EBUSY;
    Require(sceNpUniversalDataSystemIntCreateRecordData("type", reinterpret_cast<void**>(std::uintptr_t{0x10})) == kInvalidArgument, "unwritable data output");
    Require(errno == EBUSY, "unwritable data output did not keep errno");
    Require(sceNpUniversalDataSystemIntCreateRecordData("type", &data) == 0 && data != nullptr, "create data");

    std::size_t size = 0;
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(nullptr, &size) == kNullRecord, "null estimate record");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(data, nullptr) == kInvalidArgument, "null estimate output");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(data, &size) == 0 && size > 0, "estimate record data");
    std::size_t emptyArraySize = 0;
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(array, &emptyArraySize) == 0 && emptyArraySize > 0, "estimate empty array");

    Require(sceNpUniversalDataSystemIntRecordArraySetBool(array, true) == 0, "array bool");
    Require(sceNpUniversalDataSystemIntRecordArraySetInt32(array, -1) == 0, "array int32");
    Require(sceNpUniversalDataSystemIntRecordArraySetInt64(array, -2) == 0, "array int64");
    Require(sceNpUniversalDataSystemIntRecordArraySetUInt32(array, 3) == 0, "array uint32");
    Require(sceNpUniversalDataSystemIntRecordArraySetUInt64(array, 4) == 0, "array uint64");
    Require(sceNpUniversalDataSystemIntRecordArraySetFloat32(array, 1.5f) == 0, "array float32");
    Require(sceNpUniversalDataSystemIntRecordArraySetFloat64(array, 2.5) == 0, "array float64");
    Require(sceNpUniversalDataSystemIntRecordArraySetString(array, "value") == 0, "array string");
    Require(sceNpUniversalDataSystemIntRecordArraySetString(array, nullptr) == kNullValue, "null array string");
    const std::uint8_t bytes[3] = {1, 2, 3};
    Require(sceNpUniversalDataSystemIntRecordArraySetBinary(array, bytes, sizeof(bytes)) == 0, "array binary");
    Require(sceNpUniversalDataSystemIntRecordArraySetBinary(array, nullptr, 0) == kNullValue, "null array binary");

    void* child = nullptr;
    Require(sceNpUniversalDataSystemIntRecordArraySetObject(array, nullptr, &child) == kNullValue, "null array object");
    Require(sceNpUniversalDataSystemIntRecordArraySetObject(array, reinterpret_cast<void*>(1), &child) == kNullObject, "unknown array object");
    const std::size_t sizeBeforeUnwritableArrayObject = static_cast<NpUniversalDataSystemEventPropertyArray*>(array)->values.size();
    errno = EBUSY;
    Require(sceNpUniversalDataSystemIntRecordArraySetObject(array, object, reinterpret_cast<void**>(std::uintptr_t{0x10})) == kInvalidArgument,
            "unwritable array object output");
    Require(errno == EBUSY, "unwritable array object output did not keep errno");
    Require(static_cast<NpUniversalDataSystemEventPropertyArray*>(array)->values.size() == sizeBeforeUnwritableArrayObject,
            "a rejected array object copy was still added");
    Require(sceNpUniversalDataSystemIntRecordArraySetObject(array, object, &child) == 0 && child != nullptr, "array object");
    const auto* storedArrayObject = static_cast<NpUniversalDataSystemEventPropertyArray*>(array);
    Require(storedArrayObject->values.size() >= 1, "array object stored");
    const auto* storedObject = std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyObject>>(&storedArrayObject->values.back());
    Require(storedObject != nullptr && storedObject->get() == child, "array object child pointer");
    Require(sceNpUniversalDataSystemIntRecordObjectSetInt32(child, "nested", 7) == 0, "set on array child");
    Require((*storedObject)->Find("nested") != nullptr, "array child mutation stored");
    Require(static_cast<NpUniversalDataSystemEventPropertyObject*>(object)->Find("nested") == nullptr, "array child clone independent");

    void* arrayChild = nullptr;
    Require(sceNpUniversalDataSystemIntRecordArraySetArray(array, nullptr, &arrayChild) == kNullValue, "null array array");
    const std::size_t sizeBeforeUnwritableArrayArray = static_cast<NpUniversalDataSystemEventPropertyArray*>(array)->values.size();
    errno = EBUSY;
    Require(sceNpUniversalDataSystemIntRecordArraySetArray(array, array, reinterpret_cast<void**>(std::uintptr_t{0x10})) == kInvalidArgument,
            "unwritable array array output");
    Require(errno == EBUSY, "unwritable array array output did not keep errno");
    Require(static_cast<NpUniversalDataSystemEventPropertyArray*>(array)->values.size() == sizeBeforeUnwritableArrayArray,
            "a rejected array array copy was still added");
    Require(sceNpUniversalDataSystemIntRecordArraySetArray(array, array, &arrayChild) == 0 && arrayChild != nullptr, "array array");
    const auto* storedNestedArray = std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyArray>>(&storedArrayObject->values.back());
    Require(storedNestedArray != nullptr && storedNestedArray->get() == arrayChild, "array array child pointer");
    Require(sceNpUniversalDataSystemIntRecordArraySetInt32(arrayChild, 8) == 0, "set on array child array");
    Require((*storedNestedArray)->values.size() >= 1, "array child array mutation stored");
    Require(sceNpUniversalDataSystemIntRecordArraySetInt32(nullptr, 0) == kNullArray, "null array target");
    Require(sceNpUniversalDataSystemIntRecordArraySetInt32(reinterpret_cast<void*>(1), 0) == kNullArray, "unknown array target");
    std::size_t fullArraySize = 0;
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(array, &fullArraySize) == 0 && fullArraySize > emptyArraySize,
            "estimate grows with array values");

    Require(sceNpUniversalDataSystemIntRecordObjectSetBool(object, nullptr, true) == kInvalidKey, "null object key");
    Require(sceNpUniversalDataSystemIntRecordObjectSetBool(object, "", true) == kInvalidKey, "empty object key");
    Require(sceNpUniversalDataSystemIntRecordObjectSetBool(object, "bool", true) == 0, "object bool");
    Require(sceNpUniversalDataSystemIntRecordObjectSetInt32(object, "int32", -1) == 0, "object int32");
    Require(sceNpUniversalDataSystemIntRecordObjectSetInt64(object, "int64", -2) == 0, "object int64");
    Require(sceNpUniversalDataSystemIntRecordObjectSetUInt32(object, "uint32", 3) == 0, "object uint32");
    Require(sceNpUniversalDataSystemIntRecordObjectSetUInt64(object, "uint64", 4) == 0, "object uint64");
    Require(sceNpUniversalDataSystemIntRecordObjectSetFloat32(object, "float32", 1.5f) == 0, "object float32");
    Require(sceNpUniversalDataSystemIntRecordObjectSetFloat64(object, "float64", 2.5) == 0, "object float64");
    Require(sceNpUniversalDataSystemIntRecordObjectSetString(object, "string", "value") == 0, "object string");
    Require(sceNpUniversalDataSystemIntRecordObjectSetString(object, "string", nullptr) == kNullValue, "null object string");
    Require(sceNpUniversalDataSystemIntRecordObjectSetBinary(object, "binary", bytes, sizeof(bytes)) == 0, "object binary");
    Require(sceNpUniversalDataSystemIntRecordObjectSetBinary(object, "binary", nullptr, 0) == kNullValue, "null object binary");

    child = nullptr;
    Require(sceNpUniversalDataSystemIntRecordObjectSetObject(object, "object", nullptr, &child) == kNullValue, "null object object");
    Require(sceNpUniversalDataSystemIntRecordObjectSetObject(object, "object", reinterpret_cast<void*>(1), &child) == kNullObject, "unknown object object");
    const std::size_t sizeBeforeUnwritableObjectObject = static_cast<NpUniversalDataSystemEventPropertyObject*>(object)->properties.size();
    errno = EBUSY;
    Require(sceNpUniversalDataSystemIntRecordObjectSetObject(object, "object", object, reinterpret_cast<void**>(std::uintptr_t{0x10})) == kInvalidArgument,
            "unwritable object object output");
    Require(errno == EBUSY, "unwritable object object output did not keep errno");
    Require(static_cast<NpUniversalDataSystemEventPropertyObject*>(object)->properties.size() == sizeBeforeUnwritableObjectObject,
            "a rejected object object copy was still added");
    Require(sceNpUniversalDataSystemIntRecordObjectSetObject(object, "object", object, &child) == 0 && child != nullptr, "object object");
    const auto* storedChild = StoredObject(object, "object");
    Require(storedChild != nullptr && storedChild->get() == child, "object child pointer");
    Require(sceNpUniversalDataSystemIntRecordObjectSetInt32(child, "nested", 9) == 0, "set on object child");
    Require((*storedChild)->Find("nested") != nullptr, "object child mutation stored");
    Require(static_cast<NpUniversalDataSystemEventPropertyObject*>(object)->Find("nested") == nullptr, "object child clone independent");

    void* objectArrayChild = nullptr;
    Require(sceNpUniversalDataSystemIntRecordObjectSetArray(object, "array", nullptr, &objectArrayChild) == kNullValue, "null object array");
    Require(sceNpUniversalDataSystemIntRecordObjectSetArray(object, "array", reinterpret_cast<void*>(1), &objectArrayChild) == kNullArray, "unknown object array");
    const std::size_t sizeBeforeUnwritableObjectArray = static_cast<NpUniversalDataSystemEventPropertyObject*>(object)->properties.size();
    errno = EBUSY;
    Require(sceNpUniversalDataSystemIntRecordObjectSetArray(object, "array", array, reinterpret_cast<void**>(std::uintptr_t{0x10})) == kInvalidArgument,
            "unwritable object array output");
    Require(errno == EBUSY, "unwritable object array output did not keep errno");
    Require(static_cast<NpUniversalDataSystemEventPropertyObject*>(object)->properties.size() == sizeBeforeUnwritableObjectArray,
            "a rejected object array copy was still added");
    Require(sceNpUniversalDataSystemIntRecordObjectSetArray(object, "array", array, &objectArrayChild) == 0 && objectArrayChild != nullptr, "object array");
    const auto* storedChildArray = StoredArray(object, "array");
    Require(storedChildArray != nullptr && storedChildArray->get() == objectArrayChild, "object child array pointer");
    Require(sceNpUniversalDataSystemIntRecordArraySetInt32(objectArrayChild, 10) == 0, "set on object child array");
    Require((*storedChildArray)->values.size() >= 1, "object child array mutation stored");
    Require(sceNpUniversalDataSystemIntRecordObjectSetInt32(nullptr, "key", 0) == kNullObject, "null object target");
    Require(sceNpUniversalDataSystemIntRecordObjectSetInt32(reinterpret_cast<void*>(1), "key", 0) == kNullObject, "unknown object target");

    Require(sceNpUniversalDataSystemIntRecordObjectSetString(data, "extra", "value") == 0, "set on record data");
    Require(static_cast<NpUniversalDataSystemEventPropertyObject*>(data)->Find("extra") != nullptr, "record data mutation stored");

    void* destroyedChild = nullptr;
    Require(sceNpUniversalDataSystemIntRecordObjectSetObject(data, "child", object, &destroyedChild) == 0, "destroyed child setup");
    Require(sceNpUniversalDataSystemIntDestroyRecordData(data) == 0, "destroy data");
    Require(sceNpUniversalDataSystemIntRecordObjectSetInt32(destroyedChild, "key", 0) == kNullObject, "destroyed owner invalidates child");
    Require(sceNpUniversalDataSystemIntDestroyRecordObject(destroyedChild) == 0, "destroy child is a no-op");

    Require(sceNpUniversalDataSystemIntDestroyRecordArray(nullptr) == 0, "destroy null array");
    Require(sceNpUniversalDataSystemIntDestroyRecordObject(nullptr) == 0, "destroy null object");
    Require(sceNpUniversalDataSystemIntDestroyRecordData(nullptr) == 0, "destroy null data");
    Require(sceNpUniversalDataSystemIntDestroyRecordArray(array) == 0, "destroy array");
    Require(sceNpUniversalDataSystemIntDestroyRecordObject(object) == 0, "destroy object");
    std::size_t unused = 0;
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(array, &unused) == kNullRecord, "destroyed array estimate");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(object, &unused) == kNullRecord, "destroyed object estimate");

    Require(sceNpUniversalDataSystemIntCreateRecordArray(&array) == 0, "recreate array");
    Require(sceNpUniversalDataSystemIntCreateRecordObject(&object) == 0, "recreate object");
    Require(sceNpUniversalDataSystemIntDeleteAllRecord() == 0, "delete all records");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(array, &unused) == kNullRecord, "deleted array estimate");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(object, &unused) == kNullRecord, "deleted object estimate");

    for (int round = 0; round < 32; ++round) {
        Require(sceNpUniversalDataSystemIntTerminate() == 0, "contention terminate");
        std::atomic<bool> go{false};
        std::atomic<int> succeeded{0};
        std::atomic<int> rejected{0};
        std::vector<std::thread> threads;
        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&] {
                while (!go.load()) std::this_thread::yield();
                const int result = sceNpUniversalDataSystemIntInitialize(&param);
                if (result == 0) ++succeeded;
                if (result == kAlreadyInitialized) ++rejected;
            });
        }
        go.store(true);
        for (auto& thread : threads) thread.join();
        Require(succeeded.load() == 1, "one initializer wins");
        Require(rejected.load() == 7, "other initializers rejected");
    }
    Require(sceNpUniversalDataSystemIntTerminate() == 0, "terminate after contention");
    Require(sceNpUniversalDataSystemIntInitialize(&param) == 0, "reinitialize after contention");

    for (int round = 0; round < 64; ++round) {
        void* owner = nullptr;
        Require(sceNpUniversalDataSystemIntCreateRecordObject(&owner) == 0, "lifetime owner");
        void* lifetimeChild = nullptr;
        Require(sceNpUniversalDataSystemIntRecordObjectSetObject(owner, "child", owner, &lifetimeChild) == 0, "lifetime child");
        std::atomic<bool> go{false};
        std::atomic<bool> stop{false};
        std::atomic<int> unexpected{0};
        std::thread worker([&] {
            while (!go.load()) std::this_thread::yield();
            while (!stop.load()) {
                const int result = sceNpUniversalDataSystemIntRecordObjectSetInt32(lifetimeChild, "key", 1);
                if (result != 0 && result != kNullObject) ++unexpected;
            }
        });
        go.store(true);
        for (int i = 0; i < 100; ++i) std::this_thread::yield();
        Require(sceNpUniversalDataSystemIntDestroyRecordObject(owner) == 0, "destroy owner");
        stop.store(true);
        worker.join();
        Require(unexpected.load() == 0, "operation against destruction");
        Require(sceNpUniversalDataSystemIntRecordObjectSetInt32(lifetimeChild, "key", 1) == kNullObject, "child invalid after destruction");
    }

    Require(sceNpUniversalDataSystemIntTerminate() == 0, "terminate failed");
    Require(sceNpUniversalDataSystemIntTerminate() == kNotInitialized, "second terminate");
    Require(sceNpUniversalDataSystemIntCreateRecordArray(&array) == kNotInitialized, "create after terminate");
    Require(sceNpUniversalDataSystemIntInitialize(&param) == 0, "reinitialize");
    Require(sceNpUniversalDataSystemIntTerminate() == 0, "final terminate");
}
