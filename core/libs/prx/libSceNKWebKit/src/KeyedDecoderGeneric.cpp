// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/generic/KeyedDecoderGeneric.cpp
/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/generic/KeyedEncoderGeneric.h
/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/persistence/PersistentDecoder.cpp
/*
 * Copyright (C) 2010, 2011, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/persistence/PersistentDecoder.h
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/KeyedDecoderExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::GenericKeyedDecoder;
using NKWebKit::WebCoreAbi::KeyedDecoderPtr;
using NKWebKit::WebCoreAbi::KeyedDecoderVtableStorage;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

enum class Type : std::uint64_t {
    Bytes,
    Bool,
    UInt32,
    UInt64,
    Int32,
    Int64,
    Float,
    Double,
    String,
    BeginObject,
    EndObject,
    BeginArray,
    BeginArrayElement,
    EndArrayElement,
    EndArray,
};

constexpr std::uint64_t kTypeCount = 15;

struct Dictionary;
using Array = std::vector<std::unique_ptr<Dictionary>>;

struct RawBool {
    std::uint8_t byte;
};

struct FloatBits {
    std::uint32_t bits;
};

struct DoubleBits {
    std::uint64_t bits;
};

using Node = std::variant<std::vector<std::uint8_t>, RawBool, std::uint32_t, std::uint64_t, std::int32_t, std::int64_t, FloatBits, DoubleBits, String,
                          std::unique_ptr<Dictionary>, std::unique_ptr<Array>>;

struct Dictionary {
    std::map<std::u16string, Node> map;
};

struct State {
    std::unique_ptr<Dictionary> rootDictionary = std::make_unique<Dictionary>();
    std::vector<std::unique_ptr<Dictionary>> unreachableDictionaries;
    std::vector<std::unique_ptr<Array>> unreachableArrays;
    std::vector<Dictionary*> dictionaryStack;
    std::vector<Array*> arrayStack;
    std::vector<std::size_t> arrayIndexStack;
};

struct Decoder {
    GenericKeyedDecoder base;
    State* state;
};

std::u16string Units(const String& string) {
    const StringView view = NKWebKit::WtfAbi::ViewOf(string.impl);
    std::u16string text(view.length, u'\0');
    for (unsigned index = 0; index < view.length; ++index) text[index] = NKWebKit::WtfAbi::CodeUnitAt(view, index);
    return text;
}

struct Reader {
    const std::uint8_t* position;
    const std::uint8_t* end;

    bool Has(std::uint64_t size) const {
        return size <= static_cast<std::uint64_t>(end - position);
    }

    template<typename T>
    std::optional<T> Number() {
        if (!Has(sizeof(T))) return std::nullopt;
        T value;
        std::memcpy(&value, position, sizeof(T));
        position += sizeof(T);
        return value;
    }
};

std::optional<String> ReadString(Reader& reader) {
    const std::optional<std::uint64_t> size = reader.Number<std::uint64_t>();
    if (!size) return std::nullopt;
    if (*size > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("WebCore::KeyedDecoder::decoder: string longer than a WTF::Vector can hold");
    if (!reader.Has(*size)) return std::nullopt;
    String result = _ZN3WTF6String8fromUTF8EPKhm(reader.position, static_cast<std::size_t>(*size));
    reader.position += *size;
    if (!result.impl) return std::nullopt;
    return result;
}

Dictionary& Top(State& state) {
    if (state.dictionaryStack.empty()) throw std::out_of_range("WebCore::KeyedDecoder: empty dictionary stack");
    return *state.dictionaryStack.back();
}

template<typename T, typename TStored = T>
bool ReadSimpleValue(Reader& reader, Dictionary& dictionary) {
    const std::optional<String> key = ReadString(reader);
    if (!key) return false;
    const std::optional<T> value = reader.Number<T>();
    if (!value) return false;
    dictionary.map.try_emplace(Units(*key), TStored{*value});
    return true;
}

void Parse(State& state, const std::uint8_t* data, std::size_t size) {
    Reader reader{data, data + size};
    state.dictionaryStack.push_back(state.rootDictionary.get());
    bool ok = true;
    while (ok) {
        const std::optional<std::uint64_t> type = reader.Number<std::uint64_t>();
        if (!type || *type >= kTypeCount) break;
        switch (static_cast<Type>(*type)) {
        case Type::Bytes: {
            const std::optional<String> key = ReadString(reader);
            if (!key) {
                ok = false;
                break;
            }
            const std::optional<std::uint64_t> length = reader.Number<std::uint64_t>();
            if (!length || !reader.Has(*length)) {
                ok = false;
                break;
            }
            Top(state).map.try_emplace(Units(*key), std::vector<std::uint8_t>(reader.position, reader.position + *length));
            reader.position += *length;
            break;
        }
        case Type::Bool:
            ok = ReadSimpleValue<std::uint8_t, RawBool>(reader, Top(state));
            break;
        case Type::UInt32:
            ok = ReadSimpleValue<std::uint32_t>(reader, Top(state));
            break;
        case Type::UInt64:
            ok = ReadSimpleValue<std::uint64_t>(reader, Top(state));
            break;
        case Type::Int32:
            ok = ReadSimpleValue<std::int32_t>(reader, Top(state));
            break;
        case Type::Int64:
            ok = ReadSimpleValue<std::int64_t>(reader, Top(state));
            break;
        case Type::Float:
            ok = ReadSimpleValue<std::uint32_t, FloatBits>(reader, Top(state));
            break;
        case Type::Double:
            ok = ReadSimpleValue<std::uint64_t, DoubleBits>(reader, Top(state));
            break;
        case Type::String: {
            const std::optional<String> key = ReadString(reader);
            if (!key) {
                ok = false;
                break;
            }
            std::optional<String> value = ReadString(reader);
            if (!value) {
                ok = false;
                break;
            }
            Top(state).map.try_emplace(Units(*key), std::move(*value));
            break;
        }
        case Type::BeginObject: {
            const std::optional<String> key = ReadString(reader);
            if (!key) {
                ok = false;
                break;
            }
            Dictionary& current = Top(state);
            auto newDictionary = std::make_unique<Dictionary>();
            state.dictionaryStack.push_back(newDictionary.get());
            const std::u16string name = Units(*key);
            if (current.map.contains(name)) state.unreachableDictionaries.push_back(std::move(newDictionary));
            else current.map.emplace(name, std::move(newDictionary));
            break;
        }
        case Type::EndObject:
        case Type::EndArrayElement:
            state.dictionaryStack.pop_back();
            if (state.dictionaryStack.empty()) ok = false;
            break;
        case Type::BeginArray: {
            const std::optional<String> key = ReadString(reader);
            if (!key) {
                ok = false;
                break;
            }
            auto newArray = std::make_unique<Array>();
            state.arrayStack.push_back(newArray.get());
            Dictionary& current = Top(state);
            const std::u16string name = Units(*key);
            if (current.map.contains(name)) state.unreachableArrays.push_back(std::move(newArray));
            else current.map.emplace(name, std::move(newArray));
            break;
        }
        case Type::BeginArrayElement: {
            if (state.arrayStack.empty()) {
                ok = false;
                break;
            }
            auto newDictionary = std::make_unique<Dictionary>();
            state.dictionaryStack.push_back(newDictionary.get());
            state.arrayStack.back()->push_back(std::move(newDictionary));
            break;
        }
        case Type::EndArray:
            if (state.arrayStack.empty()) {
                ok = false;
                break;
            }
            state.arrayStack.pop_back();
            break;
        }
    }
    if (state.dictionaryStack.size() > 1) state.dictionaryStack.resize(1);
    state.arrayStack.clear();
}

State& StateOf(GenericKeyedDecoder* self) {
    if (!self) throw std::invalid_argument("null WebCore::KeyedDecoder");
    State* state = reinterpret_cast<Decoder*>(self)->state;
    if (!state) throw std::invalid_argument("destroyed WebCore::KeyedDecoder");
    return *state;
}

template<typename T>
const T* Find(GenericKeyedDecoder* self, const String* key) {
    Dictionary& dictionary = Top(StateOf(self));
    if (!key) throw std::invalid_argument("null WTF::String key for WebCore::KeyedDecoder");
    if (dictionary.map.empty()) return nullptr;
    if (!key->impl) throw std::invalid_argument("null WTF::String looked up in a WebCore::KeyedDecoder dictionary");
    const auto entry = dictionary.map.find(Units(*key));
    if (entry == dictionary.map.end()) return nullptr;
    return std::get_if<T>(&entry->second);
}

template<typename T>
T& Output(T* result) {
    if (!result) throw std::invalid_argument("null result for WebCore::KeyedDecoder");
    return *result;
}

template<typename TStored, typename TResult>
bool DecodeSimpleValue(GenericKeyedDecoder* self, const String* key, TResult* result) {
    TResult& output = Output(result);
    const TStored* value = Find<TStored>(self, key);
    if (!value) return false;
    std::memcpy(&output, value, sizeof(TResult));
    return true;
}

void APS5_VABI CompleteDestructor(GenericKeyedDecoder* self) {
    if (!self) throw std::invalid_argument("null WebCore::KeyedDecoder");
    delete std::exchange(reinterpret_cast<Decoder*>(self)->state, nullptr);
}

void APS5_VABI DeletingDestructor(GenericKeyedDecoder* self) {
    CompleteDestructor(self);
    _ZN3WTF8fastFreeEPv(self);
}

bool APS5_VABI DecodeBytes(GenericKeyedDecoder* self, const String* key, const std::uint8_t** bytes, std::size_t* size) {
    const std::uint8_t*& data = Output(bytes);
    std::size_t& length = Output(size);
    const auto* value = Find<std::vector<std::uint8_t>>(self, key);
    if (!value) return false;
    data = value->empty() ? nullptr : value->data();
    length = value->size();
    return true;
}

bool APS5_VABI DecodeBool(GenericKeyedDecoder* self, const String* key, bool* result) {
    return DecodeSimpleValue<RawBool, bool>(self, key, result);
}

bool APS5_VABI DecodeUInt32(GenericKeyedDecoder* self, const String* key, std::uint32_t* result) {
    return DecodeSimpleValue<std::uint32_t, std::uint32_t>(self, key, result);
}

bool APS5_VABI DecodeUInt64(GenericKeyedDecoder* self, const String* key, std::uint64_t* result) {
    return DecodeSimpleValue<std::uint64_t, std::uint64_t>(self, key, result);
}

bool APS5_VABI DecodeInt32(GenericKeyedDecoder* self, const String* key, std::int32_t* result) {
    return DecodeSimpleValue<std::int32_t, std::int32_t>(self, key, result);
}

bool APS5_VABI DecodeInt64(GenericKeyedDecoder* self, const String* key, std::int64_t* result) {
    return DecodeSimpleValue<std::int64_t, std::int64_t>(self, key, result);
}

bool APS5_VABI DecodeFloat(GenericKeyedDecoder* self, const String* key, float* result) {
    return DecodeSimpleValue<FloatBits, float>(self, key, result);
}

bool APS5_VABI DecodeDouble(GenericKeyedDecoder* self, const String* key, double* result) {
    return DecodeSimpleValue<DoubleBits, double>(self, key, result);
}

bool APS5_VABI DecodeString(GenericKeyedDecoder* self, const String* key, String* result) {
    String& output = Output(result);
    const String* value = Find<String>(self, key);
    if (!value) return false;
    output = *value;
    return true;
}

bool APS5_VABI BeginObject(GenericKeyedDecoder* self, const String* key) {
    const auto* value = Find<std::unique_ptr<Dictionary>>(self, key);
    if (!value) return false;
    StateOf(self).dictionaryStack.push_back(value->get());
    return true;
}

void APS5_VABI EndObject(GenericKeyedDecoder* self) {
    State& state = StateOf(self);
    if (state.dictionaryStack.empty()) throw std::out_of_range("WebCore::KeyedDecoder: empty dictionary stack");
    state.dictionaryStack.pop_back();
}

bool APS5_VABI BeginArray(GenericKeyedDecoder* self, const String* key) {
    const auto* value = Find<std::unique_ptr<Array>>(self, key);
    if (!value) return false;
    State& state = StateOf(self);
    state.arrayStack.push_back(value->get());
    state.arrayIndexStack.push_back(0);
    return true;
}

bool APS5_VABI BeginArrayElement(GenericKeyedDecoder* self) {
    State& state = StateOf(self);
    if (state.arrayStack.empty()) throw std::out_of_range("WebCore::KeyedDecoder: empty array stack");
    Array& array = *state.arrayStack.back();
    std::size_t& index = state.arrayIndexStack.back();
    if (index >= array.size()) return false;
    state.dictionaryStack.push_back(array[index++].get());
    return true;
}

void APS5_VABI EndArrayElement(GenericKeyedDecoder* self) {
    EndObject(self);
}

void APS5_VABI EndArray(GenericKeyedDecoder* self) {
    State& state = StateOf(self);
    if (state.arrayStack.empty()) throw std::out_of_range("WebCore::KeyedDecoder: empty array stack");
    state.arrayStack.pop_back();
    state.arrayIndexStack.pop_back();
}

const KeyedDecoderVtableStorage vtable = {
    0,
    nullptr,
    {
        &CompleteDestructor,
        &DeletingDestructor,
        &DecodeBytes,
        &DecodeBool,
        &DecodeUInt32,
        &DecodeUInt64,
        &DecodeInt32,
        &DecodeInt64,
        &DecodeFloat,
        &DecodeDouble,
        &DecodeString,
        &BeginObject,
        &EndObject,
        &BeginArray,
        &BeginArrayElement,
        &EndArrayElement,
        &EndArray,
    },
};

}  // namespace

extern "C" KeyedDecoderPtr APS5_VABI _ZN7WebCore12KeyedDecoder7decoderEPKhm(const std::uint8_t* data, std::size_t size) {
    if (!data && size) throw std::invalid_argument("null data for WebCore::KeyedDecoder::decoder");
    auto state = std::make_unique<State>();
    Parse(*state, data, size);
    auto* decoder = static_cast<Decoder*>(_ZN3WTF10fastMallocEm(sizeof(Decoder)));
    decoder->base.vptr = &vtable.dispatch;
    decoder->state = state.release();
    return KeyedDecoderPtr(&decoder->base);
}
