#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstddef>
#include <cstdint>

namespace NKWebKit {

struct OpaqueJSContextGroup;
struct OpaqueJSContext;
struct OpaqueJSClass;
struct OpaqueJSValue;
struct OpaqueJSPropertyNameArray;
struct OpaqueJSPropertyNameAccumulator;
struct OpaqueJSScript;
struct OpaqueJSWeak;
struct OpaqueJSWeakObjectMap;

using JSContextGroupRef = const OpaqueJSContextGroup*;
using JSContextRef = const OpaqueJSContext*;
using JSGlobalContextRef = OpaqueJSContext*;
using JSStringRef = GuestJSString*;
using JSClassRef = OpaqueJSClass*;
using JSPropertyNameArrayRef = OpaqueJSPropertyNameArray*;
using JSPropertyNameAccumulatorRef = OpaqueJSPropertyNameAccumulator*;
using JSValueRef = const OpaqueJSValue*;
using JSObjectRef = OpaqueJSValue*;
using JSScriptRef = OpaqueJSScript*;
using JSWeakRef = OpaqueJSWeak*;
using JSWeakObjectMapRef = OpaqueJSWeakObjectMap*;
using JSChar = std::uint16_t;

enum JSType : int {
    kJSTypeUndefined = 0,
    kJSTypeNull = 1,
    kJSTypeBoolean = 2,
    kJSTypeNumber = 3,
    kJSTypeString = 4,
    kJSTypeObject = 5,
    kJSTypeSymbol = 6,
};

enum JSTypedArrayType : int {
    kJSTypedArrayTypeInt8Array = 0,
    kJSTypedArrayTypeInt16Array = 1,
    kJSTypedArrayTypeInt32Array = 2,
    kJSTypedArrayTypeUint8Array = 3,
    kJSTypedArrayTypeUint8ClampedArray = 4,
    kJSTypedArrayTypeUint16Array = 5,
    kJSTypedArrayTypeUint32Array = 6,
    kJSTypedArrayTypeFloat32Array = 7,
    kJSTypedArrayTypeFloat64Array = 8,
    kJSTypedArrayTypeArrayBuffer = 9,
    kJSTypedArrayTypeNone = 10,
    kJSTypedArrayTypeBigInt64Array = 11,
    kJSTypedArrayTypeBigUint64Array = 12,
};

enum : unsigned {
    kJSPropertyAttributeNone = 0,
    kJSPropertyAttributeReadOnly = 1 << 1,
    kJSPropertyAttributeDontEnum = 1 << 2,
    kJSPropertyAttributeDontDelete = 1 << 3,
};
using JSPropertyAttributes = unsigned;

enum : unsigned {
    kJSClassAttributeNone = 0,
    kJSClassAttributeNoAutomaticPrototype = 1 << 1,
};
using JSClassAttributes = unsigned;

using JSObjectInitializeCallback = void(APS5_VABI*)(JSContextRef ctx, JSObjectRef object);
using JSObjectFinalizeCallback = void(APS5_VABI*)(JSObjectRef object);
using JSObjectHasPropertyCallback = bool(APS5_VABI*)(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName);
using JSObjectGetPropertyCallback = JSValueRef(APS5_VABI*)(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);
using JSObjectSetPropertyCallback = bool(APS5_VABI*)(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception);
using JSObjectDeletePropertyCallback = bool(APS5_VABI*)(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);
using JSObjectGetPropertyNamesCallback = void(APS5_VABI*)(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames);
using JSObjectCallAsFunctionCallback = JSValueRef(APS5_VABI*)(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
using JSObjectCallAsConstructorCallback = JSObjectRef(APS5_VABI*)(JSContextRef ctx, JSObjectRef constructor, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
using JSObjectHasInstanceCallback = bool(APS5_VABI*)(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception);
using JSObjectConvertToTypeCallback = JSValueRef(APS5_VABI*)(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception);
using JSTypedArrayBytesDeallocator = void(APS5_VABI*)(void* bytes, void* deallocatorContext);
using JSShouldTerminateCallback = bool(APS5_VABI*)(JSContextRef ctx, void* context);
using JSHeapFinalizer = void(APS5_VABI*)(JSContextGroupRef group, void* userData);
using JSWeakMapDestroyedCallback = void(APS5_VABI*)(OpaqueJSWeakObjectMap* map, void* data);

struct JSMarker {
    bool(APS5_VABI* IsMarked)(JSMarker*, JSObjectRef);
    void(APS5_VABI* Mark)(JSMarker*, JSObjectRef);
};
using JSMarkingConstraint = void(APS5_VABI*)(JSMarker* marker, void* userData);

struct JSStaticValue {
    const char* name;
    JSObjectGetPropertyCallback getProperty;
    JSObjectSetPropertyCallback setProperty;
    JSPropertyAttributes attributes;
};

struct JSStaticFunction {
    const char* name;
    JSObjectCallAsFunctionCallback callAsFunction;
    JSPropertyAttributes attributes;
};

struct JSClassDefinition {
    int version;
    JSClassAttributes attributes;
    const char* className;
    JSClassRef parentClass;
    const JSStaticValue* staticValues;
    const JSStaticFunction* staticFunctions;
    JSObjectInitializeCallback initialize;
    JSObjectFinalizeCallback finalize;
    JSObjectHasPropertyCallback hasProperty;
    JSObjectGetPropertyCallback getProperty;
    JSObjectSetPropertyCallback setProperty;
    JSObjectDeletePropertyCallback deleteProperty;
    JSObjectGetPropertyNamesCallback getPropertyNames;
    JSObjectCallAsFunctionCallback callAsFunction;
    JSObjectCallAsConstructorCallback callAsConstructor;
    JSObjectHasInstanceCallback hasInstance;
    JSObjectConvertToTypeCallback convertToType;
};

inline constexpr JSClassDefinition kJSClassDefinitionEmpty = {
    0,
    kJSClassAttributeNone,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

}  // namespace NKWebKit
