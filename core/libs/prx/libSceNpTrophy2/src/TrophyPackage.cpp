#include "prx/libSceNpTrophy2/include/TrophyPackage.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <stdexcept>
#include <system_error>

namespace NpTrophy2 {

namespace {

constexpr int MaxJsonDepth = 64;
constexpr std::size_t MaxJsonValues = std::size_t{1} << 20u;

std::uint64_t bigEndian(std::string_view data, std::uint64_t offset, std::size_t size) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < size; ++i) value = (value << 8u) | static_cast<std::uint8_t>(data[offset + i]);
    return value;
}

std::string hex(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

void appendUtf8(std::string& out, std::uint32_t code) {
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

class JsonReader {
public:
    explicit JsonReader(std::string_view text) : text(text) {}

    Json Document() {
        if (text.starts_with("\xEF\xBB\xBF")) position = 3;
        auto document = readValue(0);
        skipSpace();
        if (position != text.size()) fail("text follows the document");
        return document;
    }

private:
    [[noreturn]] void fail(const std::string& problem) const {
        throw std::runtime_error(problem + " at byte " + std::to_string(position));
    }

    void skipSpace() {
        while (position < text.size() && (text[position] == ' ' || text[position] == '\t' || text[position] == '\n' || text[position] == '\r')) ++position;
    }

    bool take(char expected) {
        if (position >= text.size() || text[position] != expected) return false;
        ++position;
        return true;
    }

    void expect(std::string_view word) {
        if (text.substr(position, word.size()) != word) fail("expected " + std::string(word));
        position += word.size();
    }

    Json readValue(int depth) {
        if (depth >= MaxJsonDepth) fail("values nest deeper than " + std::to_string(MaxJsonDepth) + " levels");
        if (++values > MaxJsonValues) fail("the document holds more than " + std::to_string(MaxJsonValues) + " values");
        skipSpace();
        if (position >= text.size()) fail("the document ends before a value");
        Json result;
        switch (text[position]) {
        case '{':
            result.kind = Json::Kind::Object;
            readObject(result, depth);
            break;
        case '[':
            result.kind = Json::Kind::Array;
            readArray(result, depth);
            break;
        case '"':
            result.kind = Json::Kind::String;
            result.text = readString();
            break;
        case 't':
            expect("true");
            result.kind = Json::Kind::Bool;
            result.boolean = true;
            break;
        case 'f':
            expect("false");
            result.kind = Json::Kind::Bool;
            break;
        case 'n':
            expect("null");
            break;
        default:
            result.kind = Json::Kind::Number;
            result.text = readNumber();
            break;
        }
        return result;
    }

    void readObject(Json& result, int depth) {
        ++position;
        skipSpace();
        if (take('}')) return;
        while (true) {
            skipSpace();
            if (position >= text.size() || text[position] != '"') fail("expected a member name");
            auto key = readString();
            skipSpace();
            if (!take(':')) fail("expected ':'");
            result.members.emplace_back(std::move(key), readValue(depth + 1));
            skipSpace();
            if (take('}')) return;
            if (!take(',')) fail("expected ',' or '}'");
        }
    }

    void readArray(Json& result, int depth) {
        ++position;
        skipSpace();
        if (take(']')) return;
        while (true) {
            result.items.push_back(readValue(depth + 1));
            skipSpace();
            if (take(']')) return;
            if (!take(',')) fail("expected ',' or ']'");
        }
    }

    std::uint32_t readHexDigits() {
        if (text.size() - position < 4) fail("a \\u escape is cut short");
        std::uint32_t code = 0;
        const auto* end = text.data() + position + 4;
        const auto [stop, error] = std::from_chars(text.data() + position, end, code, 16);
        if (error != std::errc{} || stop != end) fail("a \\u escape does not have 4 hex digits");
        position += 4;
        return code;
    }

    std::string readString() {
        ++position;
        std::string out;
        while (true) {
            if (position >= text.size()) fail("a string is not closed");
            const char character = text[position++];
            if (character == '"') return out;
            if (static_cast<unsigned char>(character) < 0x20) fail("a string holds a control character");
            if (character != '\\') {
                out += character;
                continue;
            }
            if (position >= text.size()) fail("a string is not closed");
            switch (text[position++]) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                auto code = readHexDigits();
                if (code >= 0xDC00 && code < 0xE000) fail("a low surrogate has no high surrogate");
                if (code >= 0xD800 && code < 0xDC00) {
                    if (text.substr(position, 2) != "\\u") fail("a high surrogate has no low surrogate");
                    position += 2;
                    const auto low = readHexDigits();
                    if (low < 0xDC00 || low >= 0xE000) fail("a high surrogate has no low surrogate");
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                appendUtf8(out, code);
                break;
            }
            default:
                fail("a string has an unknown escape");
            }
        }
    }

    bool skipDigits() {
        const auto start = position;
        while (position < text.size() && text[position] >= '0' && text[position] <= '9') ++position;
        return position != start;
    }

    std::string readNumber() {
        const auto start = position;
        take('-');
        if (!take('0') && !skipDigits()) fail("expected a value");
        if (take('.') && !skipDigits()) fail("a number has no digits after '.'");
        if (take('e') || take('E')) {
            if (!take('+')) take('-');
            if (!skipDigits()) fail("a number has no exponent digits");
        }
        return std::string(text.substr(start, position - start));
    }

    std::string_view text;
    std::size_t position = 0;
    std::size_t values = 0;
};

std::string describe(const Json& value) {
    switch (value.kind) {
    case Json::Kind::Null: return "null";
    case Json::Kind::Bool: return value.boolean ? "true" : "false";
    case Json::Kind::Number: return value.text;
    case Json::Kind::String: return "\"" + value.text + "\"";
    case Json::Kind::Array: return "an array";
    case Json::Kind::Object: return "an object";
    }
    throw std::logic_error("a JSON value has an unknown kind");
}

std::string kindName(Json::Kind kind) {
    switch (kind) {
    case Json::Kind::Null: return "null";
    case Json::Kind::Bool: return "boolean";
    case Json::Kind::Number: return "number";
    case Json::Kind::String: return "string";
    case Json::Kind::Array: return "array";
    case Json::Kind::Object: return "object";
    }
    throw std::logic_error("a JSON value has an unknown kind");
}

const Json& member(const Json& object, std::string_view key, Json::Kind kind, const std::string& where) {
    const auto* found = object.Find(key);
    if (found == nullptr || found->kind != kind) throw std::runtime_error(where + " has no " + std::string(key) + " " + kindName(kind));
    return *found;
}

Json document(const UcpFiles& files, const std::string& name) {
    const auto found = files.find(name);
    if (found == files.end()) throw std::runtime_error("it has no " + name);
    Json parsed;
    try {
        parsed = ParseJson(found->second);
    } catch (const std::runtime_error& error) {
        throw std::runtime_error(name + " is not JSON: " + error.what());
    }
    if (parsed.kind != Json::Kind::Object) throw std::runtime_error(name + " is not a JSON object");
    return parsed;
}

Comparator comparatorOf(const Json& value) {
    if (value.kind != Json::Kind::String) return Comparator::None;
    if (value.text == "ge") return Comparator::GreaterEqual;
    if (value.text == "gt") return Comparator::Greater;
    if (value.text == "le") return Comparator::LessEqual;
    if (value.text == "lt") return Comparator::Less;
    return Comparator::None;
}

std::string join(const std::vector<std::string>& parts, std::string_view separator) {
    std::string out;
    for (const auto& part : parts) {
        if (!out.empty()) out += separator;
        out += part;
    }
    return out;
}

std::string alternatives(const std::vector<std::string>& parts) {
    std::string out;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index != 0) out += index + 1 == parts.size() ? " or " : ", ";
        out += parts[index];
    }
    return out;
}

TrophyDefinition readTrophy(const Json& definition, std::size_t index, std::vector<std::string>& skipped) {
    const auto where = "tropconf.json trophies[" + std::to_string(index) + "]";
    if (definition.kind != Json::Kind::Object) throw std::runtime_error(where + " is not an object");
    const auto* id = definition.Find("id");
    const auto parsedId = id != nullptr ? id->Id() : std::nullopt;
    if (!parsedId) throw std::runtime_error(where + " has no id from 0 to 2147483647");
    TrophyDefinition trophy;
    trophy.id = *parsedId;
    const auto name = "tropconf.json trophy " + std::to_string(trophy.id);
    const auto* grade = definition.Find("grade");
    const auto letter = grade != nullptr && grade->kind == Json::Kind::String && grade->text.size() == 1 ? std::string_view("PGSB").find(grade->text[0]) : std::string_view::npos;
    if (letter == std::string_view::npos) throw std::runtime_error(name + " has no grade P, G, S or B");
    trophy.grade = GradePlatinum + static_cast<std::int32_t>(letter);
    if (const auto* group = definition.Find("groupId")) {
        const auto groupId = group->Id();
        if (!groupId) throw std::runtime_error(name + " has groupId " + describe(*group) + ", not an id from 0 to 2147483647");
        trophy.groupId = *groupId;
    }
    if (const auto* platinum = definition.Find("platinumTrophyId")) trophy.platinumId = platinum->Id();
    const auto* hidden = definition.Find("hidden");
    trophy.hidden = hidden != nullptr && hidden->IsTrue();
    const auto* reward = definition.Find("hasReward");
    trophy.hasReward = reward != nullptr && reward->IsTrue();
    const auto* condition = definition.Find("unlockCondition");
    if (condition == nullptr || condition->kind != Json::Kind::Object) return trophy;
    const auto* progressive = condition->Find("progressive");
    trophy.progressive = progressive != nullptr && progressive->IsTrue();
    if (const auto* stat = condition->Find("udsStatId")) {
        trophy.statId = stat->Unsigned();
        if (!trophy.statId) throw std::runtime_error(name + " has udsStatId " + describe(*stat) + ", not an unsigned integer");
    }
    std::vector<std::string> problems;
    if (const auto* target = condition->Find("targetValue")) {
        trophy.target = target->Unsigned();
        if (!trophy.target) problems.push_back("its targetValue " + describe(*target) + " is not an unsigned integer");
    } else if (trophy.statId) {
        problems.emplace_back("it has no targetValue");
    }
    if (const auto* comparator = condition->Find("comparator")) {
        trophy.comparator = comparatorOf(*comparator);
        if (trophy.comparator == Comparator::None) problems.push_back("its comparator " + describe(*comparator) + " is not ge, gt, le or lt");
    } else if (trophy.statId) {
        problems.emplace_back("it has no comparator");
    }
    if (!problems.empty()) skipped.push_back(name + " never unlocks through stats: " + join(problems, ", "));
    return trophy;
}

std::optional<std::string> propertyPath(std::string_view input) {
    if (!input.starts_with("$.")) return std::nullopt;
    const auto path = input.substr(2);
    bool segmentStart = true;
    for (const char character : path) {
        if (character == '.') {
            if (segmentStart) return std::nullopt;
            segmentStart = true;
            continue;
        }
        const bool word = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '_';
        if (!word) return std::nullopt;
        segmentStart = false;
    }
    if (segmentStart) return std::nullopt;
    return std::string(path);
}

void readStat(const Json& definition, std::size_t index, StatRules& rules) {
    const auto* id = definition.Find("statId");
    const auto statId = id != nullptr ? id->Unsigned() : std::nullopt;
    if (!statId) {
        rules.skipped.push_back("stats_definition.json statDefinitionArray[" + std::to_string(index) + "] has no unsigned statId");
        return;
    }
    const auto name = "stat " + std::to_string(*statId);
    const auto* aggregation = definition.Find("aggregation");
    if (aggregation == nullptr || aggregation->kind != Json::Kind::String || aggregation->text != "latest") {
        rules.skipped.push_back(name + " has aggregation " + (aggregation != nullptr ? describe(*aggregation) : std::string("none")) + "; only \"latest\" is supported");
        return;
    }
    const auto* type = definition.Find("dataType");
    StatDefinition stat;
    if (type != nullptr && type->kind == Json::Kind::String && type->text == "uint32") {
        stat.type = StatType::UInt32;
        stat.maximum = UINT32_MAX;
    } else if (type != nullptr && type->kind == Json::Kind::String && type->text == "uint64") {
        stat.type = StatType::UInt64;
        stat.maximum = UINT64_MAX;
    } else {
        rules.skipped.push_back(name + " has dataType " + (type != nullptr ? describe(*type) : std::string("none")) + "; only \"uint32\" and \"uint64\" are supported");
        return;
    }
    if (const auto* minimum = definition.Find("minValue")) {
        const auto value = minimum->Unsigned();
        if (!value || *value > stat.maximum) {
            rules.skipped.push_back(name + " has minValue " + describe(*minimum) + ", which its " + type->text + " type cannot hold");
            return;
        }
        stat.minimum = *value;
    }
    if (const auto* maximum = definition.Find("maxValue")) {
        const auto value = maximum->Unsigned();
        if (!value || *value > stat.maximum || *value < stat.minimum) {
            rules.skipped.push_back(name + " has maxValue " + describe(*maximum) + ", which is not from its minValue to its " + type->text + " limit");
            return;
        }
        stat.maximum = *value;
    }
    if (!rules.stats.emplace(*statId, stat).second) rules.skipped.push_back(name + " is defined twice; the first definition is used");
}

void readRule(const Json& rule, std::size_t index, StatRules& rules) {
    const auto where = "stats_extraction.json statsExtractionRuleArray[" + std::to_string(index) + "]";
    const auto* condition = rule.Find("condition");
    const auto* action = rule.Find("action");
    if (condition == nullptr || condition->kind != Json::Kind::Object || action == nullptr || action->kind != Json::Kind::Object) {
        rules.skipped.push_back(where + " has no condition and action objects");
        return;
    }
    const auto* event = condition->Find("eventName");
    if (event == nullptr || event->kind != Json::Kind::String) {
        rules.skipped.push_back(where + " has no eventName");
        return;
    }
    if (condition->members.size() != 1) {
        rules.skipped.push_back(where + " filters " + event->text + " by more than its eventName; only eventName conditions are supported");
        return;
    }
    const auto* output = action->Find("output");
    const auto* id = output != nullptr ? output->Find("statId") : nullptr;
    const auto statId = id != nullptr ? id->Unsigned() : std::nullopt;
    if (!statId) {
        rules.skipped.push_back(where + " for " + event->text + " has no unsigned output statId");
        return;
    }
    if (!rules.stats.contains(*statId)) {
        rules.skipped.push_back(where + " for " + event->text + " writes stat " + std::to_string(*statId) + ", which has no supported definition");
        return;
    }
    const auto* input = action->Find("input");
    const auto path = input != nullptr && input->kind == Json::Kind::String ? propertyPath(input->text) : std::nullopt;
    if (!path) {
        rules.skipped.push_back(where + " for " + event->text + " reads " + (input != nullptr ? describe(*input) : std::string("no input")) + "; only \"$.name\" property paths are supported");
        return;
    }
    rules.events[event->text].push_back({*path, *statId});
}

}

const Json* Json::Find(std::string_view key) const {
    for (auto member = members.rbegin(); member != members.rend(); ++member) {
        if (member->first == key) return &member->second;
    }
    return nullptr;
}

bool Json::IsTrue() const {
    return kind == Kind::Bool && boolean;
}

std::optional<std::uint64_t> Json::Unsigned() const {
    if (kind != Kind::Number && kind != Kind::String) return std::nullopt;
    std::uint64_t value = 0;
    const auto* end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    if (error != std::errc{} || stop != end) return std::nullopt;
    return value;
}

std::optional<std::int32_t> Json::Id() const {
    const auto value = Unsigned();
    if (!value || *value > static_cast<std::uint64_t>(INT32_MAX)) return std::nullopt;
    return static_cast<std::int32_t>(*value);
}

Json ParseJson(std::string_view text) {
    return JsonReader(text).Document();
}

UcpFiles ReadUcp(std::string_view data) {
    if (data.size() < UcpHeaderSize) throw std::runtime_error("it has " + std::to_string(data.size()) + " bytes, fewer than the " + std::to_string(UcpHeaderSize) + "-byte header");
    const auto magic = bigEndian(data, 0, 4);
    if (magic != UcpMagic) throw std::runtime_error("its magic is " + hex(magic) + ", not " + hex(UcpMagic));
    const auto version = bigEndian(data, 4, 4);
    if (version != UcpVersion) throw std::runtime_error("its version is " + std::to_string(version) + ", not " + std::to_string(UcpVersion));
    const auto size = bigEndian(data, 8, 8);
    if (size < UcpHeaderSize || size > data.size()) throw std::runtime_error("its size " + hex(size) + " is not from the header size " + hex(UcpHeaderSize) + " to the file size " + hex(data.size()));
    const auto count = bigEndian(data, 0x10, 4);
    if (count > UcpMaxEntries) throw std::runtime_error("it lists " + std::to_string(count) + " files, more than " + std::to_string(UcpMaxEntries));
    const auto toc = bigEndian(data, 0x14, 4);
    if (toc > size || UcpTocHeaderSize + count * UcpEntrySize > size - toc) {
        throw std::runtime_error("its table of " + std::to_string(count) + " files at " + hex(toc) + " ends past its size " + hex(size));
    }
    UcpFiles files;
    for (std::uint64_t index = 0; index < count; ++index) {
        const auto entry = toc + UcpTocHeaderSize + index * UcpEntrySize;
        const auto field = data.substr(entry, UcpNameSize);
        const std::string name(field.substr(0, field.find('\0')));
        const auto offset = bigEndian(data, entry + UcpNameSize, 8);
        const auto length = bigEndian(data, entry + UcpNameSize + 8, 8);
        if (offset > size || length > size - offset) {
            throw std::runtime_error("its file \"" + name + "\" at " + hex(offset) + " with " + hex(length) + " bytes ends past its size " + hex(size));
        }
        files.try_emplace(name, data.substr(offset, length));
    }
    return files;
}

bool UnlockCondition::Reached(std::uint64_t value) const {
    switch (comparator) {
    case Comparator::GreaterEqual: return value >= target;
    case Comparator::Greater: return value > target;
    case Comparator::LessEqual: return value <= target;
    case Comparator::Less: return value < target;
    case Comparator::None: break;
    }
    throw std::logic_error("an unlock condition on stat " + std::to_string(statId) + " has no comparator");
}

std::optional<UnlockCondition> TrophyDefinition::Condition() const {
    if (grade == GradePlatinum || !statId || !target || comparator == Comparator::None) return std::nullopt;
    return UnlockCondition{*statId, *target, comparator};
}

TrophyPackage ParseTrophyPackage(std::string_view ucp, std::string_view language) {
    const auto files = ReadUcp(ucp);
    const auto conf = document(files, "tropconf.json");
    const auto& trophies = member(conf, "trophies", Json::Kind::Array, "tropconf.json");
    if (trophies.items.size() > MaxTrophies) throw std::runtime_error("tropconf.json lists " + std::to_string(trophies.items.size()) + " trophies, more than " + std::to_string(MaxTrophies));
    const auto& defaultLanguage = member(conf, "defaultLanguage", Json::Kind::String, "tropconf.json");
    TrophyPackage package;
    const std::array<std::string_view, 3> candidates{language, "en-US", defaultLanguage.text};
    std::vector<std::string> tried;
    for (std::size_t index = 0; index < candidates.size() && package.language.empty(); ++index) {
        const auto candidate = candidates[index];
        const auto earlier = candidates.begin() + static_cast<std::ptrdiff_t>(index);
        if (candidate.empty() || std::find(candidates.begin(), earlier, candidate) != earlier) continue;
        if (files.contains("tropmeta_" + std::string(candidate) + ".json")) {
            package.language = candidate;
        } else {
            tried.push_back(index + 1 == candidates.size() ? "its defaultLanguage \"" + std::string(candidate) + "\"" : std::string(candidate));
        }
    }
    if (package.language.empty()) throw std::runtime_error("it has no tropmeta_<language>.json for " + alternatives(tried));
    const auto metaName = "tropmeta_" + package.language + ".json";
    const auto meta = document(files, metaName);
    const auto& metadata = member(meta, "metadata", Json::Kind::Object, metaName);
    const auto& titleMetadata = member(metadata, "titleMetadata", Json::Kind::Object, metaName + " metadata");
    package.title = member(titleMetadata, "name", Json::Kind::String, metaName + " titleMetadata").text;
    const auto& trophyMetadata = member(metadata, "trophyMetadata", Json::Kind::Array, metaName + " metadata");
    for (std::size_t index = 0; index < trophies.items.size(); ++index) {
        auto trophy = readTrophy(trophies.items[index], index, package.skipped);
        package.groups.try_emplace(trophy.groupId, trophy.groupId == BaseGroup ? package.title : std::string());
        if (package.groups.size() > MaxGroups) throw std::runtime_error("tropconf.json uses more than " + std::to_string(MaxGroups) + " groups");
        const auto id = trophy.id;
        if (!package.trophies.emplace(id, std::move(trophy)).second) throw std::runtime_error("tropconf.json lists trophy " + std::to_string(id) + " twice");
    }
    if (const auto* groups = metadata.Find("groupMetadata"); groups != nullptr && groups->kind == Json::Kind::Array) {
        for (const auto& group : groups->items) {
            const auto* id = group.Find("id");
            const auto* name = group.Find("name");
            const auto groupId = id != nullptr ? id->Id() : std::nullopt;
            if (!groupId || name == nullptr || name->kind != Json::Kind::String) continue;
            if (const auto found = package.groups.find(*groupId); found != package.groups.end()) found->second = name->text;
        }
    }
    for (const auto& text : trophyMetadata.items) {
        const auto* id = text.Find("id");
        const auto trophyId = id != nullptr ? id->Id() : std::nullopt;
        const auto found = trophyId ? package.trophies.find(*trophyId) : package.trophies.end();
        if (found == package.trophies.end()) continue;
        for (const auto& [key, field] : {std::pair{"name", &found->second.name}, std::pair{"detail", &found->second.detail}, std::pair{"reward", &found->second.reward}}) {
            if (const auto* value = text.Find(key); value != nullptr && value->kind == Json::Kind::String) *field = value->text;
        }
    }
    return package;
}

StatRules ParseStatRules(std::string_view ucp) {
    const auto files = ReadUcp(ucp);
    const auto definitions = document(files, "stats_definition.json");
    const auto extraction = document(files, "stats_extraction.json");
    const auto& stats = member(definitions, "statDefinitionArray", Json::Kind::Array, "stats_definition.json");
    const auto& extractionRules = member(extraction, "statsExtractionRuleArray", Json::Kind::Array, "stats_extraction.json");
    StatRules rules;
    for (std::size_t index = 0; index < stats.items.size(); ++index) readStat(stats.items[index], index, rules);
    for (std::size_t index = 0; index < extractionRules.items.size(); ++index) readRule(extractionRules.items[index], index, rules);
    return rules;
}

std::string_view LanguageTag(int consoleLanguage) {
    static constexpr std::array<std::string_view, 31> tags = {
        "ja-JP", "en-US", "fr-FR", "es-ES", "de-DE", "it-IT", "nl-NL", "pt-PT", "ru-RU", "ko-KR", "zh-Hant",
        "zh-Hans", "fi-FI", "sv-SE", "da-DK", "no-NO", "pl-PL", "pt-BR", "en-GB", "tr-TR", "es-419", "ar-AE",
        "fr-CA", "cs-CZ", "hu-HU", "el-GR", "ro-RO", "th-TH", "vi-VN", "id-ID", "uk-UA"};
    if (consoleLanguage < 0 || consoleLanguage >= static_cast<int>(tags.size())) {
        throw std::out_of_range("console language " + std::to_string(consoleLanguage) + " has no locale tag");
    }
    return tags[static_cast<std::size_t>(consoleLanguage)];
}

}
