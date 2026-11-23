#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace NKWebKit::WebCoreAbi {

struct GenericKeyedDecoder;

struct GenericKeyedDecoderVtable {
    void (APS5_VABI *completeDestructor)(GenericKeyedDecoder*);
    void (APS5_VABI *deletingDestructor)(GenericKeyedDecoder*);
    bool (APS5_VABI *decodeBytes)(GenericKeyedDecoder*, const WtfAbi::String* key, const std::uint8_t** bytes, std::size_t* size);
    bool (APS5_VABI *decodeBool)(GenericKeyedDecoder*, const WtfAbi::String* key, bool* result);
    bool (APS5_VABI *decodeUInt32)(GenericKeyedDecoder*, const WtfAbi::String* key, std::uint32_t* result);
    bool (APS5_VABI *decodeUInt64)(GenericKeyedDecoder*, const WtfAbi::String* key, std::uint64_t* result);
    bool (APS5_VABI *decodeInt32)(GenericKeyedDecoder*, const WtfAbi::String* key, std::int32_t* result);
    bool (APS5_VABI *decodeInt64)(GenericKeyedDecoder*, const WtfAbi::String* key, std::int64_t* result);
    bool (APS5_VABI *decodeFloat)(GenericKeyedDecoder*, const WtfAbi::String* key, float* result);
    bool (APS5_VABI *decodeDouble)(GenericKeyedDecoder*, const WtfAbi::String* key, double* result);
    bool (APS5_VABI *decodeString)(GenericKeyedDecoder*, const WtfAbi::String* key, WtfAbi::String* result);
    bool (APS5_VABI *beginObject)(GenericKeyedDecoder*, const WtfAbi::String* key);
    void (APS5_VABI *endObject)(GenericKeyedDecoder*);
    bool (APS5_VABI *beginArray)(GenericKeyedDecoder*, const WtfAbi::String* key);
    bool (APS5_VABI *beginArrayElement)(GenericKeyedDecoder*);
    void (APS5_VABI *endArrayElement)(GenericKeyedDecoder*);
    void (APS5_VABI *endArray)(GenericKeyedDecoder*);
};

struct KeyedDecoderVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    GenericKeyedDecoderVtable dispatch;
};

struct GenericKeyedDecoder {
    const GenericKeyedDecoderVtable* vptr;
};

struct KeyedDecoderPtr {
    GenericKeyedDecoder* ptr = nullptr;

    KeyedDecoderPtr() = default;
    explicit KeyedDecoderPtr(GenericKeyedDecoder* decoder) : ptr(decoder) {}
    KeyedDecoderPtr(const KeyedDecoderPtr&) = delete;
    KeyedDecoderPtr(KeyedDecoderPtr&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}
    KeyedDecoderPtr& operator=(const KeyedDecoderPtr&) = delete;
    KeyedDecoderPtr& operator=(KeyedDecoderPtr&&) = delete;
    ~KeyedDecoderPtr() {
        if (ptr) ptr->vptr->deletingDestructor(ptr);
    }
};

static_assert(sizeof(GenericKeyedDecoderVtable) == 17 * sizeof(void*));
static_assert(offsetof(GenericKeyedDecoderVtable, completeDestructor) == 0);
static_assert(offsetof(GenericKeyedDecoderVtable, deletingDestructor) == 8);
static_assert(offsetof(GenericKeyedDecoderVtable, decodeBytes) == 16);
static_assert(offsetof(GenericKeyedDecoderVtable, decodeBool) == 24);
static_assert(offsetof(GenericKeyedDecoderVtable, decodeUInt32) == 32);
static_assert(offsetof(GenericKeyedDecoderVtable, decodeUInt64) == 40);
static_assert(offsetof(GenericKeyedDecoderVtable, decodeInt32) == 48);
static_assert(offsetof(GenericKeyedDecoderVtable, decodeInt64) == 56);
static_assert(offsetof(GenericKeyedDecoderVtable, decodeFloat) == 64);
static_assert(offsetof(GenericKeyedDecoderVtable, decodeDouble) == 72);
static_assert(offsetof(GenericKeyedDecoderVtable, decodeString) == 80);
static_assert(offsetof(GenericKeyedDecoderVtable, beginObject) == 88);
static_assert(offsetof(GenericKeyedDecoderVtable, endObject) == 96);
static_assert(offsetof(GenericKeyedDecoderVtable, beginArray) == 104);
static_assert(offsetof(GenericKeyedDecoderVtable, beginArrayElement) == 112);
static_assert(offsetof(GenericKeyedDecoderVtable, endArrayElement) == 120);
static_assert(offsetof(GenericKeyedDecoderVtable, endArray) == 128);
static_assert(sizeof(KeyedDecoderVtableStorage) == 19 * sizeof(void*));
static_assert(offsetof(KeyedDecoderVtableStorage, dispatch) == 16);
static_assert(sizeof(GenericKeyedDecoder) == 8);
static_assert(sizeof(KeyedDecoderPtr) == 8);
static_assert(!std::is_trivially_copyable_v<KeyedDecoderPtr>);
static_assert(!std::is_trivially_destructible_v<KeyedDecoderPtr>);

}  // namespace NKWebKit::WebCoreAbi
