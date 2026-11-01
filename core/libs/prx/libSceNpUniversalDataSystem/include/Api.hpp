#pragma once

#include "prx/libSceNpUniversalDataSystem/include/UdsEvent.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemIntAbortHandle(void);
int APS5_VABI sceNpUniversalDataSystemIntCheckNetSyncTitles(void);
int APS5_VABI sceNpUniversalDataSystemIntCreateContext(void);
int APS5_VABI sceNpUniversalDataSystemIntCreateHandle(void);
int APS5_VABI sceNpUniversalDataSystemIntDeleteAllData(void);
int APS5_VABI sceNpUniversalDataSystemIntDeleteUserDataWithServerData(void);
int APS5_VABI sceNpUniversalDataSystemIntDestroyContext(void);
int APS5_VABI sceNpUniversalDataSystemIntDestroyHandle(void);
int APS5_VABI sceNpUniversalDataSystemIntInitialize(void);
int APS5_VABI sceNpUniversalDataSystemIntNetSyncTitles(void);
int APS5_VABI sceNpUniversalDataSystemIntTerminate(void);
int APS5_VABI sceNpUniversalDataSystemCreateContext(int* context, int userId, uint32_t serviceLabel, uint64_t options);
int APS5_VABI sceNpUniversalDataSystemDestroyContext(int context);
int APS5_VABI sceNpUniversalDataSystemRegisterContext(int context, int handle, uint64_t options);
int APS5_VABI sceNpUniversalDataSystemCreateEvent( const char* eventName, const NpUniversalDataSystemEventPropertyObject* prop, NpUniversalDataSystemEvent** newEvent, NpUniversalDataSystemEventPropertyObject** propPtr);
int APS5_VABI sceNpUniversalDataSystemDestroyEvent(NpUniversalDataSystemEvent* event);
int APS5_VABI sceNpUniversalDataSystemPostEvent(int context, int handle, const void* event, uint64_t options);
int APS5_VABI sceNpUniversalDataSystemEventEstimateSize(const NpUniversalDataSystemEvent* event, size_t* size);
int APS5_VABI sceNpUniversalDataSystemEventToString( const NpUniversalDataSystemEvent* event, char* buf, size_t bufSize, size_t* stringSize);
int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyArray(NpUniversalDataSystemEventPropertyArray** newArray);
int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyArray(NpUniversalDataSystemEventPropertyArray* array);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetString( NpUniversalDataSystemEventPropertyArray* array, const char* value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetInt32( NpUniversalDataSystemEventPropertyArray* array, int32_t value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetUInt32( NpUniversalDataSystemEventPropertyArray* array, uint32_t value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetInt64( NpUniversalDataSystemEventPropertyArray* array, int64_t value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetUInt64( NpUniversalDataSystemEventPropertyArray* array, uint64_t value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetFloat32( NpUniversalDataSystemEventPropertyArray* array, float value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetFloat64( NpUniversalDataSystemEventPropertyArray* array, double value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetBool( NpUniversalDataSystemEventPropertyArray* array, bool value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetBinary( NpUniversalDataSystemEventPropertyArray* array, const void* value, size_t valueSize);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetObject( NpUniversalDataSystemEventPropertyArray* array, const NpUniversalDataSystemEventPropertyObject* value, NpUniversalDataSystemEventPropertyObject** valuePtr);
int APS5_VABI sceNpUniversalDataSystemEventPropertyArraySetArray( NpUniversalDataSystemEventPropertyArray* array, const NpUniversalDataSystemEventPropertyArray* value, NpUniversalDataSystemEventPropertyArray** valuePtr);
int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyObject(NpUniversalDataSystemEventPropertyObject** newObject);
int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyObject(NpUniversalDataSystemEventPropertyObject* object);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetString( NpUniversalDataSystemEventPropertyObject* object, const char* key, const char* value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetInt32( NpUniversalDataSystemEventPropertyObject* object, const char* key, int32_t value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetUInt32( NpUniversalDataSystemEventPropertyObject* object, const char* key, uint32_t value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetInt64( NpUniversalDataSystemEventPropertyObject* object, const char* key, int64_t value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetUInt64( NpUniversalDataSystemEventPropertyObject* object, const char* key, uint64_t value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetFloat32( NpUniversalDataSystemEventPropertyObject* object, const char* key, float value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetFloat64( NpUniversalDataSystemEventPropertyObject* object, const char* key, double value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetBool( NpUniversalDataSystemEventPropertyObject* object, const char* key, bool value);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetBinary( NpUniversalDataSystemEventPropertyObject* object, const char* key, const void* value, size_t valueSize);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetObject( NpUniversalDataSystemEventPropertyObject* object, const char* key, const NpUniversalDataSystemEventPropertyObject* value, NpUniversalDataSystemEventPropertyObject** valuePtr);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetArray( NpUniversalDataSystemEventPropertyObject* object, const char* key, const NpUniversalDataSystemEventPropertyArray* value, NpUniversalDataSystemEventPropertyArray** valuePtr);
int APS5_VABI sceNpUniversalDataSystemCreateHandle(int* handle);
int APS5_VABI sceNpUniversalDataSystemDestroyHandle(int handle);
int APS5_VABI sceNpUniversalDataSystemAbortHandle(int handle);
int APS5_VABI sceNpUniversalDataSystemInitialize(const NpUniversalDataSystemInitParam* param);
int APS5_VABI sceNpUniversalDataSystemTerminate(void);
int APS5_VABI sceNpUniversalDataSystemGetMemoryStat(NpUniversalDataSystemMemoryStat* stat);
int APS5_VABI sceNpUniversalDataSystemGetStorageStat(int context, NpUniversalDataSystemStorageStat* stat);

int APS5_VABI sceNpUniversalDataSystemIntCreateRecordArray();
int APS5_VABI sceNpUniversalDataSystemIntCreateRecordData();
int APS5_VABI sceNpUniversalDataSystemIntCreateRecordObject();
int APS5_VABI sceNpUniversalDataSystemIntDeleteAllRecord();
int APS5_VABI sceNpUniversalDataSystemIntDestroyRecordArray();
int APS5_VABI sceNpUniversalDataSystemIntDestroyRecordData();
int APS5_VABI sceNpUniversalDataSystemIntDestroyRecordObject();
int APS5_VABI sceNpUniversalDataSystemIntGetMemoryStat();
int APS5_VABI sceNpUniversalDataSystemIntPostRecordData();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetArray();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetBinary();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetBool();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetFloat32();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetFloat64();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetInt32();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetInt64();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetObject();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetString();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetUInt32();
int APS5_VABI sceNpUniversalDataSystemIntRecordArraySetUInt64();
int APS5_VABI sceNpUniversalDataSystemIntRecordDataEstimateSize();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetArray();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetBinary();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetBool();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetFloat32();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetFloat64();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetInt32();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetInt64();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetObject();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetString();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetUInt32();
int APS5_VABI sceNpUniversalDataSystemIntRecordObjectSetUInt64();

}
