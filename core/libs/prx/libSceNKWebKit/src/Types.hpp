#pragma once

#include "prx/libSceNKWebKit/src/Object.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace NKWebKit {

std::optional<std::u16string> DecodeUtf8(const char* text, std::size_t length);
std::optional<std::u16string> DecodeUtf8(const char* text);
std::string EncodeUtf8(std::u16string_view text);
std::size_t ConvertUtf16ToUtf8(std::u16string_view text, char* buffer, std::size_t bufferSize, bool strict);

class String final : public ObjectOf<ObjectType::String> {
public:
    String() : null(true) {}
    explicit String(std::u16string text) : characters(std::move(text)) {}
    explicit String(std::optional<std::u16string> text) : characters(text.value_or(std::u16string())), null(!text.has_value()) {}
    static Retained<String> FromUtf8(const char* text) { return Make<String>(DecodeUtf8(text)); }
    std::string Utf8() const { return EncodeUtf8(characters); }

    std::u16string characters;
    bool null = false;
};

class URL final : public ObjectOf<ObjectType::URL> {
public:
    explicit URL(std::string text) : text(std::move(text)) {}

    std::string text;
};

class Array final : public ObjectOf<ObjectType::Array> {
public:
    Array() = default;
    explicit Array(std::vector<Retained<Object>> elements) : elements(std::move(elements)) {}

    std::vector<Retained<Object>> elements;
    bool mutableArray = false;
};

class Dictionary final : public ObjectOf<ObjectType::Dictionary> {
public:
    Dictionary() = default;
    explicit Dictionary(std::map<std::u16string, Retained<Object>> entries) : entries(std::move(entries)) {}

    std::map<std::u16string, Retained<Object>> entries;
    bool mutableDictionary = false;
};

class Data final : public ObjectOf<ObjectType::Data> {
public:
    explicit Data(std::vector<std::uint8_t> bytes) : bytes(std::move(bytes)) {}

    std::vector<std::uint8_t> bytes;
};

template<ObjectType kind, typename Value>
class Number final : public ObjectOf<kind> {
public:
    explicit Number(Value value) : value(value) {}

    Value value;
};

using Boolean = Number<ObjectType::Boolean, bool>;
using Double = Number<ObjectType::Double, double>;
using UInt64 = Number<ObjectType::UInt64, std::uint64_t>;
using Int64 = Number<ObjectType::Int64, std::int64_t>;

struct PointValue {
    double x;
    double y;
};

struct SizeValue {
    double width;
    double height;
};

struct RectValue {
    PointValue origin;
    SizeValue size;
};

using Point = Number<ObjectType::Point, PointValue>;
using Size = Number<ObjectType::Size, SizeValue>;
using Rect = Number<ObjectType::Rect, RectValue>;

class UserContentURLPattern final : public ObjectOf<ObjectType::UserContentURLPattern> {
public:
    std::u16string scheme;
    std::u16string host;
    std::u16string path;
    bool matchSubdomains = false;
    bool valid = false;
};

}  // namespace NKWebKit
