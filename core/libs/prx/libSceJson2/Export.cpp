#include <algorithm>
#include <bit>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <list>
#include <locale>
#include <mutex>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

enum ValueType : std::int32_t {
    TypeNull = 0,
    TypeBoolean = 1,
    TypeInteger = 2,
    TypeUInteger = 3,
    TypeReal = 4,
    TypeString = 5,
    TypeArray = 6,
    TypeObject = 7,
};

constexpr int JsonErrorParse = static_cast<int>(0x80848101);

struct Node;
struct Value { Node* node; };
struct String { std::string* text; };
struct Array { std::list<Value>* items; };
struct Pair;
struct Object { std::list<Pair>* items; };
struct ArrayIteratorState {
    std::list<Value>::iterator position;
    std::list<Value>::iterator end;
};
struct ArrayIterator { ArrayIteratorState* state; };
struct ObjectIteratorState {
    std::list<Pair>* owner = nullptr;
    std::list<Pair>::iterator position{};
};
struct ObjectIterator { ObjectIteratorState* state; };
struct Pair {
    String key;
    std::uint64_t reserved;
    Value value;
};
static_assert(sizeof(Value) == 8 && sizeof(String) == 8 && sizeof(Array) == 8 && sizeof(Object) == 8);
static_assert(sizeof(ArrayIterator) == 8 && sizeof(ObjectIterator) == 8);
static_assert(offsetof(Pair, value) == 0x10);

using TypeMismatchHandler = const Value& (APS5_VABI*)(ValueType, const Value*, const Value*, void*);
using SpecialFloatHandler = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
using ElementAccessFailureHandler = void (APS5_VABI*)(int, const String*, const std::size_t*, const Value*, const Value*, void*);

struct Handlers {
    TypeMismatchHandler typeMismatch = nullptr;
    void* typeMismatchContext = nullptr;
    SpecialFloatHandler specialFloat = nullptr;
    void* specialFloatContext = nullptr;
    ElementAccessFailureHandler elementAccessFailure = nullptr;
    void* elementAccessFailureContext = nullptr;
};

struct Node {
    ValueType type = TypeNull;
    bool boolean = false;
    std::int64_t integer = 0;
    std::uint64_t uinteger = 0;
    double real = 0;
    String string{};
    Array array{};
    Object object{};
    Handlers handlers{};
};

using NullAccessCallback = const Value& (APS5_VABI*)(ValueType, const Value*, void*);

struct Globals {
    std::mutex mutex;
    NullAccessCallback callback = nullptr;
    void* context = nullptr;
};

Globals& State() {
    static Globals globals;
    return globals;
}

void Construct(String& s, const std::string& text) { s.text = new std::string(text); }
void Construct(Array& a) { a.items = new std::list<Value>(); }
void Construct(Object& o) { o.items = new std::list<Pair>(); }
void Destroy(Value& v);
void CopyInto(Value& destination, const Value& source);

void Destroy(String& s) { delete s.text; s.text = nullptr; }
void Destroy(Array& a) {
    if (!a.items) return;
    for (auto& item : *a.items) Destroy(item);
    delete a.items;
    a.items = nullptr;
}
void Destroy(Object& o) {
    if (!o.items) return;
    for (auto& pair : *o.items) {
        Destroy(pair.key);
        Destroy(pair.value);
    }
    delete o.items;
    o.items = nullptr;
}

void Clear(Node& n) {
    if (n.type == TypeString) Destroy(n.string);
    if (n.type == TypeArray) Destroy(n.array);
    if (n.type == TypeObject) Destroy(n.object);
    const Handlers handlers = n.handlers;
    n = Node{};
    n.handlers = handlers;
}

void Destroy(Value& v) {
    if (!v.node) return;
    Clear(*v.node);
    delete v.node;
    v.node = nullptr;
}

Node& NodeOf(const Value& v) {
    if (!v.node) throw std::runtime_error("sce::Json::Value used before construction");
    return *v.node;
}

void Construct(Value& v) { v.node = new Node(); }

template<typename TInitialize>
void Construct(Value& destination, TInitialize initialize) {
    Value pending{};
    Construct(pending);
    try {
        initialize(*pending.node);
    } catch (...) {
        Destroy(pending);
        throw;
    }
    destination = pending;
}

void CopyArray(Array& destination, const Array& source) {
    Array pending{};
    Construct(pending);
    try {
        for (const auto& item : *source.items) {
            CopyInto(pending.items->emplace_back(), item);
        }
    } catch (...) {
        Destroy(pending);
        throw;
    }
    destination = pending;
}

void CopyObject(Object& destination, const Object& source) {
    Object pending{};
    Construct(pending);
    try {
        for (const auto& pair : *source.items) {
            auto& copy = pending.items->emplace_back();
            Construct(copy.key, *pair.key.text);
            CopyInto(copy.value, pair.value);
        }
    } catch (...) {
        Destroy(pending);
        throw;
    }
    destination = pending;
}

void AssignNode(Node& destination, const Node& source) {
    Node copy{};
    copy.type = source.type;
    copy.boolean = source.boolean;
    copy.integer = source.integer;
    copy.uinteger = source.uinteger;
    copy.real = source.real;
    try {
        if (source.type == TypeString) Construct(copy.string, *source.string.text);
        if (source.type == TypeArray) CopyArray(copy.array, source.array);
        if (source.type == TypeObject) CopyObject(copy.object, source.object);
    } catch (...) {
        Clear(copy);
        throw;
    }
    Clear(destination);
    copy.handlers = destination.handlers;
    destination = copy;
}

void CopyInto(Value& destination, const Value& source) {
    Construct(destination, [&](Node& node) { AssignNode(node, NodeOf(source)); });
}

void SetType(Node& n, ValueType type) {
    if (type < TypeNull || type > TypeObject) throw std::invalid_argument("sce::Json::Value::set: invalid value type");
    Node pending{};
    pending.type = type;
    if (type == TypeString) Construct(pending.string, std::string());
    if (type == TypeArray) Construct(pending.array);
    if (type == TypeObject) Construct(pending.object);
    Clear(n);
    pending.handlers = n.handlers;
    n = pending;
}

const Value& Defaults(ValueType type) {
    static Value values[8]{};
    static std::once_flag once;
    std::call_once(once, [] {
        for (int i = 0; i < 8; ++i) {
            Construct(values[i]);
            SetType(*values[i].node, static_cast<ValueType>(i));
        }
    });
    return values[type];
}

const Value& NullAccess(ValueType requested, const Value* parent) {
    auto& state = State();
    NullAccessCallback callback;
    void* context;
    {
        std::lock_guard lock(state.mutex);
        callback = state.callback;
        context = state.context;
    }
    if (callback) {
        const Value& result = callback(requested, parent, context);
        if (result.node && (requested == TypeNull || result.node->type == requested)) return result;
    }
    return Defaults(requested);
}

const Value& TypeMismatch(ValueType requested, const Value* self) {
    const Handlers& handlers = NodeOf(*self).handlers;
    const Value& result = handlers.typeMismatch(requested, self, nullptr, handlers.typeMismatchContext);
    if (result.node && result.node->type == requested) return result;
    return Defaults(requested);
}

const Node& Typed(const Value* self, ValueType requested) {
    const Node& n = NodeOf(*self);
    if (n.type == requested) return n;
    if (n.type != TypeNull && n.handlers.typeMismatch) return NodeOf(TypeMismatch(requested, self));
    return NodeOf(NullAccess(requested, self));
}

const Value& MissingElement(const Value* self) {
    if (NodeOf(*self).handlers.elementAccessFailure)
        throw std::runtime_error("sce::Json::Value: element access failure handler: the error code the console passes is unverified");
    return NullAccess(TypeNull, self);
}

template <typename TSetter>
void SetHandlers(Node& n, const TSetter& setter) {
    setter(n.handlers);
    if (n.type == TypeArray)
        for (auto& item : *n.array.items) SetHandlers(NodeOf(item), setter);
    if (n.type == TypeObject)
        for (auto& pair : *n.object.items) SetHandlers(NodeOf(pair.value), setter);
}

void AppendEscaped(std::string& out, const std::string& text) {
    out += '"';
    for (const unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                } else out += static_cast<char>(c);
        }
    }
    out += '"';
}

std::string RealText(double value) {
    if (!std::isfinite(value)) throw std::runtime_error("sce::Json: cannot serialize a non-finite number");
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, 17);
    if (result.ec != std::errc{}) throw std::runtime_error("sce::Json: real number formatting failed");
    return std::string(buffer, result.ptr);
}

void Serialize(std::string& out, const Node& n) {
    switch (n.type) {
        case TypeNull: out += "null"; return;
        case TypeBoolean: out += n.boolean ? "true" : "false"; return;
        case TypeInteger: out += std::to_string(n.integer); return;
        case TypeUInteger: out += std::to_string(n.uinteger); return;
        case TypeReal:
            if (!std::isfinite(n.real) && n.handlers.specialFloat)
                throw std::runtime_error("sce::Json::Value: special float handler: the FunctionType the console passes is unverified");
            out += RealText(n.real);
            return;
        case TypeString: AppendEscaped(out, *n.string.text); return;
        case TypeArray: {
            out += '[';
            bool first = true;
            for (const auto& item : *n.array.items) {
                if (!first) out += ',';
                first = false;
                Serialize(out, NodeOf(item));
            }
            out += ']';
            return;
        }
        case TypeObject: {
            out += '{';
            bool first = true;
            for (const auto& pair : *n.object.items) {
                if (!first) out += ',';
                first = false;
                AppendEscaped(out, *pair.key.text);
                out += ':';
                Serialize(out, NodeOf(pair.value));
            }
            out += '}';
            return;
        }
    }
    throw std::runtime_error("sce::Json: invalid value type");
}

int CompareText(const char* left, const char* right) {
    while (*left != '\0' && *left == *right) {
        ++left;
        ++right;
    }
    return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

std::list<Pair>::iterator ObjectPosition(const Object& object, const std::string& key) {
    return std::find_if(object.items->begin(), object.items->end(), [&](const Pair& pair) { return std::strcmp(pair.key.text->c_str(), key.c_str()) >= 0; });
}

Value& ObjectEntry(Object& object, const std::string& key) {
    auto position = ObjectPosition(object, key);
    if (position != object.items->end() && std::strcmp(position->key.text->c_str(), key.c_str()) == 0) return position->value;
    Pair pair{};
    Construct(pair.key, key);
    try {
        Construct(pair.value);
        return object.items->insert(position, pair)->value;
    } catch (...) {
        Destroy(pair.value);
        Destroy(pair.key);
        throw;
    }
}

using ParserReadCallback = int (APS5_VABI*)(char&, void*);

class Parser {
public:
    Parser(const char* text, std::size_t size, const Handlers& handlers) : text(text), end(text + size), handlers(handlers) {}
    Parser(ParserReadCallback read, void* context, const Handlers& handlers) : read(read), context(context), handlers(handlers) {}

    bool Parse(Node& out) {
        skip();
        return value(out);
    }

    int Error() const { return readError < 0 ? readError : JsonErrorParse; }

private:
    static constexpr std::size_t MaxNestingDepth = 512;

    const char* text = nullptr;
    const char* end = nullptr;
    ParserReadCallback read = nullptr;
    void* context = nullptr;
    Handlers handlers;
    std::size_t depth = 0;
    int current = -2;
    int readError = 0;

    int peek() {
        if (current != -2) return current;
        if (read != nullptr) {
            char character = 0;
            readError = read(character, context);
            current = readError < 0 ? -1 : static_cast<unsigned char>(character);
        } else if (text == end) {
            readError = JsonErrorParse;
            current = -1;
        } else {
            current = static_cast<unsigned char>(*text++);
        }
        return current;
    }
    int take() {
        const int character = peek();
        if (character >= 0) current = -2;
        return character;
    }
    void skip() { while (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r') take(); }
    bool literal(const char* word) {
        for (; *word != '\0'; ++word) {
            if (peek() != static_cast<unsigned char>(*word)) return false;
            take();
        }
        return true;
    }
    static void appendUtf8(std::string& out, std::uint32_t code) {
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) { out += static_cast<char>(0xc0 | (code >> 6)); out += static_cast<char>(0x80 | (code & 0x3f)); }
        else if (code < 0x10000) { out += static_cast<char>(0xe0 | (code >> 12)); out += static_cast<char>(0x80 | ((code >> 6) & 0x3f)); out += static_cast<char>(0x80 | (code & 0x3f)); }
        else { out += static_cast<char>(0xf0 | (code >> 18)); out += static_cast<char>(0x80 | ((code >> 12) & 0x3f)); out += static_cast<char>(0x80 | ((code >> 6) & 0x3f)); out += static_cast<char>(0x80 | (code & 0x3f)); }
    }
    bool hex4(std::uint32_t& code) {
        code = 0;
        for (int i = 0; i < 4; ++i) {
            const int c = take();
            code <<= 4;
            if (c >= '0' && c <= '9') code |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') code |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') code |= static_cast<std::uint32_t>(c - 'A' + 10);
            else return false;
        }
        return true;
    }
    bool string(std::string& out) {
        if (peek() < 0 || peek() != '"') return false;
        take();
        while (peek() >= 0 && peek() != '"') {
            const int c = take();
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c != '\\') { out += c; continue; }
            if (peek() < 0) return false;
            switch (take()) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    std::uint32_t code = 0;
                    if (!hex4(code)) return false;
                    if (code >= 0xd800 && code < 0xdc00) {
                        std::uint32_t low = 0;
                        if (!literal("\\u") || !hex4(low) || low < 0xdc00 || low >= 0xe000) return false;
                        code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                    } else if (code >= 0xdc00 && code < 0xe000) return false;
                    appendUtf8(out, code);
                    break;
                }
                default: return false;
            }
        }
        if (peek() < 0) return false;
        take();
        return true;
    }
    bool number(Node& out) {
        std::string token;
        if (peek() >= 0 && peek() == '-') token.push_back(static_cast<char>(take()));
        if (peek() < 0 || peek() < '0' || peek() > '9') return false;
        while (peek() >= '0' && peek() <= '9') token.push_back(static_cast<char>(take()));
        bool integral = true;
        if (peek() >= 0 && peek() == '.') {
            integral = false;
            token.push_back(static_cast<char>(take()));
            if (peek() < 0 || peek() < '0' || peek() > '9') return false;
            while (peek() >= 0 && peek() >= '0' && peek() <= '9') token.push_back(static_cast<char>(take()));
        }
        if (peek() >= 0 && (peek() == 'e' || peek() == 'E')) {
            integral = false;
            token.push_back(static_cast<char>(take()));
            if (peek() >= 0 && (peek() == '+' || peek() == '-')) token.push_back(static_cast<char>(take()));
            if (peek() < 0 || peek() < '0' || peek() > '9') return false;
            while (peek() >= 0 && peek() >= '0' && peek() <= '9') token.push_back(static_cast<char>(take()));
        }
        const std::string& text = token;
        if (integral) {
            errno = 0;
            if (text[0] == '-') {
                const long long value = std::strtoll(text.c_str(), nullptr, 10);
                if (errno == 0) { out.type = TypeInteger; out.integer = value; return true; }
            } else {
                const unsigned long long value = std::strtoull(text.c_str(), nullptr, 10);
                if (errno == 0) {
                    out.type = TypeUInteger;
                    out.uinteger = value;
                    return true;
                }
            }
        }
        double real = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), real);
        bool converted = parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
        if (parsed.ec == std::errc::result_out_of_range) {
            std::istringstream stream(text);
            stream.imbue(std::locale::classic());
            stream >> real;
            converted = !stream.fail() && stream.eof();
        }
        if (!converted || !std::isfinite(real)) throw std::runtime_error("sce::Json::Parser::parse: number " + text + " overflows a double; the console's behaviour is unverified");
        out.type = TypeReal;
        out.real = real;
        return true;
    }
    bool value(Node& out) {
        if (peek() < 0) return false;
        switch (peek()) {
            case 'n': if (!literal("null")) return false; SetType(out, TypeNull); return true;
            case 't': if (!literal("true")) return false; SetType(out, TypeBoolean); out.boolean = true; return true;
            case 'f': if (!literal("false")) return false; SetType(out, TypeBoolean); return true;
            case '"': {
                std::string text;
                if (!string(text)) return false;
                SetType(out, TypeString);
                *out.string.text = std::move(text);
                return true;
            }
            case '[': {
                if (depth == MaxNestingDepth) return false;
                take();
                ++depth;
                SetType(out, TypeArray);
                skip();
                if (peek() >= 0 && peek() == ']') { take(); --depth; return true; }
                for (;;) {
                    Value item{};
                    Construct(item);
                    item.node->handlers = handlers;
                    out.array.items->push_back(item);
                    skip();
                    if (!value(*item.node)) return false;
                    skip();
                    if (peek() >= 0 && peek() == ',') { take(); continue; }
                    if (peek() >= 0 && peek() == ']') { take(); --depth; return true; }
                    return false;
                }
            }
            case '{': {
                if (depth == MaxNestingDepth) return false;
                take();
                ++depth;
                SetType(out, TypeObject);
                skip();
                if (peek() >= 0 && peek() == '}') { take(); --depth; return true; }
                for (;;) {
                    skip();
                    std::string key;
                    if (!string(key)) return false;
                    skip();
                    if (peek() < 0 || peek() != ':') return false;
                    take();
                    skip();
                    Value& entry = ObjectEntry(out.object, key);
                    Clear(*entry.node);
                    entry.node->handlers = handlers;
                    if (!value(*entry.node)) return false;
                    skip();
                    if (peek() >= 0 && peek() == ',') { take(); continue; }
                    if (peek() >= 0 && peek() == '}') { take(); --depth; return true; }
                    return false;
                }
            }
            default:
                return number(out);
        }
    }
};

int ParseValue(Value* out, Parser& parser) {
    Node parsed{};
    parsed.handlers = NodeOf(*out).handlers;
    bool parsedOk = false;
    try {
        parsedOk = parser.Parse(parsed);
    } catch (...) {
        Clear(parsed);
        throw;
    }
    if (!parsedOk) {
        Clear(parsed);
        return parser.Error();
    }
    Clear(NodeOf(*out));
    *out->node = parsed;
    return 0;
}

[[noreturn]] void APS5_VABI JsonMemAllocatorPureVirtual() {
    throw std::runtime_error("sce::Json::MemAllocator: pure virtual function called");
}

void* const JsonMemAllocatorVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual), reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual), reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual), reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
};


}

extern "C" {

int APS5_VABI _ZN3sce4Json11InitializerC1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce4Json11InitializerD1Ev(void* self) {
    (void)self;
    return 0;
}

int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void* self, const void* parameter) {
    (void)self;
    if (parameter == nullptr) throw std::invalid_argument("sce::Json::Initializer::initialize: null parameter");
    return 0;
}

int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void* self) {
    (void)self;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.callback = nullptr;
    state.context = nullptr;
    return 0;
}

int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void* self, NullAccessCallback callback, void* context) {
    (void)self;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.callback = callback;
    state.context = context;
    return 0;
}

int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void* self, NullAccessCallback callback, void* context) {
    return _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(self, callback, context);
}

void APS5_VABI _ZN3sce4Json12MemAllocatorC2Ev(void* self) {
    *static_cast<void* const**>(self) = &JsonMemAllocatorVtable[2];
}

void APS5_VABI _ZN3sce4Json12MemAllocatorD2Ev(void* self) {
    *static_cast<void* const**>(self) = &JsonMemAllocatorVtable[2];
}

std::size_t _ZN3sce4Json6String4nposE = static_cast<std::size_t>(-1);

void APS5_VABI _ZN3sce4Json6StringC1Ev(String* self) { Construct(*self, std::string()); }
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String* self, const char* text) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::String: null text");
    Construct(*self, text);
}
void APS5_VABI _ZN3sce4Json6StringC1ERKS1_(String* self, const String* other) { Construct(*self, *other->text); }
void APS5_VABI _ZN3sce4Json6StringD1Ev(String* self) { Destroy(*self); }
String* APS5_VABI _ZN3sce4Json6StringaSERKS1_(String* self, const String* other) {
    if (self != other) *self->text = *other->text;
    return self;
}
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String* self) { return self->text->c_str(); }
bool APS5_VABI _ZNK3sce4Json6String5emptyEv(const String* self) { return self->text->empty(); }
std::size_t APS5_VABI _ZNK3sce4Json6String6lengthEv(const String* self) { return self->text->size(); }
std::size_t APS5_VABI _ZNK3sce4Json6String4sizeEv(const String* self) { return self->text->size(); }
std::size_t APS5_VABI _ZNK3sce4Json6String8capacityEv(const String* self) { return self->text->capacity(); }
bool APS5_VABI _ZNK3sce4Json6StringeqEPKc(const String* self, const char* text) { return text != nullptr && CompareText(self->text->c_str(), text) == 0; }
bool APS5_VABI _ZNK3sce4Json6StringeqERKS1_(const String* self, const String* other) { return other != nullptr && other->text != nullptr && CompareText(self->text->c_str(), other->text->c_str()) == 0; }
void APS5_VABI _ZN3sce4Json6String5clearEv(String* self) { self->text->clear(); }
void APS5_VABI _ZN3sce4Json6String6resizeEm(String* self, std::size_t count) { self->text->resize(count); }
void APS5_VABI _ZN3sce4Json6String7reserveEm(String* self, std::size_t capacity) { self->text->reserve(capacity); }
String* APS5_VABI _ZN3sce4Json6String6appendERKS1_(String* self, const String* other) {
    const std::string copy = *other->text;
    self->text->append(copy);
    return self;
}
String* APS5_VABI _ZN3sce4Json6String6appendEPKc(String* self, const char* text) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::String::append: null text");
    self->text->append(text);
    return self;
}
String* APS5_VABI _ZN3sce4Json6String6appendEPKcm(String* self, const char* text, std::size_t length) {
    if (text == nullptr && length > 0) throw std::invalid_argument("sce::Json::String::append: null text");
    self->text->append(text, length);
    return self;
}
String* APS5_VABI _ZN3sce4Json6StringpLEPKc(String* self, const char* text) {
    return _ZN3sce4Json6String6appendEPKc(self, text);
}
String* APS5_VABI _ZN3sce4Json6StringpLEh(String* self, unsigned char ch) {
    self->text->push_back(static_cast<char>(ch));
    return self;
}
String* APS5_VABI _ZNK3sce4Json6String6substrEmm(String* result, const String* self, std::size_t position, std::size_t length) {
    if (position > self->text->size()) throw std::out_of_range("sce::Json::String::substr: position past the end");
    Construct(*result, self->text->substr(position, length));
    return result;
}
std::size_t APS5_VABI _ZNK3sce4Json6String4findEcm(const String* self, char character, std::size_t position) { return self->text->find(character, position); }
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEcm(const String* self, char character, std::size_t position) { return self->text->rfind(character, position); }
char APS5_VABI _ZNK3sce4Json6String2atEm(const String* self, std::size_t position) {
    if (position >= self->text->size()) throw std::out_of_range("sce::Json::String::at: position past the end");
    return (*self->text)[position];
}
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcm(const String* self, const char* text, std::size_t position) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::String::find: null text");
    return self->text->find(text, position);
}
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcmm(const String* self, const char* text, std::size_t position, std::size_t length) {
    if (text == nullptr && length > 0) throw std::invalid_argument("sce::Json::String::find: null text");
    return self->text->find(std::string_view(text == nullptr ? "" : text, length), position);
}
std::size_t APS5_VABI _ZNK3sce4Json6String4findERKS1_m(const String* self, const String* other, std::size_t position) {
    if (other == nullptr || other->text == nullptr) throw std::invalid_argument("sce::Json::String::find: null other");
    return self->text->find(*other->text, position);
}
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcm(const String* self, const char* text, std::size_t position) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::String::rfind: null text");
    return self->text->rfind(text, position);
}
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcmm(const String* self, const char* text, std::size_t position, std::size_t length) {
    if (text == nullptr && length > 0) throw std::invalid_argument("sce::Json::String::rfind: null text");
    return self->text->rfind(std::string_view(text == nullptr ? "" : text, length), position);
}
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindERKS1_m(const String* self, const String* other, std::size_t position) {
    if (other == nullptr || other->text == nullptr) throw std::invalid_argument("sce::Json::String::rfind: null other");
    return self->text->rfind(*other->text, position);
}
int APS5_VABI _ZNK3sce4Json6String7compareEPKc(const String* self, const char* text) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::String::compare: null text");
    return CompareText(self->text->c_str(), text);
}
int APS5_VABI _ZNK3sce4Json6String7compareERKS1_(const String* self, const String* other) {
    if (other == nullptr || other->text == nullptr) throw std::invalid_argument("sce::Json::String::compare: null other");
    return CompareText(self->text->c_str(), other->text->c_str());
}

void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array* self) { Construct(*self); }
void APS5_VABI _ZN3sce4Json5ArrayC1ERKS1_(Array* self, const Array* other) { CopyArray(*self, *other); }
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array* self) { Destroy(*self); }
Array* APS5_VABI _ZN3sce4Json5ArrayaSERKS1_(Array* self, const Array* other) {
    if (self == other) return self;
    Array copy{};
    CopyArray(copy, *other);
    Destroy(*self);
    *self = copy;
    return self;
}
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array* self, const Value* value) {
    Value copy{};
    CopyInto(copy, *value);
    try {
        self->items->push_back(copy);
    } catch (...) {
        Destroy(copy);
        throw;
    }
}
ArrayIterator* APS5_VABI _ZNK3sce4Json5Array5beginEv(ArrayIterator* result, const Array* self) { result->state = new ArrayIteratorState{self->items->begin(), self->items->end()}; return result; }
ArrayIterator* APS5_VABI _ZNK3sce4Json5Array3endEv(ArrayIterator* result, const Array* self) { result->state = new ArrayIteratorState{self->items->end(), self->items->end()}; return result; }
bool APS5_VABI _ZNK3sce4Json5Array5emptyEv(const Array* self) { return self->items->empty(); }
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array* self) { return self->items->size(); }
void APS5_VABI _ZN3sce4Json5Array5clearEv(Array* self) {
    for (auto& item : *self->items) Destroy(item);
    self->items->clear();
}
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array* self) {
    if (self->items->empty()) throw std::out_of_range("sce::Json::Array::front: empty array");
    return &self->items->front();
}
const Value* APS5_VABI _ZNK3sce4Json5Array4backEv(const Array* self) {
    if (self->items->empty()) throw std::out_of_range("sce::Json::Array::back: empty array");
    return &self->items->back();
}
void APS5_VABI _ZN3sce4Json5Array9pop_frontEv(Array* self) {
    if (self->items->empty()) throw std::out_of_range("sce::Json::Array::pop_front: empty array");
    Destroy(self->items->front());
    self->items->pop_front();
}
void APS5_VABI _ZN3sce4Json5Array8iteratorD1Ev(ArrayIterator* self) { delete self->state; self->state = nullptr; }
ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEv(ArrayIterator* self) {
    if (self->state && self->state->position != self->state->end) ++self->state->position;
    return self;
}
Value* APS5_VABI _ZNK3sce4Json5Array8iteratordeEv(const ArrayIterator* self) {
    if (!self->state || self->state->position == self->state->end) throw std::out_of_range("sce::Json::Array::iterator: no current element");
    return &*self->state->position;
}
Value* APS5_VABI _ZNK3sce4Json5Array8iteratorptEv(const ArrayIterator* self) { return _ZNK3sce4Json5Array8iteratordeEv(self); }
bool APS5_VABI _ZNK3sce4Json5Array8iteratorneERKS2_(const ArrayIterator* self, const ArrayIterator* other) {
    if (!self->state || !other->state) return false;
    const Value* left = self->state && self->state->position != self->state->end ? &*self->state->position : nullptr;
    const Value* right = other->state && other->state->position != other->state->end ? &*other->state->position : nullptr;
    return left != right;
}
void APS5_VABI _ZN3sce4Json5Array8iteratorC1ERKS2_(ArrayIterator* self, const ArrayIterator* other) {
    self->state = other->state ? new ArrayIteratorState(*other->state) : nullptr;
}
ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iterator7advanceEm(ArrayIterator* self, std::size_t count) {
    if (self->state) {
        while (count && self->state->position != self->state->end) {
            ++self->state->position;
            --count;
        }
    }
    return self;
}

void APS5_VABI _ZN3sce4Json6ObjectC1Ev(Object* self) { Construct(*self); }
void APS5_VABI _ZN3sce4Json6ObjectC1ERKS1_(Object* self, const Object* other) { CopyObject(*self, *other); }
void APS5_VABI _ZN3sce4Json6ObjectD1Ev(Object* self) { Destroy(*self); }
Object* APS5_VABI _ZN3sce4Json6ObjectaSERKS1_(Object* self, const Object* other) {
    if (self == other) return self;
    Object copy{};
    CopyObject(copy, *other);
    Destroy(*self);
    *self = copy;
    return self;
}
void APS5_VABI _ZN3sce4Json6Object5clearEv(Object* self) {
    for (auto& pair : *self->items) {
        Destroy(pair.key);
        Destroy(pair.value);
    }
    self->items->clear();
}
Value* APS5_VABI _ZN3sce4Json6ObjectixERKNS0_6StringE(Object* self, const String* key) { return &ObjectEntry(*self, *key->text); }
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object5beginEv(ObjectIterator* result, const Object* self) { result->state = new ObjectIteratorState{self->items, self->items->begin()}; return result; }
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object3endEv(ObjectIterator* result, const Object* self) { result->state = new ObjectIteratorState{self->items, self->items->end()}; return result; }
void APS5_VABI _ZN3sce4Json6Object8iteratorD1Ev(ObjectIterator* self) { delete self->state; self->state = nullptr; }
ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEv(ObjectIterator* self) {
    if (self->state && self->state->owner) {
        if (self->state->position == self->state->owner->end()) throw std::out_of_range("sce::Json::Object::iterator: advance past the end");
        ++self->state->position;
    }
    return self;
}
Pair* APS5_VABI _ZNK3sce4Json6Object8iteratordeEv(const ObjectIterator* self) {
    if (!self->state || !self->state->owner || self->state->position == self->state->owner->end()) throw std::out_of_range("sce::Json::Object::iterator: no current element");
    return &*self->state->position;
}
Pair* APS5_VABI _ZNK3sce4Json6Object8iteratorptEv(const ObjectIterator* self) { return _ZNK3sce4Json6Object8iteratordeEv(self); }
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object4findERKNS0_6StringE(ObjectIterator* result, const Object* self, const String* key) {
    const auto position = std::find_if(self->items->begin(), self->items->end(), [&](const Pair& pair) { return std::strcmp(pair.key.text->c_str(), key->text->c_str()) == 0; });
    result->state = new ObjectIteratorState{self->items, position};
    return result;
}
bool APS5_VABI _ZNK3sce4Json6Object8iteratoreqERKS2_(const ObjectIterator* self, const ObjectIterator* other) {
    if (!self->state || !other->state) return false;
    return self->state->owner == other->state->owner && self->state->position == other->state->position;
}
bool APS5_VABI _ZNK3sce4Json6Object8iteratorneERKS2_(const ObjectIterator* self, const ObjectIterator* other) {
    if (!self->state || !other->state) return false;
    return !_ZNK3sce4Json6Object8iteratoreqERKS2_(self, other);
}
void APS5_VABI _ZN3sce4Json6Object8iteratorC1ERKS2_(ObjectIterator* self, const ObjectIterator* other) {
    self->state = other->state ? new ObjectIteratorState(*other->state) : nullptr;
}
ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iterator7advanceEm(ObjectIterator* self, std::size_t count) {
    if (self->state && self->state->owner) {
        while (count) {
            _ZN3sce4Json6Object8iteratorppEv(self);
            --count;
        }
    }
    return self;
}
bool APS5_VABI _ZNK3sce4Json6Object5emptyEv(const Object* self) { return self->items->empty(); }
std::size_t APS5_VABI _ZNK3sce4Json6Object4sizeEv(const Object* self) { return self->items->size(); }
void APS5_VABI _ZN3sce4Json6Object5eraseERKNS0_6StringE(Object* self, const String* key) {
    if (key == nullptr || key->text == nullptr) throw std::invalid_argument("sce::Json::Object::erase: null key");
    auto it = std::find_if(self->items->begin(), self->items->end(), [&](const Pair& pair) { return std::strcmp(pair.key.text->c_str(), key->text->c_str()) == 0; });
    if (it == self->items->end()) return;
    Destroy(it->key);
    Destroy(it->value);
    self->items->erase(it);
}

void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value* self) { Construct(*self); }
void APS5_VABI _ZN3sce4Json5ValueC1Eb(Value* self, bool value) { Construct(*self); self->node->type = TypeBoolean; self->node->boolean = value; }
void APS5_VABI _ZN3sce4Json5ValueC1El(Value* self, std::int64_t value) { Construct(*self); self->node->type = TypeInteger; self->node->integer = value; }
void APS5_VABI _ZN3sce4Json5ValueC1Em(Value* self, std::uint64_t value) { Construct(*self); self->node->type = TypeUInteger; self->node->uinteger = value; }
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value* self, double value) { Construct(*self); self->node->type = TypeReal; self->node->real = value; }
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value* self, const char* text) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::Value: null text");
    Construct(*self, [text](Node& node) {
        SetType(node, TypeString);
        *node.string.text = text;
    });
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6StringE(Value* self, const String* text) {
    Construct(*self, [text](Node& node) {
        SetType(node, TypeString);
        *node.string.text = *text->text;
    });
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(Value* self, const Array* array) {
    Construct(*self, [array](Node& node) {
        node.type = TypeArray;
        CopyArray(node.array, *array);
    });
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(Value* self, const Object* object) {
    Construct(*self, [object](Node& node) {
        node.type = TypeObject;
        CopyObject(node.object, *object);
    });
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(Value* self, const Value* other) { CopyInto(*self, *other); }
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value* self) { Destroy(*self); }
Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1_(Value* self, const Value* other) {
    if (self != other) AssignNode(NodeOf(*self), NodeOf(*other));
    return self;
}
void APS5_VABI _ZN3sce4Json5Value4swapERS1_(Value* self, Value* other) {
    Node temporary{};
    try {
        AssignNode(temporary, NodeOf(*self));
        AssignNode(NodeOf(*self), NodeOf(*other));
        AssignNode(NodeOf(*other), temporary);
    } catch (...) {
        Clear(temporary);
        throw;
    }
    Clear(temporary);
}

int APS5_VABI _ZN3sce4Json5Value3setENS0_9ValueTypeE(Value* self, ValueType type) { SetType(NodeOf(*self), type); return 0; }
int APS5_VABI _ZN3sce4Json5Value3setEb(Value* self, bool value) { SetType(NodeOf(*self), TypeBoolean); self->node->boolean = value; return 0; }
int APS5_VABI _ZN3sce4Json5Value3setEl(Value* self, std::int64_t value) { SetType(NodeOf(*self), TypeInteger); self->node->integer = value; return 0; }
int APS5_VABI _ZN3sce4Json5Value3setEm(Value* self, std::uint64_t value) { SetType(NodeOf(*self), TypeUInteger); self->node->uinteger = value; return 0; }
int APS5_VABI _ZN3sce4Json5Value3setEd(Value* self, double value) { SetType(NodeOf(*self), TypeReal); self->node->real = value; return 0; }
int APS5_VABI _ZN3sce4Json5Value3setEPKc(Value* self, const char* text) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::Value::set: null text");
    const std::string copy(text);
    SetType(NodeOf(*self), TypeString);
    *self->node->string.text = copy;
    return 0;
}
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_6StringE(Value* self, const String* text) {
    const std::string copy(*text->text);
    SetType(NodeOf(*self), TypeString);
    *self->node->string.text = copy;
    return 0;
}
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_5ArrayE(Value* self, const Array* array) {
    Array copy{};
    CopyArray(copy, *array);
    Clear(NodeOf(*self));
    self->node->type = TypeArray;
    self->node->array = copy;
    return 0;
}
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_6ObjectE(Value* self, const Object* object) {
    Object copy{};
    CopyObject(copy, *object);
    Clear(NodeOf(*self));
    self->node->type = TypeObject;
    self->node->object = copy;
    return 0;
}
int APS5_VABI _ZN3sce4Json5Value3setERKS1_(Value* self, const Value* other) {
    if (self != other) AssignNode(NodeOf(*self), NodeOf(*other));
    return 0;
}

ValueType APS5_VABI _ZNK3sce4Json5Value7getTypeEv(const Value* self) { return NodeOf(*self).type; }
const bool* APS5_VABI _ZNK3sce4Json5Value10getBooleanEv(const Value* self) { return &Typed(self, TypeBoolean).boolean; }
const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv(const Value* self) {
    Node& n = const_cast<Node&>(NodeOf(*self));
    if (n.type == TypeUInteger) {
        n.integer = std::bit_cast<std::int64_t>(n.uinteger);
        return &n.integer;
    }
    return &Typed(self, TypeInteger).integer;
}
const std::uint64_t* APS5_VABI _ZNK3sce4Json5Value11getUIntegerEv(const Value* self) {
    Node& n = const_cast<Node&>(NodeOf(*self));
    if (n.type == TypeInteger) {
        n.uinteger = static_cast<std::uint64_t>(n.integer);
        return &n.uinteger;
    }
    return &Typed(self, TypeUInteger).uinteger;
}
const double* APS5_VABI _ZNK3sce4Json5Value7getRealEv(const Value* self) {
    Node& n = const_cast<Node&>(NodeOf(*self));
    if (n.type == TypeInteger || n.type == TypeUInteger) {
        n.real = n.type == TypeInteger ? static_cast<double>(n.integer) : static_cast<double>(n.uinteger);
        return &n.real;
    }
    return &Typed(self, TypeReal).real;
}
const String* APS5_VABI _ZNK3sce4Json5Value9getStringEv(const Value* self) { return &Typed(self, TypeString).string; }
const Array* APS5_VABI _ZNK3sce4Json5Value8getArrayEv(const Value* self) { return &Typed(self, TypeArray).array; }
const Object* APS5_VABI _ZNK3sce4Json5Value9getObjectEv(const Value* self) { return &Typed(self, TypeObject).object; }
std::size_t APS5_VABI _ZNK3sce4Json5Value5countEv(const Value* self) {
    const Node& n = NodeOf(*self);
    if (n.type == TypeArray) return n.array.items->size();
    if (n.type == TypeObject) return n.object.items->size();
    return 0;
}
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value* self, const char* key) {
    const Node& n = NodeOf(*self);
    if (key != nullptr && n.type == TypeObject)
        for (const auto& pair : *n.object.items)
            if (std::strcmp(pair.key.text->c_str(), key) == 0) return &pair.value;
    return &MissingElement(self);
}
const Value* APS5_VABI _ZNK3sce4Json5ValueixEm(const Value* self, std::size_t index) {
    const Node& n = NodeOf(*self);
    if (n.type == TypeArray && index < n.array.items->size()) {
        auto it = n.array.items->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(index));
        return &*it;
    }
    return &MissingElement(self);
}
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(Value* self, ValueType type) {
    Construct(*self, [type](Node& node) { SetType(node, type); });
}
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueEm(const Value* self, std::size_t index) { return _ZNK3sce4Json5ValueixEm(self, index); }
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKNS0_6StringE(const Value* self, const String* key) {
    const Node& n = NodeOf(*self);
    if (key != nullptr && n.type == TypeObject)
        for (const auto& pair : *n.object.items)
            if (std::strcmp(pair.key.text->c_str(), key->text->c_str()) == 0) return &pair.value;
    return &MissingElement(self);
}
Value* APS5_VABI _ZN3sce4Json5Value10referValueEm(Value* self, std::size_t index) {
    Node& n = NodeOf(*self);
    if (n.type != TypeArray || index >= n.array.items->size()) return nullptr;
    auto it = n.array.items->begin();
    std::advance(it, static_cast<std::ptrdiff_t>(index));
    return &*it;
}
Value* APS5_VABI _ZN3sce4Json5Value10referValueERKNS0_6StringE(Value* self, const String* key) {
    Node& n = NodeOf(*self);
    if (key == nullptr || n.type != TypeObject) return nullptr;
    for (auto& pair : *n.object.items)
        if (std::strcmp(pair.key.text->c_str(), key->text->c_str()) == 0) return &pair.value;
    return nullptr;
}
Array* APS5_VABI _ZN3sce4Json5Value10referArrayEv(Value* self) {
    Node& n = NodeOf(*self);
    if (n.type == TypeNull) SetType(n, TypeArray);
    if (n.type != TypeArray) throw std::runtime_error("sce::Json::Value::referArray: value is not an array");
    return &n.array;
}
Object* APS5_VABI _ZN3sce4Json5Value11referObjectEv(Value* self) {
    Node& n = NodeOf(*self);
    if (n.type == TypeNull) SetType(n, TypeObject);
    if (n.type != TypeObject) throw std::runtime_error("sce::Json::Value::referObject: value is not an object");
    return &n.object;
}
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value* self, String* out) {
    std::string text;
    Serialize(text, NodeOf(*self));
    *out->text = std::move(text);
    return 0;
}
int APS5_VABI _ZNK3sce4Json5Value8toStringERNS0_6StringE(const Value* self, String* out) {
    const Node& n = NodeOf(*self);
    if (n.type == TypeString) {
        *out->text = *n.string.text;
        return 0;
    }
    std::string text;
    Serialize(text, n);
    *out->text = std::move(text);
    return 0;
}


int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(Value* out, ParserReadCallback read, void* context) {
    if (read == nullptr) throw std::invalid_argument("sce::Json::Parser::parse: null read callback");
    Parser parser(read, context, NodeOf(*out).handlers);
    return ParseValue(out, parser);
}

int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value* out, const char* text, std::size_t size) {
    if (size == 0) return JsonErrorParse;
    if (text == nullptr) throw std::invalid_argument("sce::Json::Parser::parse: null text");
    Parser parser(text, size, NodeOf(*out).handlers);
    return ParseValue(out, parser);
}


struct InitParameterRtti {
    void* allocParam;
    void* userData;
    std::size_t fileBufferSize;
};

void APS5_VABI _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(InitParameterRtti* self, void* allocParam, void* userData, std::size_t fileBufferSize) {
    self->allocParam = allocParam;
    self->userData = userData;
    self->fileBufferSize = fileBufferSize;
}

struct InitParameter2 {
    void* allocator;
    void* userData;
    std::size_t fileBufferSize;
    std::int32_t specialFloatFormatType;
    std::uint32_t reserved[3];
};
static_assert(sizeof(InitParameter2) == 0x28 && offsetof(InitParameter2, specialFloatFormatType) == 0x18);

void APS5_VABI _ZN3sce4Json14InitParameter2C1Ev(InitParameter2* self) {
    *self = {};
    self->specialFloatFormatType = 2;
}

void APS5_VABI _ZN3sce4Json14InitParameter2C2Ev(InitParameter2* self) {
    _ZN3sce4Json14InitParameter2C1Ev(self);
}

void APS5_VABI _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(InitParameter2* self, void* allocator, void* userData) {
    self->allocator = allocator;
    self->userData = userData;
}

void APS5_VABI _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(InitParameter2* self, void* allocParam, void* userData) {
    self->allocator = allocParam;
    self->userData = userData;
}

void APS5_VABI _ZN3sce4Json14InitParameter217setFileBufferSizeEm(InitParameter2* self, std::size_t size) {
    self->fileBufferSize = size;
}

void APS5_VABI _ZN3sce4Json14InitParameter225setSpecialFloatFormatTypeENS0_22SpecialFloatFormatTypeE(InitParameter2* self, std::int32_t type) {
    self->specialFloatFormatType = type;
}

int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void* self, const InitParameter2* parameter) {
    (void)self;
    if (parameter == nullptr) throw std::invalid_argument("sce::Json::Initializer::initialize: null parameter");
    return 0;
}

void APS5_VABI _ZN3sce4Json5Value5clearEv(Value* self) {
    if (self == nullptr) return;
    Clear(NodeOf(*self));
}

void APS5_VABI _ZN3sce4Json12MemAllocator11notifyErrorEimPv(void* self, int error, std::size_t size, void* userData) {
    (void)self;
    (void)error;
    (void)size;
    (void)userData;
}

void APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(NullAccessCallback callback, void* context) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.callback = callback;
    state.context = context;
}

int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(Value* self, ElementAccessFailureHandler handler, void* context) {
    SetHandlers(NodeOf(*self), [&](Handlers& handlers) {
        handlers.elementAccessFailure = handler;
        handlers.elementAccessFailureContext = context;
    });
    return 0;
}

int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value* self, SpecialFloatHandler handler, void* context) {
    SetHandlers(NodeOf(*self), [&](Handlers& handlers) {
        handlers.specialFloat = handler;
        handlers.specialFloatContext = context;
    });
    return 0;
}

int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value* self, TypeMismatchHandler handler, void* context) {
    SetHandlers(NodeOf(*self), [&](Handlers& handlers) {
        handlers.typeMismatch = handler;
        handlers.typeMismatchContext = context;
    });
    return 0;
}

bool APS5_VABI _ZNK3sce4Json5ValuecvbEv(const Value* self) {
    const Node& n = NodeOf(*self);
    switch (n.type) {
        case TypeNull: return false;
        case TypeBoolean: return n.boolean;
        case TypeInteger: return n.integer != 0;
        case TypeUInteger: return n.uinteger != 0;
        case TypeReal: return n.real != 0.0;
        case TypeString: return n.string.text && !n.string.text->empty();
        case TypeArray:
        case TypeObject: return true;
        default: throw std::runtime_error("sce::Json::Value: invalid value type");
    }
}

void APS5_VABI _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(Pair* self, const String* key, const Value* value) {
    Pair pair{};
    pair.reserved = 0;
    Construct(pair.key, *key->text);
    try {
        CopyInto(pair.value, *value);
    } catch (...) {
        Destroy(pair.key);
        throw;
    }
    *self = pair;
}

void APS5_VABI _ZN3sce4Json6Object4PairD1Ev(Pair* self) {
    Destroy(self->key);
    Destroy(self->value);
}

ObjectIterator* APS5_VABI _ZN3sce4Json6Object6insertERKNS1_4PairE(ObjectIterator* result, Object* self, const Pair* pair) {
    auto it = ObjectPosition(*self, *pair->key.text);
    auto state = std::make_unique<ObjectIteratorState>(ObjectIteratorState{self->items, it});
    if (it != self->items->end() && std::strcmp(it->key.text->c_str(), pair->key.text->c_str()) == 0) {
        result->state = state.release();
        return result;
    }
    Pair copy{};
    copy.reserved = 0;
    Construct(copy.key, *pair->key.text);
    try {
        CopyInto(copy.value, pair->value);
        state->position = self->items->insert(it, copy);
        result->state = state.release();
        return result;
    } catch (...) {
        Destroy(copy.value);
        Destroy(copy.key);
        throw;
    }
}

ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEi(ArrayIterator* result, ArrayIterator* self, int) {
    _ZN3sce4Json5Array8iteratorC1ERKS2_(result, self);
    _ZN3sce4Json5Array8iteratorppEv(self);
    return result;
}

ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratoraSERKS2_(ArrayIterator* self, const ArrayIterator* other) {
    if (self->state) {
        if (other->state && other->state->position != other->state->end) {
            *self->state = *other->state;
        } else {
            delete self->state;
            self->state = nullptr;
        }
    }
    return self;
}

void APS5_VABI _ZN3sce4Json5Array8iteratorC1Ev(ArrayIterator* self) {
    self->state = new ArrayIteratorState{};
}

ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEi(ObjectIterator* result, ObjectIterator* self, int) {
    auto state = self->state ? std::make_unique<ObjectIteratorState>(*self->state) : nullptr;
    _ZN3sce4Json6Object8iteratorppEv(self);
    result->state = state.release();
    return result;
}

ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratoraSERKS2_(ObjectIterator* self, const ObjectIterator* other) {
    if (self->state) {
        if (other->state && other->state->owner) {
            *self->state = *other->state;
        } else {
            delete self->state;
            self->state = nullptr;
        }
    }
    return self;
}

void APS5_VABI _ZN3sce4Json6Object8iteratorC1Ev(ObjectIterator* self) {
    self->state = new ObjectIteratorState{};
}

void APS5_VABI _ZN3sce4Json6Object4PairC1Ev(Pair* self) {
    Pair pair{};
    pair.reserved = 0;
    Construct(pair.key, std::string());
    try {
        Construct(pair.value);
    } catch (...) {
        Destroy(pair.key);
        throw;
    }
    *self = pair;
}
}
