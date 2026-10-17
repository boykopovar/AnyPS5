#include "prx/libSceNpUniversalDataSystem/include/UdsEvent.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"

#include <cstdio>
#include <optional>
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
            std::snprintf(number, sizeof(number), "%.9g", static_cast<double>(held));
            out += number;
        } else if constexpr (std::is_same_v<Held, double>) {
            std::snprintf(number, sizeof(number), "%.17g", held);
            out += number;
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
