#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <list>
#include <mutex>
#include <stdexcept>
#include <string>
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

constexpr int JsonErrorParse = static_cast<int>(0x80920101);

struct Node;
struct Value { Node* node; };
struct String { std::string* text; };
struct Array { std::list<Value>* items; };
struct Pair;
struct Object { std::list<Pair>* items; };
struct ArrayIterator { std::list<Value>::iterator it; };
struct ObjectIterator { std::list<Pair>::iterator it; };
struct Pair {
    String key;
    std::uint64_t reserved;
    Value value;
};
static_assert(sizeof(Value) == 8 && sizeof(String) == 8 && sizeof(Array) == 8 && sizeof(Object) == 8);
static_assert(sizeof(ArrayIterator) == 8 && sizeof(ObjectIterator) == 8);
static_assert(offsetof(Pair, value) == 0x10);

struct Node {
    ValueType type = TypeNull;
    bool boolean = false;
    std::int64_t integer = 0;
    std::uint64_t uinteger = 0;
    double real = 0;
    String string{};
    Array array{};
    Object object{};
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
    n = Node{};
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

void CopyArray(Array& destination, const Array& source) {
    Construct(destination);
    for (const auto& item : *source.items) {
        Value copy{};
        CopyInto(copy, item);
        destination.items->push_back(copy);
    }
}

void CopyObject(Object& destination, const Object& source) {
    Construct(destination);
    for (const auto& pair : *source.items) {
        Pair copy{};
        Construct(copy.key, *pair.key.text);
        CopyInto(copy.value, pair.value);
        destination.items->push_back(copy);
    }
}

void AssignNode(Node& destination, const Node& source) {
    Node copy{};
    copy.type = source.type;
    copy.boolean = source.boolean;
    copy.integer = source.integer;
    copy.uinteger = source.uinteger;
    copy.real = source.real;
    if (source.type == TypeString) Construct(copy.string, *source.string.text);
    if (source.type == TypeArray) CopyArray(copy.array, source.array);
    if (source.type == TypeObject) CopyObject(copy.object, source.object);
    Clear(destination);
    destination = copy;
}

void CopyInto(Value& destination, const Value& source) {
    Construct(destination);
    AssignNode(*destination.node, NodeOf(source));
}

void SetType(Node& n, ValueType type) {
    Clear(n);
    n.type = type;
    if (type == TypeString) Construct(n.string, std::string());
    if (type == TypeArray) Construct(n.array);
    if (type == TypeObject) Construct(n.object);
    if (type < TypeNull || type > TypeObject) throw std::invalid_argument("sce::Json::Value::set: invalid value type");
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

const Node& Typed(const Value* self, ValueType requested) {
    const Node& n = NodeOf(*self);
    if (n.type == requested) return n;
    return NodeOf(NullAccess(requested, self));
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
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    return buffer;
}

void Serialize(std::string& out, const Node& n) {
    switch (n.type) {
        case TypeNull: out += "null"; return;
        case TypeBoolean: out += n.boolean ? "true" : "false"; return;
        case TypeInteger: out += std::to_string(n.integer); return;
        case TypeUInteger: out += std::to_string(n.uinteger); return;
        case TypeReal: out += RealText(n.real); return;
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

class Parser {
public:
    Parser(const char* text, std::size_t size) : _text(text), _end(text + size) {}

    bool Parse(Node& out) {
        skip();
        if (!value(out)) return false;
        skip();
        return _text == _end || *_text == '\0';
    }

private:
    static constexpr std::size_t MaxNestingDepth = 512;

    const char* _text;
    const char* _end;
    std::size_t _depth = 0;

    void skip() { while (_text < _end && (*_text == ' ' || *_text == '\t' || *_text == '\n' || *_text == '\r')) ++_text; }
    bool literal(const char* word) {
        const auto length = std::strlen(word);
        if (static_cast<std::size_t>(_end - _text) < length || std::memcmp(_text, word, length) != 0) return false;
        _text += length;
        return true;
    }
    static void appendUtf8(std::string& out, std::uint32_t code) {
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) { out += static_cast<char>(0xc0 | (code >> 6)); out += static_cast<char>(0x80 | (code & 0x3f)); }
        else if (code < 0x10000) { out += static_cast<char>(0xe0 | (code >> 12)); out += static_cast<char>(0x80 | ((code >> 6) & 0x3f)); out += static_cast<char>(0x80 | (code & 0x3f)); }
        else { out += static_cast<char>(0xf0 | (code >> 18)); out += static_cast<char>(0x80 | ((code >> 12) & 0x3f)); out += static_cast<char>(0x80 | ((code >> 6) & 0x3f)); out += static_cast<char>(0x80 | (code & 0x3f)); }
    }
    bool hex4(std::uint32_t& code) {
        if (_end - _text < 4) return false;
        code = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = *_text++;
            code <<= 4;
            if (c >= '0' && c <= '9') code |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') code |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') code |= static_cast<std::uint32_t>(c - 'A' + 10);
            else return false;
        }
        return true;
    }
    bool string(std::string& out) {
        if (_text >= _end || *_text != '"') return false;
        ++_text;
        while (_text < _end && *_text != '"') {
            const char c = *_text++;
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c != '\\') { out += c; continue; }
            if (_text >= _end) return false;
            switch (*_text++) {
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
        if (_text >= _end) return false;
        ++_text;
        return true;
    }
    bool number(Node& out) {
        const char* start = _text;
        if (_text < _end && *_text == '-') ++_text;
        if (_text >= _end || *_text < '0' || *_text > '9') return false;
        if (*_text == '0') ++_text;
        else while (_text < _end && *_text >= '0' && *_text <= '9') ++_text;
        bool integral = true;
        if (_text < _end && *_text == '.') {
            integral = false;
            ++_text;
            if (_text >= _end || *_text < '0' || *_text > '9') return false;
            while (_text < _end && *_text >= '0' && *_text <= '9') ++_text;
        }
        if (_text < _end && (*_text == 'e' || *_text == 'E')) {
            integral = false;
            ++_text;
            if (_text < _end && (*_text == '+' || *_text == '-')) ++_text;
            if (_text >= _end || *_text < '0' || *_text > '9') return false;
            while (_text < _end && *_text >= '0' && *_text <= '9') ++_text;
        }
        const std::string text(start, _text);
        if (integral) {
            errno = 0;
            if (text[0] == '-') {
                const long long value = std::strtoll(text.c_str(), nullptr, 10);
                if (errno == 0) { out.type = TypeInteger; out.integer = value; return true; }
            } else {
                const unsigned long long value = std::strtoull(text.c_str(), nullptr, 10);
                if (errno == 0) {
                    if (value <= static_cast<unsigned long long>(INT64_MAX)) { out.type = TypeInteger; out.integer = static_cast<std::int64_t>(value); }
                    else { out.type = TypeUInteger; out.uinteger = value; }
                    return true;
                }
            }
        }
        const double real = std::strtod(text.c_str(), nullptr);
        if (!std::isfinite(real)) throw std::runtime_error("sce::Json::Parser::parse: number " + text + " overflows a double; the console's behaviour is unverified");
        out.type = TypeReal;
        out.real = real;
        return true;
    }
    bool value(Node& out) {
        if (_text >= _end) return false;
        switch (*_text) {
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
                if (_depth == MaxNestingDepth) return false;
                ++_text;
                ++_depth;
                SetType(out, TypeArray);
                skip();
                if (_text < _end && *_text == ']') { ++_text; --_depth; return true; }
                for (;;) {
                    Value item{};
                    Construct(item);
                    out.array.items->push_back(item);
                    skip();
                    if (!value(*item.node)) return false;
                    skip();
                    if (_text < _end && *_text == ',') { ++_text; continue; }
                    if (_text < _end && *_text == ']') { ++_text; --_depth; return true; }
                    return false;
                }
            }
            case '{': {
                if (_depth == MaxNestingDepth) return false;
                ++_text;
                ++_depth;
                SetType(out, TypeObject);
                skip();
                if (_text < _end && *_text == '}') { ++_text; --_depth; return true; }
                for (;;) {
                    skip();
                    std::string key;
                    if (!string(key)) return false;
                    skip();
                    if (_text >= _end || *_text != ':') return false;
                    ++_text;
                    skip();
                    Pair pair{};
                    Construct(pair.key, key);
                    Construct(pair.value);
                    out.object.items->push_back(pair);
                    if (!value(*pair.value.node)) return false;
                    skip();
                    if (_text < _end && *_text == ',') { ++_text; continue; }
                    if (_text < _end && *_text == '}') { ++_text; --_depth; return true; }
                    return false;
                }
            }
            default:
                return number(out);
        }
    }
};

[[noreturn]] void APS5_VABI JsonMemAllocatorPureVirtual() {
    throw std::runtime_error("sce::Json::MemAllocator: pure virtual function called");
}

void* const JsonMemAllocatorVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual), reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual), reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
    reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual), reinterpret_cast<void*>(&JsonMemAllocatorPureVirtual),
};

Value& ObjectEntry(Object& object, const std::string& key) {
    for (auto& pair : *object.items)
        if (*pair.key.text == key) return pair.value;
    Pair pair{};
    Construct(pair.key, key);
    Construct(pair.value);
    object.items->push_back(pair);
    return object.items->back().value;
}

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
bool APS5_VABI _ZNK3sce4Json6StringeqEPKc(const String* self, const char* text) { return text != nullptr && *self->text == text; }

void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array* self) { Construct(*self); }
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array* self) { Destroy(*self); }
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array* self, const Value* value) {
    Value copy{};
    CopyInto(copy, *value);
    self->items->push_back(copy);
}
ArrayIterator* APS5_VABI _ZNK3sce4Json5Array5beginEv(ArrayIterator* result, const Array* self) { result->it = self->items->begin(); return result; }
ArrayIterator* APS5_VABI _ZNK3sce4Json5Array3endEv(ArrayIterator* result, const Array* self) { result->it = self->items->end(); return result; }
bool APS5_VABI _ZNK3sce4Json5Array5emptyEv(const Array* self) { return self->items->empty(); }
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array* self) { return self->items->size(); }
const Value* APS5_VABI _ZNK3sce4Json5Array4backEv(const Array* self) {
    if (self->items->empty()) throw std::out_of_range("sce::Json::Array::back: empty array");
    return &self->items->back();
}
void APS5_VABI _ZN3sce4Json5Array8iteratorD1Ev(ArrayIterator* self) { (void)self; }
ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEv(ArrayIterator* self) { ++self->it; return self; }
Value* APS5_VABI _ZNK3sce4Json5Array8iteratordeEv(const ArrayIterator* self) { return &*self->it; }
bool APS5_VABI _ZNK3sce4Json5Array8iteratorneERKS2_(const ArrayIterator* self, const ArrayIterator* other) { return self->it != other->it; }

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
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object5beginEv(ObjectIterator* result, const Object* self) { result->it = self->items->begin(); return result; }
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object3endEv(ObjectIterator* result, const Object* self) { result->it = self->items->end(); return result; }
void APS5_VABI _ZN3sce4Json6Object8iteratorD1Ev(ObjectIterator* self) { (void)self; }
ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEv(ObjectIterator* self) { ++self->it; return self; }
Pair* APS5_VABI _ZNK3sce4Json6Object8iteratordeEv(const ObjectIterator* self) { return &*self->it; }
bool APS5_VABI _ZNK3sce4Json6Object8iteratorneERKS2_(const ObjectIterator* self, const ObjectIterator* other) { return self->it != other->it; }
bool APS5_VABI _ZNK3sce4Json6Object5emptyEv(const Object* self) { return self->items->empty(); }
std::size_t APS5_VABI _ZNK3sce4Json6Object4sizeEv(const Object* self) { return self->items->size(); }

void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value* self) { Construct(*self); }
void APS5_VABI _ZN3sce4Json5ValueC1Eb(Value* self, bool value) { Construct(*self); self->node->type = TypeBoolean; self->node->boolean = value; }
void APS5_VABI _ZN3sce4Json5ValueC1El(Value* self, std::int64_t value) { Construct(*self); self->node->type = TypeInteger; self->node->integer = value; }
void APS5_VABI _ZN3sce4Json5ValueC1Em(Value* self, std::uint64_t value) { Construct(*self); self->node->type = TypeUInteger; self->node->uinteger = value; }
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value* self, double value) { Construct(*self); self->node->type = TypeReal; self->node->real = value; }
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value* self, const char* text) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::Value: null text");
    Construct(*self);
    SetType(*self->node, TypeString);
    *self->node->string.text = text;
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6StringE(Value* self, const String* text) {
    Construct(*self);
    SetType(*self->node, TypeString);
    *self->node->string.text = *text->text;
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(Value* self, const Array* array) {
    Construct(*self);
    self->node->type = TypeArray;
    CopyArray(self->node->array, *array);
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(Value* self, const Object* object) {
    Construct(*self);
    self->node->type = TypeObject;
    CopyObject(self->node->object, *object);
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(Value* self, const Value* other) { CopyInto(*self, *other); }
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value* self) { Destroy(*self); }
Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1_(Value* self, const Value* other) {
    if (self != other) AssignNode(NodeOf(*self), NodeOf(*other));
    return self;
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
    if (n.type == TypeUInteger && n.uinteger <= static_cast<std::uint64_t>(INT64_MAX)) {
        n.integer = static_cast<std::int64_t>(n.uinteger);
        return &n.integer;
    }
    return &Typed(self, TypeInteger).integer;
}
const std::uint64_t* APS5_VABI _ZNK3sce4Json5Value11getUIntegerEv(const Value* self) {
    Node& n = const_cast<Node&>(NodeOf(*self));
    if (n.type == TypeInteger && n.integer >= 0) {
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
            if (*pair.key.text == key) return &pair.value;
    return &NullAccess(TypeNull, self);
}
const Value* APS5_VABI _ZNK3sce4Json5ValueixEm(const Value* self, std::size_t index) {
    const Node& n = NodeOf(*self);
    if (n.type == TypeArray && index < n.array.items->size()) {
        auto it = n.array.items->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(index));
        return &*it;
    }
    return &NullAccess(TypeNull, self);
}
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(Value* self, ValueType type) {
    Construct(*self);
    SetType(*self->node, type);
}
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueEm(const Value* self, std::size_t index) { return _ZNK3sce4Json5ValueixEm(self, index); }
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKNS0_6StringE(const Value* self, const String* key) {
    const Node& n = NodeOf(*self);
    if (key != nullptr && n.type == TypeObject)
        for (const auto& pair : *n.object.items)
            if (*pair.key.text == *key->text) return &pair.value;
    return &NullAccess(TypeNull, self);
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
        if (*pair.key.text == *key->text) return &pair.value;
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

int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value* out, const char* text, std::size_t size) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::Parser::parse: null text");
    Node parsed{};
    Parser parser(text, size);
    bool parsedOk = false;
    try {
        parsedOk = parser.Parse(parsed);
    } catch (...) {
        Clear(parsed);
        throw;
    }
    if (!parsedOk) {
        Clear(parsed);
        return JsonErrorParse;
    }
    Clear(NodeOf(*out));
    *out->node = parsed;
    return 0;
}


int APS5_VABI _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
struct InitParameter2 {
    void* allocator;
    void* userData;
    std::size_t fileBufferSize;
};
static_assert(sizeof(InitParameter2) <= 40);

void APS5_VABI _ZN3sce4Json14InitParameter2C1Ev(InitParameter2* self) {
    *self = {};
}

void APS5_VABI _ZN3sce4Json14InitParameter212setAllocatorEPNS0_12MemAllocatorEPv(InitParameter2* self, void* allocator, void* userData) {
    self->allocator = allocator;
    self->userData = userData;
}

void APS5_VABI _ZN3sce4Json14InitParameter217setFileBufferSizeEm(InitParameter2* self, std::size_t size) {
    self->fileBufferSize = size;
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

int APS5_VABI _ZN3sce4Json12MemAllocator11notifyErrorEimPv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("Eu95jmqn5Rw", sceJson2Unknown01);
int APS5_VABI sceJson2Unknown01(void) {
    NotImplemented_nid_no_patch("Eu95jmqn5Rw");
    return 0;
}

APS5_EXPORT("GvGvswb0v34", sceJson2Unknown02);
int APS5_VABI sceJson2Unknown02(void) {
    NotImplemented_nid_no_patch("GvGvswb0v34");
    return 0;
}

APS5_EXPORT("IXW-z8pggfg", sceJson2Unknown03);
int APS5_VABI sceJson2Unknown03(void) {
    NotImplemented_nid_no_patch("IXW-z8pggfg");
    return 0;
}

APS5_EXPORT("W72B9ylU2JA", sceJson2Unknown04);
int APS5_VABI sceJson2Unknown04(void) {
    NotImplemented_nid_no_patch("W72B9ylU2JA");
    return 0;
}
}
