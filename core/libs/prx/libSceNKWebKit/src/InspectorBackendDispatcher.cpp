// Derived from Source/JavaScriptCore/inspector/InspectorBackendDispatcher.cpp@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WTF/wtf/HashTable.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2013-2017 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 The Chromium Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/InspectorBackendDispatcherExports.hpp"
#include "prx/libSceNKWebKit/include/InspectorFrontendRouterExports.hpp"
#include "prx/libSceNKWebKit/include/JsonExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::InspectorAbi::BackendDispatcher;
using NKWebKit::InspectorAbi::CallbackBase;
using NKWebKit::InspectorAbi::CommonErrorCode;
using NKWebKit::InspectorAbi::DispatcherMapEntry;
using NKWebKit::InspectorAbi::FrontendRouter;
using NKWebKit::InspectorAbi::ProtocolError;
using NKWebKit::InspectorAbi::SupplementalBackendDispatcher;
using NKWebKit::InspectorAbi::SupplementalBackendDispatcherVtable;
using NKWebKit::JsonAbi::Array;
using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::RefPtr;
using NKWebKit::JsonAbi::Value;
using NKWebKit::JsonAbi::ValueType;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

constexpr std::size_t MetadataSize = 4 * sizeof(unsigned);
constexpr unsigned MinimumTableSize = 8;
constexpr unsigned MinLoad = 6;
constexpr unsigned MaxLoad = 2;
constexpr int TableSizeOffset = -1;
constexpr int TableSizeMaskOffset = -2;
constexpr int KeyCountOffset = -3;
constexpr int DeletedCountOffset = -4;
constexpr unsigned HashFlagCount = 8;
constexpr int ErrorCodes[] = {-32700, -32600, -32601, -32602, -32603, -32000};

template<typename T> T& Checked(T* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + " requires an object");
    return *value;
}

String Literal(const char* text) {
    const unsigned length = static_cast<unsigned>(std::strlen(text));
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(length, true, characters));
    std::memcpy(characters, text, length);
    return result;
}

String Substring(const String& string, unsigned start, unsigned length) {
    const bool is8Bit = string.impl->hashAndFlags & (1U << 2);
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(length, is8Bit, characters));
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    for (unsigned i = 0; i < length; ++i) {
        char16_t codeUnit = NKWebKit::WtfAbi::CodeUnitAt(view, start + i);
        if (is8Bit) static_cast<unsigned char*>(characters)[i] = static_cast<unsigned char>(codeUnit);
        else static_cast<char16_t*>(characters)[i] = codeUnit;
    }
    return result;
}

unsigned* Metadata(DispatcherMapEntry* table) {
    return reinterpret_cast<unsigned*>(table);
}

StringImpl* DeletedKey() {
    return reinterpret_cast<StringImpl*>(~std::uintptr_t(0));
}

unsigned KeyHash(const StringImpl* key) {
    unsigned hash = key->hashAndFlags >> HashFlagCount;
    return hash ? hash : _ZNK3WTF10StringImpl12hashSlowCaseEv(key);
}

DispatcherMapEntry* AllocateTable(unsigned tableSize) {
    auto* storage = static_cast<unsigned char*>(_ZN3WTF16fastZeroedMallocEm(MetadataSize + std::size_t(tableSize) * sizeof(DispatcherMapEntry)));
    auto* table = reinterpret_cast<DispatcherMapEntry*>(storage + MetadataSize);
    Metadata(table)[TableSizeOffset] = tableSize;
    Metadata(table)[TableSizeMaskOffset] = tableSize - 1U;
    Metadata(table)[DeletedCountOffset] = 0U;
    Metadata(table)[KeyCountOffset] = 0U;
    return table;
}

void FreeTable(DispatcherMapEntry* table) {
    _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(table) - MetadataSize);
}

DispatcherMapEntry* FindEntry(DispatcherMapEntry* table, const String& key) {
    if (!table) return nullptr;
    unsigned sizeMask = Metadata(table)[TableSizeMaskOffset];
    unsigned i = KeyHash(key.impl) & sizeMask;
    unsigned probeCount = 0;
    while (true) {
        DispatcherMapEntry* entry = table + i;
        if (!entry->key.impl) return nullptr;
        if (entry->key.impl != DeletedKey() && _ZN3WTF5equalEPKNS_10StringImplES2_(entry->key.impl, key.impl)) return entry;
        ++probeCount;
        i = (i + probeCount) & sizeMask;
    }
}

void Rehash(BackendDispatcher& dispatcher, unsigned newTableSize) {
    DispatcherMapEntry* oldTable = dispatcher.dispatchers;
    unsigned oldTableSize = oldTable ? Metadata(oldTable)[TableSizeOffset] : 0U;
    DispatcherMapEntry* table = AllocateTable(newTableSize);
    unsigned sizeMask = newTableSize - 1U;
    for (unsigned j = 0; j < oldTableSize; ++j) {
        DispatcherMapEntry& oldEntry = oldTable[j];
        if (!oldEntry.key.impl || oldEntry.key.impl == DeletedKey()) continue;
        unsigned i = KeyHash(oldEntry.key.impl) & sizeMask;
        unsigned probeCount = 0;
        while (table[i].key.impl) {
            ++probeCount;
            i = (i + probeCount) & sizeMask;
        }
        table[i].key.impl = std::exchange(oldEntry.key.impl, nullptr);
        table[i].value = oldEntry.value;
        Metadata(table)[KeyCountOffset] += 1U;
    }
    if (oldTable) FreeTable(oldTable);
    dispatcher.dispatchers = table;
}

void Expand(BackendDispatcher& dispatcher) {
    if (!dispatcher.dispatchers) {
        Rehash(dispatcher, MinimumTableSize);
        return;
    }
    unsigned* metadata = Metadata(dispatcher.dispatchers);
    unsigned tableSize = metadata[TableSizeOffset];
    Rehash(dispatcher, metadata[KeyCountOffset] * MinLoad < tableSize * 2U ? tableSize : tableSize * 2U);
}

void SetDispatcher(BackendDispatcher& dispatcher, const String& key, SupplementalBackendDispatcher* value) {
    if (!dispatcher.dispatchers) Expand(dispatcher);
    DispatcherMapEntry* table = dispatcher.dispatchers;
    unsigned sizeMask = Metadata(table)[TableSizeMaskOffset];
    unsigned i = KeyHash(key.impl) & sizeMask;
    unsigned probeCount = 0;
    DispatcherMapEntry* deletedEntry = nullptr;
    DispatcherMapEntry* entry;
    while (true) {
        entry = table + i;
        if (!entry->key.impl) break;
        if (entry->key.impl == DeletedKey()) {
            if (!deletedEntry) deletedEntry = entry;
        } else if (_ZN3WTF5equalEPKNS_10StringImplES2_(entry->key.impl, key.impl)) {
            entry->value = value;
            return;
        }
        ++probeCount;
        i = (i + probeCount) & sizeMask;
    }
    unsigned* metadata = Metadata(table);
    if (deletedEntry) {
        entry = deletedEntry;
        metadata[DeletedCountOffset] -= 1U;
    }
    key.impl->refCount += 2U;
    entry->key.impl = key.impl;
    entry->value = value;
    metadata[KeyCountOffset] += 1U;
    if ((metadata[KeyCountOffset] + metadata[DeletedCountOffset]) * MaxLoad >= metadata[TableSizeOffset]) Expand(dispatcher);
}

void DestroyDispatchers(BackendDispatcher& dispatcher) {
    DispatcherMapEntry* table = std::exchange(dispatcher.dispatchers, nullptr);
    if (!table) return;
    unsigned tableSize = Metadata(table)[TableSizeOffset];
    for (unsigned i = 0; i < tableSize; ++i) {
        if (table[i].key.impl == DeletedKey()) continue;
        NKWebKit::WtfAbi::Release(std::exchange(table[i].key.impl, nullptr));
    }
    FreeTable(table);
}

void ClearProtocolErrors(BackendDispatcher& dispatcher) {
    for (unsigned i = 0; i < dispatcher.protocolErrorCount; ++i)
        NKWebKit::WtfAbi::Release(std::exchange(dispatcher.protocolErrors[i].message.impl, nullptr));
    if (dispatcher.protocolErrors) _ZN3WTF8fastFreeEPv(dispatcher.protocolErrors);
    dispatcher.protocolErrors = nullptr;
    dispatcher.protocolErrorCapacity = 0;
    dispatcher.protocolErrorCount = 0;
}

void AppendProtocolError(BackendDispatcher& dispatcher, CommonErrorCode code, const String& message) {
    if (dispatcher.protocolErrorCount == dispatcher.protocolErrorCapacity) {
        unsigned capacity = dispatcher.protocolErrorCapacity;
        unsigned newCapacity = std::max(dispatcher.protocolErrorCount + 1U, std::max(16U, capacity + capacity / 4U + 1U));
        auto* buffer = static_cast<ProtocolError*>(_ZN3WTF10fastMallocEm(std::size_t(newCapacity) * sizeof(ProtocolError)));
        if (dispatcher.protocolErrors) {
            std::memcpy(static_cast<void*>(buffer), static_cast<const void*>(dispatcher.protocolErrors), std::size_t(dispatcher.protocolErrorCount) * sizeof(ProtocolError));
            _ZN3WTF8fastFreeEPv(dispatcher.protocolErrors);
        }
        dispatcher.protocolErrors = buffer;
        dispatcher.protocolErrorCapacity = newCapacity;
    }
    ProtocolError& error = dispatcher.protocolErrors[dispatcher.protocolErrorCount++];
    error.code = code;
    if (message.impl) message.impl->refCount += 2U;
    new (&error.message) String(message.impl);
}

void ReportProtocolError(BackendDispatcher& dispatcher, bool hasRelatedRequestId, std::int64_t relatedRequestId, CommonErrorCode code, const String& message) {
    if (static_cast<int>(code) < 0 || code > CommonErrorCode::ServerError)
        throw std::invalid_argument("Inspector::BackendDispatcher::reportProtocolError requires a known error code");
    if (!dispatcher.hasCurrentRequestId) {
        dispatcher.hasCurrentRequestId = hasRelatedRequestId;
        dispatcher.currentRequestId = relatedRequestId;
    }
    AppendProtocolError(dispatcher, code, message);
}

void ReportProtocolError(BackendDispatcher& dispatcher, CommonErrorCode code, const char* message) {
    ReportProtocolError(dispatcher, dispatcher.hasCurrentRequestId, dispatcher.currentRequestId, code, Literal(message));
}

void SetValue(Object& object, const char* key, Ref<Value>&& value) {
    NKWebKit::JsonAbi::SetValue(object, Literal(key), std::move(value));
}

void SetInteger(Object& object, const char* key, int value) {
    SetValue(object, key, _ZN3WTF8JSONImpl5Value6createEi(value));
}

void SetString(Object& object, const char* key, const String& value) {
    SetValue(object, key, _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(value));
}

void SendPendingErrors(BackendDispatcher& dispatcher) {
    CommonErrorCode errorCode = CommonErrorCode::InternalError;
    String errorMessage;
    Ref<Array> payload = _ZN3WTF8JSONImpl5Array6createEv();
    for (unsigned i = 0; i < dispatcher.protocolErrorCount; ++i) {
        errorCode = dispatcher.protocolErrors[i].code;
        errorMessage = dispatcher.protocolErrors[i].message;
        Ref<Object> error = _ZN3WTF8JSONImpl6Object6createEv();
        SetInteger(*error, "code", ErrorCodes[static_cast<int>(errorCode)]);
        SetString(*error, "message", errorMessage);
        NKWebKit::JsonAbi::PushValue(*payload, Ref<Value>::Adopt(error.Leak()));
    }
    Ref<Object> topLevelError = _ZN3WTF8JSONImpl6Object6createEv();
    SetInteger(*topLevelError, "code", ErrorCodes[static_cast<int>(errorCode)]);
    SetString(*topLevelError, "message", errorMessage);
    SetValue(*topLevelError, "data", Ref<Value>::Adopt(payload.Leak()));
    Ref<Object> message = _ZN3WTF8JSONImpl6Object6createEv();
    SetValue(*message, "error", Ref<Value>::Adopt(topLevelError.Leak()));
    if (dispatcher.hasCurrentRequestId)
        SetInteger(*message, "id", static_cast<int>(dispatcher.currentRequestId));
    else
        SetValue(*message, "id", _ZN3WTF8JSONImpl5Value4nullEv());
    String text = NKWebKit::JsonAbi::ToJSONString(*message);
    _ZNK9Inspector14FrontendRouter12sendResponseERKN3WTF6StringE(dispatcher.frontendRouter, &text);
    ClearProtocolErrors(dispatcher);
    dispatcher.hasCurrentRequestId = false;
    dispatcher.currentRequestId = 0;
}

struct ScopedRequestId {
    BackendDispatcher& dispatcher;
    bool savedHasValue;
    std::int64_t savedValue;
    ScopedRequestId(BackendDispatcher& target, bool hasValue, std::int64_t value)
        : dispatcher(target)
        , savedHasValue(target.hasCurrentRequestId)
        , savedValue(target.currentRequestId) {
        dispatcher.hasCurrentRequestId = hasValue;
        dispatcher.currentRequestId = hasValue ? value : 0;
    }
    ~ScopedRequestId() {
        dispatcher.hasCurrentRequestId = savedHasValue;
        dispatcher.currentRequestId = savedValue;
    }
};

struct Protector {
    BackendDispatcher* dispatcher;
    ~Protector() { NKWebKit::InspectorAbi::DerefBackendDispatcher(dispatcher); }
};

struct MessageReference {
    Object* object;
    ~MessageReference() { NKWebKit::JsonAbi::Deref(object); }
};

std::int64_t TruncateToLong(double number) {
    if (number >= -9223372036854775808.0 && number < 9223372036854775808.0) return static_cast<std::int64_t>(number);
    return INT64_MIN;
}

bool IsNumber(const Value& value) {
    return value.valueType == ValueType::Double || value.valueType == ValueType::Integer;
}

void Dispatch(BackendDispatcher& dispatcher, const String& message) {
    NKWebKit::InspectorAbi::RefBackendDispatcher(dispatcher);
    Protector protect{&dispatcher};
    std::int64_t requestId = 0;
    RefPtr<Value> messageValue;
    {
        ScopedRequestId scopedRequestId(dispatcher, false, 0);
        RefPtr<Value> parsedMessage = NKWebKit::JsonAbi::ParseJSON(message);
        if (!parsedMessage) {
            ReportProtocolError(dispatcher, CommonErrorCode::ParseError, "Message must be in JSON format");
            SendPendingErrors(dispatcher);
            return;
        }
        if (parsedMessage->valueType != ValueType::Object) {
            ReportProtocolError(dispatcher, CommonErrorCode::InvalidRequest, "Message must be a JSONified object");
            SendPendingErrors(dispatcher);
            return;
        }
        messageValue = std::move(parsedMessage);
        auto& messageObject = static_cast<Object&>(*messageValue);
        Value* requestIdValue = NKWebKit::JsonAbi::FindValue(messageObject, Literal("id"));
        if (!requestIdValue) {
            ReportProtocolError(dispatcher, CommonErrorCode::InvalidRequest, "'id' property was not found");
            SendPendingErrors(dispatcher);
            return;
        }
        if (!IsNumber(*requestIdValue)) {
            ReportProtocolError(dispatcher, CommonErrorCode::InvalidRequest, "The type of 'id' property must be integer");
            SendPendingErrors(dispatcher);
            return;
        }
        requestId = TruncateToLong(requestIdValue->data.number);
    }
    {
        ScopedRequestId scopedRequestId(dispatcher, true, requestId);
        auto& messageObject = static_cast<Object&>(*messageValue);
        Value* methodValue = NKWebKit::JsonAbi::FindValue(messageObject, Literal("method"));
        if (!methodValue) {
            ReportProtocolError(dispatcher, CommonErrorCode::InvalidRequest, "'method' property wasn't found");
            SendPendingErrors(dispatcher);
            return;
        }
        if (methodValue->valueType != ValueType::String) {
            ReportProtocolError(dispatcher, CommonErrorCode::InvalidRequest, "The type of 'method' property must be string");
            SendPendingErrors(dispatcher);
            return;
        }
        String methodString(methodValue->data.string);
        if (methodString.impl) methodString.impl->refCount += 2U;
        const unsigned length = methodString.impl ? methodString.impl->length : 0U;
        unsigned separators = 0;
        unsigned separator = 0;
        if (methodString.impl) {
            const auto view = NKWebKit::WtfAbi::ViewOf(methodString.impl);
            for (unsigned i = 0; i < length; ++i) {
                if (NKWebKit::WtfAbi::CodeUnitAt(view, i) != '.') continue;
                if (!separators) separator = i;
                ++separators;
            }
        }
        if (separators != 1 || !separator || separator + 1 == length) {
            ReportProtocolError(dispatcher, CommonErrorCode::InvalidRequest, "The 'method' property was formatted incorrectly. It should be 'Domain.method'");
            SendPendingErrors(dispatcher);
            return;
        }
        String domain = Substring(methodString, 0, separator);
        DispatcherMapEntry* entry = FindEntry(dispatcher.dispatchers, domain);
        SupplementalBackendDispatcher* domainDispatcher = entry ? entry->value : nullptr;
        if (!domainDispatcher) {
            std::u16string text = u"'";
            const auto view = NKWebKit::WtfAbi::ViewOf(domain.impl);
            for (unsigned i = 0; i < domain.impl->length; ++i) text.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, i));
            text += u"' domain was not found";
            bool is8Bit = std::all_of(text.begin(), text.end(), [](char16_t c) { return c <= 0xFF; });
            void* characters = nullptr;
            String errorText(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(text.size()), is8Bit, characters));
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (is8Bit) static_cast<unsigned char*>(characters)[i] = static_cast<unsigned char>(text[i]);
                else static_cast<char16_t*>(characters)[i] = text[i];
            }
            ReportProtocolError(dispatcher, dispatcher.hasCurrentRequestId, dispatcher.currentRequestId, CommonErrorCode::MethodNotFound, errorText);
            SendPendingErrors(dispatcher);
            return;
        }
        String method = Substring(methodString, separator + 1, length - separator - 1);
        MessageReference messageReference{static_cast<Object*>(messageValue.Leak())};
        domainDispatcher->vptr->dispatch(domainDispatcher, requestId, &method, &messageReference.object);
        if (dispatcher.protocolErrorCount) SendPendingErrors(dispatcher);
    }
}

void APS5_VABI SupplementalCompleteDestructor(SupplementalBackendDispatcher* self) {
    _ZN9Inspector29SupplementalBackendDispatcherD1Ev(self);
}

void APS5_VABI SupplementalDeletingDestructor(SupplementalBackendDispatcher* self) {
    _ZN9Inspector29SupplementalBackendDispatcherD0Ev(self);
}

void APS5_VABI SupplementalPureDispatch(SupplementalBackendDispatcher*, std::int64_t, const String*, Object**) {
    throw std::logic_error("Inspector::SupplementalBackendDispatcher::dispatch is pure virtual");
}

const SupplementalBackendDispatcherVtable SupplementalVtable{SupplementalCompleteDestructor, SupplementalDeletingDestructor, SupplementalPureDispatch};

void DestroySupplemental(SupplementalBackendDispatcher& dispatcher) {
    dispatcher.vptr = &SupplementalVtable;
    NKWebKit::InspectorAbi::DerefBackendDispatcher(std::exchange(dispatcher.backendDispatcher, nullptr));
}

void DestroyBackendDispatcher(BackendDispatcher& dispatcher) {
    ClearProtocolErrors(dispatcher);
    DestroyDispatchers(dispatcher);
    NKWebKit::InspectorAbi::FrontendRouterRef(std::exchange(dispatcher.frontendRouter, nullptr));
}

}  // namespace

namespace NKWebKit::InspectorAbi {

BackendDispatcher* CreateBackendDispatcher(FrontendRouter& router) {
    auto* dispatcher = static_cast<BackendDispatcher*>(_Znwm_nid_postfix(sizeof(BackendDispatcher)));
    std::atomic_ref<unsigned>(router.refCount).fetch_add(1, std::memory_order_relaxed);
    dispatcher->refCount = 1;
    dispatcher->frontendRouter = &router;
    dispatcher->dispatchers = nullptr;
    dispatcher->protocolErrors = nullptr;
    dispatcher->protocolErrorCapacity = 0;
    dispatcher->protocolErrorCount = 0;
    dispatcher->hasCurrentRequestId = false;
    dispatcher->currentRequestId = 0;
    return dispatcher;
}

void RefBackendDispatcher(BackendDispatcher& dispatcher) {
    ++dispatcher.refCount;
}

void DerefBackendDispatcher(BackendDispatcher* dispatcher) {
    if (!dispatcher) return;
    if (dispatcher->refCount != 1) {
        --dispatcher->refCount;
        return;
    }
    DestroyBackendDispatcher(*dispatcher);
    _ZdlPv_nid_postfix(dispatcher);
}

}  // namespace NKWebKit::InspectorAbi

extern "C" {

void APS5_VABI _ZN9Inspector29SupplementalBackendDispatcherC2ERNS_17BackendDispatcherE(SupplementalBackendDispatcher* self, BackendDispatcher* backendDispatcher) {
    auto& backend = Checked(backendDispatcher, "Inspector::SupplementalBackendDispatcher::SupplementalBackendDispatcher");
    auto& dispatcher = Checked(self, "Inspector::SupplementalBackendDispatcher::SupplementalBackendDispatcher");
    dispatcher.vptr = &SupplementalVtable;
    dispatcher.refCount = 1;
    NKWebKit::InspectorAbi::RefBackendDispatcher(backend);
    dispatcher.backendDispatcher = &backend;
}

void APS5_VABI _ZN9Inspector29SupplementalBackendDispatcherD2Ev(SupplementalBackendDispatcher* self) {
    DestroySupplemental(Checked(self, "Inspector::SupplementalBackendDispatcher::~SupplementalBackendDispatcher"));
}

void APS5_VABI _ZN9Inspector29SupplementalBackendDispatcherD1Ev(SupplementalBackendDispatcher* self) {
    DestroySupplemental(Checked(self, "Inspector::SupplementalBackendDispatcher::~SupplementalBackendDispatcher"));
}

void APS5_VABI _ZN9Inspector29SupplementalBackendDispatcherD0Ev(SupplementalBackendDispatcher* self) {
    DestroySupplemental(Checked(self, "Inspector::SupplementalBackendDispatcher::~SupplementalBackendDispatcher"));
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZN9Inspector17BackendDispatcherD2Ev(BackendDispatcher* self) {
    DestroyBackendDispatcher(Checked(self, "Inspector::BackendDispatcher::~BackendDispatcher"));
}

bool APS5_VABI _ZNK9Inspector17BackendDispatcher8isActiveEv(const BackendDispatcher* self) {
    return _ZNK9Inspector14FrontendRouter12hasFrontendsEv(Checked(self, "Inspector::BackendDispatcher::isActive").frontendRouter);
}

bool APS5_VABI _ZNK9Inspector17BackendDispatcher17hasProtocolErrorsEv(const BackendDispatcher* self) {
    return Checked(self, "Inspector::BackendDispatcher::hasProtocolErrors").protocolErrorCount > 0;
}

void APS5_VABI _ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE(BackendDispatcher* self, const String* domain, SupplementalBackendDispatcher* dispatcher) {
    const char* function = "Inspector::BackendDispatcher::registerDispatcherForDomain";
    const String& key = Checked(domain, function);
    Checked(dispatcher, function);
    if (!key.impl) throw std::invalid_argument(std::string(function) + " requires a domain");
    SetDispatcher(Checked(self, function), key, dispatcher);
}

void APS5_VABI _ZN9Inspector17BackendDispatcher8dispatchERKN3WTF6StringE(BackendDispatcher* self, const String* message) {
    const String& text = Checked(message, "Inspector::BackendDispatcher::dispatch");
    Dispatch(Checked(self, "Inspector::BackendDispatcher::dispatch"), text);
}

void APS5_VABI _ZN9Inspector17BackendDispatcher17sendPendingErrorsEv(BackendDispatcher* self) {
    SendPendingErrors(Checked(self, "Inspector::BackendDispatcher::sendPendingErrors"));
}

void APS5_VABI _ZN9Inspector17BackendDispatcher19reportProtocolErrorENS0_15CommonErrorCodeERKN3WTF6StringE(BackendDispatcher* self, CommonErrorCode code, const String* message) {
    const String& text = Checked(message, "Inspector::BackendDispatcher::reportProtocolError");
    auto& dispatcher = Checked(self, "Inspector::BackendDispatcher::reportProtocolError");
    ReportProtocolError(dispatcher, dispatcher.hasCurrentRequestId, dispatcher.currentRequestId, code, text);
}

bool APS5_VABI _ZNK9Inspector17BackendDispatcher12CallbackBase8isActiveEv(const CallbackBase* self) {
    const auto& callback = Checked(self, "Inspector::BackendDispatcher::CallbackBase::isActive");
    return !callback.alreadySent && _ZNK9Inspector17BackendDispatcher8isActiveEv(callback.backendDispatcher);
}

void APS5_VABI _ZN9Inspector17BackendDispatcher12CallbackBase7disableEv(CallbackBase* self) {
    Checked(self, "Inspector::BackendDispatcher::CallbackBase::disable").alreadySent = true;
}

void APS5_VABI _ZN9Inspector17BackendDispatcher12CallbackBase11sendFailureERKN3WTF6StringE(CallbackBase* self, const String* error) {
    const String& text = Checked(error, "Inspector::BackendDispatcher::CallbackBase::sendFailure");
    auto& callback = Checked(self, "Inspector::BackendDispatcher::CallbackBase::sendFailure");
    if (callback.alreadySent) return;
    callback.alreadySent = true;
    auto& dispatcher = Checked(callback.backendDispatcher, "Inspector::BackendDispatcher::CallbackBase::sendFailure");
    ReportProtocolError(dispatcher, true, callback.requestId, CommonErrorCode::ServerError, text);
    SendPendingErrors(dispatcher);
}

void APS5_VABI _ZN9Inspector17BackendDispatcher12CallbackBaseD2Ev(CallbackBase* self) {
    NKWebKit::InspectorAbi::DerefBackendDispatcher(std::exchange(Checked(self, "Inspector::BackendDispatcher::CallbackBase::~CallbackBase").backendDispatcher, nullptr));
}

void APS5_VABI _ZN9Inspector17BackendDispatcher12CallbackBaseD1Ev(CallbackBase* self) {
    _ZN9Inspector17BackendDispatcher12CallbackBaseD2Ev(self);
}

}
