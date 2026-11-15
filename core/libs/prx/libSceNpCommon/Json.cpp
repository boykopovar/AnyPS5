#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <type_traits>
#include "prx/libSceNpCommon/include/Api.hpp"
#include "SceTypes.hpp"

namespace {

constexpr int NP_UTIL_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550601);
constexpr int NP_UTIL_ERROR_BUFFER_TOO_SMALL = static_cast<int>(0x80550602);
constexpr int NP_JSON_ERROR_TOO_DEEP = static_cast<int>(0x80550603);
constexpr int NP_SERVER_ERROR_JSON_BAD_CODE = static_cast<int>(0x80559C02);

constexpr int NP_SERVER_ERROR_BASE = static_cast<int>(0x82000000);
constexpr int NP_SERVER_ERROR_HTTP_BASE = static_cast<int>(0x82F00000);
constexpr int NP_SERVER_ERROR_UNKNOWN = static_cast<int>(0x82FFFFFF);
constexpr std::uint32_t NP_SERVER_ERROR_CODE_LIMIT = 0xC00000;
constexpr long long NP_SERVER_ERROR_CODE_MAX = 0xFFFFFF;
constexpr std::int32_t NP_SERVER_ERROR_NO_CODE = -1;
constexpr std::uint32_t HTTP_STATUS_FIRST = 100;
constexpr std::uint32_t HTTP_STATUS_COUNT = 500;

constexpr std::uint32_t JSON_MAX_DEPTH = 16;
constexpr std::size_t JSON_KEY_SIZE = 0x80;
constexpr std::size_t JSON_TEXT_SIZE = 0x600;
constexpr std::size_t JSON_MIN_VALUE_SIZE = 0x20;
constexpr std::size_t JSON_CHAR_WINDOW = 6;
constexpr std::size_t JSON_CHAR_SIZE = 8;
constexpr std::int32_t JSON_NO_INDEX = -1;

enum NpJsonEvent : std::int32_t {
    NP_JSON_OBJECT_START = 1,
    NP_JSON_OBJECT_END = 2,
    NP_JSON_ARRAY_START = 3,
    NP_JSON_ARRAY_END = 4,
    NP_JSON_KEY = 5,
    NP_JSON_VALUE = 6,
};

enum NpJsonValueType : std::uint32_t {
    NP_JSON_NULL = 0,
    NP_JSON_TRUE = 1,
    NP_JSON_FALSE = 2,
    NP_JSON_NUMBER = 3,
    NP_JSON_STRING = 4,
};

enum NpJsonRuleKind : std::uint32_t {
    NP_JSON_RULE_MEMBER = 1,
    NP_JSON_RULE_ELEMENT = 2,
};

enum ParserState : std::uint32_t {
    STATE_ROOT,
    STATE_OBJECT,
    STATE_OBJECT_END,
    STATE_ARRAY,
    STATE_ARRAY_END,
    STATE_KEY,
    STATE_KEY_TEXT,
    STATE_COLON,
    STATE_VALUE,
    STATE_NEXT,
    STATE_LITERAL,
    STATE_NUMBER,
    STATE_STRING,
    STATE_SKIP,
    STATE_SKIP_STRING,
    STATE_COUNT,
};

struct NpJsonIndices {
    std::int32_t slot[2 * JSON_MAX_DEPTH + 1];

    std::int32_t& Element(std::uint32_t depth) { return slot[depth]; }
    std::int32_t& Array(std::uint32_t depth) { return slot[JSON_MAX_DEPTH + depth]; }
    std::uint32_t State() const { return static_cast<std::uint32_t>(slot[2 * JSON_MAX_DEPTH]); }
    void SetState(std::uint32_t state) { slot[2 * JSON_MAX_DEPTH] = static_cast<std::int32_t>(state); }
};

struct NpJsonPending {
    char bytes[2 * JSON_CHAR_WINDOW];
};

struct NpJsonParseContext {
    const NpJsonRuleSet* rules;
    char data[JSON_KEY_SIZE + sizeof(std::uint32_t) + JSON_TEXT_SIZE];
    std::uint32_t depth;
    NpJsonIndices indices;
    std::uint32_t length;
    const char* literal;
    std::uint32_t matched;
    std::uint32_t descend;
    std::uint32_t rootDone;
    std::int32_t firstNamedDepth;
    std::uint32_t skipDepth;
    NpJsonPending pending;
    std::uint64_t pendingLength;
    std::uint64_t pendingNeed;
    NpJsonCallback callback;
    void* user;
};

struct NpJsonParseBufContext {
    const NpJsonRuleSet* rules;
    char* key;
    std::uint64_t keySize;
    std::uint32_t valueType;
    char* value;
    std::uint64_t valueSize;
    std::uint32_t depth;
    NpJsonIndices indices;
    std::uint32_t length;
    const char* literal;
    std::uint32_t matched;
    std::uint32_t descend;
    std::uint32_t rootDone;
    std::int32_t firstNamedDepth;
    std::uint32_t skipDepth;
    NpJsonPending pending;
    std::uint64_t pendingLength;
    std::uint64_t pendingNeed;
    NpJsonCallback callback;
    void* user;
};

struct NpJsonParseExContext {
    const NpJsonRuleSet* rules;
    char* key;
    std::uint64_t keySize;
    std::uint32_t valueType;
    char* value;
    std::uint64_t valueSize;
    std::uint32_t depth;
    NpJsonIndices indices;
    std::uint32_t offset;
    std::uint32_t tokenOffset;
    std::uint32_t length;
    const char* literal;
    std::uint32_t matched;
    std::uint32_t descend;
    std::uint32_t rootDone;
    std::int32_t firstNamedDepth;
    std::uint32_t skipDepth;
    NpJsonPending pending;
    std::uint64_t pendingLength;
    std::uint64_t pendingNeed;
    NpJsonExCallback callback;
    void* user;
};

struct NpJsonBufValue {
    std::uint32_t type;
    const char* text;
};

struct NpJsonExValue {
    std::uint32_t type;
    const char* text;
    std::uint32_t last;
};

static_assert(offsetof(NpJsonParseContext, data) == 0x8 && offsetof(NpJsonParseContext, depth) == 0x68C);
static_assert(offsetof(NpJsonParseContext, indices) == 0x690 && offsetof(NpJsonParseContext, length) == 0x714);
static_assert(offsetof(NpJsonParseContext, literal) == 0x718 && offsetof(NpJsonParseContext, matched) == 0x720);
static_assert(offsetof(NpJsonParseContext, firstNamedDepth) == 0x72C && offsetof(NpJsonParseContext, pending) == 0x734);
static_assert(offsetof(NpJsonParseContext, pendingLength) == 0x740 && offsetof(NpJsonParseContext, callback) == 0x750);
static_assert(sizeof(NpJsonParseContext) == 0x760);
static_assert(offsetof(NpJsonParseBufContext, valueType) == 0x18 && offsetof(NpJsonParseBufContext, depth) == 0x30);
static_assert(offsetof(NpJsonParseBufContext, length) == 0xB8 && offsetof(NpJsonParseBufContext, matched) == 0xC8);
static_assert(offsetof(NpJsonParseBufContext, pending) == 0xDC && offsetof(NpJsonParseBufContext, callback) == 0xF8);
static_assert(sizeof(NpJsonParseBufContext) == 0x108);
static_assert(offsetof(NpJsonParseExContext, offset) == 0xB8 && offsetof(NpJsonParseExContext, length) == 0xC0);
static_assert(offsetof(NpJsonParseExContext, matched) == 0xD0 && offsetof(NpJsonParseExContext, pending) == 0xE4);
static_assert(offsetof(NpJsonParseExContext, pendingNeed) == 0xF8 && offsetof(NpJsonParseExContext, callback) == 0x100);
static_assert(sizeof(NpJsonParseExContext) == 0x110);
static_assert(sizeof(NpJsonExValue) == 0x18);

constexpr char LITERAL_TRUE[] = "true";
constexpr char LITERAL_FALSE[] = "false";
constexpr char LITERAL_NULL[] = "null";
constexpr char EMPTY_TEXT[] = "";

bool IsNumberChar(unsigned char c) {
    return (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E';
}

struct DecodedChar {
    bool complete;
    char bytes[JSON_CHAR_SIZE];
    std::size_t length;
    std::size_t consumed;
    std::size_t raw;

    bool IsQuote() const { return complete && length == 1 && raw == 1 && bytes[0] == '"'; }
};

template <typename TContext>
class JsonParser {
public:
    static constexpr bool EX = std::is_same_v<TContext, NpJsonParseExContext>;
    static constexpr bool BUFFERED = !std::is_same_v<TContext, NpJsonParseContext>;

    explicit JsonParser(TContext* context) : c(context) {}

    int Parse(const char* data, int length) {
        state = c->indices.State();
        const int result = Run(data, length);
        if (result == 0) c->indices.SetState(state);
        return result;
    }

private:
    TContext* c;
    std::uint32_t state = STATE_ROOT;

    char* key() {
        if constexpr (BUFFERED) return c->key;
        else return c->data;
    }

    std::size_t keySize() const {
        if constexpr (BUFFERED) return c->keySize;
        else return JSON_KEY_SIZE;
    }

    char* text() {
        if constexpr (BUFFERED) return c->value;
        else return c->data + JSON_KEY_SIZE + sizeof(std::uint32_t);
    }

    std::size_t textSize() const {
        if constexpr (BUFFERED) return c->valueSize;
        else return JSON_TEXT_SIZE;
    }

    std::uint32_t valueType() const {
        if constexpr (BUFFERED) {
            return c->valueType;
        } else {
            std::uint32_t type = 0;
            std::memcpy(&type, c->data + JSON_KEY_SIZE, sizeof(type));
            return type;
        }
    }

    void setValueType(std::uint32_t type) {
        if constexpr (BUFFERED) c->valueType = type;
        else std::memcpy(c->data + JSON_KEY_SIZE, &type, sizeof(type));
    }

    void clearValue() {
        if constexpr (BUFFERED) std::memset(c->value, 0, c->valueSize);
        else std::memset(c->data + JSON_KEY_SIZE, 0, sizeof(std::uint32_t) + JSON_TEXT_SIZE);
    }

    void consume(std::size_t count) {
        if constexpr (EX) c->offset += static_cast<std::uint32_t>(count);
    }

    void markToken() {
        if constexpr (EX) c->tokenOffset = c->offset;
    }

    bool wildcard(std::uint32_t depth) const {
        return c->firstNamedDepth == JSON_NO_INDEX || static_cast<std::int32_t>(depth) < c->firstNamedDepth;
    }

    const NpJsonRule* level(std::uint32_t depth) const {
        if (depth >= static_cast<std::uint32_t>(c->rules->count)) return nullptr;
        const NpJsonRule* rules = c->rules->levels[depth];
        const auto kind = static_cast<std::uint32_t>(rules->kind);
        return kind == NP_JSON_RULE_MEMBER || kind == NP_JSON_RULE_ELEMENT ? rules : nullptr;
    }

    static bool validKind(const NpJsonRule& rule) {
        const auto kind = static_cast<std::uint32_t>(rule.kind);
        return kind == NP_JSON_RULE_MEMBER || kind == NP_JSON_RULE_ELEMENT;
    }

    bool keyMatches(const NpJsonRule& rule) {
        return rule.key != nullptr && strncasecmp(rule.key, key(), BUFFERED ? keySize() : JSON_KEY_SIZE) == 0;
    }

    bool findRule(std::uint32_t depth, bool colon, std::int32_t* element, std::uint32_t* descend) {
        if constexpr (EX) {
            if (c->rules == nullptr) {
                if (colon && depth > JSON_MAX_DEPTH) return false;
                *element = JSON_NO_INDEX;
                *descend = !colon || depth != JSON_MAX_DEPTH ? 1 : 0;
                return true;
            }
            if (colon && depth > JSON_MAX_DEPTH - 1) return false;
        }
        const NpJsonRule* rules = level(depth);
        if (rules == nullptr) return false;
        const bool wild = wildcard(depth);
        const bool inArray = !colon && c->indices.Array(depth) != JSON_NO_INDEX;
        for (std::int32_t i = 0; validKind(rules[i]); ++i) {
            const NpJsonRule& rule = rules[i];
            bool hit = false;
            if (wild) hit = rule.key == nullptr;
            else if (inArray) hit = static_cast<std::uint32_t>(rule.kind) == NP_JSON_RULE_ELEMENT;
            else hit = static_cast<std::uint32_t>(rule.kind) == NP_JSON_RULE_MEMBER && keyMatches(rule);
            if (hit) {
                *element = i;
                *descend = static_cast<std::uint32_t>(rule.descend);
                return true;
            }
        }
        return false;
    }

    bool findElementRule(std::uint32_t depth, std::int32_t* element, std::uint32_t* descend) {
        if constexpr (EX) {
            if (c->rules == nullptr) {
                *element = JSON_NO_INDEX;
                *descend = depth != JSON_MAX_DEPTH ? 1 : 0;
                return true;
            }
            if (depth > JSON_MAX_DEPTH - 1) return false;
        }
        const NpJsonRule* rules = level(depth);
        if (rules == nullptr) return false;
        for (std::int32_t i = 0; validKind(rules[i]); ++i) {
            const bool hit = static_cast<std::int32_t>(depth) >= c->firstNamedDepth
                ? static_cast<std::uint32_t>(rules[i].kind) == NP_JSON_RULE_ELEMENT
                : rules[i].key == nullptr;
            if (hit) {
                *element = i;
                *descend = static_cast<std::uint32_t>(rules[i].descend);
                return true;
            }
        }
        return false;
    }

    void setFirstNamedDepth(std::uint32_t depth) {
        if (c->firstNamedDepth == JSON_NO_INDEX) c->firstNamedDepth = static_cast<std::int32_t>(depth);
    }

    int emit(std::int32_t event, std::uint32_t depth, const char* eventKey, const void* value, bool atToken) {
        const std::int32_t element = c->indices.Element(depth);
        const std::int32_t arrayIndex = c->indices.Array(depth);
        if constexpr (EX) {
            const std::uint32_t offset = atToken ? c->tokenOffset : c->offset;
            return c->callback(c, event, depth, element, eventKey, value, arrayIndex, offset, c->user);
        } else {
            return c->callback(c, event, depth, element, eventKey, value, arrayIndex, c->user);
        }
    }

    int emitContainer(std::int32_t event, std::uint32_t depth) {
        return emit(event, depth, nullptr, nullptr, false);
    }

    int deliver(bool literal, bool last) {
        const char* valueKey = c->indices.Array(c->depth) == JSON_NO_INDEX ? key() : nullptr;
        if constexpr (EX) {
            const NpJsonExValue value{valueType(), literal ? EMPTY_TEXT : text(), last ? 1u : 0u};
            return emit(NP_JSON_VALUE, c->depth, valueKey, &value, true);
        } else if constexpr (BUFFERED) {
            const NpJsonBufValue value{valueType(), literal ? EMPTY_TEXT : text()};
            return emit(NP_JSON_VALUE, c->depth, valueKey, &value, true);
        } else {
            return emit(NP_JSON_VALUE, c->depth, valueKey, c->data + JSON_KEY_SIZE, true);
        }
    }

    DecodedChar readChar(const char* data, std::size_t remaining) {
        DecodedChar out{};
        if (c->pendingNeed != 0) {
            const std::size_t take = c->pendingNeed < remaining ? static_cast<std::size_t>(c->pendingNeed) : remaining;
            std::memcpy(c->pending.bytes + c->pendingLength, data, take);
            c->pendingLength += take;
            c->pendingNeed -= take;
            out.consumed = take;
            if (c->pendingNeed != 0) return out;
            const int result = sceNpUtilJsonGetOneChar(out.bytes, JSON_CHAR_SIZE, c->pending.bytes,
                static_cast<std::size_t>(c->pendingLength), &out.length, &out.raw);
            if (result == NP_UTIL_ERROR_BUFFER_TOO_SMALL) {
                c->pendingNeed = out.raw - c->pendingLength;
                return out;
            }
            out.complete = true;
            if (result < 0) {
                out.bytes[0] = c->pending.bytes[0];
                out.length = out.raw = out.consumed = 1;
                return out;
            }
            c->pendingNeed = 0;
            c->pendingLength = 0;
            return out;
        }
        const std::size_t window = remaining < JSON_CHAR_WINDOW ? remaining : JSON_CHAR_WINDOW;
        out.consumed = window;
        const int result = sceNpUtilJsonGetOneChar(out.bytes, JSON_CHAR_SIZE, data, window, &out.length, &out.raw);
        if (result == NP_UTIL_ERROR_BUFFER_TOO_SMALL) {
            std::memcpy(c->pending.bytes, data, window);
            c->pendingLength = window;
            c->pendingNeed = out.raw - window;
            return out;
        }
        out.complete = true;
        if (result < 0) {
            out.bytes[0] = data[0];
            out.length = out.raw = out.consumed = 1;
            return out;
        }
        out.consumed = out.raw;
        return out;
    }

    int openContainer(bool isArray) {
        const std::uint32_t depth = c->depth;
        if (depth > JSON_MAX_DEPTH - 1) return NP_JSON_ERROR_TOO_DEEP;
        std::int32_t element = 0;
        std::uint32_t descend = 0;
        if (!findRule(depth, false, &element, &descend)) {
            c->matched = 0;
            if (depth == 0) {
                state = STATE_ROOT;
                if (!isArray) setFirstNamedDepth(0);
                return 0;
            }
            c->depth = depth + 1;
            c->indices.Array(depth + 1) = isArray ? 0 : JSON_NO_INDEX;
            c->skipDepth = depth + 1;
            state = STATE_SKIP;
            if (!isArray) setFirstNamedDepth(depth + 1);
            return 0;
        }
        c->matched = 1;
        c->indices.Element(depth) = element;
        c->descend = descend;
        const int result = emitContainer(isArray ? NP_JSON_ARRAY_START : NP_JSON_OBJECT_START, depth);
        if (result < 0) return result;
        if (!isArray) {
            c->depth = depth + 1;
            c->indices.Array(depth + 1) = JSON_NO_INDEX;
            if (c->descend == 1) {
                state = STATE_KEY;
            } else {
                state = STATE_SKIP;
                c->skipDepth = depth + 1;
            }
            setFirstNamedDepth(depth + 1);
            return 0;
        }
        if (c->firstNamedDepth < 0) {
            if constexpr (EX) clearValue();
            c->depth = depth + 1;
            c->indices.Element(depth + 1) = element;
            c->indices.Array(depth + 1) = 0;
            c->descend = descend;
            state = STATE_VALUE;
            return 0;
        }
        if (!findElementRule(depth + 1, &element, &descend)) {
            c->matched = 0;
            if (depth == 0) {
                state = STATE_ROOT;
                return 0;
            }
            c->depth = depth + 1;
            c->indices.Array(depth + 1) = 0;
            c->skipDepth = depth + 1;
            state = STATE_SKIP;
            return 0;
        }
        c->matched = 1;
        if constexpr (EX) clearValue();
        c->depth = depth + 1;
        c->indices.Element(depth + 1) = element;
        c->indices.Array(depth + 1) = 0;
        c->descend = descend;
        state = STATE_VALUE;
        return 0;
    }

    int closeContainer(bool isArray) {
        if (c->matched == 1 && c->depth != 0) {
            --c->depth;
            const int result = emitContainer(isArray ? NP_JSON_ARRAY_END : NP_JSON_OBJECT_END, c->depth);
            if (result < 0) return result;
        }
        if (c->depth == 0) {
            c->rootDone = 1;
            state = STATE_ROOT;
        } else {
            state = STATE_NEXT;
        }
        return 0;
    }

    int matchKey() {
        const std::uint32_t depth = c->depth;
        std::int32_t element = 0;
        std::uint32_t descend = 0;
        if (!findRule(depth, true, &element, &descend)) {
            c->matched = 0;
            c->skipDepth = depth + 1;
            state = STATE_SKIP;
            return 0;
        }
        c->matched = 1;
        c->indices.Element(depth) = element;
        c->descend = descend;
        const int result = emit(NP_JSON_KEY, depth, key(), nullptr, true);
        if (result < 0) return result;
        clearValue();
        state = STATE_VALUE;
        return 0;
    }

    void startLiteral(std::uint32_t type, const char* literal) {
        setValueType(type);
        c->literal = literal;
        c->length = 1;
        markToken();
        state = STATE_LITERAL;
    }

    int flushPiece() {
        text()[c->length] = '\0';
        const int result = deliver(false, false);
        if (result < 0) return result;
        c->length = 0;
        clearValue();
        return 0;
    }

    int Run(const char* data, int length) {
        std::size_t position = 0;
        const auto total = static_cast<std::size_t>(length > 0 ? length : 0);
        while (position < total) {
            const auto ch = static_cast<unsigned char>(data[position]);
            const bool inObject = c->indices.Array(c->depth) == JSON_NO_INDEX;
            switch (state) {
            case STATE_ROOT:
                if (ch == '"') {
                    state = STATE_KEY;
                } else if (ch == '[' || ch == '{') {
                    if (c->rootDone) ++position;
                    else state = ch == '[' ? STATE_ARRAY : STATE_OBJECT;
                } else {
                    consume(1);
                    ++position;
                }
                break;
            case STATE_OBJECT:
            case STATE_ARRAY: {
                const bool isArray = state == STATE_ARRAY;
                if (ch == (isArray ? '[' : '{')) {
                    const int result = openContainer(isArray);
                    if (result < 0) return result;
                }
                consume(1);
                ++position;
                break;
            }
            case STATE_OBJECT_END:
            case STATE_ARRAY_END: {
                const bool isArray = state == STATE_ARRAY_END;
                if (ch == (isArray ? ']' : '}')) {
                    const int result = closeContainer(isArray);
                    if (result < 0) return result;
                }
                consume(1);
                ++position;
                break;
            }
            case STATE_KEY:
                if (ch == '}' && inObject) {
                    state = STATE_OBJECT_END;
                    break;
                }
                if (ch == ']' && !inObject) {
                    state = STATE_ARRAY_END;
                    break;
                }
                if (ch == '"') {
                    std::memset(key(), 0, keySize());
                    c->length = 0;
                    markToken();
                    state = STATE_KEY_TEXT;
                }
                consume(1);
                ++position;
                break;
            case STATE_KEY_TEXT: {
                const DecodedChar decoded = readChar(data + position, total - position);
                if (decoded.complete && (c->length + decoded.length >= keySize() || decoded.IsQuote())) {
                    key()[c->length] = '\0';
                    state = STATE_COLON;
                    break;
                }
                if (decoded.complete) {
                    std::memcpy(key() + c->length, decoded.bytes, decoded.length);
                    c->length += static_cast<std::uint32_t>(decoded.length);
                }
                consume(decoded.consumed);
                position += decoded.consumed;
                break;
            }
            case STATE_COLON:
                if (ch == ':') {
                    const int result = matchKey();
                    if (result < 0) return result;
                }
                consume(1);
                ++position;
                break;
            case STATE_VALUE:
                if (ch == '[') {
                    state = STATE_ARRAY;
                } else if (ch == '{') {
                    state = STATE_OBJECT;
                } else if (ch == ']' && !inObject) {
                    state = STATE_ARRAY_END;
                } else if (ch == '}' && inObject) {
                    state = STATE_OBJECT_END;
                } else {
                    if (ch == 't') startLiteral(NP_JSON_TRUE, LITERAL_TRUE);
                    else if (ch == 'f') startLiteral(NP_JSON_FALSE, LITERAL_FALSE);
                    else if (ch == 'n') startLiteral(NP_JSON_NULL, LITERAL_NULL);
                    else if ((ch >= '0' && ch <= '9') || ch == '-') {
                        setValueType(NP_JSON_NUMBER);
                        markToken();
                        text()[0] = static_cast<char>(ch);
                        c->length = 1;
                        state = STATE_NUMBER;
                    } else if (ch == '"') {
                        setValueType(NP_JSON_STRING);
                        markToken();
                        c->length = 0;
                        state = STATE_STRING;
                    }
                    consume(1);
                    ++position;
                }
                break;
            case STATE_LITERAL: {
                const char* literal = c->literal;
                const std::uint32_t matchedLength = c->length;
                if (literal == nullptr || std::strlen(literal) <= matchedLength
                    || ch != static_cast<unsigned char>(literal[matchedLength])) {
                    state = STATE_NEXT;
                    break;
                }
                c->length = matchedLength + 1;
                if (literal[matchedLength + 1] == '\0') {
                    const int result = deliver(true, true);
                    if (result < 0) return result;
                    if constexpr (EX) c->literal = nullptr;
                    state = STATE_NEXT;
                }
                consume(1);
                ++position;
                break;
            }
            case STATE_NUMBER:
                if (!IsNumberChar(ch)) {
                    text()[c->length] = '\0';
                    state = STATE_NEXT;
                    const int result = deliver(false, true);
                    if (result < 0) return result;
                    break;
                }
                if (c->length + 1 >= textSize()) {
                    if constexpr (!EX) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
                    const int result = flushPiece();
                    if (result < 0) return result;
                }
                text()[c->length] = static_cast<char>(ch);
                ++c->length;
                consume(1);
                ++position;
                break;
            case STATE_STRING: {
                const DecodedChar decoded = readChar(data + position, total - position);
                if (!decoded.complete) {
                    consume(decoded.consumed);
                    position += decoded.consumed;
                    break;
                }
                if (decoded.IsQuote()) {
                    text()[c->length] = '\0';
                    const int result = deliver(false, true);
                    if (result < 0) return result;
                    state = STATE_NEXT;
                    consume(1);
                    ++position;
                    break;
                }
                if (c->length + decoded.length >= textSize()) {
                    if constexpr (!EX) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
                    const int result = flushPiece();
                    if (result < 0) return result;
                }
                std::memcpy(text() + c->length, decoded.bytes, decoded.length);
                c->length += static_cast<std::uint32_t>(decoded.length);
                consume(decoded.consumed);
                position += decoded.consumed;
                break;
            }
            case STATE_NEXT:
                if (ch == '}' && inObject) {
                    c->matched = 1;
                    state = STATE_OBJECT_END;
                    break;
                }
                if (ch == ']' && !inObject) {
                    c->matched = 1;
                    state = STATE_ARRAY_END;
                    break;
                }
                if (ch == ',') {
                    if (inObject) {
                        state = STATE_KEY;
                    } else {
                        clearValue();
                        if (c->matched == 1) ++c->indices.Array(c->depth);
                        state = STATE_VALUE;
                    }
                }
                consume(1);
                ++position;
                break;
            case STATE_SKIP: {
                const std::uint32_t depth = c->depth;
                if (ch == '[' || ch == '{') {
                    if (depth > JSON_MAX_DEPTH - 1) return NP_JSON_ERROR_TOO_DEEP;
                    c->depth = depth + 1;
                    c->indices.Array(depth + 1) = ch == '[' ? 0 : JSON_NO_INDEX;
                } else if (ch == ']' || ch == '}') {
                    const bool closesHere = (c->indices.Array(depth) != JSON_NO_INDEX) == (ch == ']');
                    if (closesHere && depth != 0) {
                        if (depth < c->skipDepth) {
                            c->skipDepth = 0;
                            c->matched = 1;
                            state = ch == ']' ? STATE_ARRAY_END : STATE_OBJECT_END;
                            break;
                        }
                        c->depth = depth - 1;
                    }
                } else if (ch == '"') {
                    state = STATE_SKIP_STRING;
                } else if (ch == ',' && depth < c->skipDepth) {
                    c->skipDepth = 0;
                    state = STATE_NEXT;
                    break;
                }
                consume(1);
                ++position;
                break;
            }
            case STATE_SKIP_STRING: {
                const DecodedChar decoded = readChar(data + position, total - position);
                if (decoded.IsQuote()) {
                    state = STATE_SKIP;
                    consume(1);
                    ++position;
                    break;
                }
                consume(decoded.consumed);
                position += decoded.consumed;
                break;
            }
            default:
                consume(1);
                ++position;
                break;
            }
        }
        return 0;
    }
};

template <typename TContext>
void ResetParserState(TContext* context) {
    context->depth = 0;
    for (std::uint32_t i = 0; i < JSON_MAX_DEPTH; ++i) context->indices.Array(i) = JSON_NO_INDEX;
    context->firstNamedDepth = JSON_NO_INDEX;
}

template <typename TContext>
int InitBuffered(TContext* context, const NpJsonRuleSet* rules, char* key, std::size_t keySize, char* value,
    std::size_t valueSize) {
    if (context == nullptr || key == nullptr || value == nullptr) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    if (keySize == 0 || valueSize < JSON_MIN_VALUE_SIZE) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
    std::memset(&context->valueType, 0, sizeof(TContext) - offsetof(TContext, valueType));
    context->rules = rules;
    context->key = key;
    context->keySize = keySize;
    context->value = value;
    context->valueSize = valueSize;
    ResetParserState(context);
    return 0;
}

static_assert(offsetof(NpServerErrorJsonResult, code) == 4);

constexpr NpJsonRule SERVER_ERROR_TOP[] = {
    {NP_JSON_RULE_MEMBER, "error", 1},
    {0, "", 0},
};
constexpr NpJsonRule SERVER_ERROR_CODE[] = {
    {NP_JSON_RULE_MEMBER, "code", 0},
    {0, "", 0},
};
constexpr NpJsonRule SERVER_ERROR_MULTI_TOP[] = {
    {NP_JSON_RULE_MEMBER, "error", 1},
    {NP_JSON_RULE_MEMBER, "code", 0},
    {0, "", 0},
};
constexpr const NpJsonRule* SERVER_ERROR_LEVELS[] = {SERVER_ERROR_TOP, SERVER_ERROR_CODE};
constexpr const NpJsonRule* SERVER_ERROR_MULTI_LEVELS[] = {SERVER_ERROR_MULTI_TOP, SERVER_ERROR_CODE};
constexpr NpJsonRuleSet SERVER_ERROR_RULES = {SERVER_ERROR_LEVELS, 2};
constexpr NpJsonRuleSet SERVER_ERROR_MULTI_RULES = {SERVER_ERROR_MULTI_LEVELS, 2};

int StoreServerErrorCode(const void* value, void* user) {
    std::uint32_t type = 0;
    std::memcpy(&type, value, sizeof(type));
    if (type != NP_JSON_NUMBER) return NP_SERVER_ERROR_JSON_BAD_CODE;
    const long long code = std::strtoll(static_cast<const char*>(value) + sizeof(type), nullptr, 10);
    if (code < 0 || code > NP_SERVER_ERROR_CODE_MAX) return NP_SERVER_ERROR_JSON_BAD_CODE;
    auto* result = static_cast<NpServerErrorJsonResult*>(user);
    result->code = static_cast<std::int32_t>(code);
    result->found = 1;
    return 0;
}

int APS5_VABI ServerErrorCallback_nid_no_patch(void*, std::int32_t event, std::uint32_t depth, std::int32_t element,
    const char*, const void* value, std::int32_t, void* user) {
    if (event != NP_JSON_VALUE || depth != 1 || element != 0) return 0;
    return StoreServerErrorCode(value, user);
}

int APS5_VABI ServerErrorMultiCallback_nid_no_patch(void*, std::int32_t event, std::uint32_t depth,
    std::int32_t element, const char*, const void* value, std::int32_t, void* user) {
    if (event != NP_JSON_VALUE) return 0;
    if (!(depth == 1 && element == 0) && !(depth == 0 && element == 1)) return 0;
    return StoreServerErrorCode(value, user);
}

int InitServerError(NpJsonParseContext* context, NpServerErrorJsonResult* result, const NpJsonRuleSet* rules,
    NpJsonCallback callback);

int HttpStatusError(std::int32_t httpStatus) {
    if (static_cast<std::uint32_t>(httpStatus) - HTTP_STATUS_FIRST < HTTP_STATUS_COUNT)
        return NP_SERVER_ERROR_HTTP_BASE | httpStatus;
    return NP_SERVER_ERROR_UNKNOWN;
}

int ServerErrorCode(std::int32_t httpStatus, const char* json, std::size_t length, const NpJsonRuleSet* rules,
    NpJsonCallback callback) {
    if (json == nullptr || length == 0) return HttpStatusError(httpStatus);
    NpJsonParseContext context{};
    NpServerErrorJsonResult result{0, NP_SERVER_ERROR_NO_CODE};
    if (InitServerError(&context, &result, rules, callback) < 0) return HttpStatusError(httpStatus);
    const int parsed = JsonParser<NpJsonParseContext>(&context).Parse(json, static_cast<int>(length));
    if (parsed < 0) return HttpStatusError(httpStatus);
    if (result.found == 0 || result.code == NP_SERVER_ERROR_NO_CODE) return HttpStatusError(httpStatus);
    if (static_cast<std::uint32_t>(result.code) < NP_SERVER_ERROR_CODE_LIMIT) return NP_SERVER_ERROR_BASE | result.code;
    return NP_SERVER_ERROR_UNKNOWN;
}

} // namespace

extern "C" {

int APS5_VABI sceNpJsonParseInit(void* guestContext, const NpJsonRuleSet* rules, NpJsonCallback callback,
    void* user) {
    auto* context = static_cast<NpJsonParseContext*>(guestContext);
    if (context == nullptr) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    std::memset(context->data, 0, offsetof(NpJsonParseContext, callback) - offsetof(NpJsonParseContext, data));
    context->rules = rules;
    context->callback = callback;
    context->user = user;
    ResetParserState(context);
    return 0;
}

int APS5_VABI sceNpJsonParse(void* guestContext, const char* data, int length) {
    auto* context = static_cast<NpJsonParseContext*>(guestContext);
    return JsonParser<NpJsonParseContext>(context).Parse(data, length);
}

int APS5_VABI sceNpJsonParseBufInit(void* guestContext, const NpJsonRuleSet* rules, char* key,
    std::size_t keySize, char* value, std::size_t valueSize, NpJsonCallback callback, void* user) {
    auto* context = static_cast<NpJsonParseBufContext*>(guestContext);
    const int result = InitBuffered(context, rules, key, keySize, value, valueSize);
    if (result != 0) return result;
    context->callback = callback;
    context->user = user;
    return 0;
}

int APS5_VABI sceNpJsonParseBuf(void* guestContext, const char* data, int length) {
    auto* context = static_cast<NpJsonParseBufContext*>(guestContext);
    return JsonParser<NpJsonParseBufContext>(context).Parse(data, length);
}

int APS5_VABI sceNpJsonParseExInit(void* guestContext, const NpJsonRuleSet* rules, char* key,
    std::size_t keySize, char* value, std::size_t valueSize, NpJsonExCallback callback, void* user) {
    auto* context = static_cast<NpJsonParseExContext*>(guestContext);
    const int result = InitBuffered(context, rules, key, keySize, value, valueSize);
    if (result != 0) return result;
    context->callback = callback;
    context->user = user;
    return 0;
}

int APS5_VABI sceNpJsonParseEx(void* guestContext, const char* data, int length) {
    auto* context = static_cast<NpJsonParseExContext*>(guestContext);
    return JsonParser<NpJsonParseExContext>(context).Parse(data, length);
}

int APS5_VABI sceNpServerErrorJsonParseInit(void* guestContext, NpServerErrorJsonResult* result) {
    auto* context = static_cast<NpJsonParseContext*>(guestContext);
    return InitServerError(context, result, &SERVER_ERROR_RULES, ServerErrorCallback_nid_no_patch);
}

int APS5_VABI sceNpServerErrorJsonParseMultiInit(void* guestContext, NpServerErrorJsonResult* result) {
    auto* context = static_cast<NpJsonParseContext*>(guestContext);
    return InitServerError(context, result, &SERVER_ERROR_MULTI_RULES, ServerErrorMultiCallback_nid_no_patch);
}

int APS5_VABI sceNpServerErrorJsonParse(void* guestContext, const char* data, int length) {
    auto* context = static_cast<NpJsonParseContext*>(guestContext);
    return sceNpJsonParse(context, data, length);
}

int APS5_VABI sceNpServerErrorJsonGetErrorCode(std::int32_t httpStatus, const char* json, std::size_t length) {
    return ServerErrorCode(httpStatus, json, length, &SERVER_ERROR_RULES, ServerErrorCallback_nid_no_patch);
}

int APS5_VABI sceNpServerErrorJsonMultiGetErrorCode(std::int32_t httpStatus, const char* json, std::size_t length) {
    return ServerErrorCode(httpStatus, json, length, &SERVER_ERROR_MULTI_RULES, ServerErrorMultiCallback_nid_no_patch);
}

}

namespace {

int InitServerError(NpJsonParseContext* context, NpServerErrorJsonResult* result, const NpJsonRuleSet* rules,
    NpJsonCallback callback) {
    if (result == nullptr) return NP_UTIL_ERROR_INVALID_ARGUMENT;
    result->found = 0;
    result->code = NP_SERVER_ERROR_NO_CODE;
    return sceNpJsonParseInit(context, rules, callback, result);
}

} // namespace
