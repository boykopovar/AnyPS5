#include "Jsc.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace NKWebKit {

namespace {

int ToQuickJsPropFlags(JSPropertyAttributes attributes) {
    int flags = 0;
    if (!(attributes & kJSPropertyAttributeReadOnly)) flags |= JS_PROP_WRITABLE;
    if (!(attributes & kJSPropertyAttributeDontEnum)) flags |= JS_PROP_ENUMERABLE;
    if (!(attributes & kJSPropertyAttributeDontDelete)) flags |= JS_PROP_CONFIGURABLE;
    return flags;
}

JSValueConst StoredValue(const OpaqueJSValue* value) {
    if (!value) return JS_NULL;
    return value->alive ? value->qval : JS_UNDEFINED;
}

JSValueConst ArgumentValue(const JSValueRef arguments[], std::size_t index) {
    return arguments ? StoredValue(arguments[index]) : JS_UNDEFINED;
}

enum class SetRefusal {
    NotExtensible,
    ReadOnly,
    NoSetter,
};

int RefuseSetAsTheEngineDoes(JSContext* qctx, JSAtom prop, JSValueConst value, int flags, SetRefusal refusal) {
    JSValue model = JS_NewObjectProto(qctx, JS_NULL);
    if (refusal == SetRefusal::ReadOnly) {
        JS_DefineProperty(qctx, model, prop, JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED, JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE);
    } else if (refusal == SetRefusal::NoSetter) {
        JS_DefineProperty(qctx, model, prop, JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED, JS_PROP_HAS_GET | JS_PROP_HAS_SET);
    }
    JS_PreventExtensions(qctx, model);
    const int result = JS_SetPropertyValue(qctx, model, JS_AtomToValue(qctx, prop), JS_DupValue(qctx, value), flags);
    JS_FreeValue(qctx, model);
    return result;
}

OpaqueJSValue* CoerceThisToObject(OpaqueJSContext* ctx, JSValueConst thisVal) {
    if (JS_IsUndefined(thisVal) || JS_IsNull(thisVal)) {
        return ctx->globalObject;
    }
    if (JS_IsObject(thisVal)) {
        return ctx->group->BorrowHandle(ctx, thisVal);
    }
    JSValue boxed = JS_ToObject(ctx->qctx, thisVal);
    if (JS_IsException(boxed)) {
        JS_FreeValue(ctx->qctx, JS_GetException(ctx->qctx));
        return ctx->globalObject;
    }
    return ctx->group->ToHandle(ctx, boxed);
}

OpaqueJSClass* ObjectClass(const CallbackObjectData* data) {
    return data->isDirectConstructor ? nullptr : data->jsClass;
}

JSValue OrdinaryToPrimitive(JSContext* qctx, JSValueConst object, bool preferString) {
    static constexpr const char* names[] {"toString", "valueOf"};
    for (int attempt = 0; attempt < 2; ++attempt) {
        JSValue method = JS_GetPropertyStr(qctx, object, names[preferString ? attempt : 1 - attempt]);
        if (JS_IsException(method)) return method;
        if (!JS_IsFunction(qctx, method)) {
            JS_FreeValue(qctx, method);
            continue;
        }
        JSValue result = JS_Call(qctx, method, object, 0, nullptr);
        JS_FreeValue(qctx, method);
        if (JS_IsException(result) || !JS_IsObject(result)) return result;
        JS_FreeValue(qctx, result);
    }
    return JS_ThrowTypeError(qctx, "No default value");
}

JSValue CallbackToPrimitive(JSContext* qctx, JSValueConst thisVal, int argc, JSValueConst* argv, int forGlobalObject) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(JS_GetRuntime(qctx)));
    CallbackObjectData* data = group && JS_IsObject(thisVal) ? group->FindCallbackDataByPtr(JS_VALUE_GET_PTR(thisVal)) : nullptr;
    const bool usable = data && data->ctx && data->handle && ObjectClass(data) && !data->isGlobalInterceptor;
    if (!usable || (data->handle == data->ctx->globalObject) != (forGlobalObject != 0)) {
        return JS_ThrowTypeError(qctx, "JSCallbackObject[Symbol.toPrimitive] method called on incompatible |this| value.");
    }
    if (argc < 1 || !JS_IsString(argv[0])) return JS_ThrowTypeError(qctx, "Primitive hint is not a string.");
    const char* hintStr = JS_ToCString(qctx, argv[0]);
    if (!hintStr) return JS_EXCEPTION;
    const bool preferString = std::strcmp(hintStr, "string") == 0;
    const bool knownHint = preferString || std::strcmp(hintStr, "default") == 0 || std::strcmp(hintStr, "number") == 0;
    JS_FreeCString(qctx, hintStr);
    if (!knownHint) return JS_ThrowTypeError(qctx, "Expected primitive hint to match one of 'default', 'number', 'string'.");
    OpaqueJSContext* ctx = data->ctx;
    const JSType hint = preferString ? kJSTypeString : kJSTypeNumber;
    OpaqueJSValue* targetHandle = data->handle;
    CallbackPinScope pinScope(ctx->group);
    pinScope.Pin(targetHandle);
    for (OpaqueJSClass* cls = ObjectClass(data); cls; cls = cls->parentClass) {
        if (cls->convertToType) {
            JSValueRef exc = nullptr;
            JSValueRef res = cls->convertToType(ctx, targetHandle, hint, &exc);
            if (exc) {
                return JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
            }
            if (res && res->alive) {
                if (JS_IsObject(res->qval)) return OrdinaryToPrimitive(qctx, res->qval, hint == kJSTypeString);
                return JS_DupValue(qctx, res->qval);
            }
        }
    }
    return OrdinaryToPrimitive(qctx, targetHandle->qval, hint == kJSTypeString);
}

JSValue HasInstanceClosure(JSContext* qctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, void* opaque) {
    (void)thisVal;
    (void)magic;
    auto* data = static_cast<CallbackObjectData*>(opaque);
    if (!data || !data->ctx || !data->handle || !data->jsClass) return JS_FALSE;
    OpaqueJSContext* ctx = data->ctx;
    JSValueConst candidateVal = argc > 0 ? argv[0] : JS_UNDEFINED;
    OpaqueJSValue* candidateHandle = ctx->group->BorrowHandle(ctx, candidateVal);
    CallbackPinScope pinScope(ctx->group);
    pinScope.Pin(data->handle);
    pinScope.Pin(candidateHandle);
    for (OpaqueJSClass* cls = data->jsClass; cls; cls = cls->parentClass) {
        if (cls->hasInstance) {
            JSValueRef exc = nullptr;
            const bool result = cls->hasInstance(ctx, data->handle, candidateHandle, &exc);
            if (exc) {
                return JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
            }
            return JS_NewBool(qctx, result);
        }
    }
    return JS_FALSE;
}

JSValue StaticFunctionClosure(JSContext* qctx, JSValueConst thisVal, int argc, JSValueConst* argv, int magic, void* opaque) {
    (void)magic;
    auto* fnPtr = reinterpret_cast<JSObjectCallAsFunctionCallback>(opaque);
    auto* ctx = static_cast<OpaqueJSContext*>(JS_GetContextOpaque(qctx));
    if (!ctx || !fnPtr) return JS_UNDEFINED;
    OpaqueJSValue* thisObj = CoerceThisToObject(ctx, thisVal);
    std::vector<JSValueRef> args(static_cast<std::size_t>(std::max(argc, 0)));
    CallbackPinScope pinScope(ctx->group);
    pinScope.Pin(thisObj);
    for (int i = 0; i < argc; ++i) {
        OpaqueJSValue* argHandle = ctx->group->BorrowHandle(ctx, argv[i]);
        args[static_cast<std::size_t>(i)] = argHandle;
        pinScope.Pin(argHandle);
    }
    JSValueRef exc = nullptr;
    JSValueRef res = fnPtr(ctx, thisObj, thisObj, args.size(), args.empty() ? nullptr : args.data(), &exc);
    if (exc) {
        return JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
    }
    if (!res || !res->alive) return JS_UNDEFINED;
    return JS_DupValue(qctx, res->qval);
}

JSValue CreateStaticFunctionObject(OpaqueJSContext* ctx, const StaticFunctionEntry& entry) {
    JSValue fnObj = JS_NewCClosure(
        ctx->qctx,
        &StaticFunctionClosure,
        entry.name.c_str(),
        nullptr,
        0,
        0,
        reinterpret_cast<void*>(entry.callAsFunction));
    return fnObj;
}

JSValue NullStaticFunctionGetter(JSContext* qctx, JSValueConst thisVal, int argc, JSValueConst* argv) {
    (void)thisVal;
    (void)argc;
    (void)argv;
    return JS_ThrowReferenceError(qctx, "Static function property defined with NULL callAsFunction callback.");
}

int CallbackExoticGetOwnProperty(JSContext* qctx, JSPropertyDescriptor* desc, JSValueConst obj, JSAtom prop) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(JS_GetRuntime(qctx)));
    if (!group || group->shuttingDown) return 0;
    CallbackObjectData* data = group->FindCallbackDataByPtr(JS_VALUE_GET_PTR(obj));
    if (!data || !data->ctx || !data->handle) return 0;
    OpaqueJSContext* ctx = data->ctx;
    OpaqueJSValue* targetHandle = data->handle;

    if (prop == group->toStringTagAtom) {
        if (ObjectClass(data)) {
            if (!JS_IsUndefined(data->propStore)) {
                const int ownRes = JS_GetOwnProperty(qctx, desc, data->propStore, prop);
                if (ownRes != 0) return ownRes;
            }
            const std::string& className = ObjectClass(data)->className;
            if (!className.empty() || !data->isGlobalInterceptor) {
                if (desc) {
                    desc->flags = JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE;
                    desc->value = JS_NewString(qctx, className.empty() ? "CallbackObject" : className.c_str());
                    desc->getter = JS_UNDEFINED;
                    desc->setter = JS_UNDEFINED;
                }
                return 1;
            }
        }
    } else if (prop == group->hasInstanceAtom) {
        if (ObjectClass(data)) {
            if (desc) {
                desc->flags = JS_PROP_CONFIGURABLE;
                desc->value = JS_NewCClosure(qctx, &HasInstanceClosure, "[Symbol.hasInstance]", nullptr, 1, 0, data);
                desc->getter = JS_UNDEFINED;
                desc->setter = JS_UNDEFINED;
            }
            return 1;
        }
    }

    JSValue atomVal = JS_AtomToValue(qctx, prop);
    const bool isSymbol = JS_IsSymbol(atomVal);
    JS_FreeValue(qctx, atomVal);

    if (!isSymbol && ObjectClass(data)) {
        const std::string propUtf8 = AtomToUtf8(qctx, prop);
        JSStringRef propNameRef = nullptr;
        auto getPropNameRef = [&]() -> JSStringRef {
            if (!propNameRef) {
                propNameRef = AtomToJsString(qctx, prop);
            }
            return propNameRef;
        };

        CallbackPinScope pinScope(group);
        pinScope.Pin(targetHandle);

        for (OpaqueJSClass* cls = ObjectClass(data); cls; cls = cls->parentClass) {
            if (cls->hasProperty) {
                if (cls->hasProperty(ctx, targetHandle, getPropNameRef())) {
                    if (!desc) {
                        if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                        return 1;
                    }
                    for (OpaqueJSClass* getterCls = ObjectClass(data); getterCls; getterCls = getterCls->parentClass) {
                        if (getterCls->getProperty) {
                            JSValueRef exc = nullptr;
                            JSValueRef val = getterCls->getProperty(ctx, targetHandle, getPropNameRef(), &exc);
                            if (exc) {
                                if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                                JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
                                return -1;
                            }
                            if (val && val->alive) {
                                if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                                desc->flags = JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE;
                                desc->value = JS_DupValue(qctx, val->qval);
                                desc->getter = JS_UNDEFINED;
                                desc->setter = JS_UNDEFINED;
                                return 1;
                            }
                        }
                    }
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    JS_ThrowReferenceError(qctx, "hasProperty callback returned true for a property that doesn't exist.");
                    return -1;
                }
            } else if (cls->getProperty) {
                JSValueRef exc = nullptr;
                JSValueRef val = cls->getProperty(ctx, targetHandle, getPropNameRef(), &exc);
                if (exc) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
                    return -1;
                }
                if (val && val->alive) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    if (desc) {
                        desc->flags = JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE;
                        desc->value = JS_DupValue(qctx, val->qval);
                        desc->getter = JS_UNDEFINED;
                        desc->setter = JS_UNDEFINED;
                    }
                    return 1;
                }
            }

            if (const StaticValueEntry* sv = cls->FindStaticValue(propUtf8)) {
                for (OpaqueJSClass* svCls = cls; svCls; svCls = svCls->parentClass) {
                    if (const StaticValueEntry* entry = svCls->FindStaticValue(propUtf8)) {
                        if (entry->getProperty) {
                            JSValueRef exc = nullptr;
                            JSValueRef val = entry->getProperty(ctx, targetHandle, entry->propertyNameRef, &exc);
                            if (exc) {
                                if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                                JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
                                return -1;
                            }
                            if (val && val->alive) {
                                if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                                if (desc) {
                                    desc->flags = JS_PROP_CONFIGURABLE | (ToQuickJsPropFlags(sv->attributes) & JS_PROP_ENUMERABLE);
                                    desc->value = JS_DupValue(qctx, val->qval);
                                    desc->getter = JS_UNDEFINED;
                                    desc->setter = JS_UNDEFINED;
                                }
                                return 1;
                            }
                        }
                    }
                }
            }

            if (const StaticFunctionEntry* sf = cls->FindStaticFunction(propUtf8)) {
                if (!desc) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    return 1;
                }
                if (!JS_IsUndefined(data->propStore)) {
                    const int ownRes = JS_GetOwnProperty(qctx, desc, data->propStore, prop);
                    if (ownRes != 0) {
                        if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                        return ownRes;
                    }
                }
                const StaticFunctionEntry* callable = nullptr;
                int enumerable = 0;
                for (OpaqueJSClass* fnCls = cls; fnCls; fnCls = fnCls->parentClass) {
                    const StaticFunctionEntry* entry = fnCls->FindStaticFunction(propUtf8);
                    if (!entry) continue;
                    if (!(entry->attributes & kJSPropertyAttributeDontEnum)) enumerable = JS_PROP_ENUMERABLE;
                    if (!callable && entry->callAsFunction) callable = entry;
                }
                if (!callable) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    desc->flags = JS_PROP_GETSET | JS_PROP_CONFIGURABLE | enumerable;
                    desc->value = JS_UNDEFINED;
                    desc->getter = JS_NewCFunction(qctx, &NullStaticFunctionGetter, sf->name.c_str(), 0);
                    desc->setter = JS_UNDEFINED;
                    return 1;
                }
                JSValue fnObj = CreateStaticFunctionObject(ctx, *callable);
                const int propFlags = (ToQuickJsPropFlags(callable->attributes) & ~JS_PROP_ENUMERABLE) | enumerable;
                if (!JS_IsUndefined(data->propStore)) {
                    JS_DefinePropertyValue(qctx, data->propStore, prop, JS_DupValue(qctx, fnObj), propFlags | JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE | JS_PROP_HAS_ENUMERABLE | JS_PROP_HAS_CONFIGURABLE);
                }
                if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                desc->flags = propFlags;
                desc->value = fnObj;
                desc->getter = JS_UNDEFINED;
                desc->setter = JS_UNDEFINED;
                return 1;
            }
        }

        if (propNameRef) JSStringRelease_nid_postfix(propNameRef);

        if (!JS_IsUndefined(data->propStore)) {
            const int ownRes = JS_GetOwnProperty(qctx, desc, data->propStore, prop);
            if (ownRes != 0) return ownRes;
        }

        if (data->dynamicEnumeratedNames.count(propUtf8) > 0) {
            if (desc) {
                desc->flags = JS_PROP_C_W_E;
                desc->value = JS_UNDEFINED;
                desc->getter = JS_UNDEFINED;
                desc->setter = JS_UNDEFINED;
            }
            return 1;
        }
        return 0;
    }

    if (!JS_IsUndefined(data->propStore)) {
        return JS_GetOwnProperty(qctx, desc, data->propStore, prop);
    }
    return 0;
}

int SetWithAlteredReceiver(JSContext* qctx, CallbackObjectData* data, JSValueConst obj, JSAtom prop, JSValueConst value, JSValueConst receiver) {
    JSValue target = JS_UNDEFINED;
    if (!JS_IsUndefined(data->propStore)) {
        const int hasOwn = JS_GetOwnProperty(qctx, nullptr, data->propStore, prop);
        if (hasOwn < 0) return -1;
        if (hasOwn > 0) target = JS_DupValue(qctx, data->propStore);
    }
    if (JS_IsUndefined(target)) {
        JSValue proto = JS_GetPrototype(qctx, obj);
        if (JS_IsException(proto)) return -1;
        target = JS_NewObjectProto(qctx, proto);
        JS_FreeValue(qctx, proto);
        if (JS_IsException(target)) return -1;
    }
    JSValue key = JS_AtomToValue(qctx, prop);
    JSValueConst arguments[4] = {target, key, value, receiver};
    auto* running = static_cast<OpaqueJSContext*>(JS_GetContextOpaque(qctx));
    JSValue stored = JS_Call(qctx, (running ? running : data->ctx)->reflectSet, JS_UNDEFINED, 4, arguments);
    JS_FreeValue(qctx, key);
    JS_FreeValue(qctx, target);
    if (JS_IsException(stored)) return -1;
    const int result = JS_ToBool(qctx, stored);
    JS_FreeValue(qctx, stored);
    return result;
}

int CallbackExoticSetProperty(JSContext* qctx, JSValueConst obj, JSAtom prop, JSValueConst value, JSValueConst receiver, int flags) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(JS_GetRuntime(qctx)));
    if (!group || group->shuttingDown) return 0;
    CallbackObjectData* data = group->FindCallbackDataByPtr(JS_VALUE_GET_PTR(obj));
    if (!data || !data->ctx || !data->handle) return 0;
    OpaqueJSContext* ctx = data->ctx;
    OpaqueJSValue* targetHandle = data->handle;
    if (!JS_IsObject(receiver) || JS_VALUE_GET_PTR(receiver) != JS_VALUE_GET_PTR(targetHandle->qval)) {
        return SetWithAlteredReceiver(qctx, data, obj, prop, value, receiver);
    }

    JSValue atomVal = JS_AtomToValue(qctx, prop);
    const bool isSymbol = JS_IsSymbol(atomVal);
    JS_FreeValue(qctx, atomVal);

    if (!isSymbol && ObjectClass(data)) {
        const std::string propUtf8 = AtomToUtf8(qctx, prop);
        JSStringRef propNameRef = nullptr;
        auto getPropNameRef = [&]() -> JSStringRef {
            if (!propNameRef) {
                propNameRef = AtomToJsString(qctx, prop);
            }
            return propNameRef;
        };
        OpaqueJSValue* valHandle = group->BorrowHandle(ctx, value);
        CallbackPinScope pinScope(group);
        pinScope.Pin(targetHandle);
        pinScope.Pin(valHandle);

        for (OpaqueJSClass* cls = ObjectClass(data); cls; cls = cls->parentClass) {
            if (cls->setProperty) {
                JSValueRef exc = nullptr;
                const bool handled = cls->setProperty(ctx, targetHandle, getPropNameRef(), valHandle, &exc);
                if (exc) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
                    return -1;
                }
                if (handled) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    return 1;
                }
            }

            if (const StaticValueEntry* sv = cls->FindStaticValue(propUtf8)) {
                if (sv->attributes & kJSPropertyAttributeReadOnly) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    if (flags & JS_PROP_THROW) {
                        JS_ThrowTypeError(qctx, "Cannot assign to read only property");
                        return -1;
                    }
                    return 0;
                }
                if (sv->setProperty) {
                    JSValueRef exc = nullptr;
                    const bool handled = sv->setProperty(ctx, targetHandle, sv->propertyNameRef, valHandle, &exc);
                    if (exc) {
                        if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                        JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
                        return -1;
                    }
                    if (handled) {
                        if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                        return 1;
                    }
                }
            }

            if (const StaticFunctionEntry* sf = cls->FindStaticFunction(propUtf8)) {
                if (!JS_IsUndefined(data->propStore)) {
                    const int hasOwn = JS_GetOwnProperty(qctx, nullptr, data->propStore, prop);
                    if (hasOwn > 0) {
                        if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                        return JS_SetPropertyValue(qctx, data->propStore, JS_AtomToValue(qctx, prop), JS_DupValue(qctx, value), flags);
                    }
                }
                if (sf->attributes & kJSPropertyAttributeReadOnly) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    if (flags & JS_PROP_THROW) {
                        JS_ThrowTypeError(qctx, "Cannot assign to read only property");
                        return -1;
                    }
                    return 0;
                }
                if (!JS_IsUndefined(data->propStore)) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    return JS_DefinePropertyValue(qctx, data->propStore, prop, JS_DupValue(qctx, value), JS_PROP_C_W_E | JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE | JS_PROP_HAS_ENUMERABLE | JS_PROP_HAS_CONFIGURABLE);
                }
            }
        }
        if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
    }

    if (data->isGlobalInterceptor) {
        return JS_DefinePropertyValue(qctx, targetHandle->qval, prop, JS_DupValue(qctx, value), JS_PROP_C_W_E | JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE | JS_PROP_HAS_ENUMERABLE | JS_PROP_HAS_CONFIGURABLE);
    }

    if (!JS_IsUndefined(data->propStore)) {
        const int hasOwn = JS_GetOwnProperty(qctx, nullptr, data->propStore, prop);
        if (hasOwn > 0) {
            return JS_SetPropertyValue(qctx, data->propStore, JS_AtomToValue(qctx, prop), JS_DupValue(qctx, value), flags);
        }
        JSValue curProto = JS_GetPrototype(qctx, obj);
        while (JS_IsObject(curProto)) {
            JSPropertyDescriptor protoDesc{};
            const int protoHas = JS_GetOwnProperty(qctx, &protoDesc, curProto, prop);
            if (protoHas < 0) {
                JS_FreeValue(qctx, curProto);
                return -1;
            }
            if (protoHas > 0) {
                if (protoDesc.flags & JS_PROP_GETSET) {
                    JSValue setter = protoDesc.setter;
                    JS_FreeValue(qctx, protoDesc.getter);
                    JS_FreeValue(qctx, protoDesc.value);
                    JS_FreeValue(qctx, curProto);
                    if (JS_IsUndefined(setter)) {
                        return RefuseSetAsTheEngineDoes(qctx, prop, value, flags, SetRefusal::NoSetter);
                    }
                    JSValueConst arg = value;
                    JSValue res = JS_Call(qctx, setter, targetHandle->qval, 1, &arg);
                    JS_FreeValue(qctx, setter);
                    if (JS_IsException(res)) return -1;
                    JS_FreeValue(qctx, res);
                    return 1;
                }
                const bool writable = (protoDesc.flags & JS_PROP_WRITABLE) != 0;
                JS_FreeValue(qctx, protoDesc.getter);
                JS_FreeValue(qctx, protoDesc.setter);
                JS_FreeValue(qctx, protoDesc.value);
                JS_FreeValue(qctx, curProto);
                if (!writable) return RefuseSetAsTheEngineDoes(qctx, prop, value, flags, SetRefusal::ReadOnly);
                break;
            }
            JSValue nextProto = JS_GetPrototype(qctx, curProto);
            JS_FreeValue(qctx, curProto);
            curProto = nextProto;
        }
        JS_FreeValue(qctx, curProto);
        if (JS_IsExtensible(qctx, obj) == 0) return RefuseSetAsTheEngineDoes(qctx, prop, value, flags, SetRefusal::NotExtensible);
        return JS_SetPropertyValue(qctx, data->propStore, JS_AtomToValue(qctx, prop), JS_DupValue(qctx, value), flags);
    }
    return 0;
}

int CallbackExoticDeleteProperty(JSContext* qctx, JSValueConst obj, JSAtom prop) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(JS_GetRuntime(qctx)));
    if (!group || group->shuttingDown) return 1;
    CallbackObjectData* data = group->FindCallbackDataByPtr(JS_VALUE_GET_PTR(obj));
    if (!data || !data->ctx || !data->handle) return 1;
    OpaqueJSContext* ctx = data->ctx;
    OpaqueJSValue* targetHandle = data->handle;

    JSValue atomVal = JS_AtomToValue(qctx, prop);
    const bool isSymbol = JS_IsSymbol(atomVal);
    JS_FreeValue(qctx, atomVal);

    if (!isSymbol && ObjectClass(data)) {
        const std::string propUtf8 = AtomToUtf8(qctx, prop);
        JSStringRef propNameRef = nullptr;
        auto getPropNameRef = [&]() -> JSStringRef {
            if (!propNameRef) {
                propNameRef = AtomToJsString(qctx, prop);
            }
            return propNameRef;
        };
        CallbackPinScope pinScope(group);
        pinScope.Pin(targetHandle);

        for (OpaqueJSClass* cls = ObjectClass(data); cls; cls = cls->parentClass) {
            if (cls->deleteProperty) {
                JSValueRef exc = nullptr;
                const bool deleted = cls->deleteProperty(ctx, targetHandle, getPropNameRef(), &exc);
                if (exc) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
                    return -1;
                }
                if (deleted) {
                    if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                    return 1;
                }
            }
            if (const StaticValueEntry* sv = cls->FindStaticValue(propUtf8)) {
                if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                return (sv->attributes & kJSPropertyAttributeDontDelete) ? 0 : 1;
            }
            if (const StaticFunctionEntry* sf = cls->FindStaticFunction(propUtf8)) {
                if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
                return (sf->attributes & kJSPropertyAttributeDontDelete) ? 0 : 1;
            }
        }
        if (propNameRef) JSStringRelease_nid_postfix(propNameRef);
    }

    if (!JS_IsUndefined(data->propStore)) {
        return JS_DeleteProperty(qctx, data->propStore, prop, 0);
    }
    return 1;
}

int CallbackExoticDefineOwnProperty(JSContext* qctx, JSValueConst thisObj, JSAtom prop, JSValueConst val, JSValueConst getter, JSValueConst setter, int flags) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(JS_GetRuntime(qctx)));
    if (!group || group->shuttingDown) return -1;
    CallbackObjectData* data = group->FindCallbackDataByPtr(JS_VALUE_GET_PTR(thisObj));
    if (!data || JS_IsUndefined(data->propStore)) return -1;
    if (JS_IsExtensible(qctx, thisObj) == 0) {
        const int present = JS_GetOwnProperty(qctx, nullptr, thisObj, prop);
        if (present < 0) return -1;
        if (present == 0) {
            JSValue model = JS_NewObjectProto(qctx, JS_NULL);
            JS_PreventExtensions(qctx, model);
            const int result = JS_DefineProperty(qctx, model, prop, val, getter, setter, flags);
            JS_FreeValue(qctx, model);
            return result;
        }
    }
    return JS_DefineProperty(qctx, data->propStore, prop, val, getter, setter, flags);
}

int CallbackExoticGetOwnPropertyNames(JSContext* qctx, JSPropertyEnum** ptab, std::uint32_t* plen, JSValueConst obj) {
    *ptab = nullptr;
    *plen = 0;
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(JS_GetRuntime(qctx)));
    if (!group || group->shuttingDown) return 0;
    CallbackObjectData* data = group->FindCallbackDataByPtr(JS_VALUE_GET_PTR(obj));
    if (!data || !data->ctx || !data->handle) return 0;
    OpaqueJSContext* ctx = data->ctx;
    OpaqueJSValue* targetHandle = data->handle;

    OpaqueJSPropertyNameAccumulator acc;
    CallbackPinScope pinScope(group);
    pinScope.Pin(targetHandle);

    data->dynamicEnumeratedNames.clear();
    for (OpaqueJSClass* cls = ObjectClass(data); cls; cls = cls->parentClass) {
        if (cls->getPropertyNames) {
            cls->getPropertyNames(ctx, targetHandle, &acc);
        }
        for (const auto& sv : cls->staticValues) {
            if (sv.getProperty && !(sv.attributes & kJSPropertyAttributeDontEnum)) {
                acc.AddName(sv.propertyNameRef);
            }
        }
        for (const auto& sf : cls->staticFunctions) {
            if (!(sf.attributes & kJSPropertyAttributeDontEnum)) {
                JSStringRef s = Utf8ToJsString(sf.name);
                acc.AddName(s);
                JSStringRelease_nid_postfix(s);
            }
        }
    }
    for (auto* nameRef : acc.names) {
        data->dynamicEnumeratedNames.insert(JsStringToUtf8(nameRef));
    }

    std::vector<JSPropertyEnum> collected;
    std::unordered_set<JSAtom> seenAtoms;
    for (auto* nameRef : acc.names) {
        JSAtom atom = JsStringToAtom(qctx, nameRef);
        if (atom != JS_ATOM_NULL) {
            if (seenAtoms.insert(atom).second) {
                collected.push_back(JSPropertyEnum{true, atom});
            } else {
                JS_FreeAtom(qctx, atom);
            }
        }
    }

    if (!JS_IsUndefined(data->propStore)) {
        JSPropertyEnum* storeTab = nullptr;
        std::uint32_t storeLen = 0;
        if (JS_GetOwnPropertyNames(qctx, &storeTab, &storeLen, data->propStore, JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK | JS_GPN_SET_ENUM) == 0) {
            for (std::uint32_t i = 0; i < storeLen; ++i) {
                if (seenAtoms.insert(storeTab[i].atom).second) {
                    collected.push_back(JSPropertyEnum{storeTab[i].is_enumerable, JS_DupAtom(qctx, storeTab[i].atom)});
                }
            }
            JS_FreePropertyEnum(qctx, storeTab, storeLen);
        }
    }

    const auto count = static_cast<std::uint32_t>(collected.size());
    auto* tab = static_cast<JSPropertyEnum*>(js_malloc(qctx, sizeof(JSPropertyEnum) * std::max<std::uint32_t>(count, 1u)));
    if (!tab) {
        for (auto& item : collected) JS_FreeAtom(qctx, item.atom);
        return -1;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        tab[i] = collected[i];
    }
    *ptab = tab;
    *plen = count;
    return 0;
}

JSClassExoticMethods g_callbackExoticMethods = {
    &CallbackExoticGetOwnProperty,
    &CallbackExoticGetOwnPropertyNames,
    &CallbackExoticDeleteProperty,
    &CallbackExoticDefineOwnProperty,
    nullptr,
    nullptr,
    &CallbackExoticSetProperty,
};

void CallbackClassFinalizer(JSRuntime* rt, JSValueConst val) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(rt));
    if (!group || group->shuttingDown) return;
    void* ptr = JS_VALUE_GET_PTR(val);
    CallbackObjectData* data = group->FindCallbackDataByPtr(ptr);
    if (!data) return;
    OpaqueJSValue* handle = data->handle;
    if (!data->isGlobalInterceptor) {
        RunClassFinalize(data);
    }
    CallbackObjectData* owned[2] = {data, data->isGlobalInterceptor ? group->FindCallbackData(handle) : nullptr};
    for (CallbackObjectData* holder : owned) {
        if (!holder) continue;
        if (!JS_IsUndefined(holder->propStore)) {
            JS_FreeValueRT(rt, holder->propStore);
            holder->propStore = JS_UNDEFINED;
        }
        for (auto& [k, slot] : holder->privateProperties) {
            if (!JS_IsUndefined(slot.strongVal)) {
                JS_FreeValueRT(rt, slot.strongVal);
                slot.strongVal = JS_UNDEFINED;
            }
        }
        holder->privateProperties.clear();
    }
    group->callbackDataByPtr.erase(ptr);
    if (!data->isGlobalInterceptor) {
        group->NotifyObjectFinalized(ptr, handle);
    }
    delete data;
}

void CallbackClassGcMark(JSRuntime* rt, JSValueConst val, JS_MarkFunc* markFunc) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(rt));
    if (!group) return;
    void* ptr = JS_VALUE_GET_PTR(val);
    CallbackObjectData* data = group->FindCallbackDataByPtr(ptr);
    if (!data) return;
    if (!JS_IsUndefined(data->propStore)) {
        JS_MarkValue(rt, data->propStore, markFunc);
    }
    for (const auto& [k, slot] : data->privateProperties) {
        if (!JS_IsUndefined(slot.strongVal)) {
            JS_MarkValue(rt, slot.strongVal, markFunc);
        }
    }
    if (data->isGlobalInterceptor && data->handle) {
        if (CallbackObjectData* globalData = group->FindCallbackData(data->handle)) {
            if (!JS_IsUndefined(globalData->propStore)) {
                JS_MarkValue(rt, globalData->propStore, markFunc);
            }
            for (const auto& [k, slot] : globalData->privateProperties) {
                if (!JS_IsUndefined(slot.strongVal)) {
                    JS_MarkValue(rt, slot.strongVal, markFunc);
                }
            }
        }
    }
}

bool IsGlobalObjectOfTheGroup(const OpaqueJSContextGroup* group, JSValueConst object) {
    for (const OpaqueJSContext* ctx : group->contexts) {
        if (ctx->globalObject && JS_VALUE_GET_PTR(ctx->globalObject->qval) == JS_VALUE_GET_PTR(object)) return true;
    }
    return false;
}

bool AdoptDerivedPrototype(JSContext* qctx, const OpaqueJSContextGroup* group, JSValueConst constructor, JSValueConst made, JSValueConst derivedPrototype) {
    JSValue constructorPrototype = JS_GetPropertyStr(qctx, constructor, "prototype");
    if (JS_IsException(constructorPrototype)) return false;
    bool samePrototype = false;
    if (!JS_IsProxy(made)) {
        JSValue current = JS_GetPrototype(qctx, made);
        samePrototype = !JS_IsException(current) && JS_IsStrictEqual(qctx, current, constructorPrototype);
        JS_FreeValue(qctx, current);
    }
    JS_FreeValue(qctx, constructorPrototype);
    if (!samePrototype || IsGlobalObjectOfTheGroup(group, made)) return true;
    if (JS_SetPrototype(qctx, made, derivedPrototype) < 0) {
        JS_FreeValue(qctx, JS_GetException(qctx));
    }
    return true;
}

JSValue CallbackClassCall(JSContext* qctx, JSValueConst funcObj, JSValueConst thisVal, int argc, JSValueConst* argv, int flags) {
    auto* group = static_cast<OpaqueJSContextGroup*>(JS_GetRuntimeOpaque(JS_GetRuntime(qctx)));
    if (!group || group->shuttingDown) return JS_UNDEFINED;
    CallbackObjectData* data = group->FindCallbackDataByPtr(JS_VALUE_GET_PTR(funcObj));
    if (!data || !data->ctx || !data->handle) return JS_UNDEFINED;
    OpaqueJSContext* ctx = data->ctx;
    OpaqueJSValue* funcHandle = data->handle;

    std::vector<JSValueRef> args(static_cast<std::size_t>(std::max(argc, 0)));
    CallbackPinScope pinScope(group);
    pinScope.Pin(funcHandle);
    for (int i = 0; i < argc; ++i) {
        OpaqueJSValue* argHandle = group->BorrowHandle(ctx, argv[i]);
        args[static_cast<std::size_t>(i)] = argHandle;
        pinScope.Pin(argHandle);
    }

    if (flags & JS_CALL_FLAG_CONSTRUCTOR) {
        if (data->isDirectConstructor) {
            if (data->directConstructorCallback) {
                const bool derived = JS_IsObject(thisVal) && JS_VALUE_GET_PTR(thisVal) != JS_VALUE_GET_PTR(funcObj);
                JSValue derivedPrototype = JS_UNDEFINED;
                if (derived) {
                    derivedPrototype = JS_GetPropertyStr(qctx, thisVal, "prototype");
                    if (JS_IsException(derivedPrototype)) return JS_EXCEPTION;
                }
                JSValueRef exc = nullptr;
                JSObjectRef res = data->directConstructorCallback(ctx, funcHandle, args.size(), args.empty() ? nullptr : args.data(), &exc);
                if (exc) {
                    JS_FreeValue(qctx, derivedPrototype);
                    return JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
                }
                if (!res || !res->alive) {
                    JS_FreeValue(qctx, derivedPrototype);
                    return res ? JS_UNDEFINED : JS_ThrowTypeError(qctx, "Type error");
                }
                const bool adopted = !derived || AdoptDerivedPrototype(qctx, group, funcObj, res->qval, derivedPrototype);
                JS_FreeValue(qctx, derivedPrototype);
                return adopted ? JS_DupValue(qctx, res->qval) : JS_EXCEPTION;
            }
            OpaqueJSValue* instance = MakeCallbackObject(ctx, data->jsClass, nullptr);
            if (!instance || !instance->alive) return JS_UNDEFINED;
            return JS_DupValue(qctx, instance->qval);
        }
        for (OpaqueJSClass* cls = data->jsClass; cls; cls = cls->parentClass) {
            if (cls->callAsConstructor) {
                JSValueRef exc = nullptr;
                JSObjectRef res = cls->callAsConstructor(ctx, funcHandle, args.size(), args.empty() ? nullptr : args.data(), &exc);
                if (exc) {
                    return JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
                }
                if (!res || !res->alive) return JS_UNDEFINED;
                return JS_DupValue(qctx, res->qval);
            }
        }
        return JS_ThrowTypeError(qctx, "Object is not a constructor");
    }

    OpaqueJSValue* thisObj = CoerceThisToObject(ctx, thisVal);
    pinScope.Pin(thisObj);

    if (data->isDirectFunction) {
        if (!data->directFunctionCallback) return JS_UNDEFINED;
        JSValueRef exc = nullptr;
        JSValueRef res = data->directFunctionCallback(ctx, funcHandle, thisObj, args.size(), args.empty() ? nullptr : args.data(), &exc);
        if (exc) {
            return JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
        }
        if (!res || !res->alive) return JS_UNDEFINED;
        return JS_DupValue(qctx, res->qval);
    }

    for (OpaqueJSClass* cls = ObjectClass(data); cls; cls = cls->parentClass) {
        if (cls->callAsFunction) {
            JSValueRef exc = nullptr;
            JSValueRef res = cls->callAsFunction(ctx, funcHandle, thisObj, args.size(), args.empty() ? nullptr : args.data(), &exc);
            if (exc) {
                return JS_Throw(qctx, JS_DupValue(qctx, exc->qval));
            }
            if (!res) return JS_NULL;
            if (!res->alive) return JS_UNDEFINED;
            return JS_DupValue(qctx, res->qval);
        }
    }
    return JS_ThrowTypeError(qctx, "Object is not a function");
}

struct NoCopyBufferRecord {
    JSTypedArrayBytesDeallocator deallocator = nullptr;
    void* deallocatorContext = nullptr;
    void* originalBytes = nullptr;
};

std::uint8_t g_emptyBufferByte = 0;

void* ArrayBufferNoCopyRealloc(JSRuntime* rt, void* opaque, void* ptr, std::size_t size) {
    (void)rt;
    (void)ptr;
    if (size == 0) {
        auto* record = static_cast<NoCopyBufferRecord*>(opaque);
        if (record) {
            if (record->deallocator) {
                record->deallocator(record->originalBytes, record->deallocatorContext);
            }
            delete record;
        }
        return nullptr;
    }
    return nullptr;
}

bool ToQuickJsTypedArrayEnum(JSTypedArrayType type, JSTypedArrayEnum* outEnum, std::size_t* outElemSize) {
    switch (type) {
    case kJSTypedArrayTypeInt8Array:
        *outEnum = JS_TYPED_ARRAY_INT8;
        *outElemSize = 1;
        return true;
    case kJSTypedArrayTypeInt16Array:
        *outEnum = JS_TYPED_ARRAY_INT16;
        *outElemSize = 2;
        return true;
    case kJSTypedArrayTypeInt32Array:
        *outEnum = JS_TYPED_ARRAY_INT32;
        *outElemSize = 4;
        return true;
    case kJSTypedArrayTypeUint8Array:
        *outEnum = JS_TYPED_ARRAY_UINT8;
        *outElemSize = 1;
        return true;
    case kJSTypedArrayTypeUint8ClampedArray:
        *outEnum = JS_TYPED_ARRAY_UINT8C;
        *outElemSize = 1;
        return true;
    case kJSTypedArrayTypeUint16Array:
        *outEnum = JS_TYPED_ARRAY_UINT16;
        *outElemSize = 2;
        return true;
    case kJSTypedArrayTypeUint32Array:
        *outEnum = JS_TYPED_ARRAY_UINT32;
        *outElemSize = 4;
        return true;
    case kJSTypedArrayTypeFloat32Array:
        *outEnum = JS_TYPED_ARRAY_FLOAT32;
        *outElemSize = 4;
        return true;
    case kJSTypedArrayTypeFloat64Array:
        *outEnum = JS_TYPED_ARRAY_FLOAT64;
        *outElemSize = 8;
        return true;
    case kJSTypedArrayTypeBigInt64Array:
        *outEnum = JS_TYPED_ARRAY_BIG_INT64;
        *outElemSize = 8;
        return true;
    case kJSTypedArrayTypeBigUint64Array:
        *outEnum = JS_TYPED_ARRAY_BIG_UINT64;
        *outElemSize = 8;
        return true;
    default:
        return false;
    }
}

JSValue CallIntrinsicGetter(OpaqueJSContextGroup* group, JSContext* qctx, JSValueConst receiver, const char* constructorName, bool onParentPrototype,
    const char* property) {
    JSContext* uctx = group->utilityCtx;
    JSValue global = JS_GetGlobalObject(uctx);
    JSValue constructor = JS_GetPropertyStr(uctx, global, constructorName);
    JSValue prototype = JS_GetPropertyStr(uctx, constructor, "prototype");
    if (onParentPrototype) {
        JSValue parent = JS_GetPrototype(uctx, prototype);
        JS_FreeValue(uctx, prototype);
        prototype = parent;
    }
    JSAtom atom = JS_NewAtom(uctx, property);
    JSPropertyDescriptor desc{};
    JSValue result = JS_EXCEPTION;
    const int found = JS_GetOwnProperty(uctx, &desc, prototype, atom);
    if (found > 0) {
        result = JS_Call(qctx, desc.getter, receiver, 0, nullptr);
        JS_FreeValue(uctx, desc.value);
        JS_FreeValue(uctx, desc.getter);
        JS_FreeValue(uctx, desc.setter);
    } else if (found == 0) {
        result = JS_ThrowTypeError(qctx, "intrinsic accessor not found");
    }
    JS_FreeAtom(uctx, atom);
    JS_FreeValue(uctx, prototype);
    JS_FreeValue(uctx, constructor);
    JS_FreeValue(uctx, global);
    return result;
}

void DropPendingException(JSContext* qctx) {
    JS_FreeValue(qctx, JS_GetException(qctx));
}

std::size_t TakeSizeOrZero(JSContext* qctx, JSValue value) {
    if (JS_IsException(value)) {
        DropPendingException(qctx);
        return 0;
    }
    double number = 0.0;
    if (JS_ToFloat64(qctx, &number, value) < 0) {
        DropPendingException(qctx);
        number = 0.0;
    }
    JS_FreeValue(qctx, value);
    return number > 0.0 ? static_cast<std::size_t>(number) : 0;
}

bool IsBufferView(JSValueConst value) {
    return JS_GetTypedArrayType(value) >= 0 || JS_IsDataView(value);
}

JSContext* QuickJsContextOf(JSContextRef ctx, JSObjectRef object) {
    if (ctx) return ctx->qctx;
    OpaqueJSContext* owner = object->ctx ? object->ctx : object->group->DefaultContext();
    return owner ? owner->qctx : object->group->utilityCtx;
}

struct BufferViewMetrics {
    std::size_t byteOffset = 0;
    std::size_t byteLength = 0;
    std::size_t length = 0;
};

BufferViewMetrics ReadBufferViewMetrics(JSContextRef ctx, JSObjectRef object) {
    BufferViewMetrics metrics;
    if (!object || !object->alive || !object->group || !IsBufferView(object->qval)) return metrics;
    JSContext* qctx = QuickJsContextOf(ctx, object);
    if (JS_IsDataView(object->qval)) {
        metrics.byteLength = TakeSizeOrZero(qctx, CallIntrinsicGetter(object->group, qctx, object->qval, "DataView", false, "byteLength"));
        metrics.byteOffset = TakeSizeOrZero(qctx, CallIntrinsicGetter(object->group, qctx, object->qval, "DataView", false, "byteOffset"));
        metrics.length = metrics.byteLength;
        return metrics;
    }
    std::size_t bytesPerElement = 0;
    JSValue buffer = JS_GetTypedArrayBuffer(qctx, object->qval, &metrics.byteOffset, &metrics.byteLength, &bytesPerElement);
    if (JS_IsException(buffer)) {
        DropPendingException(qctx);
        return BufferViewMetrics{};
    }
    JS_FreeValue(qctx, buffer);
    metrics.length = bytesPerElement > 0 ? metrics.byteLength / bytesPerElement : 0;
    return metrics;
}

JSValue BufferOfView(OpaqueJSContextGroup* group, JSContext* qctx, JSValueConst view) {
    if (JS_IsDataView(view)) return CallIntrinsicGetter(group, qctx, view, "DataView", false, "buffer");
    return CallIntrinsicGetter(group, qctx, view, "Uint8Array", true, "buffer");
}

}  // namespace

void OpaqueJSClass::DestroyInPlace() {
    for (auto& sv : staticValues) {
        if (sv.propertyNameRef) {
            JSStringRelease_nid_postfix(sv.propertyNameRef);
            sv.propertyNameRef = nullptr;
        }
    }
    staticValues.clear();
    staticFunctions.clear();
    if (parentClass) {
        if (parentClass->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            parentClass->DestroyInPlace();
            delete parentClass;
        }
        parentClass = nullptr;
    }
    if (prototypeClass) {
        OpaqueJSClass* released = prototypeClass.release();
        if (released->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            released->DestroyInPlace();
            delete released;
        }
    }
}

bool OpaqueJSClass::HasCallAsFunction() const {
    for (const OpaqueJSClass* cls = this; cls; cls = cls->parentClass) {
        if (cls->callAsFunction) return true;
    }
    return false;
}

bool OpaqueJSClass::HasCallAsConstructor() const {
    for (const OpaqueJSClass* cls = this; cls; cls = cls->parentClass) {
        if (cls->callAsConstructor) return true;
    }
    return false;
}

bool OpaqueJSClass::HasConvertToType() const {
    for (const OpaqueJSClass* cls = this; cls; cls = cls->parentClass) {
        if (cls->convertToType) return true;
    }
    return false;
}

bool OpaqueJSClass::HasInstanceCallback() const {
    for (const OpaqueJSClass* cls = this; cls; cls = cls->parentClass) {
        if (cls->hasInstance) return true;
    }
    return false;
}

const StaticValueEntry* OpaqueJSClass::FindStaticValue(const std::string& propName) const {
    for (const auto& entry : staticValues) {
        if (entry.name == propName) return &entry;
    }
    return nullptr;
}

const StaticFunctionEntry* OpaqueJSClass::FindStaticFunction(const std::string& propName) const {
    for (const auto& entry : staticFunctions) {
        if (entry.name == propName) return &entry;
    }
    return nullptr;
}

void RegisterCallbackClasses(OpaqueJSContextGroup* group) {
    JS_NewClassID(group->rt, &group->callbackObjectClassId);
    JSClassDef objDef{};
    objDef.class_name = "JSCallbackObject";
    objDef.finalizer = &CallbackClassFinalizer;
    objDef.gc_mark = &CallbackClassGcMark;
    objDef.call = nullptr;
    objDef.exotic = &g_callbackExoticMethods;
    JS_NewClass(group->rt, group->callbackObjectClassId, &objDef);

    JS_NewClassID(group->rt, &group->callbackCallableClassId);
    JSClassDef callDef{};
    callDef.class_name = "JSCallbackCallable";
    callDef.finalizer = &CallbackClassFinalizer;
    callDef.gc_mark = &CallbackClassGcMark;
    callDef.call = &CallbackClassCall;
    callDef.exotic = &g_callbackExoticMethods;
    JS_NewClass(group->rt, group->callbackCallableClassId, &callDef);
}

void RunClassInitialize(OpaqueJSContext* ctx, OpaqueJSValue* objHandle, OpaqueJSClass* jsClass) {
    if (!ctx || !objHandle || !jsClass) return;
    std::vector<JSObjectInitializeCallback> initChain;
    for (OpaqueJSClass* cls = jsClass; cls; cls = cls->parentClass) {
        if (cls->initialize) {
            initChain.push_back(cls->initialize);
        }
    }
    CallbackPinScope pinScope(ctx->group);
    pinScope.Pin(objHandle);
    for (auto it = initChain.rbegin(); it != initChain.rend(); ++it) {
        (*it)(ctx, objHandle);
    }
}

void RunClassFinalize(CallbackObjectData* data) {
    if (!data || data->finalized) return;
    data->finalized = true;
    if (ObjectClass(data) && data->handle) {
        for (OpaqueJSClass* cls = ObjectClass(data); cls; cls = cls->parentClass) {
            if (cls->finalize) {
                cls->finalize(data->handle);
            }
        }
    }
    if (data->jsClass) {
        if (data->jsClass->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            data->jsClass->DestroyInPlace();
            delete data->jsClass;
        }
        data->jsClass = nullptr;
    }
}

OpaqueJSValue* GetOrCreateClassPrototype(OpaqueJSContext* ctx, OpaqueJSClass* jsClass) {
    if (!ctx || !jsClass) return nullptr;
    if (!jsClass->prototypeClass) return nullptr;
    auto it = ctx->classPrototypes.find(jsClass);
    if (it != ctx->classPrototypes.end() && it->second && it->second->alive) {
        return it->second;
    }

    OpaqueJSClass* protoCls = jsClass->prototypeClass.get();
    JSValue rawProto = JS_NewObjectClass(ctx->qctx, ctx->group->callbackObjectClassId);
    void* ptr = JS_VALUE_GET_PTR(rawProto);
    OpaqueJSValue* protoHandle = ctx->group->ToHandle(ctx, rawProto);

    auto* data = new CallbackObjectData();
    data->group = ctx->group;
    data->ctx = ctx;
    data->handle = protoHandle;
    if (protoCls) {
        protoCls->refCount.fetch_add(1, std::memory_order_relaxed);
        data->jsClass = protoCls;
    }
    data->propStore = JS_NewObjectProto(ctx->qctx, JS_NULL);
    JS_SetOpaque(protoHandle->qval, data);
    ctx->group->callbackDataByPtr[ptr] = data;

    if (jsClass->parentClass) {
        OpaqueJSValue* parentProto = GetOrCreateClassPrototype(ctx, jsClass->parentClass);
        if (parentProto && parentProto->alive) {
            JS_SetPrototype(ctx->qctx, protoHandle->qval, parentProto->qval);
        }
    }

    ctx->classPrototypes[jsClass] = protoHandle;
    return protoHandle;
}

OpaqueJSValue* MakeCallbackObject(OpaqueJSContext* ctx, OpaqueJSClass* jsClass, void* data) {
    if (!ctx) return nullptr;
    if (!jsClass) {
        return ctx->group->ToHandle(ctx, JS_NewObject(ctx->qctx));
    }

    const bool callable = jsClass->HasCallAsFunction();
    const bool constructible = jsClass->HasCallAsConstructor();
    const JSClassID classId = (callable || constructible) ? ctx->group->callbackCallableClassId : ctx->group->callbackObjectClassId;

    JSValue rawObj = JS_NewObjectClass(ctx->qctx, classId);
    if (constructible) {
        JS_SetConstructorBit(ctx->qctx, rawObj, true);
    }
    void* ptr = JS_VALUE_GET_PTR(rawObj);
    OpaqueJSValue* handle = ctx->group->ToHandle(ctx, rawObj);

    auto* cbData = new CallbackObjectData();
    cbData->group = ctx->group;
    cbData->ctx = ctx;
    cbData->handle = handle;
    jsClass->refCount.fetch_add(1, std::memory_order_relaxed);
    cbData->jsClass = jsClass;
    cbData->privateData = data;
    cbData->propStore = JS_NewObjectProto(ctx->qctx, JS_NULL);
    JS_SetOpaque(handle->qval, cbData);
    ctx->group->callbackDataByPtr[ptr] = cbData;

    OpaqueJSValue* proto = GetOrCreateClassPrototype(ctx, jsClass);
    if (proto && proto->alive) {
        JS_SetPrototype(ctx->qctx, handle->qval, proto->qval);
    }

    if (jsClass->HasConvertToType()) DefineToPrimitive(ctx, cbData->propStore, false);
    RunClassInitialize(ctx, handle, jsClass);
    return handle;
}

void DefineToPrimitive(OpaqueJSContext* ctx, JSValueConst target, bool forGlobalObject) {
    JSValue function = JS_NewCFunctionMagic(ctx->qctx, &CallbackToPrimitive, "[Symbol.toPrimitive]", 1, JS_CFUNC_generic_magic, forGlobalObject ? 1 : 0);
    JS_DefinePropertyValue(ctx->qctx, target, ctx->group->toPrimitiveAtom, function, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
}

bool IsObjectCallable(const OpaqueJSContext* ctx, OpaqueJSValue* object) {
    if (!ctx || !object || !object->alive || !object->isObjectHandle) return false;
    if (CallbackObjectData* cbData = ctx->group->FindCallbackData(object)) {
        if (cbData->isDirectFunction) return true;
        if (cbData->isDirectConstructor) return false;
        return cbData->jsClass && cbData->jsClass->HasCallAsFunction();
    }
    return JS_IsFunction(ctx->qctx, object->qval);
}

namespace {

bool Compiles(JSContext* qctx, const std::string& code) {
    JSEvalOptions options{};
    options.version = JS_EVAL_OPTIONS_VERSION;
    options.eval_flags = JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY;
    options.filename = "";
    options.line_num = 1;
    JSValue compiled = JS_Eval2(qctx, code.c_str(), code.size(), &options);
    if (JS_IsException(compiled)) {
        JS_FreeValue(qctx, JS_GetException(qctx));
        return false;
    }
    JS_FreeValue(qctx, compiled);
    return true;
}

const char* SingleFunctionError(JSContext* qctx, const std::string& name, const std::string& parameters, const std::string& body) {
    const std::string head = "(function " + name + "(";
    if (!Compiles(qctx, head + parameters + ") {\n" + body + "\n})")) return nullptr;
    if (!Compiles(qctx, head + parameters + "\n) {})") || !Compiles(qctx, head + ") {\n" + body + "\n})")) {
        return "Parameters should match arguments offered as parameters in Function constructor.";
    }
    std::string probe = "scopeProbe";
    while ((name + parameters + body).find(probe) != std::string::npos) probe += "0";
    const std::string tail = ") {\n" + body + "\n;let " + probe + ";\n})";
    const bool nameIsOneBinding = Compiles(qctx, "var " + name + ";");
    const bool endsInAFunctionBody = Compiles(qctx, head + parameters + tail);
    const bool endsInAnotherFunction = Compiles(qctx, head + probe + (parameters.empty() ? "" : ", ") + parameters + tail);
    return nameIsOneBinding && endsInAFunctionBody && !endsInAnotherFunction ? nullptr : "Parser error";
}

}  // namespace

}  // namespace NKWebKit

extern "C" {
using namespace NKWebKit;

JSClassRef APS5_VABI JSClassCreate(const JSClassDefinition* definition) {
    if (!definition) return nullptr;
    auto* cls = new OpaqueJSClass();
    cls->attributes = definition->attributes;
    if (definition->className) {
        cls->className = definition->className;
    }
    if (definition->parentClass) {
        definition->parentClass->refCount.fetch_add(1, std::memory_order_relaxed);
        cls->parentClass = definition->parentClass;
    }
    if (definition->staticValues) {
        for (const JSStaticValue* sv = definition->staticValues; sv->name != nullptr; ++sv) {
            StaticValueEntry entry;
            entry.name = sv->name;
            entry.getProperty = sv->getProperty;
            entry.setProperty = sv->setProperty;
            entry.attributes = sv->attributes;
            entry.propertyNameRef = Utf8ToJsString(entry.name);
            auto existingValue = std::find_if(cls->staticValues.begin(), cls->staticValues.end(), [&entry](const StaticValueEntry& other) { return other.name == entry.name; });
            if (existingValue == cls->staticValues.end()) {
                cls->staticValues.push_back(std::move(entry));
            } else {
                JSStringRelease_nid_postfix(existingValue->propertyNameRef);
                *existingValue = std::move(entry);
            }
        }
    }
    std::vector<StaticFunctionEntry> functions;
    if (definition->staticFunctions) {
        for (const JSStaticFunction* sf = definition->staticFunctions; sf->name != nullptr; ++sf) {
            StaticFunctionEntry entry;
            entry.name = sf->name;
            entry.callAsFunction = sf->callAsFunction;
            entry.attributes = sf->attributes;
            auto existingFunction = std::find_if(functions.begin(), functions.end(), [&entry](const StaticFunctionEntry& other) { return other.name == entry.name; });
            if (existingFunction == functions.end()) {
                functions.push_back(std::move(entry));
            } else {
                *existingFunction = std::move(entry);
            }
        }
    }
    if (cls->attributes & kJSClassAttributeNoAutomaticPrototype) {
        cls->staticFunctions = std::move(functions);
    } else {
        auto protoCls = std::make_unique<OpaqueJSClass>();
        protoCls->staticFunctions = std::move(functions);
        cls->prototypeClass = std::move(protoCls);
    }
    cls->initialize = definition->initialize;
    cls->finalize = definition->finalize;
    cls->hasProperty = definition->hasProperty;
    cls->getProperty = definition->getProperty;
    cls->setProperty = definition->setProperty;
    cls->deleteProperty = definition->deleteProperty;
    cls->getPropertyNames = definition->getPropertyNames;
    cls->callAsFunction = definition->callAsFunction;
    cls->callAsConstructor = definition->callAsConstructor;
    cls->hasInstance = definition->hasInstance;
    cls->convertToType = definition->convertToType;
    return cls;
}

JSClassRef APS5_VABI JSClassRetain(JSClassRef jsClass) {
    if (!jsClass) return nullptr;
    jsClass->refCount.fetch_add(1, std::memory_order_relaxed);
    return jsClass;
}

void APS5_VABI JSClassRelease(JSClassRef jsClass) {
    if (!jsClass) return;
    if (jsClass->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        jsClass->DestroyInPlace();
        delete jsClass;
    }
}

void APS5_VABI _ZN13OpaqueJSClassD1Ev(OpaqueJSClass* self) {
    if (!self) return;
    self->DestroyInPlace();
}

void APS5_VABI _ZN13OpaqueJSClassD2Ev(OpaqueJSClass* self) {
    if (!self) return;
    self->DestroyInPlace();
}

JSObjectRef APS5_VABI JSObjectMake(JSContextRef ctx, JSClassRef jsClass, void* data) {
    if (!ctx) return nullptr;
    return MakeCallbackObject(const_cast<OpaqueJSContext*>(ctx), jsClass, data);
}

JSObjectRef APS5_VABI JSObjectMakeFunctionWithCallback(JSContextRef ctx, JSStringRef name, JSObjectCallAsFunctionCallback callAsFunction) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSValue fnProto = JS_GetFunctionProto(mutCtx->qctx);
    JSValue rawObj = JS_NewObjectProtoClass(mutCtx->qctx, fnProto, mutCtx->group->callbackCallableClassId);
    JS_FreeValue(mutCtx->qctx, fnProto);

    void* ptr = JS_VALUE_GET_PTR(rawObj);
    OpaqueJSValue* handle = mutCtx->group->ToHandle(mutCtx, rawObj);

    auto* cbData = new CallbackObjectData();
    cbData->group = mutCtx->group;
    cbData->ctx = mutCtx;
    cbData->handle = handle;
    cbData->propStore = JS_NewObjectProto(mutCtx->qctx, JS_NULL);
    cbData->directFunctionCallback = callAsFunction;
    cbData->isDirectFunction = true;
    JS_SetOpaque(handle->qval, cbData);
    mutCtx->group->callbackDataByPtr[ptr] = cbData;

    const std::string fnName = name ? JsStringToUtf8(name) : std::string("anonymous");
    JS_DefinePropertyValueStr(mutCtx->qctx, cbData->propStore, "name", JS_NewString(mutCtx->qctx, fnName.c_str()), JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(mutCtx->qctx, cbData->propStore, "length", JS_NewInt32(mutCtx->qctx, 0), JS_PROP_CONFIGURABLE);
    return handle;
}

JSObjectRef APS5_VABI JSObjectMakeConstructor(JSContextRef ctx, JSClassRef jsClass, JSObjectCallAsConstructorCallback callAsConstructor) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSValue fnProto = JS_GetFunctionProto(mutCtx->qctx);
    JSValue rawObj = JS_NewObjectProtoClass(mutCtx->qctx, fnProto, mutCtx->group->callbackCallableClassId);
    JS_FreeValue(mutCtx->qctx, fnProto);
    JS_SetConstructorBit(mutCtx->qctx, rawObj, true);

    void* ptr = JS_VALUE_GET_PTR(rawObj);
    OpaqueJSValue* handle = mutCtx->group->ToHandle(mutCtx, rawObj);

    auto* cbData = new CallbackObjectData();
    cbData->group = mutCtx->group;
    cbData->ctx = mutCtx;
    cbData->handle = handle;
    if (jsClass) {
        JSClassRetain(jsClass);
        cbData->jsClass = jsClass;
    }
    cbData->propStore = JS_NewObjectProto(mutCtx->qctx, JS_NULL);
    cbData->directConstructorCallback = callAsConstructor;
    cbData->isDirectConstructor = true;
    JS_SetOpaque(handle->qval, cbData);
    mutCtx->group->callbackDataByPtr[ptr] = cbData;

    OpaqueJSValue* classProto = jsClass ? GetOrCreateClassPrototype(mutCtx, jsClass) : nullptr;
    JSValue ctorProto = (classProto && classProto->alive) ? JS_DupValue(mutCtx->qctx, classProto->qval) : JS_GetClassProto(mutCtx->qctx, mutCtx->group->callbackObjectClassId);
    JS_DefinePropertyValueStr(mutCtx->qctx, cbData->propStore, "prototype", ctorProto, 0);
    return handle;
}

JSObjectRef APS5_VABI JSObjectMakeFunction(JSContextRef ctx, JSStringRef name, unsigned parameterCount, const JSStringRef parameterNames[], JSStringRef body, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception) {
    if (!ctx || !body) return nullptr;
    if (parameterCount > 0 && !parameterNames) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);

    const std::string fnName = (name && !name->nullString) ? JsStringToUtf8(name) : "anonymous";
    std::string parameters;
    for (unsigned i = 0; i < parameterCount; ++i) {
        if (!parameterNames[i]) return nullptr;
        if (i > 0) parameters += ", ";
        parameters += JsStringToUtf8(parameterNames[i]);
    }
    const std::string bodyText = JsStringToUtf8(body);
    if (const char* message = SingleFunctionError(mutCtx->qctx, fnName, parameters, bodyText)) {
        JS_ThrowSyntaxError(mutCtx->qctx, "%s", message);
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }

    const std::string code = "(function " + fnName + "(" + parameters + ") {\n" + bodyText + "\n})";
    const std::string url = sourceURL ? JsStringToUtf8(sourceURL) : std::string();
    const int clampedLine = startingLineNumber <= 0 ? 1 : startingLineNumber;

    JSEvalOptions options{};
    options.version = JS_EVAL_OPTIONS_VERSION;
    options.eval_flags = JS_EVAL_TYPE_GLOBAL;
    options.filename = url.c_str();
    options.line_num = clampedLine;

    JSValue fnVal = JS_Eval2(mutCtx->qctx, code.c_str(), code.size(), &options);
    if (JS_IsException(fnVal)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, fnVal);
}

JSObjectRef APS5_VABI JSObjectMakeArray(JSContextRef ctx, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSValue arr = JS_NewArray(mutCtx->qctx);
    if (JS_IsException(arr)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    for (std::size_t i = 0; i < argumentCount; ++i) {
        JSValue elem = JS_DupValue(mutCtx->qctx, ArgumentValue(arguments, i));
        if (JS_DefinePropertyValueUint32(mutCtx->qctx, arr, static_cast<std::uint32_t>(i), elem, JS_PROP_C_W_E) < 0) {
            JS_FreeValue(mutCtx->qctx, arr);
            CapturePendingException(mutCtx, exception);
            return nullptr;
        }
    }
    return mutCtx->group->ToHandle(mutCtx, arr);
}

JSObjectRef APS5_VABI JSObjectMakeDate(JSContextRef ctx, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    std::vector<JSValueConst> args(argumentCount);
    VmEntryScope vmScope(mutCtx->group);
    for (std::size_t i = 0; i < argumentCount; ++i) {
        args[i] = ArgumentValue(arguments, i);
    }
    JSValue dateObj = JS_CallConstructor(mutCtx->qctx, mutCtx->dateConstructor, static_cast<int>(argumentCount), args.empty() ? nullptr : args.data());
    if (JS_IsException(dateObj)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, dateObj);
}

JSObjectRef APS5_VABI JSObjectMakeError(JSContextRef ctx, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    std::vector<JSValueConst> args(argumentCount);
    VmEntryScope vmScope(mutCtx->group);
    for (std::size_t i = 0; i < argumentCount; ++i) {
        args[i] = ArgumentValue(arguments, i);
    }
    JSValue errObj = JS_CallConstructor(mutCtx->qctx, mutCtx->errorConstructor, static_cast<int>(argumentCount), args.empty() ? nullptr : args.data());
    if (JS_IsException(errObj)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, errObj);
}

JSObjectRef APS5_VABI JSObjectMakeRegExp(JSContextRef ctx, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    std::vector<JSValueConst> args(argumentCount);
    VmEntryScope vmScope(mutCtx->group);
    for (std::size_t i = 0; i < argumentCount; ++i) {
        args[i] = ArgumentValue(arguments, i);
    }
    JSValue regExpObj = JS_CallConstructor(mutCtx->qctx, mutCtx->regExpConstructor, static_cast<int>(argumentCount), args.empty() ? nullptr : args.data());
    if (JS_IsException(regExpObj)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, regExpObj);
}

JSObjectRef APS5_VABI JSObjectMakeDeferredPromise(JSContextRef ctx, JSObjectRef* resolve, JSObjectRef* reject, JSValueRef* exception) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSValue funcs[2] = {JS_UNDEFINED, JS_UNDEFINED};
    JSValue promise = JS_NewPromiseCapability(mutCtx->qctx, funcs);
    if (JS_IsException(promise)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    if (resolve) {
        *resolve = mutCtx->group->ToHandle(mutCtx, funcs[0]);
    } else {
        JS_FreeValue(mutCtx->qctx, funcs[0]);
    }
    if (reject) {
        *reject = mutCtx->group->ToHandle(mutCtx, funcs[1]);
    } else {
        JS_FreeValue(mutCtx->qctx, funcs[1]);
    }
    return mutCtx->group->ToHandle(mutCtx, promise);
}

JSValueRef APS5_VABI JSObjectGetPrototype(JSContextRef ctx, JSObjectRef object) {
    if (!ctx || !object || !object->alive) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    if (object == mutCtx->globalObject && !JS_IsUndefined(mutCtx->globalExoticInterceptor)) {
        JSValue proto = JS_GetPrototype(mutCtx->qctx, mutCtx->globalExoticInterceptor);
        return mutCtx->group->ToHandle(mutCtx, proto);
    }
    JSValue proto = JS_GetPrototype(mutCtx->qctx, object->qval);
    if (JS_IsException(proto)) {
        JS_FreeValue(mutCtx->qctx, JS_GetException(mutCtx->qctx));
        return &mutCtx->group->nullHandle;
    }
    return mutCtx->group->ToHandle(mutCtx, proto);
}

void APS5_VABI JSObjectSetPrototype(JSContextRef ctx, JSObjectRef object, JSValueRef value) {
    if (!ctx || !object || !object->alive || (value && !value->alive)) return;
    for (const OpaqueJSContext* context : ctx->group->contexts) {
        if (context && context->globalObject == object) return;
    }
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSValueConst protoVal = (value && value->isObjectHandle) ? value->qval : JS_NULL;
    if (JS_SetPrototype(mutCtx->qctx, object->qval, protoVal) < 0) {
        JS_FreeValue(mutCtx->qctx, JS_GetException(mutCtx->qctx));
    }
}

bool APS5_VABI JSObjectHasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
    if (!ctx || !object || !propertyName || !object->alive) return false;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSAtom atom = JsStringToAtom(mutCtx->qctx, propertyName);
    if (atom == JS_ATOM_NULL) {
        if (JS_HasException(mutCtx->qctx)) {
            JS_FreeValue(mutCtx->qctx, JS_GetException(mutCtx->qctx));
        }
        return false;
    }
    const int res = JS_HasProperty(mutCtx->qctx, object->qval, atom);
    JS_FreeAtom(mutCtx->qctx, atom);
    if (res < 0) {
        JS_FreeValue(mutCtx->qctx, JS_GetException(mutCtx->qctx));
        return false;
    }
    return res != 0;
}

JSValueRef APS5_VABI JSObjectGetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    if (!ctx || !object || !propertyName || !object->alive) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSAtom atom = JsStringToAtom(mutCtx->qctx, propertyName);
    if (atom == JS_ATOM_NULL) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    JSValue val = JS_GetProperty(mutCtx->qctx, object->qval, atom);
    JS_FreeAtom(mutCtx->qctx, atom);
    if (JS_IsException(val)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, val);
}

void APS5_VABI JSObjectSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception) {
    if (!ctx || !object || !propertyName || !object->alive) return;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSAtom atom = JsStringToAtom(mutCtx->qctx, propertyName);
    if (atom == JS_ATOM_NULL) {
        CapturePendingException(mutCtx, exception);
        return;
    }
    JSValue val = JS_DupValue(mutCtx->qctx, StoredValue(value));
    const int present = attributes != kJSPropertyAttributeNone ? JS_HasProperty(mutCtx->qctx, object->qval, atom) : 1;
    if (present < 0) {
        JS_FreeValue(mutCtx->qctx, val);
        JS_FreeAtom(mutCtx->qctx, atom);
        CapturePendingException(mutCtx, exception);
        return;
    }
    if (!present) {
        const int flags = ToQuickJsPropFlags(attributes) | JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE | JS_PROP_HAS_ENUMERABLE | JS_PROP_HAS_CONFIGURABLE;
        CallbackObjectData* cbData = mutCtx->group->FindCallbackData(object);
        JSValue target = (cbData && !JS_IsUndefined(cbData->propStore) && object != mutCtx->globalObject) ? cbData->propStore : object->qval;
        const int res = JS_DefinePropertyValue(mutCtx->qctx, target, atom, val, flags);
        JS_FreeAtom(mutCtx->qctx, atom);
        if (res < 0) {
            CapturePendingException(mutCtx, exception);
        }
        return;
    }

    JSValue propVal = JS_AtomToValue(mutCtx->qctx, atom);
    JS_FreeAtom(mutCtx->qctx, atom);
    const int res = JS_SetPropertyValue(mutCtx->qctx, object->qval, propVal, val, 0);
    if (res < 0) {
        CapturePendingException(mutCtx, exception);
    }
}

bool APS5_VABI JSObjectDeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    if (!ctx || !object || !propertyName || !object->alive) return false;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSAtom atom = JsStringToAtom(mutCtx->qctx, propertyName);
    if (atom == JS_ATOM_NULL) {
        CapturePendingException(mutCtx, exception);
        return false;
    }
    const int res = JS_DeleteProperty(mutCtx->qctx, object->qval, atom, 0);
    JS_FreeAtom(mutCtx->qctx, atom);
    if (res < 0) {
        CapturePendingException(mutCtx, exception);
        return false;
    }
    return res != 0;
}

JSValueRef APS5_VABI JSObjectGetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef* exception) {
    if (!ctx || !object || !object->alive) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSValue val = JS_GetPropertyUint32(mutCtx->qctx, object->qval, propertyIndex);
    if (JS_IsException(val)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, val);
}

void APS5_VABI JSObjectSetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef value, JSValueRef* exception) {
    if (!ctx || !object || !object->alive) return;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSValue val = JS_DupValue(mutCtx->qctx, StoredValue(value));
    const int res = JS_SetPropertyValue(mutCtx->qctx, object->qval, JS_NewUint32(mutCtx->qctx, propertyIndex), val, 0);
    if (res < 0) {
        CapturePendingException(mutCtx, exception);
    }
}

JSValueRef APS5_VABI JSObjectGetPropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef propertyKey, JSValueRef* exception) {
    if (!ctx || !object || !object->alive || (propertyKey && !propertyKey->alive)) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSValue val = JS_GetPropertyValue(mutCtx->qctx, object->qval, JS_DupValue(mutCtx->qctx, ValueOrNull(propertyKey)));
    if (JS_IsException(val)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, val);
}

void APS5_VABI JSObjectSetPropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef propertyKey, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception) {
    if (!ctx || !object || !object->alive || (propertyKey && !propertyKey->alive)) return;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSAtom atom = JS_ValueToAtom(mutCtx->qctx, ValueOrNull(propertyKey));
    if (atom == JS_ATOM_NULL) {
        CapturePendingException(mutCtx, exception);
        return;
    }
    JSValue val = JS_DupValue(mutCtx->qctx, StoredValue(value));
    const int present = attributes != kJSPropertyAttributeNone ? JS_HasProperty(mutCtx->qctx, object->qval, atom) : 1;
    if (present < 0) {
        JS_FreeValue(mutCtx->qctx, val);
        JS_FreeAtom(mutCtx->qctx, atom);
        CapturePendingException(mutCtx, exception);
        return;
    }
    if (!present) {
        const int flags = ToQuickJsPropFlags(attributes) | JS_PROP_HAS_VALUE | JS_PROP_HAS_WRITABLE | JS_PROP_HAS_ENUMERABLE | JS_PROP_HAS_CONFIGURABLE;
        CallbackObjectData* cbData = mutCtx->group->FindCallbackData(object);
        JSValue target = (cbData && !JS_IsUndefined(cbData->propStore) && object != mutCtx->globalObject) ? cbData->propStore : object->qval;
        const int res = JS_DefinePropertyValue(mutCtx->qctx, target, atom, val, flags);
        JS_FreeAtom(mutCtx->qctx, atom);
        if (res < 0) {
            CapturePendingException(mutCtx, exception);
        }
        return;
    }
    JSValue propVal = JS_AtomToValue(mutCtx->qctx, atom);
    JS_FreeAtom(mutCtx->qctx, atom);
    const int res = JS_SetPropertyValue(mutCtx->qctx, object->qval, propVal, val, 0);
    if (res < 0) {
        CapturePendingException(mutCtx, exception);
    }
}

bool APS5_VABI JSObjectDeletePropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef propertyKey, JSValueRef* exception) {
    if (!ctx || !object || !object->alive || (propertyKey && !propertyKey->alive)) return false;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    JSAtom atom = JS_ValueToAtom(mutCtx->qctx, ValueOrNull(propertyKey));
    if (atom == JS_ATOM_NULL) {
        CapturePendingException(mutCtx, exception);
        return false;
    }
    const int res = JS_DeleteProperty(mutCtx->qctx, object->qval, atom, 0);
    JS_FreeAtom(mutCtx->qctx, atom);
    if (res < 0) {
        CapturePendingException(mutCtx, exception);
        return false;
    }
    return res != 0;
}

void* APS5_VABI JSObjectGetPrivate(JSObjectRef object) {
    if (!object || !object->alive || !object->group) return nullptr;
    CallbackObjectData* data = object->group->FindCallbackData(object);
    if (!data || !ObjectClass(data)) return nullptr;
    return data->privateData;
}

bool APS5_VABI JSObjectSetPrivate(JSObjectRef object, void* data) {
    if (!object || !object->alive || !object->group) return false;
    CallbackObjectData* cbData = object->group->FindCallbackData(object);
    if (!cbData || !ObjectClass(cbData)) return false;
    cbData->privateData = data;
    return true;
}

JSValueRef APS5_VABI JSObjectGetPrivateProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
    if (!ctx || !object || !propertyName || !object->alive) return nullptr;
    CallbackObjectData* cbData = ctx->group->FindCallbackData(object);
    if (!cbData || !ObjectClass(cbData)) return nullptr;
    const std::string key = JsStringToUtf8(propertyName);
    auto it = cbData->privateProperties.find(key);
    if (it == cbData->privateProperties.end() || !it->second.handle || !it->second.handle->alive) return nullptr;
    return it->second.handle;
}

bool APS5_VABI JSObjectSetPrivateProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value) {
    if (!ctx || !object || !propertyName || !object->alive) return false;
    CallbackObjectData* cbData = ctx->group->FindCallbackData(object);
    if (!cbData || !ObjectClass(cbData)) return false;
    const std::string key = JsStringToUtf8(propertyName);
    auto existing = cbData->privateProperties.find(key);
    if (existing != cbData->privateProperties.end()) {
        if (!JS_IsUndefined(existing->second.strongVal)) {
            JS_FreeValueRT(ctx->group->rt, existing->second.strongVal);
        }
        cbData->privateProperties.erase(existing);
    }
    if (value && value->alive) {
        PrivatePropertySlot slot;
        slot.handle = const_cast<OpaqueJSValue*>(value);
        slot.strongVal = JS_DupValueRT(ctx->group->rt, value->qval);
        cbData->privateProperties[key] = slot;
    }
    return true;
}

bool APS5_VABI JSObjectDeletePrivateProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
    if (!ctx || !object || !propertyName || !object->alive) return false;
    CallbackObjectData* cbData = ctx->group->FindCallbackData(object);
    if (!cbData || !ObjectClass(cbData)) return false;
    const std::string key = JsStringToUtf8(propertyName);
    auto existing = cbData->privateProperties.find(key);
    if (existing != cbData->privateProperties.end()) {
        if (!JS_IsUndefined(existing->second.strongVal)) {
            JS_FreeValueRT(ctx->group->rt, existing->second.strongVal);
        }
        cbData->privateProperties.erase(existing);
    }
    return true;
}

bool APS5_VABI JSObjectIsFunction(JSContextRef ctx, JSObjectRef object) {
    return IsObjectCallable(ctx, object);
}

JSValueRef APS5_VABI JSObjectCallAsFunction(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    if (!ctx || !object || !object->alive) return nullptr;
    if (!JSObjectIsFunction(ctx, object)) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSValueConst thisVal = (thisObject && thisObject->alive) ? thisObject->qval : mutCtx->globalObject->qval;
    std::vector<JSValueConst> args(argumentCount);
    for (std::size_t i = 0; i < argumentCount; ++i) {
        args[i] = ArgumentValue(arguments, i);
    }
    VmEntryScope vmScope(mutCtx->group);
    JSValue res = JS_Call(mutCtx->qctx, object->qval, thisVal, static_cast<int>(argumentCount), args.empty() ? nullptr : args.data());
    if (JS_IsException(res)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    OpaqueJSValue* handle = mutCtx->group->ToHandle(mutCtx, res);
    return handle;
}

bool APS5_VABI JSObjectIsConstructor(JSContextRef ctx, JSObjectRef object) {
    if (!ctx || !object || !object->alive || !object->isObjectHandle) return false;
    if (CallbackObjectData* cbData = ctx->group->FindCallbackData(object)) {
        if (cbData->isDirectConstructor) return true;
        if (cbData->isDirectFunction) return false;
        return cbData->jsClass && cbData->jsClass->HasCallAsConstructor();
    }
    return JS_IsConstructor(ctx->qctx, object->qval);
}

JSObjectRef APS5_VABI JSObjectCallAsConstructor(JSContextRef ctx, JSObjectRef object, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    if (!ctx || !object || !object->alive) return nullptr;
    if (!JSObjectIsConstructor(ctx, object)) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    std::vector<JSValueConst> args(argumentCount);
    for (std::size_t i = 0; i < argumentCount; ++i) {
        args[i] = ArgumentValue(arguments, i);
    }
    VmEntryScope vmScope(mutCtx->group);
    JSValue res = JS_CallConstructor(mutCtx->qctx, object->qval, static_cast<int>(argumentCount), args.empty() ? nullptr : args.data());
    if (JS_IsException(res)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    OpaqueJSValue* handle = mutCtx->group->ToHandle(mutCtx, res);
    return handle;
}

JSPropertyNameArrayRef APS5_VABI JSObjectCopyPropertyNames(JSContextRef ctx, JSObjectRef object) {
    if (!ctx || !object || !object->alive) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    VmEntryScope vmScope(mutCtx->group);
    OpaqueJSPropertyNameAccumulator acc;

    JSValue cur = JS_DupValue(mutCtx->qctx, object->qval);
    while (JS_IsObject(cur)) {
        void* ptr = JS_VALUE_GET_PTR(cur);
        if (CallbackObjectData* cbData = mutCtx->group->FindCallbackDataByPtr(ptr)) {
            OpaqueJSValue* targetHandle = cbData->handle ? cbData->handle : object;
            CallbackPinScope pinScope(mutCtx->group);
            pinScope.Pin(targetHandle);
            for (OpaqueJSClass* cls = cbData->jsClass; cls; cls = cls->parentClass) {
                if (cls->getPropertyNames) {
                    cls->getPropertyNames(mutCtx, targetHandle, &acc);
                }
                for (const auto& sv : cls->staticValues) {
                    if (sv.getProperty && !(sv.attributes & kJSPropertyAttributeDontEnum)) {
                        acc.AddName(sv.propertyNameRef);
                    }
                }
                for (const auto& sf : cls->staticFunctions) {
                    if (!(sf.attributes & kJSPropertyAttributeDontEnum)) {
                        JSStringRef s = Utf8ToJsString(sf.name);
                        acc.AddName(s);
                        JSStringRelease_nid_postfix(s);
                    }
                }
            }
            if (!JS_IsUndefined(cbData->propStore)) {
                JSPropertyEnum* storeTab = nullptr;
                std::uint32_t storeLen = 0;
                if (JS_GetOwnPropertyNames(mutCtx->qctx, &storeTab, &storeLen, cbData->propStore, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
                    for (std::uint32_t i = 0; i < storeLen; ++i) {
                        JSStringRef s = AtomToJsString(mutCtx->qctx, storeTab[i].atom);
                        if (s) {
                            acc.AddName(s);
                            JSStringRelease_nid_postfix(s);
                        }
                    }
                    JS_FreePropertyEnum(mutCtx->qctx, storeTab, storeLen);
                }
            }
        } else {
            JSPropertyEnum* tab = nullptr;
            std::uint32_t len = 0;
            if (JS_GetOwnPropertyNames(mutCtx->qctx, &tab, &len, cur, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
                for (std::uint32_t i = 0; i < len; ++i) {
                    JSStringRef s = AtomToJsString(mutCtx->qctx, tab[i].atom);
                    if (s) {
                        acc.AddName(s);
                        JSStringRelease_nid_postfix(s);
                    }
                }
                JS_FreePropertyEnum(mutCtx->qctx, tab, len);
            } else {
                JS_FreeValue(mutCtx->qctx, JS_GetException(mutCtx->qctx));
                break;
            }
        }
        JSValue next = JS_GetPrototype(mutCtx->qctx, cur);
        JS_FreeValue(mutCtx->qctx, cur);
        if (JS_IsException(next)) {
            JS_FreeValue(mutCtx->qctx, JS_GetException(mutCtx->qctx));
            break;
        }
        cur = next;
    }
    JS_FreeValue(mutCtx->qctx, cur);

    auto* array = new OpaqueJSPropertyNameArray();
    array->names = std::move(acc.names);
    acc.names.clear();
    return array;
}

JSPropertyNameArrayRef APS5_VABI JSPropertyNameArrayRetain(JSPropertyNameArrayRef array) {
    if (!array) return nullptr;
    array->refCount.fetch_add(1, std::memory_order_relaxed);
    return array;
}

void APS5_VABI JSPropertyNameArrayRelease(JSPropertyNameArrayRef array) {
    if (!array) return;
    if (array->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete array;
    }
}

std::size_t APS5_VABI JSPropertyNameArrayGetCount(JSPropertyNameArrayRef array) {
    if (!array) return 0;
    return array->names.size();
}

JSStringRef APS5_VABI JSPropertyNameArrayGetNameAtIndex(JSPropertyNameArrayRef array, std::size_t index) {
    if (!array || index >= array->names.size()) return nullptr;
    return array->names[index];
}

void APS5_VABI JSPropertyNameAccumulatorAddName(JSPropertyNameAccumulatorRef accumulator, JSStringRef propertyName) {
    if (!accumulator || !propertyName) return;
    accumulator->AddName(propertyName);
}

JSObjectRef APS5_VABI JSObjectGetProxyTarget(JSObjectRef object) {
    if (!object || !object->alive || !object->isObjectHandle || !object->group) return nullptr;
    if (!JS_IsProxy(object->qval)) return nullptr;
    OpaqueJSContext* ctx = object->ctx ? object->ctx : object->group->DefaultContext();
    JSContext* qctx = ctx ? ctx->qctx : object->group->utilityCtx;
    JSValue target = JS_GetProxyTarget(qctx, object->qval);
    if (JS_IsException(target)) {
        JS_FreeValue(qctx, JS_GetException(qctx));
        return nullptr;
    }
    return object->group->ToHandle(ctx, target);
}

JSGlobalContextRef APS5_VABI JSObjectGetGlobalContext(JSObjectRef object) {
    if (!object || !object->alive) return nullptr;
    return object->ctx ? object->ctx : (object->group ? object->group->DefaultContext() : nullptr);
}

JSObjectRef APS5_VABI JSObjectMakeArrayBufferWithBytesNoCopy(JSContextRef ctx, void* bytes, std::size_t byteLength, JSTypedArrayBytesDeallocator bytesDeallocator, void* deallocatorContext, JSValueRef* exception) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    if (!bytes && byteLength > 0) {
        SetExceptionValue(mutCtx, exception, JS_NewTypeError(mutCtx->qctx, "Bytes pointer is null but byte length is non-zero"));
        return nullptr;
    }
    auto* record = new NoCopyBufferRecord{bytesDeallocator, deallocatorContext, bytes};
    std::uint8_t* rawBuf = bytes ? static_cast<std::uint8_t*>(bytes) : &g_emptyBufferByte;
    JSValue abuf = JS_NewArrayBuffer(mutCtx->qctx, rawBuf, byteLength, 0, &ArrayBufferNoCopyRealloc, record, false);
    if (JS_IsException(abuf)) {
        delete record;
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, abuf);
}

void* APS5_VABI JSObjectGetArrayBufferBytesPtr(JSContextRef ctx, JSObjectRef object, JSValueRef* exception) {
    (void)exception;
    if (!ctx || !object || !object->alive || !JS_IsArrayBuffer(object->qval)) return nullptr;
    std::size_t size = 0;
    std::uint8_t* ptr = JS_GetArrayBuffer(ctx->qctx, &size, object->qval);
    if (!ptr && JS_HasException(ctx->qctx)) {
        DropPendingException(ctx->qctx);
        return nullptr;
    }
    if (ptr == &g_emptyBufferByte && size == 0) return nullptr;
    return ptr;
}

std::size_t APS5_VABI JSObjectGetArrayBufferByteLength(JSContextRef ctx, JSObjectRef object, JSValueRef* exception) {
    (void)exception;
    if (!object) throw std::invalid_argument("JSObjectGetArrayBufferByteLength: null object");
    if (!object->alive || !object->group || !JS_IsArrayBuffer(object->qval)) return 0;
    JSContext* qctx = QuickJsContextOf(ctx, object);
    std::size_t size = 0;
    if (!JS_GetArrayBuffer(qctx, &size, object->qval) && JS_HasException(qctx)) {
        DropPendingException(qctx);
        return 0;
    }
    return size;
}

JSObjectRef APS5_VABI JSObjectMakeTypedArray(JSContextRef ctx, JSTypedArrayType arrayType, std::size_t length, JSValueRef* exception) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSTypedArrayEnum qtype{};
    std::size_t elemSize = 0;
    if (!ToQuickJsTypedArrayEnum(arrayType, &qtype, &elemSize)) return nullptr;
    JSValue lenVal = JS_NewUint64(mutCtx->qctx, length);
    JSValueConst arg = lenVal;
    JSValue ta = JS_NewTypedArray(mutCtx->qctx, 1, &arg, qtype);
    JS_FreeValue(mutCtx->qctx, lenVal);
    if (JS_IsException(ta)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, ta);
}

JSObjectRef APS5_VABI JSObjectMakeTypedArrayWithArrayBuffer(JSContextRef ctx, JSTypedArrayType arrayType, JSObjectRef buffer, JSValueRef* exception) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSTypedArrayEnum qtype{};
    std::size_t elemSize = 0;
    if (!ToQuickJsTypedArrayEnum(arrayType, &qtype, &elemSize)) return nullptr;
    if (!buffer || !buffer->alive || !JS_IsArrayBuffer(buffer->qval)) {
        SetExceptionValue(mutCtx, exception, JS_NewTypeError(mutCtx->qctx, "JSObjectMakeTypedArrayWithArrayBuffer expects buffer to be an Array Buffer object"));
        return nullptr;
    }
    JSValue resizable = CallIntrinsicGetter(mutCtx->group, mutCtx->qctx, buffer->qval, "ArrayBuffer", false, "resizable");
    if (JS_IsException(resizable)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    const bool isResizable = JS_ToBool(mutCtx->qctx, resizable) > 0;
    JS_FreeValue(mutCtx->qctx, resizable);
    if (isResizable) throw std::invalid_argument("JSObjectMakeTypedArrayWithArrayBuffer: resizable buffer");
    std::size_t byteLength = 0;
    if (!JS_GetArrayBuffer(mutCtx->qctx, &byteLength, buffer->qval) && JS_HasException(mutCtx->qctx)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return JSObjectMakeTypedArrayWithArrayBufferAndOffset(ctx, arrayType, buffer, 0, byteLength / elemSize, exception);
}

JSObjectRef APS5_VABI JSObjectMakeTypedArrayWithArrayBufferAndOffset(JSContextRef ctx, JSTypedArrayType arrayType, JSObjectRef buffer, std::size_t byteOffset, std::size_t length, JSValueRef* exception) {
    if (!ctx) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSTypedArrayEnum qtype{};
    std::size_t elemSize = 0;
    if (!ToQuickJsTypedArrayEnum(arrayType, &qtype, &elemSize)) return nullptr;
    if (!buffer || !buffer->alive || !JS_IsArrayBuffer(buffer->qval)) {
        SetExceptionValue(mutCtx, exception, JS_NewTypeError(mutCtx->qctx, "JSObjectMakeTypedArrayWithArrayBuffer expects buffer to be an Array Buffer object"));
        return nullptr;
    }
    JSValue offsetVal = JS_NewUint64(mutCtx->qctx, byteOffset);
    JSValue lenVal = JS_NewUint64(mutCtx->qctx, length);
    JSValueConst args[3] = {buffer->qval, offsetVal, lenVal};
    JSValue ta = JS_NewTypedArray(mutCtx->qctx, 3, args, qtype);
    JS_FreeValue(mutCtx->qctx, offsetVal);
    JS_FreeValue(mutCtx->qctx, lenVal);
    if (JS_IsException(ta)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, ta);
}

JSObjectRef APS5_VABI JSObjectMakeTypedArrayWithBytesNoCopy(JSContextRef ctx, JSTypedArrayType arrayType, void* bytes, std::size_t byteLength, JSTypedArrayBytesDeallocator bytesDeallocator, void* deallocatorContext, JSValueRef* exception) {
    if (!ctx) return nullptr;
    JSTypedArrayEnum qtype{};
    std::size_t elemSize = 0;
    if (!ToQuickJsTypedArrayEnum(arrayType, &qtype, &elemSize)) return nullptr;
    JSValueRef localExc = nullptr;
    JSObjectRef abuf = JSObjectMakeArrayBufferWithBytesNoCopy(ctx, bytes, byteLength, bytesDeallocator, deallocatorContext, &localExc);
    if (!abuf) {
        if (exception) *exception = localExc;
        return nullptr;
    }
    return JSObjectMakeTypedArrayWithArrayBufferAndOffset(ctx, arrayType, abuf, 0, byteLength / elemSize, exception);
}

void* APS5_VABI JSObjectGetTypedArrayBytesPtr(JSContextRef ctx, JSObjectRef object, JSValueRef* exception) {
    if (!ctx || !object || !object->alive || !IsBufferView(object->qval)) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSValue abuf = BufferOfView(mutCtx->group, mutCtx->qctx, object->qval);
    if (JS_IsException(abuf)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    std::size_t bufSize = 0;
    std::uint8_t* ptr = JS_GetArrayBuffer(mutCtx->qctx, &bufSize, abuf);
    JS_FreeValue(mutCtx->qctx, abuf);
    if (!ptr && JS_HasException(mutCtx->qctx)) {
        DropPendingException(mutCtx->qctx);
        return nullptr;
    }
    if (ptr == &g_emptyBufferByte && bufSize == 0) return nullptr;
    return ptr;
}

std::size_t APS5_VABI JSObjectGetTypedArrayLength(JSContextRef ctx, JSObjectRef object, JSValueRef* exception) {
    (void)exception;
    return ReadBufferViewMetrics(ctx, object).length;
}

std::size_t APS5_VABI JSObjectGetTypedArrayByteLength(JSContextRef ctx, JSObjectRef object, JSValueRef* exception) {
    (void)exception;
    return ReadBufferViewMetrics(ctx, object).byteLength;
}

std::size_t APS5_VABI JSObjectGetTypedArrayByteOffset(JSContextRef ctx, JSObjectRef object, JSValueRef* exception) {
    (void)exception;
    return ReadBufferViewMetrics(ctx, object).byteOffset;
}

JSObjectRef APS5_VABI JSObjectGetTypedArrayBuffer(JSContextRef ctx, JSObjectRef object, JSValueRef* exception) {
    if (!ctx || !object || !object->alive || !IsBufferView(object->qval)) return nullptr;
    auto* mutCtx = const_cast<OpaqueJSContext*>(ctx);
    JSValue abuf = BufferOfView(mutCtx->group, mutCtx->qctx, object->qval);
    if (JS_IsException(abuf)) {
        CapturePendingException(mutCtx, exception);
        return nullptr;
    }
    return mutCtx->group->ToHandle(mutCtx, abuf);
}

}
