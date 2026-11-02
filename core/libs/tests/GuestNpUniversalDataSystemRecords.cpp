#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

std::size_t Estimate(void* record) {
    std::size_t size = 0;
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(record, &size) == 0, "estimate failed");
    return size;
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
    Require(sceNpUniversalDataSystemIntInitialize(&wrongSize) == kInvalidParamSize, "wrong parameter size");
    param.memory = nullptr;
    Require(sceNpUniversalDataSystemIntInitialize(&param) == kInvalidArgument, "null memory pool");
    param.memory = memory;
    Require(sceNpUniversalDataSystemIntInitialize(&param) == 0, "initialize failed");
    Require(sceNpUniversalDataSystemIntInitialize(&param) == kAlreadyInitialized, "second initialize");

    NpUniversalDataSystemMemoryStat stat{};
    Require(sceNpUniversalDataSystemIntGetMemoryStat(nullptr) == kInvalidArgument, "null memory stat");
    Require(sceNpUniversalDataSystemIntGetMemoryStat(&stat) == 0, "memory stat failed");
    Require(stat.pool_size == 0 && stat.max_inuse_size == 0 && stat.current_inuse_size == 0, "memory stat contents");

    void* array = nullptr;
    void* object = nullptr;
    void* data = nullptr;
    Require(sceNpUniversalDataSystemIntCreateRecordArray(nullptr) == kInvalidArgument, "null array output");
    Require(sceNpUniversalDataSystemIntCreateRecordArray(&array) == 0 && array != nullptr, "create array");
    Require(sceNpUniversalDataSystemIntCreateRecordObject(nullptr) == kInvalidArgument, "null object output");
    Require(sceNpUniversalDataSystemIntCreateRecordObject(&object) == 0 && object != nullptr, "create object");
    Require(sceNpUniversalDataSystemIntCreateRecordData(nullptr, &data) == kInvalidName, "null record type");
    Require(sceNpUniversalDataSystemIntCreateRecordData("", &data) == kInvalidName, "empty record type");
    Require(sceNpUniversalDataSystemIntCreateRecordData("type", nullptr) == kInvalidArgument, "null data output");
    Require(sceNpUniversalDataSystemIntCreateRecordData("type", &data) == 0 && data != nullptr, "create data");

    std::size_t size = 0;
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(nullptr, &size) == kNullRecord, "null estimate record");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(data, nullptr) == kInvalidArgument, "null estimate output");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(data, &size) == 0 && size > 0, "estimate data");
    const auto dataSize = size;

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
    Require(sceNpUniversalDataSystemIntRecordArraySetObject(array, object, &child) == 0 && child != nullptr, "array object");
    child = nullptr;
    Require(sceNpUniversalDataSystemIntRecordArraySetArray(array, nullptr, &child) == kNullValue, "null array array");
    Require(sceNpUniversalDataSystemIntRecordArraySetArray(array, array, &child) == 0 && child != nullptr, "array array");
    Require(sceNpUniversalDataSystemIntRecordArraySetInt32(nullptr, 0) == kNullArray, "null array target");
    Require(sceNpUniversalDataSystemIntRecordArraySetInt32(reinterpret_cast<void*>(1), 0) == kNullArray, "unknown array target");
    const auto arraySize = Estimate(array);
    Require(arraySize > 0, "estimate array");

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
    Require(sceNpUniversalDataSystemIntRecordObjectSetObject(object, "object", object, &child) == 0 && child != nullptr, "object object");
    child = nullptr;
    Require(sceNpUniversalDataSystemIntRecordObjectSetArray(object, "array", nullptr, &child) == kNullValue, "null object array");
    Require(sceNpUniversalDataSystemIntRecordObjectSetArray(object, "array", reinterpret_cast<void*>(1), &child) == kNullArray, "unknown object array");
    Require(sceNpUniversalDataSystemIntRecordObjectSetArray(object, "array", array, &child) == 0 && child != nullptr, "object array");
    Require(sceNpUniversalDataSystemIntRecordObjectSetInt32(nullptr, "key", 0) == kNullObject, "null object target");
    Require(sceNpUniversalDataSystemIntRecordObjectSetInt32(reinterpret_cast<void*>(1), "key", 0) == kNullObject, "unknown object target");
    const auto objectSize = Estimate(object);
    Require(objectSize > 0, "estimate object");

    Require(sceNpUniversalDataSystemIntRecordObjectSetString(data, "extra", "value") == 0, "set on record data");
    Require(Estimate(data) > dataSize, "record data estimate grew");

    Require(sceNpUniversalDataSystemIntDestroyRecordArray(nullptr) == 0, "destroy null array");
    Require(sceNpUniversalDataSystemIntDestroyRecordObject(nullptr) == 0, "destroy null object");
    Require(sceNpUniversalDataSystemIntDestroyRecordData(nullptr) == 0, "destroy null data");
    Require(sceNpUniversalDataSystemIntDestroyRecordArray(array) == 0, "destroy array");
    Require(sceNpUniversalDataSystemIntDestroyRecordObject(object) == 0, "destroy object");
    Require(sceNpUniversalDataSystemIntDestroyRecordData(data) == 0, "destroy data");
    std::size_t unused = 0;
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(array, &unused) == kNullRecord, "destroyed array estimate");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(object, &unused) == kNullRecord, "destroyed object estimate");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(data, &unused) == kNullRecord, "destroyed data estimate");

    Require(sceNpUniversalDataSystemIntCreateRecordArray(&array) == 0, "recreate array");
    Require(sceNpUniversalDataSystemIntCreateRecordObject(&object) == 0, "recreate object");
    Require(sceNpUniversalDataSystemIntDeleteAllRecord() == 0, "delete all records");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(array, &unused) == kNullRecord, "deleted array estimate");
    Require(sceNpUniversalDataSystemIntRecordDataEstimateSize(object, &unused) == kNullRecord, "deleted object estimate");

    Require(sceNpUniversalDataSystemIntTerminate() == 0, "terminate failed");
    Require(sceNpUniversalDataSystemIntTerminate() == kNotInitialized, "second terminate");
    Require(sceNpUniversalDataSystemIntCreateRecordArray(&array) == kNotInitialized, "create after terminate");
    Require(sceNpUniversalDataSystemIntInitialize(&param) == 0, "reinitialize");
    Require(sceNpUniversalDataSystemIntTerminate() == 0, "final terminate");
}
