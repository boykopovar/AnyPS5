#include "prx/libkernel/AppMetadata/include/ParamJsonParser.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

enum class JsonType { Null, Boolean, Number, String, Array, Object };

struct JsonValue;

using JsonArray = std::vector<JsonValue>;
using JsonObject = std::vector<std::pair<std::string, JsonValue>>;

struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    JsonArray arrayValue;
    JsonObject objectValue;
};

std::string readFileToString(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open param.json");
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void skipWhitespace(const std::string& text, std::size_t& position) {
    while (position < text.size()) {
        const char c = text[position];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++position; else break;
    }
}

char peekChar(const std::string& text, std::size_t position) {
    if (position >= text.size()) throw std::runtime_error("unexpected end of param.json");
    return text[position];
}

void expectChar(const std::string& text, std::size_t& position, char expected) {
    if (position >= text.size() || text[position] != expected) throw std::runtime_error("malformed param.json");
    ++position;
}

void expectLiteral(const std::string& text, std::size_t& position, const char* literal) {
    for (const char* p = literal; *p != '\0'; ++p) {
        if (position >= text.size() || text[position] != *p) throw std::runtime_error("malformed param.json");
        ++position;
    }
}

void appendUtf8(std::string& destination, std::uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        destination.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        destination.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        destination.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        destination.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        destination.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        destination.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        destination.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        destination.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        destination.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        destination.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        throw std::runtime_error("invalid unicode codepoint in param.json");
    }
}

std::uint32_t parseHex4(const std::string& text, std::size_t& position) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        if (position >= text.size()) throw std::runtime_error("malformed unicode escape in param.json");
        const char c = text[position];
        value <<= 4;
        if (c >= '0' && c <= '9') value |= static_cast<std::uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') value |= static_cast<std::uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= static_cast<std::uint32_t>(c - 'A' + 10);
        else throw std::runtime_error("malformed unicode escape in param.json");
        ++position;
    }
    return value;
}

std::string parseJsonString(const std::string& text, std::size_t& position) {
    expectChar(text, position, '"');
    std::string result;
    while (true) {
        if (position >= text.size()) throw std::runtime_error("unterminated string in param.json");
        const char c = text[position++];
        if (c == '"') break;
        if (static_cast<unsigned char>(c) < 0x20) throw std::runtime_error("control character in param.json string");
        if (c != '\\') { result.push_back(c); continue; }
        if (position >= text.size()) throw std::runtime_error("unterminated escape in param.json");
        const char escape = text[position++];
        switch (escape) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': {
                std::uint32_t codepoint = parseHex4(text, position);
                if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                    if (position + 1 >= text.size() || text[position] != '\\' || text[position + 1] != 'u') throw std::runtime_error("unpaired surrogate in param.json");
                    position += 2;
                    const std::uint32_t low = parseHex4(text, position);
                    if (low < 0xDC00 || low > 0xDFFF) throw std::runtime_error("invalid surrogate pair in param.json");
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                    throw std::runtime_error("unpaired surrogate in param.json");
                }
                appendUtf8(result, codepoint);
                break;
            }
            default: throw std::runtime_error("invalid escape sequence in param.json");
        }
    }
    return result;
}

JsonValue parseJsonValue(const std::string& text, std::size_t& position);

JsonValue parseJsonObject(const std::string& text, std::size_t& position) {
    expectChar(text, position, '{');
    JsonValue value;
    value.type = JsonType::Object;
    skipWhitespace(text, position);
    if (peekChar(text, position) == '}') { ++position; return value; }
    while (true) {
        skipWhitespace(text, position);
        std::string key = parseJsonString(text, position);
        skipWhitespace(text, position);
        expectChar(text, position, ':');
        JsonValue member = parseJsonValue(text, position);
        value.objectValue.emplace_back(std::move(key), std::move(member));
        skipWhitespace(text, position);
        const char next = peekChar(text, position);
        if (next == ',') { ++position; continue; }
        if (next == '}') { ++position; break; }
        throw std::runtime_error("malformed object in param.json");
    }
    return value;
}

JsonValue parseJsonArray(const std::string& text, std::size_t& position) {
    expectChar(text, position, '[');
    JsonValue value;
    value.type = JsonType::Array;
    skipWhitespace(text, position);
    if (peekChar(text, position) == ']') { ++position; return value; }
    while (true) {
        JsonValue element = parseJsonValue(text, position);
        value.arrayValue.push_back(std::move(element));
        skipWhitespace(text, position);
        const char next = peekChar(text, position);
        if (next == ',') { ++position; continue; }
        if (next == ']') { ++position; break; }
        throw std::runtime_error("malformed array in param.json");
    }
    return value;
}

JsonValue parseJsonNumber(const std::string& text, std::size_t& position) {
    const std::size_t start = position;
    if (peekChar(text, position) == '-') ++position;
    if (position >= text.size() || text[position] < '0' || text[position] > '9') throw std::runtime_error("malformed number in param.json");
    while (position < text.size() && text[position] >= '0' && text[position] <= '9') ++position;
    if (position < text.size() && text[position] == '.') {
        ++position;
        if (position >= text.size() || text[position] < '0' || text[position] > '9') throw std::runtime_error("malformed number in param.json");
        while (position < text.size() && text[position] >= '0' && text[position] <= '9') ++position;
    }
    if (position < text.size() && (text[position] == 'e' || text[position] == 'E')) {
        ++position;
        if (position < text.size() && (text[position] == '+' || text[position] == '-')) ++position;
        if (position >= text.size() || text[position] < '0' || text[position] > '9') throw std::runtime_error("malformed number in param.json");
        while (position < text.size() && text[position] >= '0' && text[position] <= '9') ++position;
    }
    JsonValue value;
    value.type = JsonType::Number;
    value.numberValue = std::stod(text.substr(start, position - start));
    return value;
}

JsonValue parseJsonValue(const std::string& text, std::size_t& position) {
    skipWhitespace(text, position);
    const char c = peekChar(text, position);
    if (c == '{') return parseJsonObject(text, position);
    if (c == '[') return parseJsonArray(text, position);
    if (c == '"') { JsonValue value; value.type = JsonType::String; value.stringValue = parseJsonString(text, position); return value; }
    if (c == 't') { expectLiteral(text, position, "true"); JsonValue value; value.type = JsonType::Boolean; value.boolValue = true; return value; }
    if (c == 'f') { expectLiteral(text, position, "false"); JsonValue value; value.type = JsonType::Boolean; value.boolValue = false; return value; }
    if (c == 'n') { expectLiteral(text, position, "null"); JsonValue value; value.type = JsonType::Null; return value; }
    if (c == '-' || (c >= '0' && c <= '9')) return parseJsonNumber(text, position);
    throw std::runtime_error("unexpected character in param.json");
}

JsonValue parseJsonDocument(const std::string& text) {
    std::size_t position = 0;
    JsonValue root = parseJsonValue(text, position);
    skipWhitespace(text, position);
    if (position != text.size()) throw std::runtime_error("unexpected trailing data in param.json");
    return root;
}

const JsonValue* findObjectMember(const JsonValue& object, const std::string& key) {
    if (object.type != JsonType::Object) throw std::runtime_error("expected JSON object in param.json");
    for (const auto& member : object.objectValue) {
        if (member.first == key) return &member.second;
    }
    return nullptr;
}

const std::string& asString(const JsonValue& value, const char* context) {
    if (value.type != JsonType::String) throw std::runtime_error(std::string(context) + " is not a string in param.json");
    return value.stringValue;
}

}

ParsedParamJson parseParamJson(const std::filesystem::path& paramJsonPath) {
    const std::string text = readFileToString(paramJsonPath);
    const JsonValue root = parseJsonDocument(text);
    if (root.type != JsonType::Object) throw std::runtime_error("param.json root is not an object");
    const JsonValue* titleIdValue = findObjectMember(root, "titleId");
    if (titleIdValue == nullptr) throw std::runtime_error("param.json is missing titleId");
    const std::string titleId = asString(*titleIdValue, "titleId");
    if (titleId.empty()) throw std::runtime_error("param.json titleId is empty");
    const JsonValue* localizedParametersValue = findObjectMember(root, "localizedParameters");
    if (localizedParametersValue == nullptr) throw std::runtime_error("param.json is missing localizedParameters");
    if (localizedParametersValue->type != JsonType::Object) throw std::runtime_error("param.json localizedParameters is not an object");
    if (localizedParametersValue->objectValue.empty()) throw std::runtime_error("param.json localizedParameters is empty");
    std::string selectedLanguage;
    const JsonValue* defaultLanguageValue = findObjectMember(root, "defaultLanguage");
    if (defaultLanguageValue != nullptr) selectedLanguage = asString(*defaultLanguageValue, "defaultLanguage");
    const JsonValue* languageObject = nullptr;
    if (!selectedLanguage.empty()) languageObject = findObjectMember(*localizedParametersValue, selectedLanguage);
    if (languageObject == nullptr) languageObject = findObjectMember(*localizedParametersValue, "en-US");
    if (languageObject == nullptr) languageObject = &localizedParametersValue->objectValue.front().second;
    const JsonValue* titleValue = findObjectMember(*languageObject, "titleName");
    if (titleValue == nullptr) throw std::runtime_error("param.json localized entry is missing titleName");
    const std::string title = asString(*titleValue, "titleName");
    if (title.empty()) throw std::runtime_error("param.json titleName is empty");
    ParsedParamJson result;
    result.title = title;
    result.titleId = titleId;
    if (const JsonValue* downloadData = findObjectMember(root, "downloadDataSize")) {
        if (downloadData->type != JsonType::Number || downloadData->numberValue < 0) throw std::runtime_error("param.json downloadDataSize is not a size");
        result.downloadDataSizeMiB = static_cast<std::uint64_t>(downloadData->numberValue);
    }
    return result;
}
