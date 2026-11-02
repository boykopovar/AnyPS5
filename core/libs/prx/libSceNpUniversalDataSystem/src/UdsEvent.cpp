#include "prx/libSceNpUniversalDataSystem/include/UdsEvent.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"

#include <charconv>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace Uds {

Value Clone(const Value& value) {
    if (const auto* object = std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyObject>>(&value)) return std::make_shared<NpUniversalDataSystemEventPropertyObject>((*object)->Clone());
    if (const auto* array = std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyArray>>(&value)) return std::make_shared<NpUniversalDataSystemEventPropertyArray>((*array)->Clone());
    return value;
}

namespace {

void appendQuoted(std::string& out, std::string_view text) {
    out += '"';
    for (const char character : text) {
        if (character == '"' || character == '\\') {
            out += '\\';
            out += character;
        } else if (static_cast<unsigned char>(character) < 0x20) {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(character)));
            out += escaped;
        } else {
            out += character;
        }
    }
    out += '"';
}

void appendValue(std::string& out, const Value& value);

template <typename T>
void appendFormattedNumber(std::string& out, T value, int precision) {
    char number[40];
    const auto result = std::to_chars(number, number + sizeof(number), value, std::chars_format::general, precision);
    if (result.ec != std::errc{}) throw std::runtime_error("event property number formatting failed");
    out.append(number, static_cast<std::string::size_type>(result.ptr - number));
}

void appendObject(std::string& out, const NpUniversalDataSystemEventPropertyObject& object) {
    out += '{';
    bool first = true;
    for (const auto& [key, value] : object.properties) {
        if (!first) out += ',';
        first = false;
        appendQuoted(out, key);
        out += ':';
        appendValue(out, value);
    }
    out += '}';
}

void appendValue(std::string& out, const Value& value) {
    std::visit([&](const auto& held) {
        using Held = std::decay_t<decltype(held)>;
        char number[40];
        if constexpr (std::is_same_v<Held, bool>) {
            out += held ? "true" : "false";
        } else if constexpr (std::is_same_v<Held, std::int32_t> || std::is_same_v<Held, std::int64_t>) {
            std::snprintf(number, sizeof(number), "%lld", static_cast<long long>(held));
            out += number;
        } else if constexpr (std::is_same_v<Held, std::uint32_t> || std::is_same_v<Held, std::uint64_t>) {
            std::snprintf(number, sizeof(number), "%llu", static_cast<unsigned long long>(held));
            out += number;
        } else if constexpr (std::is_same_v<Held, float>) {
            appendFormattedNumber(out, held, 9);
        } else if constexpr (std::is_same_v<Held, double>) {
            appendFormattedNumber(out, held, 17);
        } else if constexpr (std::is_same_v<Held, std::string>) {
            appendQuoted(out, held);
        } else if constexpr (std::is_same_v<Held, Binary>) {
            std::snprintf(number, sizeof(number), "<%zu bytes>", held.size());
            out += number;
        } else if constexpr (std::is_same_v<Held, std::shared_ptr<NpUniversalDataSystemEventPropertyObject>>) {
            appendObject(out, *held);
        } else {
            out += '[';
            for (std::size_t i = 0; i < held->values.size(); ++i) {
                if (i != 0) out += ',';
                appendValue(out, held->values[i]);
            }
            out += ']';
        }
    }, value);
}

std::size_t lengthPrefixed(std::size_t length, std::size_t smallLimit) {
    if (length < smallLimit) return (smallLimit == 32 ? 1 : 2) + length;
    if (length < 0x100) return 2 + length;
    if (length < 0x10000) return 3 + length;
    return 5 + length;
}

std::size_t containerHeader(std::size_t count) {
    if (count < 16) return 1;
    if (count < 0x10000) return 3;
    return 5;
}

std::size_t packedSize(const Value& value);

std::size_t packedSize(const NpUniversalDataSystemEventPropertyObject& object) {
    std::size_t size = containerHeader(object.properties.size());
    for (const auto& [key, value] : object.properties) size += lengthPrefixed(key.size(), 32) + packedSize(value);
    return size;
}

std::size_t packedSize(const Value& value) {
    return std::visit([](const auto& held) -> std::size_t {
        using Held = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<Held, bool>) {
            return 1;
        } else if constexpr (std::is_same_v<Held, std::int32_t> || std::is_same_v<Held, std::uint32_t> || std::is_same_v<Held, float>) {
            return 5;
        } else if constexpr (std::is_same_v<Held, std::int64_t> || std::is_same_v<Held, std::uint64_t> || std::is_same_v<Held, double>) {
            return 9;
        } else if constexpr (std::is_same_v<Held, std::string>) {
            return lengthPrefixed(held.size(), 32);
        } else if constexpr (std::is_same_v<Held, Binary>) {
            return lengthPrefixed(held.size(), 0x100);
        } else if constexpr (std::is_same_v<Held, std::shared_ptr<NpUniversalDataSystemEventPropertyObject>>) {
            return packedSize(*held);
        } else {
            std::size_t size = containerHeader(held->values.size());
            for (const auto& element : held->values) size += packedSize(element);
            return size;
        }
    }, value);
}

template <typename T>
void appendTagged(std::string& out, T value, const char* tag) {
    char number[40];
    const auto result = std::to_chars(number, number + sizeof(number), value);
    if (result.ec != std::errc{}) throw std::runtime_error("event property number formatting failed");
    out.append(number, static_cast<std::string::size_type>(result.ptr - number));
    out += tag;
}

void appendTaggedReal(std::string& out, double value, const char* tag) {
    char number[40];
    const auto result = std::to_chars(number, number + sizeof(number), value, std::chars_format::general, 6);
    if (result.ec != std::errc{}) throw std::runtime_error("event property number formatting failed");
    out.append(number, static_cast<std::string::size_type>(result.ptr - number));
    out += tag;
}

void appendDebug(std::string& out, const Value& value, std::size_t indent);

void appendDebug(std::string& out, const NpUniversalDataSystemEventPropertyObject& object, std::size_t indent) {
    out += "{\n";
    for (const auto& [key, value] : object.properties) {
        out.append(indent + 4, ' ');
        out += '"';
        out += key;
        out += "\": ";
        appendDebug(out, value, indent + 4);
        out += ",\n";
    }
    out.append(indent, ' ');
    out += '}';
}

void appendDebug(std::string& out, const Value& value, std::size_t indent) {
    std::visit([&](const auto& held) {
        using Held = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<Held, bool>) {
            out += held ? "true" : "false";
        } else if constexpr (std::is_same_v<Held, std::int32_t>) {
            appendTagged(out, held, " (Int32)");
        } else if constexpr (std::is_same_v<Held, std::uint32_t>) {
            appendTagged(out, held, " (UInt32)");
        } else if constexpr (std::is_same_v<Held, std::int64_t>) {
            appendTagged(out, held, " (Int64)");
        } else if constexpr (std::is_same_v<Held, std::uint64_t>) {
            appendTagged(out, held, " (UInt64)");
        } else if constexpr (std::is_same_v<Held, float>) {
            appendTaggedReal(out, held, " (Float32)");
        } else if constexpr (std::is_same_v<Held, double>) {
            appendTaggedReal(out, held, " (Float64)");
        } else if constexpr (std::is_same_v<Held, std::string>) {
            out += '"';
            out += held;
            out += '"';
        } else if constexpr (std::is_same_v<Held, Binary>) {
            static constexpr char Digits[] = "0123456789ABCDEF";
            for (const std::uint8_t byte : held) {
                out += Digits[byte >> 4];
                out += Digits[byte & 0xF];
            }
            out += " (Binary)";
        } else if constexpr (std::is_same_v<Held, std::shared_ptr<NpUniversalDataSystemEventPropertyObject>>) {
            appendDebug(out, *held, indent);
        } else {
            out += "[\n";
            for (const auto& element : held->values) {
                out.append(indent + 4, ' ');
                appendDebug(out, element, indent + 4);
                out += ",\n";
            }
            out.append(indent, ' ');
            out += ']';
        }
    }, value);
}

std::optional<std::uint64_t> unsignedValue(const Value& value) {
    return std::visit([](const auto& held) -> std::optional<std::uint64_t> {
        using Held = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<Held, std::uint32_t> || std::is_same_v<Held, std::uint64_t>) {
            return held;
        } else if constexpr (std::is_same_v<Held, std::int32_t> || std::is_same_v<Held, std::int64_t>) {
            if (held < 0) return std::nullopt;
            return static_cast<std::uint64_t>(held);
        } else {
            return std::nullopt;
        }
    }, value);
}

void flatten(const NpUniversalDataSystemEventPropertyObject& object, const std::string& prefix, std::vector<NpTrophy2EventProperty>& properties) {
    for (const auto& [key, value] : object.properties) {
        auto path = prefix.empty() ? key : prefix + "." + key;
        if (const auto* child = std::get_if<std::shared_ptr<NpUniversalDataSystemEventPropertyObject>>(&value)) {
            flatten(**child, path, properties);
            continue;
        }
        properties.push_back({std::move(path), unsignedValue(value), Describe(value)});
    }
}

}

TrophyEvent RecognizeTrophyEvent(const NpUniversalDataSystemEvent& event) {
    TrophyEvent result;
    if (event.name == UnlockTrophyEvent) result.kind = TrophyEventKind::Unlock;
    else if (event.name == UpdateTrophyProgressEvent) result.kind = TrophyEventKind::Progress;
    else return result;
    const auto* id = event.properties.Find(TrophyIdProperty);
    const auto* value = id != nullptr ? std::get_if<std::int32_t>(id) : nullptr;
    if (value != nullptr && *value >= 0) {
        result.trophyId = *value;
    } else if (result.kind == TrophyEventKind::Unlock) {
        result.kind = TrophyEventKind::Malformed;
        result.problem = id == nullptr ? "it has no _trophy_id" : value == nullptr ? "its _trophy_id is not an int32" : "its _trophy_id is negative";
    }
    return result;
}

std::string Describe(const Value& value) {
    std::string out;
    appendValue(out, value);
    return out;
}

std::string Describe(const NpUniversalDataSystemEventPropertyObject& object) {
    std::string out;
    appendObject(out, object);
    return out;
}

std::string Describe(const NpUniversalDataSystemEvent& event) {
    return event.name + " " + Describe(event.properties);
}

std::size_t MessagePackSize(const NpUniversalDataSystemEvent& event) {
    return containerHeader(2) + lengthPrefixed(EventNameKey.size(), 32) + lengthPrefixed(event.name.size(), 32) +
           lengthPrefixed(PropertiesKey.size(), 32) + packedSize(event.properties);
}

std::string DebugString(const NpUniversalDataSystemEvent& event) {
    NpUniversalDataSystemEventPropertyObject root;
    root.Set(EventNameKey, event.name);
    root.Set(PropertiesKey, std::make_shared<NpUniversalDataSystemEventPropertyObject>(event.properties));
    std::string out;
    appendDebug(out, root, 0);
    return out;
}

NpTrophy2Event StatEvent(const NpUniversalDataSystemEvent& event) {
    NpTrophy2Event converted;
    converted.name = event.name;
    flatten(event.properties, {}, converted.properties);
    return converted;
}

}

Uds::Value& NpUniversalDataSystemEventPropertyObject::Set(std::string_view key, Uds::Value value) {
    for (auto& [name, stored] : properties) {
        if (name == key) {
            stored = std::move(value);
            return stored;
        }
    }
    return properties.emplace_back(std::string(key), std::move(value)).second;
}

const Uds::Value* NpUniversalDataSystemEventPropertyObject::Find(std::string_view key) const {
    for (const auto& [name, stored] : properties) {
        if (name == key) return &stored;
    }
    return nullptr;
}

NpUniversalDataSystemEventPropertyObject NpUniversalDataSystemEventPropertyObject::Clone() const {
    NpUniversalDataSystemEventPropertyObject copy;
    copy.properties.reserve(properties.size());
    for (const auto& [name, stored] : properties) copy.properties.emplace_back(name, Uds::Clone(stored));
    return copy;
}

Uds::Value& NpUniversalDataSystemEventPropertyArray::Add(Uds::Value value) {
    return values.emplace_back(std::move(value));
}

NpUniversalDataSystemEventPropertyArray NpUniversalDataSystemEventPropertyArray::Clone() const {
    NpUniversalDataSystemEventPropertyArray copy;
    copy.values.reserve(values.size());
    for (const auto& stored : values) copy.values.push_back(Uds::Clone(stored));
    return copy;
}
