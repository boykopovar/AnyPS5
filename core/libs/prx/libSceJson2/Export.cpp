#include <algorithm>
#include <bit>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <locale>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/File/include/File.hpp"

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
constexpr int JsonErrorOutOfMemory = static_cast<int>(0x80848102);
constexpr int JsonErrorNotInitialized = static_cast<int>(0x80848110);
constexpr int JsonErrorAlreadyInitialized = static_cast<int>(0x80848111);
constexpr int JsonErrorSpecialFloat = static_cast<int>(0x80848114);
constexpr int JsonErrorInvalidArgument = static_cast<int>(0x80848120);

struct Value;
struct Handlers;
struct Auxiliary;
struct Text { char* characters; std::uint32_t length; std::uint32_t capacity; std::uint64_t reserved; };
static_assert(sizeof(Text) == 24);
struct String { Text* text; };
struct ArrayStorage;
struct ArrayLink;
struct Array { ArrayStorage* items; };
struct Pair;
struct ObjectStorage;
struct ObjectLink;
struct Object { ObjectStorage* items; };
struct ArrayIteratorState {
    ArrayLink* position;
};
struct ArrayIterator { ArrayIteratorState* state; };
struct ObjectIteratorState {
    ObjectLink* position;
};
struct ObjectIterator { ObjectIteratorState* state; };
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

struct Value {
    Value* parent = nullptr;
    Auxiliary* auxiliary = nullptr;
    union {
        std::uint64_t payload = 0;
        bool boolean;
        std::int64_t integer;
        std::uint64_t uinteger;
        double real;
        String* string;
        Array* array;
        Object* object;
    };
    std::uint32_t reserved = 0;
    ValueType type = TypeNull;
};
using Node = Value;
static_assert(offsetof(Value, parent) == 0 && offsetof(Value, auxiliary) == 8);
static_assert(offsetof(Value, payload) == 16 && offsetof(Value, type) == 28);

struct Pair {
    String key;
    std::uint64_t reserved;
    Value value;
};
static_assert(sizeof(Value) == 32 && sizeof(String) == 8 && sizeof(Array) == 8 && sizeof(Object) == 8);
static_assert(sizeof(ArrayIterator) == 8 && sizeof(ObjectIterator) == 8);
static_assert(offsetof(Pair, value) == 0x10);


using NullAccessCallback = const Value& (APS5_VABI*)(ValueType, const Value*, void*);
template<typename TCallback>
struct HandlerRecord { TCallback callback = nullptr; void* context = nullptr; };

struct Auxiliary {
    NullAccessCallback nullAccess = nullptr;
    void* nullAccessContext = nullptr;
    Value* fallback = nullptr;
    HandlerRecord<TypeMismatchHandler>* typeMismatch = nullptr;
    HandlerRecord<SpecialFloatHandler>* specialFloat = nullptr;
    HandlerRecord<ElementAccessFailureHandler>* elementAccessFailure = nullptr;
    std::uint64_t reserved[3]{};
};
static_assert(sizeof(Auxiliary) == 72 && offsetof(Auxiliary, fallback) == 16);
static_assert(offsetof(Auxiliary, typeMismatch) == 24 && offsetof(Auxiliary, specialFloat) == 32);
static_assert(offsetof(Auxiliary, elementAccessFailure) == 40);

using RttiAllocate = void* (APS5_VABI*)(std::size_t, void*);
using RttiFree = void (APS5_VABI*)(void*, void*);
using RttiNotify = void (APS5_VABI*)(int, std::size_t, void*);

struct AllocParamRtti {
    RttiAllocate allocate;
    RttiFree free;
    RttiNotify notify;
};

struct AllocationContext {
    void* allocator = nullptr;
    void* userData = nullptr;
    std::size_t fileBufferSize = 0;
};
static_assert(sizeof(AllocationContext) == 24);

struct Globals {
    std::recursive_mutex mutex;
    NullAccessCallback callback = nullptr;
    void* context = nullptr;
    std::int32_t floatFormat = 2;
    bool floatFormatEnabled = false;
    bool initialized = false;
    AllocParamRtti rtti{};
    Handlers handlers{};
    AllocationContext allocation{};
    std::recursive_mutex callbackMutex;
    bool callbackActive = false;
    std::uint64_t allocationFailures = 0;
};

Globals& State() {
    static Globals globals;
    return globals;
}

struct CallbackScope {
    Globals& state = State();
    std::unique_lock<std::recursive_mutex> lock{state.callbackMutex};
    bool entered = !state.callbackActive;
    CallbackScope() { if (entered) state.callbackActive = true; }
    ~CallbackScope() { if (entered) state.callbackActive = false; }
};

AllocParamRtti RttiCallbacks() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return state.rtti;
}

void* Allocate(const AllocationContext& context, std::size_t size) {
    auto* const* table = *static_cast<void* const* const*>(context.allocator);
    const auto allocate = reinterpret_cast<void* (APS5_VABI*)(void*, std::size_t, void*)>(table[2]);
    void* memory = allocate(context.allocator, size, context.userData);
    if (!memory) {
        const auto notify = reinterpret_cast<void (APS5_VABI*)(void*, int, std::size_t, void*)>(table[4]);
        notify(context.allocator, JsonErrorOutOfMemory, size, context.userData);
        auto& state = State();
        std::lock_guard lock(state.mutex);
        ++state.allocationFailures;
    }
    return memory;
}

void Release(const AllocationContext& context, void* memory) {
    auto* const* table = *static_cast<void* const* const*>(context.allocator);
    const auto free = reinterpret_cast<void (APS5_VABI*)(void*, void*, void*)>(table[3]);
    free(context.allocator, memory, context.userData);
}

struct FileReader {
    AllocationContext allocation;
    void* buffer = nullptr;
    int descriptor = -1;
    std::size_t available = 0;
    std::size_t position = 0;

    ~FileReader() {
        if (descriptor >= 0) sceKernelClose(descriptor);
        Release(allocation, buffer);
    }

    static int APS5_VABI Read(char& character, void* context) {
        auto& reader = *static_cast<FileReader*>(context);
        if (reader.descriptor < 0) {
            if (reader.descriptor == -1) return JsonErrorNotInitialized;
            if (reader.descriptor == static_cast<int>(0x80020002)) return static_cast<int>(0x80848103);
            return reader.descriptor;
        }
        if (reader.position == reader.available) {
            const auto count = sceKernelRead(reader.descriptor, reader.buffer, reader.allocation.fileBufferSize);
            if (count < 0) return static_cast<int>(count);
            if (!count) return JsonErrorParse;
            reader.available = static_cast<std::size_t>(count);
            reader.position = 0;
        }
        character = static_cast<const char*>(reader.buffer)[reader.position++];
        return 0;
    }
};

void APS5_VABI RttiDestroy(void*) {}

void APS5_VABI RttiDelete(void* self) { ::operator delete(self); }

void* APS5_VABI RttiAllocateMemory(void*, std::size_t size, void* context) {
    return RttiCallbacks().allocate(size, context);
}

void APS5_VABI RttiFreeMemory(void*, void* memory, void* context) {
    RttiCallbacks().free(memory, context);
}

void APS5_VABI RttiNotifyError(void*, int error, std::size_t size, void* context) {
    const auto callback = RttiCallbacks().notify;
    if (callback) callback(error, size, context);
    else if (error == JsonErrorOutOfMemory)
        std::printf("[libSceJson] Allocation Fail. size = %zu\n", size);
    else std::printf("[libSceJson] unknown notification[%#x]\n", static_cast<unsigned>(error));
}

void* const RttiAllocatorVtable[] = {
    reinterpret_cast<void*>(&RttiDestroy), reinterpret_cast<void*>(&RttiDelete),
    reinterpret_cast<void*>(&RttiAllocateMemory), reinterpret_cast<void*>(&RttiFreeMemory),
    reinterpret_cast<void*>(&RttiNotifyError),
};

struct RttiAllocator { void* const* vtable = RttiAllocatorVtable; };
RttiAllocator rttiAllocator;

char emptyCharacters[1]{};

AllocationContext ActiveAllocation() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return state.allocation;
}

void* Allocate(std::size_t size) {
    const auto context = ActiveAllocation();
    return context.allocator ? Allocate(context, size) : nullptr;
}

void Release(void* memory) {
    const auto context = ActiveAllocation();
    if (context.allocator) Release(context, memory);
}

const char* Characters(const String& string) {
    return string.text ? string.text->characters : emptyCharacters;
}

std::string_view View(const String& string) {
    return {Characters(string), string.text ? string.text->length : 0};
}

std::uint32_t CompiledSdk() {
    std::uint32_t sdk = std::numeric_limits<std::uint32_t>::max();
    try {
        const auto* process = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
        if (process) std::memcpy(&sdk, process + 0x10, sizeof(sdk));
    } catch (const std::runtime_error&) {
    }
    return sdk;
}

bool ModernValues() {
    const auto sdk = CompiledSdk();
    return sdk >= 0x06000000 || (sdk >= 0x05508700 && sdk < 0x05510000);
}

std::uint64_t AllocationFailures() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return state.allocationFailures;
}

bool Grow(Text& text, std::size_t requested) {
    if (requested > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("sce::Json::String: capacity exceeds 32 bits");
    if (requested <= text.capacity) return true;
    std::uint32_t capacity = static_cast<std::uint32_t>(requested);
    const auto sdk = CompiledSdk();
    if (sdk > 0x044fffff) {
        capacity |= 0x10;
        if (capacity / 3 < text.capacity / 2) {
            const auto grown = std::uint64_t{text.capacity} + text.capacity / 2;
            capacity = static_cast<std::uint32_t>(std::min(grown, std::uint64_t{std::numeric_limits<std::uint32_t>::max()}));
        }
    }
    auto* memory = static_cast<char*>(Allocate(capacity));
    if (!memory) return false;
    std::memcpy(memory, text.characters, text.length);
    memory[text.length] = 0;
    if (text.characters != emptyCharacters) Release(text.characters);
    text.characters = memory;
    text.capacity = capacity;
    return true;
}

void ClearText(String& string) {
    if (!string.text) return;
    if (string.text->characters != emptyCharacters) Release(string.text->characters);
    string.text->characters = emptyCharacters;
    string.text->length = 0;
    string.text->capacity = 0;
}

void AssignText(String& string, std::string_view bytes, bool preserveBinary = false) {
    if (!string.text || bytes.data() == string.text->characters) return;
    if (bytes.size() >= std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("sce::Json::String: length exceeds 32 bits");
    ClearText(string);
    if (bytes.empty() || (!preserveBinary && !bytes.front())) return;
    auto* memory = static_cast<char*>(Allocate(bytes.size() + 1));
    if (!memory) return;
    std::memcpy(memory, bytes.data(), bytes.size());
    memory[bytes.size()] = 0;
    string.text->characters = memory;
    string.text->length = static_cast<std::uint32_t>(bytes.size());
    string.text->capacity = static_cast<std::uint32_t>(bytes.size() + 1);
}

void AppendText(String& string, std::string_view bytes) {
    if (!string.text) return;
    if (bytes.size() >= std::numeric_limits<std::uint32_t>::max() - string.text->length)
        throw std::length_error("sce::Json::String: length exceeds 32 bits");
    const auto source = reinterpret_cast<std::uintptr_t>(bytes.data());
    const auto destination = reinterpret_cast<std::uintptr_t>(string.text->characters);
    const bool aliases = source >= destination && source - destination < string.text->capacity;
    const auto offset = source - destination;
    if (!Grow(*string.text, string.text->length + bytes.size() + 1)) return;
    const char* data = aliases ? string.text->characters + offset : bytes.data();
    std::memmove(string.text->characters + string.text->length, data, bytes.size());
    string.text->length += static_cast<std::uint32_t>(bytes.size());
    string.text->characters[string.text->length] = 0;
}

void Construct(String& string, std::string_view bytes) {
    string.text = static_cast<Text*>(Allocate(sizeof(Text)));
    if (!string.text) return;
    string.text->characters = emptyCharacters;
    string.text->length = 0;
    string.text->capacity = 0;
    AssignText(string, bytes);
}

void Construct(String& string, const String& source) {
    if (!source.text) { string.text = nullptr; return; }
    Construct(string, std::string_view{});
    AssignText(string, View(source), true);
}
struct GuestOutOfMemory {};

void Construct(Value& value);
void Destroy(Value& value);
void CopyInto(Value& destination, const Value& source);

struct ArrayLink {
    ArrayLink* previous;
    ArrayLink* next;
    Value value;
};
static_assert(sizeof(ArrayLink) == 48 && offsetof(ArrayLink, value) == 16);

struct ArrayStorage {
    ArrayLink* head;
    std::size_t count;
    std::uint64_t reserved;

    struct iterator {
        using value_type = Value;
        using difference_type = std::ptrdiff_t;
        using pointer = Value*;
        using reference = Value&;
        using iterator_category = std::bidirectional_iterator_tag;
        ArrayLink* node = nullptr;
        Value& operator*() const { return node->value; }
        Value* operator->() const { return &node->value; }
        iterator& operator++() { node = node->next; return *this; }
        iterator operator++(int) { const auto previous = *this; ++*this; return previous; }
        iterator& operator--() { node = node->previous; return *this; }
        iterator operator--(int) { const auto previous = *this; --*this; return previous; }
        bool operator==(const iterator&) const = default;
    };

    Value* Owner() const {
        Value* owner = nullptr;
        if (ModernValues()) std::memcpy(&owner, reinterpret_cast<const std::byte*>(this) + 24, sizeof(owner));
        return owner;
    }
    void SetOwner(Value* owner) {
        if (ModernValues()) std::memcpy(reinterpret_cast<std::byte*>(this) + 24, &owner, sizeof(owner));
    }
    iterator begin() const { return {head ? head->next : nullptr}; }
    iterator end() const { return {head}; }
    bool empty() const { return !count; }
    std::size_t size() const { return count; }
    Value& front() { return head->next->value; }
    Value& back() { return head->previous->value; }
    const Value& front() const { return head->next->value; }
    const Value& back() const { return head->previous->value; }
    Value& emplace_back() {
        if (!head) throw GuestOutOfMemory{};
        auto* link = static_cast<ArrayLink*>(Allocate(sizeof(ArrayLink)));
        if (!link) throw GuestOutOfMemory{};
        Construct(link->value);
        link->previous = head->previous;
        link->next = head;
        head->previous->next = link;
        head->previous = link;
        ++count;
        return link->value;
    }
    void ReleaseContents() {
        if (!head) return;
        auto* link = head->next;
        while (link != head) {
            auto* next = link->next;
            Destroy(link->value);
            Release(link);
            link = next;
        }
    }
    void clear() {
        ReleaseContents();
        if (head) { head->previous = head; head->next = head; }
        count = 0;
    }
    void PopBack() {
        if (!head || !count) return;
        auto* link = head->previous;
        link->previous->next = head;
        head->previous = link->previous;
        Destroy(link->value);
        Release(link);
        --count;
    }
};
static_assert(sizeof(ArrayStorage) == 24 && sizeof(ArrayIteratorState) == 8);

ArrayLink* NewArrayHead() {
    auto* head = static_cast<ArrayLink*>(Allocate(sizeof(ArrayLink)));
    if (head) { head->previous = head; head->next = head; }
    return head;
}

void Construct(Array& array) {
    array.items = static_cast<ArrayStorage*>(Allocate(ModernValues() ? 32 : 24));
    if (!array.items) return;
    array.items->count = 0;
    array.items->head = NewArrayHead();
    array.items->SetOwner(nullptr);
}
void Destroy(String& string);
void BindChildren(Value& value);
void AssignNode(Value& destination, const Value& source);
void TransferPayload(Value& destination, Value& source);

void PoisonText(Text& text) {
    text.characters = reinterpret_cast<char*>(std::uintptr_t{0xffffffff});
    text.length = std::numeric_limits<std::uint32_t>::max();
    text.capacity = std::numeric_limits<std::uint32_t>::max();
}

struct InlineText {
    Text text;
    InlineText() { text.characters = emptyCharacters; text.length = 0; text.capacity = 0; }
    ~InlineText() { String string{&text}; ClearText(string); PoisonText(text); }
    InlineText(const InlineText&) = delete;
    InlineText& operator=(const InlineText&) = delete;
    void Assign(std::string_view bytes) { String string{&text}; AssignText(string, bytes, true); }
};

struct ObjectLink {
    ObjectLink* parent;
    ObjectLink* left;
    ObjectLink* right;
    Text key;
    Pair pair;
    std::uint8_t color;
    std::uint8_t nil;
    std::uint8_t reserved[6];
};
static_assert(sizeof(ObjectLink) == 104 && offsetof(ObjectLink, key) == 24);
static_assert(offsetof(ObjectLink, pair) == 48 && offsetof(ObjectLink, color) == 96);

ObjectLink* NextObjectLink(ObjectLink* node) {
    if (!node) return nullptr;
    if (node->nil) throw std::out_of_range("sce::Json::Object::iterator: advance past the end");
    if (!node->right->nil) {
        node = node->right;
        while (!node->left->nil) node = node->left;
        return node;
    }
    auto* parent = node->parent;
    while (!parent->nil && node == parent->right) { node = parent; parent = parent->parent; }
    return parent;
}

void DestroyObjectLink(ObjectLink* node) {
    Destroy(node->pair.value);
    Destroy(node->pair.key);
    String key{&node->key};
    ClearText(key);
    PoisonText(node->key);
    Release(node);
}

void ReleaseObjectTree(ObjectLink* node) {
    if (!node || node->nil) return;
    ReleaseObjectTree(node->left);
    ReleaseObjectTree(node->right);
    if (!node->parent->nil) {
        if (node->parent->left == node) node->parent->left = node->left;
        else node->parent->right = node->right;
    }
    DestroyObjectLink(node);
}

ObjectLink* NewObjectHead() {
    auto* head = static_cast<ObjectLink*>(Allocate(sizeof(ObjectLink)));
    if (head) { head->parent = head; head->left = head; head->right = head; head->color = 0; head->nil = 1; }
    return head;
}

struct ObjectStorage {
    ObjectLink* head;
    std::size_t count;
    std::uint64_t reserved;

    struct iterator {
        using value_type = Pair;
        using difference_type = std::ptrdiff_t;
        using pointer = Pair*;
        using reference = Pair&;
        using iterator_category = std::bidirectional_iterator_tag;
        ObjectLink* node = nullptr;
        Pair& operator*() const { return node->pair; }
        Pair* operator->() const { return &node->pair; }
        iterator& operator++() { node = NextObjectLink(node); return *this; }
        iterator operator++(int) { const auto previous = *this; ++*this; return previous; }
        iterator& operator--() {
            if (node->nil) node = node->right;
            else if (!node->left->nil) { node = node->left; while (!node->right->nil) node = node->right; }
            else { auto* parent = node->parent; while (!parent->nil && node == parent->left) { node = parent; parent = parent->parent; } node = parent; }
            return *this;
        }
        bool operator==(const iterator&) const = default;
    };

    Value* Owner() const {
        Value* owner = nullptr;
        if (ModernValues()) std::memcpy(&owner, reinterpret_cast<const std::byte*>(this) + 24, sizeof(owner));
        return owner;
    }
    void SetOwner(Value* owner) { if (ModernValues()) std::memcpy(reinterpret_cast<std::byte*>(this) + 24, &owner, sizeof(owner)); }
    iterator begin() const { return {head ? head->left : nullptr}; }
    iterator end() const { return {head}; }
    bool empty() const { return !count; }
    std::size_t size() const { return count; }
    ObjectLink* Find(const char* key) const {
        if (!head) return nullptr;
        auto* node = head->parent;
        while (!node->nil) {
            const int comparison = std::strcmp(key, node->key.characters);
            if (!comparison) return node;
            node = comparison < 0 ? node->left : node->right;
        }
        return head;
    }
    void RefreshEndpoints() {
        auto* first = head->parent;
        auto* last = first;
        if (!first->nil) {
            while (!first->left->nil) first = first->left;
            while (!last->right->nil) last = last->right;
        }
        head->left = first;
        head->right = last;
    }
    void RotateLeft(ObjectLink* node) {
        auto* pivot = node->right;
        node->right = pivot->left;
        if (!pivot->left->nil) pivot->left->parent = node;
        pivot->parent = node->parent;
        if (node->parent->nil) head->parent = pivot;
        else if (node == node->parent->left) node->parent->left = pivot;
        else node->parent->right = pivot;
        pivot->left = node;
        node->parent = pivot;
    }
    void RotateRight(ObjectLink* node) {
        auto* pivot = node->left;
        node->left = pivot->right;
        if (!pivot->right->nil) pivot->right->parent = node;
        pivot->parent = node->parent;
        if (node->parent->nil) head->parent = pivot;
        else if (node == node->parent->right) node->parent->right = pivot;
        else node->parent->left = pivot;
        pivot->right = node;
        node->parent = pivot;
    }
    void BalanceInsertion(ObjectLink* node) {
        while (node->parent->color == 1) {
            auto* parent = node->parent;
            auto* grandparent = parent->parent;
            if (parent == grandparent->left) {
                auto* uncle = grandparent->right;
                if (uncle->color == 1) { parent->color = 0; uncle->color = 0; grandparent->color = 1; node = grandparent; }
                else {
                    if (node == parent->right) { node = parent; RotateLeft(node); parent = node->parent; grandparent = parent->parent; }
                    parent->color = 0; grandparent->color = 1; RotateRight(grandparent);
                }
            } else {
                auto* uncle = grandparent->left;
                if (uncle->color == 1) { parent->color = 0; uncle->color = 0; grandparent->color = 1; node = grandparent; }
                else {
                    if (node == parent->left) { node = parent; RotateRight(node); parent = node->parent; grandparent = parent->parent; }
                    parent->color = 0; grandparent->color = 1; RotateLeft(grandparent);
                }
            }
        }
        head->parent->color = 0;
    }
    ObjectLink* Insert(const Text& key, const Pair& pair, bool& inserted) {
        inserted = false;
        if (!head) return nullptr;
        auto* parent = head;
        auto* position = head->parent;
        int comparison = 0;
        while (!position->nil) {
            parent = position;
            comparison = std::strcmp(key.characters, position->key.characters);
            if (!comparison) return position;
            position = comparison < 0 ? position->left : position->right;
        }
        auto* node = static_cast<ObjectLink*>(Allocate(sizeof(ObjectLink)));
        if (!node) return head;
        node->parent = parent; node->left = head; node->right = head;
        node->key.characters = emptyCharacters; node->key.length = 0; node->key.capacity = 0;
        node->pair.key.text = nullptr;
        Construct(node->pair.value);
        node->color = 1; node->nil = 0;
        try {
            String keyStorage{&node->key}; AssignText(keyStorage, {key.characters, key.length}, true);
            Construct(node->pair.key, pair.key);
            std::memcpy(&node->pair.reserved, &pair.reserved, sizeof(std::uint32_t));
            CopyInto(node->pair.value, pair.value);
        } catch (...) { DestroyObjectLink(node); throw; }
        if (parent == head) head->parent = node;
        else if (comparison < 0) parent->left = node;
        else parent->right = node;
        ++count;
        BalanceInsertion(node);
        RefreshEndpoints();
        inserted = true;
        return node;
    }
    void Replace(ObjectLink* node, ObjectLink* replacement) {
        if (node->parent->nil) head->parent = replacement;
        else if (node == node->parent->left) node->parent->left = replacement;
        else node->parent->right = replacement;
        if (!replacement->nil) replacement->parent = node->parent;
    }
    void BalanceErasure(ObjectLink* node, ObjectLink* parent) {
        while (node != head->parent && node->color == 0) {
            if (node == parent->left) {
                auto* sibling = parent->right;
                if (sibling->color == 1) { sibling->color = 0; parent->color = 1; RotateLeft(parent); sibling = parent->right; }
                if (sibling->left->color == 0 && sibling->right->color == 0) {
                    if (!sibling->nil) sibling->color = 1;
                    node = parent; parent = node->parent;
                } else {
                    if (sibling->right->color == 0) { sibling->left->color = 0; sibling->color = 1; RotateRight(sibling); sibling = parent->right; }
                    sibling->color = parent->color; parent->color = 0; sibling->right->color = 0;
                    RotateLeft(parent); node = head->parent;
                }
            } else {
                auto* sibling = parent->left;
                if (sibling->color == 1) { sibling->color = 0; parent->color = 1; RotateRight(parent); sibling = parent->left; }
                if (sibling->right->color == 0 && sibling->left->color == 0) {
                    if (!sibling->nil) sibling->color = 1;
                    node = parent; parent = node->parent;
                } else {
                    if (sibling->left->color == 0) { sibling->right->color = 0; sibling->color = 1; RotateLeft(sibling); sibling = parent->left; }
                    sibling->color = parent->color; parent->color = 0; sibling->left->color = 0;
                    RotateRight(parent); node = head->parent;
                }
            }
        }
        node->color = 0;
    }
    void Erase(ObjectLink* removed) {
        if (!removed || removed->nil) return;
        auto* successor = removed;
        auto color = successor->color;
        ObjectLink* child;
        ObjectLink* parent;
        if (removed->left->nil) { child = removed->right; parent = removed->parent; Replace(removed, child); }
        else if (removed->right->nil) { child = removed->left; parent = removed->parent; Replace(removed, child); }
        else {
            successor = removed->right;
            while (!successor->left->nil) successor = successor->left;
            color = successor->color;
            child = successor->right;
            if (successor->parent == removed) parent = successor;
            else {
                parent = successor->parent;
                Replace(successor, child);
                successor->right = removed->right;
                successor->right->parent = successor;
            }
            Replace(removed, successor);
            successor->left = removed->left;
            successor->left->parent = successor;
            successor->color = removed->color;
        }
        if (color == 0) BalanceErasure(child, parent);
        RefreshEndpoints();
        DestroyObjectLink(removed);
        --count;
    }
    void clear() {
        if (!head) return;
        ReleaseObjectTree(head->parent);
        head->parent = head; head->left = head; head->right = head;
        count = 0;
    }
};
static_assert(sizeof(ObjectStorage) == 24 && sizeof(ObjectIteratorState) == 8);

void Construct(Object& object) {
    object.items = static_cast<ObjectStorage*>(Allocate(ModernValues() ? 32 : 24));
    if (!object.items) return;
    object.items->count = 0;
    object.items->head = NewObjectHead();
    object.items->SetOwner(nullptr);
    if (!object.items->head) {
        object.items->head = reinterpret_cast<ObjectLink*>(std::uintptr_t{0xffffffff});
        object.items->count = std::numeric_limits<std::size_t>::max();
        Release(object.items);
        object.items = nullptr;
    }
}

ObjectLink* CloneObjectTree(ObjectLink* source, ObjectLink* parent, ObjectLink* head) {
    auto* node = static_cast<ObjectLink*>(Allocate(sizeof(ObjectLink)));
    if (!node) return head;
    node->parent = parent; node->left = head; node->right = head;
    node->key.characters = emptyCharacters; node->key.length = 0; node->key.capacity = 0;
    node->pair.key.text = nullptr;
    Construct(node->pair.value);
    node->color = source->color; node->nil = 0;
    try {
        String key{&node->key}; AssignText(key, {source->key.characters, source->key.length}, true);
        Construct(node->pair.key, source->pair.key);
        std::memcpy(&node->pair.reserved, &source->pair.reserved, sizeof(std::uint32_t));
        CopyInto(node->pair.value, source->pair.value);
        if (!source->left->nil) node->left = CloneObjectTree(source->left, node, head);
        if (!source->right->nil) node->right = CloneObjectTree(source->right, node, head);
    } catch (...) {
        ReleaseObjectTree(node->left); ReleaseObjectTree(node->right); DestroyObjectLink(node); throw;
    }
    return node;
}

void CopyObjectStorage(ObjectStorage& destination, const ObjectStorage& source) {
    auto* replacement = NewObjectHead();
    ObjectStorage pending{replacement, 0, 0};
    try {
        if (replacement && source.head && !source.head->parent->nil) {
            replacement->parent = CloneObjectTree(source.head->parent, replacement, replacement);
            if (replacement->parent != replacement) { pending.count = source.count; pending.RefreshEndpoints(); }
        }
    } catch (...) { pending.clear(); Release(replacement); throw; }
    destination.clear(); Release(destination.head);
    destination.head = replacement;
    destination.count = pending.count;
}

void Destroy(Value& v);
void CopyInto(Value& destination, const Value& source);

void Destroy(String& s) {
    if (!s.text) return;
    ClearText(s);
    PoisonText(*s.text);
    Release(s.text);
    s.text = nullptr;
}
void Destroy(Array& a) {
    if (!a.items) return;
    a.items->ReleaseContents();
    Release(a.items->head);
    a.items->head = reinterpret_cast<ArrayLink*>(std::uintptr_t{0xffffffff});
    a.items->count = std::numeric_limits<std::size_t>::max();
    Release(a.items);
    a.items = nullptr;
}
void Destroy(Object& object) {
    if (!object.items) return;
    object.items->clear();
    Release(object.items->head);
    object.items->head = reinterpret_cast<ObjectLink*>(std::uintptr_t{0xffffffff});
    object.items->count = std::numeric_limits<std::size_t>::max();
    Release(object.items);
    object.items = nullptr;
}

const Node& RootOf(const Node& n) {
    const Node* root = &n;
    while (root->parent) root = root->parent;
    return *root;
}

Handlers HandlersOf(const Node& n) {
    const auto* auxiliary = RootOf(n).auxiliary;
    Handlers handlers;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        if (state.initialized) handlers = state.handlers;
    }
    if (!auxiliary || !ModernValues()) return handlers;
    if (auxiliary->typeMismatch && auxiliary->typeMismatch->callback) {
        handlers.typeMismatch = auxiliary->typeMismatch->callback;
        handlers.typeMismatchContext = auxiliary->typeMismatch->context;
    }
    if (auxiliary->specialFloat && auxiliary->specialFloat->callback) {
        handlers.specialFloat = auxiliary->specialFloat->callback;
        handlers.specialFloatContext = auxiliary->specialFloat->context;
    }
    if (auxiliary->elementAccessFailure && auxiliary->elementAccessFailure->callback) {
        handlers.elementAccessFailure = auxiliary->elementAccessFailure->callback;
        handlers.elementAccessFailureContext = auxiliary->elementAccessFailure->context;
    }
    return handlers;
}

void BindChildren(Node& n) {
    if (n.type == TypeArray && n.array && n.array->items) {
        n.array->items->SetOwner(&n);
        for (auto& item : *n.array->items) item.parent = &n;
    }
    if (n.type == TypeObject && n.object && n.object->items) {
        n.object->items->SetOwner(&n);
        for (auto& pair : *n.object->items) pair.value.parent = &n;
    }
}

template<typename T>
void Destroy(T*& pointer) {
    if (pointer) {
        Destroy(*pointer);
        Release(pointer);
        pointer = nullptr;
    }
}

void Clear(Node& n) {
    if (n.type == TypeString) Destroy(n.string);
    if (n.type == TypeArray) Destroy(n.array);
    if (n.type == TypeObject) Destroy(n.object);
    n.type = TypeNull;
}

void Destroy(Value& v) {
    Clear(v);
    if (v.auxiliary) {
        if (ModernValues()) {
            if (v.auxiliary->typeMismatch) Release(v.auxiliary->typeMismatch);
            v.auxiliary->typeMismatch = nullptr;
            if (v.auxiliary->specialFloat) Release(v.auxiliary->specialFloat);
            v.auxiliary->specialFloat = nullptr;
            if (v.auxiliary->elementAccessFailure) Release(v.auxiliary->elementAccessFailure);
            v.auxiliary->elementAccessFailure = nullptr;
        }
        if (v.auxiliary->fallback) {
            Destroy(*v.auxiliary->fallback);
            Release(v.auxiliary->fallback);
            v.auxiliary->fallback = nullptr;
        }
        Release(v.auxiliary);
        v.auxiliary = nullptr;
    }
}

int EnsureAuxiliary(Node& n) {
    if (n.auxiliary) return 0;
    const auto bytes = ModernValues() ? sizeof(Auxiliary) : std::size_t{24};
    auto* auxiliary = static_cast<Auxiliary*>(Allocate(bytes));
    if (!auxiliary) return JsonErrorOutOfMemory;
    auxiliary->nullAccess = nullptr;
    auxiliary->nullAccessContext = nullptr;
    auxiliary->fallback = static_cast<Value*>(Allocate(sizeof(Value)));
    if (auxiliary->fallback) Construct(*auxiliary->fallback);
    if (ModernValues()) std::memset(reinterpret_cast<std::byte*>(auxiliary) + 24, 0, sizeof(Auxiliary) - 24);
    n.auxiliary = auxiliary;
    if (!auxiliary->fallback) return JsonErrorOutOfMemory;
    auxiliary->fallback->parent = &n;
    return 0;
}

Node& NodeOf(const Value& v) { return const_cast<Value&>(v); }

void Construct(Value& v) {
    v.parent = nullptr;
    v.auxiliary = nullptr;
    v.type = TypeNull;
}

void TransferPayload(Node& destination, Node& source) {
    if (source.type == TypeBoolean) destination.boolean = source.boolean;
    else if (source.type != TypeNull) std::memcpy(&destination.payload, &source.payload, sizeof(destination.payload));
    destination.type = source.type;
    source.type = TypeNull;
    BindChildren(destination);
}


bool HasStorage(const String& value) { return value.text != nullptr; }
bool HasStorage(const Array& value) { return value.items != nullptr; }
bool HasStorage(const Object& value) { return value.items != nullptr; }

template<typename T, typename TInitialize>
void ConstructHandle(T*& destination, TInitialize initialize) {
    destination = static_cast<T*>(Allocate(sizeof(T)));
    if (!destination) return;
    *destination = {};
    try { initialize(*destination); }
    catch (...) { Destroy(destination); throw; }
    if (!HasStorage(*destination)) { Release(destination); destination = nullptr; }
}

template<typename T, typename... TArgs>
void Construct(T*& destination, const TArgs&... arguments) {
    ConstructHandle(destination, [&](T& value) { Construct(value, arguments...); });
}

void CopyArray(Array& destination, const Array& source) {
    Array pending{};
    if (!source.items) { destination = pending; return; }
    Construct(pending);
    if (!pending.items) { destination = pending; return; }
    auto* originalHead = pending.items->head;
    pending.items->head = NewArrayHead();
    try {
        if (pending.items->head) {
            for (const auto& item : *source.items) {
                try {
                    CopyInto(pending.items->emplace_back(), item);
                } catch (const GuestOutOfMemory&) {
                    break;
                }
            }
        }
    } catch (...) {
        Release(originalHead);
        Destroy(pending);
        throw;
    }
    Release(originalHead);
    destination = pending;
}

void CopyObject(Object& destination, const Object& source) {
    Object pending{};
    if (!source.items) { destination = pending; return; }
    pending.items = static_cast<ObjectStorage*>(Allocate(ModernValues() ? 32 : 24));
    if (!pending.items) { destination = pending; return; }
    pending.items->count = 0;
    pending.items->head = NewObjectHead();
    pending.items->SetOwner(nullptr);
    try { CopyObjectStorage(*pending.items, *source.items); }
    catch (...) { Destroy(pending); throw; }
    destination = pending;
}

void CopyArray(Array*& destination, const Array* source) {
    ConstructHandle(destination, [&](Array& value) { CopyArray(value, *source); });
}

void CopyObject(Object*& destination, const Object* source) {
    ConstructHandle(destination, [&](Object& value) { CopyObject(value, *source); });
}

void AssignReal(Node& n, double value);

void SetType(Node& n, ValueType type);

void AssignNode(Node& destination, const Node& source) {
    if (source.type == TypeNull) { SetType(destination, TypeNull); return; }
    if (source.type == TypeBoolean) {
        const auto value = source.boolean;
        SetType(destination, TypeBoolean);
        destination.boolean = value;
        return;
    }
    if (source.type == TypeInteger || source.type == TypeUInteger) {
        const auto value = source.payload;
        SetType(destination, source.type);
        destination.payload = value;
        return;
    }
    if (source.type == TypeReal) { AssignReal(destination, source.real); return; }
    if (source.type == TypeString) {
        const auto* string = source.string;
        if (!string) NotImplemented_nid_no_patch(__func__);
        if (!string->text) return;
        SetType(destination, TypeString);
        if (destination.string && destination.string->text != string->text) AssignText(*destination.string, View(*string), true);
        return;
    }
    if (&destination == &source) return;
    Node copy{};
    copy.type = source.type;
    try {
        if (source.type == TypeArray) CopyArray(copy.array, source.array);
        else if (source.type == TypeObject) CopyObject(copy.object, source.object);
        else NotImplemented_nid_no_patch(__func__);
        if ((copy.type == TypeArray && !copy.array) || (copy.type == TypeObject && !copy.object)) copy.type = TypeNull;
    } catch (...) {
        Clear(copy);
        throw;
    }
    Clear(destination);
    TransferPayload(destination, copy);
}

void RecoverSpecial(const Node& source, const Handlers& handlers, int function, double number, Value& result, bool* error) {
    Value returned{};
    try {
        handlers.specialFloat(&returned, function, number, source.parent, handlers.specialFloatContext, error);
        AssignNode(result, returned);
    } catch (...) {
        Destroy(returned);
        throw;
    }
    Destroy(returned);
}

void CopyInto(Value& destination, const Value& source) {
    Construct(destination);
    AssignNode(destination, source);
}

void SetType(Node& n, ValueType type) {
    if (n.type == type) return;
    if (type < TypeNull || type > TypeObject) throw std::invalid_argument("sce::Json::Value::set: invalid value type");
    Clear(n);
    if (type == TypeBoolean) n.boolean = false;
    else if (type == TypeInteger || type == TypeUInteger || type == TypeReal) n.payload = 0;
    else if (type == TypeString) {
        Construct(n.string, std::string_view{});
        if (!n.string) return;
    } else if (type == TypeArray) {
        Construct(n.array);
        if (!n.array) return;
    } else if (type == TypeObject) {
        Construct(n.object);
        if (!n.object) return;
    }
    n.type = type;
    BindChildren(n);
}

struct DefaultStorage {
    Value values[8]{};
    bool ready[8]{};
};

DefaultStorage& SharedDefaults() {
    static DefaultStorage defaults;
    return defaults;
}

const Value& Defaults(ValueType type) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto& defaults = SharedDefaults();
    if (!defaults.ready[type]) {
        Construct(defaults.values[type]);
        SetType(defaults.values[type], type);
        defaults.ready[type] = true;
    }
    return defaults.values[type];
}

void ResetDefaults() {
    auto& defaults = SharedDefaults();
    for (unsigned type = 0; type < 8; ++type) {
        if (defaults.ready[type]) Destroy(defaults.values[type]);
        defaults.values[type] = {};
        defaults.ready[type] = false;
    }
}

const Node& Typed(const Value* self, ValueType requested) {
    if (self->type == requested ||
            ((requested == TypeInteger || requested == TypeUInteger) &&
             (self->type == TypeInteger || self->type == TypeUInteger))) return *self;
    CallbackScope scope;
    if (!scope.entered) return Defaults(requested);
    NullAccessCallback callback = nullptr;
    void* context = nullptr;
    bool initialized;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        initialized = state.initialized;
        callback = state.callback;
        context = state.context;
    }
    if (!initialized) return Defaults(requested);
    if (self->type == TypeNull) {
        const auto* auxiliary = RootOf(*self).auxiliary;
        if (auxiliary && auxiliary->nullAccess)
            return Typed(&auxiliary->nullAccess(requested, self->parent, auxiliary->nullAccessContext), requested);
    }
    const auto* auxiliary = RootOf(*self).auxiliary;
    if (ModernValues() && auxiliary && auxiliary->typeMismatch && auxiliary->typeMismatch->callback)
        return Typed(&auxiliary->typeMismatch->callback(requested, self, self->parent, auxiliary->typeMismatch->context), requested);
    if (self->type == TypeNull && callback)
        return Typed(&callback(requested, self->parent, context), requested);
    const auto handlers = HandlersOf(*self);
    if (handlers.typeMismatch)
        return Typed(&handlers.typeMismatch(requested, self, self->parent, handlers.typeMismatchContext), requested);
    return Defaults(requested);
}

const Value& MissingElement(const Value* self, int error, const String* key, const std::size_t* index) {
    bool initialized;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        initialized = state.initialized;
    }
    const auto* auxiliary = RootOf(*self).auxiliary;
    if (initialized) {
        CallbackScope scope;
        const auto handlers = HandlersOf(*self);
        if (scope.entered && handlers.elementAccessFailure &&
                (!auxiliary || self != auxiliary->fallback) && self != &Defaults(TypeNull))
            handlers.elementAccessFailure(error, key, index, self->parent, self, handlers.elementAccessFailureContext);
        return auxiliary && auxiliary->fallback ? *auxiliary->fallback : Defaults(TypeNull);
    }
    return Defaults(TypeNull);
}

template <typename TCallback>
int SetHandler(Node& n, HandlerRecord<TCallback>* Auxiliary::* member, TCallback callback, void* context, bool modernOnly = false) {
    if (n.parent) return static_cast<int>(0x80848104);
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        if (!state.initialized) return JsonErrorNotInitialized;
        if (modernOnly && !state.floatFormatEnabled) return static_cast<int>(0x80848113);
    }
    if (!ModernValues()) NotImplemented_nid_no_patch(__func__);
    const auto status = EnsureAuxiliary(n);
    if (status) return status;
    auto& record = n.auxiliary->*member;
    if (!record) {
        record = static_cast<HandlerRecord<TCallback>*>(Allocate(sizeof(HandlerRecord<TCallback>)));
        if (!record) return JsonErrorOutOfMemory;
    }
    record->callback = callback;
    record->context = context;
    return 0;
}

template<typename TCallback>
int SetGlobalHandler(void* self, TCallback Handlers::* callbackMember, void* Handlers::* contextMember,
        TCallback callback, void* context, bool modernOnly = false) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.initialized || !*static_cast<const std::uint8_t*>(self)) return JsonErrorNotInitialized;
    if (modernOnly && !state.floatFormatEnabled) return static_cast<int>(0x80848113);
    if (!callback) return JsonErrorInvalidArgument;
    if (state.handlers.*callbackMember) return static_cast<int>(0x80848112);
    state.handlers.*callbackMember = callback;
    state.handlers.*contextMember = context;
    return 0;
}

const char* SpecialFloatText(double value) {
    return std::isnan(value) ? "nan" : value < 0 ? "-inf" : "inf";
}

struct FloatSettings {
    std::int32_t format;
    bool enabled;
};

FloatSettings FloatConfiguration() {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return {state.floatFormat, state.floatFormatEnabled};
}

void AssignReal(Node& n, double value) {
    if (std::isfinite(value)) {
        SetType(n, TypeReal);
        n.real = value;
        return;
    }
    const auto settings = FloatConfiguration();
    if (settings.enabled) {
        CallbackScope scope;
        const auto handlers = HandlersOf(n);
        if (scope.entered && handlers.specialFloat) {
            Value result{};
            try {
                RecoverSpecial(n, handlers, 1, value, result, nullptr);
                AssignNode(n, result);
            } catch (...) {
                Destroy(result);
                throw;
            }
            Destroy(result);
            return;
        }
    }
    if (settings.enabled && settings.format != 2) {
        if (settings.format == 1) {
            SetType(n, TypeNull);
        } else {
            String temporary{};
            Construct(temporary, SpecialFloatText(value));
            try {
                if (temporary.text) {
                    SetType(n, TypeString);
                    if (n.string) AssignText(*n.string, View(temporary), true);
                }
            } catch (...) { Destroy(temporary); throw; }
            Destroy(temporary);
        }
        return;
    }
    SetType(n, TypeReal);
    n.real = value;
}

void AppendEscaped(std::string& out, std::string_view text) {
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

void AppendReal(std::string& out, double value) {
    if (!std::isfinite(value)) {
        const auto settings = FloatConfiguration();
        if (settings.enabled) {
            if (settings.format == 0) AppendEscaped(out, SpecialFloatText(value));
            else out += settings.format == 1 ? "null" : SpecialFloatText(value);
            return;
        }
    }
    out += RealText(value);
}

int Serialize(std::string& out, const Node& n) {
    switch (n.type) {
        case TypeNull: out += "null"; return 0;
        case TypeBoolean: out += n.boolean ? "true" : "false"; return 0;
        case TypeInteger: out += std::to_string(n.integer); return 0;
        case TypeUInteger: out += std::to_string(n.uinteger); return 0;
        case TypeReal: {
            if (!std::isfinite(n.real) && FloatConfiguration().enabled) {
                CallbackScope scope;
                const auto handlers = HandlersOf(n);
                if (scope.entered && handlers.specialFloat) {
                    Value replacement{};
                    int result = 0;
                    try {
                        bool error = false;
                        RecoverSpecial(n, handlers, 0, n.real, replacement, &error);
                        result = error ? JsonErrorSpecialFloat : Serialize(out, replacement);
                    } catch (...) {
                        Destroy(replacement);
                        throw;
                    }
                    Destroy(replacement);
                    return result;
                }
            }
            AppendReal(out, n.real);
            return 0;
        }
        case TypeString: AppendEscaped(out, View(*n.string)); return 0;
        case TypeArray: {
            out += '[';
            bool first = true;
            for (const auto& item : *n.array->items) {
                if (!first) out += ',';
                first = false;
                const int result = Serialize(out, item);
                if (result < 0) return result;
            }
            out += ']';
            return 0;
        }
        case TypeObject: {
            out += '{';
            bool first = true;
            for (const auto& pair : *n.object->items) {
                if (!first) out += ',';
                first = false;
                AppendEscaped(out, Characters(pair.key));
                out += ':';
                const int result = Serialize(out, pair.value);
                if (result < 0) return result;
            }
            out += '}';
            return 0;
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

struct ScopedString {
    String string{};
    explicit ScopedString(std::string_view text) { Construct(string, text); }
    explicit ScopedString(const String& source) { Construct(string, source); }
    ~ScopedString() { Destroy(string); }
};

struct ScopedValue {
    Value value{};
    ~ScopedValue() { Destroy(value); }
};

struct TemporaryTreeEntry {
    InlineText key;
    Pair pair{};
    TemporaryTreeEntry(const Text& sourceKey, const Pair& source) {
        try {
            key.Assign({sourceKey.characters, sourceKey.length});
            Construct(pair.key, source.key);
            std::memcpy(&pair.reserved, &source.reserved, sizeof(std::uint32_t));
            CopyInto(pair.value, source.value);
        } catch (...) { Destroy(pair.value); Destroy(pair.key); throw; }
    }
    ~TemporaryTreeEntry() { Destroy(pair.value); Destroy(pair.key); }
};

struct ScopedPair {
    Pair pair{};
    ~ScopedPair() { Destroy(pair.value); Destroy(pair.key); }
};

void AssignObject(Object& destination, const Object& source) {
    if (!destination.items || !source.items) return;
    if (!ModernValues()) {
        CopyObjectStorage(*destination.items, *source.items);
        return;
    }
    Object pending{};
    Construct(pending);
    if (!pending.items) return;
    pending.items->SetOwner(destination.items->Owner());
    try {
        for (const auto& pair : *source.items) {
            ScopedValue placeholder;
            SetType(placeholder.value, pair.value.type);
            ScopedPair copied;
            {
                ScopedString key{std::string_view{Characters(pair.key)}};
                Construct(copied.pair.key, key.string);
                CopyInto(copied.pair.value, placeholder.value);
            }
            const auto failures = AllocationFailures();
            ObjectLink* node;
            bool inserted;
            {
                InlineText key; key.Assign(Characters(pair.key));
                TemporaryTreeEntry entry(key.text, copied.pair);
                node = pending.items->Insert(entry.key.text, entry.pair, inserted);
            }
            if (inserted && AllocationFailures() == failures) {
                node->pair.value.parent = destination.items->Owner();
                AssignNode(node->pair.value, pair.value);
            }
        }
    } catch (...) { Destroy(pending); throw; }
    auto previous = destination;
    destination = pending;
    Destroy(previous);
}

Value* ObjectAccess(Object& object, const String& input) {
    if (!object.items || !input.text) return const_cast<Value*>(&Defaults(TypeNull));
    if (!object.items->head) NotImplemented_nid_no_patch(__func__);
    const auto failures = AllocationFailures();
    ObjectLink* node;
    {
        InlineText key; key.Assign(Characters(input));
        ScopedString defaultKey{std::string_view{}};
        Pair source{}; source.key = defaultKey.string;
        TemporaryTreeEntry entry(key.text, source);
        bool inserted;
        node = object.items->Insert(entry.key.text, entry.pair, inserted);
    }
    if (AllocationFailures() != failures) return const_cast<Value*>(&Defaults(TypeNull));
    if (!node || node->nil) NotImplemented_nid_no_patch(__func__);
    AssignText(node->pair.key, View(input), true);
    node->pair.value.parent = object.items->Owner();
    return &node->pair.value;
}

Value& ObjectEntry(Object& object, std::string_view input) {
    const auto failures = AllocationFailures();
    ScopedString key(input);
    auto* value = ObjectAccess(object, key.string);
    if (AllocationFailures() != failures) throw GuestOutOfMemory{};
    return *value;
}

using ParserReadCallback = int (APS5_VABI*)(char&, void*);

class Parser {
public:
    Parser(const char* text, std::size_t size) : text(text), end(text + size) {}
    Parser(ParserReadCallback read, void* context) : read(read), context(context) {}

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
        if (!code) return;
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
                if (out.type != TypeString) throw GuestOutOfMemory{};
                AssignText(*out.string, text);
                return true;
            }
            case '[': {
                if (depth == MaxNestingDepth) return false;
                take();
                ++depth;
                SetType(out, TypeArray);
                if (out.type != TypeArray) throw GuestOutOfMemory{};
                skip();
                if (peek() >= 0 && peek() == ']') { take(); --depth; return true; }
                for (;;) {
                    if (!out.array->items) throw GuestOutOfMemory{};
                    auto& item = out.array->items->emplace_back();
                    Construct(item);
                    item.parent = &out;
                    skip();
                    if (!value(item)) return false;
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
                if (out.type != TypeObject) throw GuestOutOfMemory{};
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
                    Value& entry = ObjectEntry(*out.object, key);
                    Clear(entry);
                    if (!value(entry)) return false;
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
    if (!out->parent) {
        const auto status = EnsureAuxiliary(*out);
        if (status) return status;
    }
    Node parsed{};
    bool parsedOk = false;
    try {
        parsedOk = parser.Parse(parsed);
    } catch (const GuestOutOfMemory&) {
        Clear(parsed);
        return JsonErrorOutOfMemory;
    } catch (...) {
        Clear(parsed);
        throw;
    }
    if (!parsedOk) {
        Clear(parsed);
        return parser.Error();
    }
    Clear(NodeOf(*out));
    TransferPayload(*out, parsed);
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

int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void* self);

void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void* self) {
    *static_cast<std::uint8_t*>(self) = 0;
}

void APS5_VABI _ZN3sce4Json11InitializerD1Ev(void* self) {
    _ZN3sce4Json11Initializer9terminateEv(self);
}

int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void* self, const void* parameter) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.initialized || *static_cast<const std::uint8_t*>(self)) return JsonErrorAlreadyInitialized;
    if (!parameter) return JsonErrorInvalidArgument;
    if (!*static_cast<void* const*>(parameter)) return JsonErrorOutOfMemory;
    ResetDefaults();
    std::memcpy(&state.allocation, parameter, sizeof(state.allocation));
    state.floatFormat = 2;
    state.floatFormatEnabled = false;
    state.initialized = true;
    *static_cast<std::uint8_t*>(self) = 1;
    return 0;
}

int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void* self) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.initialized || !*static_cast<const std::uint8_t*>(self)) return JsonErrorNotInitialized;
    state.callback = nullptr;
    state.context = nullptr;
    state.handlers = {};
    ResetDefaults();
    state.allocation = {};
    state.floatFormat = 2;
    state.floatFormatEnabled = false;
    state.initialized = false;
    *static_cast<std::uint8_t*>(self) = 0;
    return 0;
}

int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void* self, NullAccessCallback callback, void* context) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (!state.initialized || !*static_cast<const std::uint8_t*>(self)) return JsonErrorNotInitialized;
    if (!callback) return JsonErrorInvalidArgument;
    if (state.callback) return static_cast<int>(0x80848112);
    state.callback = callback;
    state.context = context;
    return 0;
}

int APS5_VABI _ZN3sce4Json11Initializer27setGlobalNullAccessCallbackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(void* self, NullAccessCallback callback, void* context) {
    return _ZN3sce4Json11Initializer27setGlobalNullAccessCallBackEPFRKNS0_5ValueENS0_9ValueTypeEPS3_PvES7_(self, callback, context);
}

int APS5_VABI _ZN3sce4Json11Initializer28setGlobalTypeMismatchHandlerEPFRKNS0_5ValueENS0_9ValueTypeEPS3_S6_PvES7_(void* self, TypeMismatchHandler callback, void* context) {
    return SetGlobalHandler(self, &Handlers::typeMismatch, &Handlers::typeMismatchContext, callback, context);
}

int APS5_VABI _ZN3sce4Json11Initializer28setGlobalSpecialFloatHandlerEPFKNS0_5ValueENS0_12FunctionTypeEdPS3_PvPbES6_(void* self, SpecialFloatHandler callback, void* context) {
    return SetGlobalHandler(self, &Handlers::specialFloat, &Handlers::specialFloatContext, callback, context, true);
}

int APS5_VABI _ZN3sce4Json11Initializer36setGlobalElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKNS0_5ValueES9_PvESA_(void* self, ElementAccessFailureHandler callback, void* context) {
    return SetGlobalHandler(self, &Handlers::elementAccessFailure, &Handlers::elementAccessFailureContext, callback, context);
}

void APS5_VABI _ZN3sce4Json12MemAllocatorC2Ev(void* self) {
    *static_cast<void* const**>(self) = &JsonMemAllocatorVtable[2];
}

void APS5_VABI _ZN3sce4Json12MemAllocatorD2Ev(void* self) {
    *static_cast<void* const**>(self) = &JsonMemAllocatorVtable[2];
}

std::size_t _ZN3sce4Json6String4nposE = static_cast<std::size_t>(-1);

void APS5_VABI _ZN3sce4Json6StringC1Ev(String* self) { Construct(*self, std::string_view{}); }
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String* self, const char* text) { Construct(*self, text ? text : ""); }
void APS5_VABI _ZN3sce4Json6StringC1ERKS1_(String* self, const String* other) { Construct(*self, *other); }
void APS5_VABI _ZN3sce4Json6StringD1Ev(String* self) { Destroy(*self); }
String* APS5_VABI _ZN3sce4Json6StringaSERKS1_(String* self, const String* other) {
    if (self->text != other->text && other->text) AssignText(*self, View(*other), true);
    return self;
}
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String* self) { return Characters(*self); }
bool APS5_VABI _ZNK3sce4Json6String5emptyEv(const String* self) { return View(*self).empty(); }
std::size_t APS5_VABI _ZNK3sce4Json6String6lengthEv(const String* self) { return View(*self).size(); }
std::size_t APS5_VABI _ZNK3sce4Json6String4sizeEv(const String* self) { return View(*self).size(); }
std::size_t APS5_VABI _ZNK3sce4Json6String8capacityEv(const String* self) { return self->text ? self->text->capacity : 0; }
bool APS5_VABI _ZNK3sce4Json6StringeqEPKc(const String* self, const char* text) { return text != nullptr && CompareText(Characters(*self), text) == 0; }
bool APS5_VABI _ZNK3sce4Json6StringeqERKS1_(const String* self, const String* other) { return other != nullptr && CompareText(Characters(*self), Characters(*other)) == 0; }
void APS5_VABI _ZN3sce4Json6String5clearEv(String* self) { ClearText(*self); }
void APS5_VABI _ZN3sce4Json6String6resizeEm(String* self, std::size_t count) {
    if (!self->text || count == self->text->length) return;
    auto& text = *self->text;
    if (count < text.length) { text.characters[count] = 0; text.length = static_cast<std::uint32_t>(count); }
    else if (count < std::numeric_limits<std::uint32_t>::max() && Grow(text, count + 1)) {
        std::memset(text.characters + text.length, 0, count - text.length + 1);
        text.length = static_cast<std::uint32_t>(count);
    } else if (count >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("sce::Json::String: length exceeds 32 bits");
    }
}
void APS5_VABI _ZN3sce4Json6String7reserveEm(String* self, std::size_t capacity) { if (self->text) Grow(*self->text, capacity); }
String* APS5_VABI _ZN3sce4Json6String6appendERKS1_(String* self, const String* other) {
    AppendText(*self, Characters(*other));
    return self;
}
String* APS5_VABI _ZN3sce4Json6String6appendEPKc(String* self, const char* text) {
    if (!text) throw std::invalid_argument("sce::Json::String::append: null text");
    AppendText(*self, text);
    return self;
}
String* APS5_VABI _ZN3sce4Json6String6appendEPKcm(String* self, const char* text, std::size_t length) {
    if (!text && length) throw std::invalid_argument("sce::Json::String::append: null text");
    AppendText(*self, {text ? text : "", length});
    return self;
}
String* APS5_VABI _ZN3sce4Json6StringpLEPKc(String* self, const char* text) { return _ZN3sce4Json6String6appendEPKc(self, text); }
String* APS5_VABI _ZN3sce4Json6StringpLEh(String* self, unsigned char ch) {
    const char text[]{static_cast<char>(ch), 0};
    AppendText(*self, text);
    return self;
}
String* APS5_VABI _ZNK3sce4Json6String6substrEmm(String* result, const String* self, std::size_t position, std::size_t length) {
    if (position > View(*self).size()) throw std::out_of_range("sce::Json::String::substr: position past the end");
    Construct(*result, View(*self).substr(position, length));
    return result;
}
std::size_t APS5_VABI _ZNK3sce4Json6String4findEcm(const String* self, char character, std::size_t position) { return View(*self).find(character, position); }
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEcm(const String* self, char character, std::size_t position) { return View(*self).rfind(character, position); }
int APS5_VABI _ZNK3sce4Json6String7compareEPKc(const String* self, const char* text) {
    if (!text) throw std::invalid_argument("sce::Json::String::compare: null text");
    return CompareText(Characters(*self), text);
}
int APS5_VABI _ZNK3sce4Json6String7compareERKS1_(const String* self, const String* other) {
    if (!other) throw std::invalid_argument("sce::Json::String::compare: null other");
    return CompareText(Characters(*self), Characters(*other));
}

void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array* self) { Construct(*self); }
void APS5_VABI _ZN3sce4Json5ArrayC1ERKS1_(Array* self, const Array* other) { CopyArray(*self, *other); }
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array* self) { Destroy(*self); }
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array* self, const Value* value) {
    if (!self->items || !self->items->head) return;
    const auto failures = AllocationFailures();
    Value copy{};
    copy.parent = self->items->Owner();
    try {
        AssignNode(copy, *value);
        auto& inserted = self->items->emplace_back();
        inserted.parent = self->items->Owner();
        TransferPayload(inserted, copy);
    } catch (const GuestOutOfMemory&) {
    } catch (...) {
        Destroy(copy);
        throw;
    }
    Destroy(copy);
    if (AllocationFailures() != failures) self->items->PopBack();
}

ArrayIterator* APS5_VABI _ZNK3sce4Json5Array5beginEv(ArrayIterator* result, const Array* self) {
    result->state = static_cast<ArrayIteratorState*>(Allocate(sizeof(ArrayIteratorState)));
    if (result->state) result->state->position = self->items && self->items->head ? self->items->head->next : nullptr;
    if (self->items && (!self->items->head || !self->items->head->next)) {
        Release(result->state);
        result->state = nullptr;
    }
    return result;
}
ArrayIterator* APS5_VABI _ZNK3sce4Json5Array3endEv(ArrayIterator* result, const Array* self) {
    result->state = static_cast<ArrayIteratorState*>(Allocate(sizeof(ArrayIteratorState)));
    if (result->state) result->state->position = self->items ? self->items->head : nullptr;
    if (self->items && !self->items->head) {
        Release(result->state);
        result->state = nullptr;
    }
    return result;
}
bool APS5_VABI _ZNK3sce4Json5Array5emptyEv(const Array* self) { return !self->items || self->items->empty(); }
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array* self) { return self->items ? self->items->size() : 0; }
void APS5_VABI _ZN3sce4Json5Array5clearEv(Array* self) {
    if (!self->items) return;
    self->items->clear();
}
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array* self) {
    if (!self->items) return &Defaults(TypeNull);
    if (!self->items->head) NotImplemented_nid_no_patch(__func__);
    return &self->items->front();
}
const Value* APS5_VABI _ZNK3sce4Json5Array4backEv(const Array* self) {
    if (!self->items) return &Defaults(TypeNull);
    if (!self->items->head) return reinterpret_cast<const Value*>(16);
    return &self->items->back();
}
void APS5_VABI _ZN3sce4Json5Array9pop_frontEv(Array* self) {
    if (!self->items || !self->items->head) NotImplemented_nid_no_patch(__func__);
    if (self->items->empty()) throw std::out_of_range("sce::Json::Array::pop_front: empty array");
    auto* link = self->items->head->next;
    self->items->head->next = link->next;
    link->next->previous = self->items->head;
    Destroy(link->value);
    Release(link);
    --self->items->count;
}
void APS5_VABI _ZN3sce4Json5Array8iteratorD1Ev(ArrayIterator* self) { Release(self->state); self->state = nullptr; }
ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEv(ArrayIterator* self) {
    if (self->state && self->state->position) self->state->position = self->state->position->next;
    return self;
}
Value* APS5_VABI _ZNK3sce4Json5Array8iteratordeEv(const ArrayIterator* self) {
    if (!self->state) return const_cast<Value*>(&Defaults(TypeNull));
    return reinterpret_cast<Value*>(reinterpret_cast<std::uintptr_t>(self->state->position) + 16);
}
Value* APS5_VABI _ZNK3sce4Json5Array8iteratorptEv(const ArrayIterator* self) { return _ZNK3sce4Json5Array8iteratordeEv(self); }
bool APS5_VABI _ZNK3sce4Json5Array8iteratorneERKS2_(const ArrayIterator* self, const ArrayIterator* other) {
    return self->state && other->state && self->state->position != other->state->position;
}
void APS5_VABI _ZN3sce4Json5Array8iteratorC1ERKS2_(ArrayIterator* self, const ArrayIterator* other) {
    self->state = other->state ? static_cast<ArrayIteratorState*>(Allocate(sizeof(ArrayIteratorState))) : nullptr;
    if (self->state) *self->state = *other->state;
}
void APS5_VABI _ZN3sce4Json5Array8iterator7advanceEm(ArrayIterator* self, std::size_t count) {
    if (!self->state || !self->state->position || !count) return;
    auto* start = self->state->position;
    auto* node = start;
    std::size_t traversed = 0;
    do {
        node = node->next;
        ++traversed;
    } while (node && node != start && traversed < count);
    if (node == start) count %= traversed;
    else { self->state->position = node; return; }
    while (count--) node = node->next;
    self->state->position = node;
}

void APS5_VABI _ZN3sce4Json6ObjectC1Ev(Object* self) { Construct(*self); }
void APS5_VABI _ZN3sce4Json6ObjectC1ERKS1_(Object* self, const Object* other) { CopyObject(*self, *other); }
void APS5_VABI _ZN3sce4Json6ObjectD1Ev(Object* self) { Destroy(*self); }
Object* APS5_VABI _ZN3sce4Json6ObjectaSERKS1_(Object* self, const Object* other) {
    AssignObject(*self, *other);
    return self;
}
void APS5_VABI _ZN3sce4Json6Object5clearEv(Object* self) { if (self->items) self->items->clear(); }
Value* APS5_VABI _ZN3sce4Json6ObjectixERKNS0_6StringE(Object* self, const String* key) { return ObjectAccess(*self, *key); }
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object5beginEv(ObjectIterator* result, const Object* self) {
    result->state = static_cast<ObjectIteratorState*>(Allocate(sizeof(ObjectIteratorState)));
    if (result->state) result->state->position = self->items && self->items->head ? self->items->head->left : nullptr;
    if (self->items && (!self->items->head || !self->items->head->left)) { Release(result->state); result->state = nullptr; }
    return result;
}
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object3endEv(ObjectIterator* result, const Object* self) {
    result->state = static_cast<ObjectIteratorState*>(Allocate(sizeof(ObjectIteratorState)));
    if (result->state) result->state->position = self->items ? self->items->head : nullptr;
    if (self->items && !self->items->head) { Release(result->state); result->state = nullptr; }
    return result;
}
void APS5_VABI _ZN3sce4Json6Object8iteratorD1Ev(ObjectIterator* self) { Release(self->state); self->state = nullptr; }
ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEv(ObjectIterator* self) {
    if (self->state) self->state->position = NextObjectLink(self->state->position);
    return self;
}
Pair* APS5_VABI _ZNK3sce4Json6Object8iteratordeEv(const ObjectIterator* self) {
    if (self->state) return reinterpret_cast<Pair*>(reinterpret_cast<std::uintptr_t>(self->state->position) + 48);
    const auto* object = Defaults(TypeObject).object;
    if (!object || !object->items) NotImplemented_nid_no_patch(__func__);
    ObjectIterator fallback{};
    _ZNK3sce4Json6Object5beginEv(&fallback, object);
    if (!fallback.state) NotImplemented_nid_no_patch(__func__);
    auto* pair = reinterpret_cast<Pair*>(reinterpret_cast<std::uintptr_t>(fallback.state->position) + 48);
    Release(fallback.state);
    return pair;
}
Pair* APS5_VABI _ZNK3sce4Json6Object8iteratorptEv(const ObjectIterator* self) { return _ZNK3sce4Json6Object8iteratordeEv(self); }
ObjectIterator* APS5_VABI _ZNK3sce4Json6Object4findERKNS0_6StringE(ObjectIterator* result, const Object* self, const String* key) {
    result->state = static_cast<ObjectIteratorState*>(Allocate(sizeof(ObjectIteratorState)));
    if (result->state) result->state->position = self->items && key->text ? self->items->Find(Characters(*key)) : nullptr;
    if (self->items && key->text && !self->items->head) { Release(result->state); result->state = nullptr; }
    return result;
}
bool APS5_VABI _ZNK3sce4Json6Object8iteratoreqERKS2_(const ObjectIterator* self, const ObjectIterator* other) {
    return self->state && other->state && self->state->position == other->state->position;
}
bool APS5_VABI _ZNK3sce4Json6Object8iteratorneERKS2_(const ObjectIterator* self, const ObjectIterator* other) {
    return self->state && other->state && self->state->position != other->state->position;
}
void APS5_VABI _ZN3sce4Json6Object8iteratorC1ERKS2_(ObjectIterator* self, const ObjectIterator* other) {
    self->state = other->state ? static_cast<ObjectIteratorState*>(Allocate(sizeof(ObjectIteratorState))) : nullptr;
    if (self->state) *self->state = *other->state;
}
void APS5_VABI _ZN3sce4Json6Object8iterator7advanceEm(ObjectIterator* self, std::size_t count) {
    if (self->state) while (count--) self->state->position = NextObjectLink(self->state->position);
}
bool APS5_VABI _ZNK3sce4Json6Object5emptyEv(const Object* self) { return !self->items || self->items->empty(); }
std::size_t APS5_VABI _ZNK3sce4Json6Object4sizeEv(const Object* self) { return self->items ? self->items->size() : 0; }
void APS5_VABI _ZN3sce4Json6Object5eraseERKNS0_6StringE(Object* self, const String* key) {
    if (!self->items || !key->text) return;
    self->items->Erase(self->items->Find(Characters(*key)));
}

void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value* self) { Construct(*self); }
void APS5_VABI _ZN3sce4Json5ValueC1Eb(Value* self, bool value) { Construct(*self); self->type = TypeBoolean; self->boolean = value; }
void APS5_VABI _ZN3sce4Json5ValueC1El(Value* self, std::int64_t value) { Construct(*self); self->type = TypeInteger; self->integer = value; }
void APS5_VABI _ZN3sce4Json5ValueC1Em(Value* self, std::uint64_t value) { Construct(*self); self->type = TypeUInteger; self->uinteger = value; }
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value* self, double value) { Construct(*self); AssignReal(*self, value); }
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value* self, const char* text) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::Value: null text");
    Construct(*self);
    SetType(*self, TypeString);
    if (self->type == TypeString) AssignText(*self->string, text);
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6StringE(Value* self, const String* text) {
    Construct(*self);
    SetType(*self, TypeString);
    if (self->type == TypeString) AssignText(*self->string, View(*text), true);
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(Value* self, const Array* array) {
    Construct(*self);
    CopyArray(self->array, array);
    self->type = self->array ? TypeArray : TypeNull;
    BindChildren(*self);
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(Value* self, const Object* object) {
    Construct(*self);
    CopyObject(self->object, object);
    self->type = self->object ? TypeObject : TypeNull;
    BindChildren(*self);
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
int APS5_VABI _ZN3sce4Json5Value3setEb(Value* self, bool value) { SetType(NodeOf(*self), TypeBoolean); self->boolean = value; return 0; }
int APS5_VABI _ZN3sce4Json5Value3setEl(Value* self, std::int64_t value) { SetType(NodeOf(*self), TypeInteger); self->integer = value; return 0; }
int APS5_VABI _ZN3sce4Json5Value3setEm(Value* self, std::uint64_t value) { SetType(NodeOf(*self), TypeUInteger); self->uinteger = value; return 0; }
void APS5_VABI _ZN3sce4Json5Value3setEd(Value* self, double value) { AssignReal(NodeOf(*self), value); }
int APS5_VABI _ZN3sce4Json5Value3setEPKc(Value* self, const char* text) {
    if (text == nullptr) throw std::invalid_argument("sce::Json::Value::set: null text");
    const std::string copy(text);
    SetType(NodeOf(*self), TypeString);
    if (self->type == TypeString) AssignText(*self->string, copy);
    return 0;
}
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_6StringE(Value* self, const String* text) {
    const std::string copy(View(*text));
    SetType(NodeOf(*self), TypeString);
    if (self->type == TypeString) AssignText(*self->string, copy, true);
    return 0;
}
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_5ArrayE(Value* self, const Array* array) {
    Array* copy = nullptr;
    CopyArray(copy, array);
    Clear(NodeOf(*self));
    self->type = copy ? TypeArray : TypeNull;
    self->array = copy;
    BindChildren(*self);
    return 0;
}
int APS5_VABI _ZN3sce4Json5Value3setERKNS0_6ObjectE(Value* self, const Object* object) {
    Object* copy = nullptr;
    CopyObject(copy, object);
    Clear(NodeOf(*self));
    self->type = copy ? TypeObject : TypeNull;
    self->object = copy;
    BindChildren(*self);
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
const double* APS5_VABI _ZNK3sce4Json5Value7getRealEv(const Value* self) { return &Typed(self, TypeReal).real; }
const String* APS5_VABI _ZNK3sce4Json5Value9getStringEv(const Value* self) { return Typed(self, TypeString).string; }
const Array* APS5_VABI _ZNK3sce4Json5Value8getArrayEv(const Value* self) { return Typed(self, TypeArray).array; }
const Object* APS5_VABI _ZNK3sce4Json5Value9getObjectEv(const Value* self) { return Typed(self, TypeObject).object; }
std::size_t APS5_VABI _ZNK3sce4Json5Value5countEv(const Value* self) {
    const Node& n = NodeOf(*self);
    if (n.type == TypeArray) return n.array->items->size();
    if (n.type == TypeObject) return n.object->items->size();
    return 0;
}
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value* self, const char* key) {
    String text{};
    Construct(text, key ? key : "");
    const Value* result;
    try {
        if (self->type == TypeObject) {
            const auto* node = self->object->items ? self->object->items->Find(Characters(text)) : nullptr;
            if (node && !node->nil) { Destroy(text); return &node->pair.value; }
        }
        result = &MissingElement(self, static_cast<int>(self->type == TypeObject ? 0x80848115 : 0x80848117), &text, nullptr);
    } catch (...) {
        Destroy(text);
        throw;
    }
    Destroy(text);
    return result;
}
const Value* APS5_VABI _ZNK3sce4Json5ValueixEm(const Value* self, std::size_t index) {
    if (self->type == TypeArray && index < self->array->items->size()) {
        auto it = self->array->items->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(index));
        return &*it;
    }
    if (self->type == TypeObject && index < self->object->items->size()) {
        auto it = self->object->items->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(index));
        return &it->value;
    }
    const auto error = self->type == TypeArray || self->type == TypeObject ? 0x80848116 : 0x80848118;
    return &MissingElement(self, static_cast<int>(error), nullptr, &index);
}
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(Value* self, ValueType type) {
    Construct(*self);
    SetType(*self, type);
}
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueEm(const Value* self, std::size_t index) { return _ZNK3sce4Json5ValueixEm(self, index); }
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKNS0_6StringE(const Value* self, const String* key) {
    const Node& n = NodeOf(*self);
    if (key && n.type == TypeObject && n.object->items) {
        const auto* node = n.object->items->Find(Characters(*key));
        if (node && !node->nil) return &node->pair.value;
    }
    return &MissingElement(self, static_cast<int>(n.type == TypeObject ? 0x80848115 : 0x80848117), key, nullptr);
}
Value* APS5_VABI _ZN3sce4Json5Value10referValueEm(Value* self, std::size_t index) {
    if (self->type == TypeArray && index < self->array->items->size()) {
        auto it = self->array->items->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(index));
        return &*it;
    }
    if (self->type == TypeObject && index < self->object->items->size()) {
        auto it = self->object->items->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(index));
        return &it->value;
    }
    return nullptr;
}
Value* APS5_VABI _ZN3sce4Json5Value10referValueERKNS0_6StringE(Value* self, const String* key) {
    Node& n = NodeOf(*self);
    if (key == nullptr || n.type != TypeObject) return nullptr;
    if (!n.object->items) return nullptr;
    auto* node = n.object->items->Find(Characters(*key));
    return node && !node->nil ? &node->pair.value : nullptr;
}
Array* APS5_VABI _ZN3sce4Json5Value10referArrayEv(Value* self) {
    Node& n = NodeOf(*self);
    if (n.type == TypeNull) SetType(n, TypeArray);
    if (n.type != TypeArray) throw std::runtime_error("sce::Json::Value::referArray: value is not an array");
    return n.array;
}
Object* APS5_VABI _ZN3sce4Json5Value11referObjectEv(Value* self) {
    Node& n = NodeOf(*self);
    if (n.type == TypeNull) SetType(n, TypeObject);
    if (n.type != TypeObject) throw std::runtime_error("sce::Json::Value::referObject: value is not an object");
    return n.object;
}
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value* self, String* out) {
    std::string text;
    const int result = Serialize(text, *self);
    AppendText(*out, text);
    return result;
}
void APS5_VABI _ZNK3sce4Json5Value8toStringERNS0_6StringE(const Value* self, String* out) {
    std::string text;
    if (self->type == TypeString) AppendText(*out, Characters(*self->string));
    else {
        if (self->type == TypeArray) text = "array";
        else if (self->type == TypeObject) text = "object";
        else if (self->type == TypeReal) AppendReal(text, self->real);
        else Serialize(text, *self);
        AppendText(*out, text);
    }
}


int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(Value* out, ParserReadCallback read, void* context) {
    if (read == nullptr) throw std::invalid_argument("sce::Json::Parser::parse: null read callback");
    Parser parser(read, context);
    return ParseValue(out, parser);
}

int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value* out, const char* text, std::size_t size) {
    if (size == 0) return JsonErrorParse;
    if (text == nullptr) throw std::invalid_argument("sce::Json::Parser::parse: null text");
    Parser parser(text, size);
    return ParseValue(out, parser);
}


int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(Value* out, const char* path) {
    AllocationContext allocation;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        if (!state.initialized) return JsonErrorNotInitialized;
        allocation = state.allocation;
    }
    if (!allocation.fileBufferSize) return static_cast<int>(0x80848105);
    FileReader reader{allocation};
    reader.buffer = Allocate(allocation, allocation.fileBufferSize);
    if (reader.buffer) reader.descriptor = sceKernelOpen(path, 0, 0555);
    Parser parser(FileReader::Read, &reader);
    return ParseValue(out, parser);
}

struct InitParameterRtti {
    void* allocParam;
    void* userData;
    std::size_t fileBufferSize;
};

void APS5_VABI _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(InitParameterRtti* self, void* allocParam, void* userData, std::size_t fileBufferSize) {
    *self = {};
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.initialized) return;
    const auto& callbacks = *static_cast<const AllocParamRtti*>(allocParam);
    if (!callbacks.allocate || !callbacks.free) return;
    state.rtti = callbacks;
    self->allocParam = &rttiAllocator;
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
    if (!allocParam) return;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.initialized) return;
    const auto& callbacks = *static_cast<const AllocParamRtti*>(allocParam);
    if (!callbacks.allocate || !callbacks.free) return;
    state.rtti = callbacks;
    self->allocator = &rttiAllocator;
    self->userData = userData;
}

void APS5_VABI _ZN3sce4Json14InitParameter217setFileBufferSizeEm(InitParameter2* self, std::size_t size) {
    self->fileBufferSize = size;
}

void APS5_VABI _ZN3sce4Json14InitParameter225setSpecialFloatFormatTypeENS0_22SpecialFloatFormatTypeE(InitParameter2* self, std::int32_t type) {
    self->specialFloatFormatType = type;
}

int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void* self, const InitParameter2* parameter) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.initialized || *static_cast<const std::uint8_t*>(self)) return JsonErrorAlreadyInitialized;
    if (!parameter || !parameter->allocator || static_cast<std::uint32_t>(parameter->specialFloatFormatType) > 2)
        return JsonErrorInvalidArgument;
    ResetDefaults();
    state.allocation = {parameter->allocator, parameter->userData, parameter->fileBufferSize};
    state.floatFormat = parameter->specialFloatFormatType;
    state.floatFormatEnabled = true;
    state.initialized = true;
    *static_cast<std::uint8_t*>(self) = 1;
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

int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(Value* self, NullAccessCallback callback, void* context) {
    if (self->parent) return static_cast<int>(0x80848104);
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        if (!state.initialized) return JsonErrorNotInitialized;
    }
    if (!ModernValues() && !self->auxiliary) return static_cast<int>(0x80848104);
    const auto status = EnsureAuxiliary(*self);
    if (status) return status;
    self->auxiliary->nullAccess = callback;
    self->auxiliary->nullAccessContext = context;
    return 0;
}

int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(Value* self, ElementAccessFailureHandler handler, void* context) {
    return SetHandler(*self, &Auxiliary::elementAccessFailure, handler, context);
}

int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value* self, SpecialFloatHandler handler, void* context) {
    return SetHandler(*self, &Auxiliary::specialFloat, handler, context, true);
}

int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value* self, TypeMismatchHandler handler, void* context) {
    return SetHandler(*self, &Auxiliary::typeMismatch, handler, context);
}

bool APS5_VABI _ZNK3sce4Json5ValuecvbEv(const Value* self) {
    const Node& n = NodeOf(*self);
    if (n.type != TypeBoolean) {
        CallbackScope scope;
        const auto handlers = HandlersOf(n);
        if (scope.entered && handlers.typeMismatch) {
            const Value& replacement = handlers.typeMismatch(TypeBoolean, self, self->parent, handlers.typeMismatchContext);
            return Typed(&replacement, TypeBoolean).boolean;
        }
    }
    switch (n.type) {
        case TypeNull: return false;
        case TypeBoolean: return n.boolean;
        case TypeInteger: return n.integer != 0;
        case TypeUInteger: return n.uinteger != 0;
        case TypeReal: return n.real != 0.0;
        case TypeString: return !View(*n.string).empty();
        case TypeArray:
        case TypeObject: return true;
        default: throw std::runtime_error("sce::Json::Value: invalid value type");
    }
}

void APS5_VABI _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(Pair* self, const String* key, const Value* value) {
    Pair pair{};
    pair.reserved = 0;
    Construct(pair.key, *key);
    try {
        CopyInto(pair.value, *value);
    } catch (...) {
        Destroy(pair.key);
        throw;
    }
    *self = pair;
    BindChildren(self->value);
}

void APS5_VABI _ZN3sce4Json6Object4PairD1Ev(Pair* self) {
    Destroy(self->key);
    Destroy(self->value);
}

ObjectIterator* APS5_VABI _ZN3sce4Json6Object6insertERKNS1_4PairE(ObjectIterator* result, Object* self, const Pair* pair) {
    result->state = static_cast<ObjectIteratorState*>(Allocate(sizeof(ObjectIteratorState)));
    if (result->state) result->state->position = nullptr;
    if (!self->items) return result;
    if (!ModernValues()) {
        ObjectLink* node;
        {
            InlineText key; key.Assign(Characters(pair->key));
            TemporaryTreeEntry entry(key.text, *pair);
            bool inserted;
            node = self->items->Insert(entry.key.text, entry.pair, inserted);
        }
        if (!node) { Release(result->state); result->state = nullptr; }
        else if (result->state) result->state->position = node;
        return result;
    }
    ScopedValue value;
    value.value.parent = self->items->Owner();
    AssignNode(value.value, pair->value);
    const auto failures = AllocationFailures();
    ObjectLink* node;
    bool inserted;
    {
        InlineText key; key.Assign(Characters(pair->key));
        ScopedString copiedKey(pair->key);
        Pair source{}; source.key = copiedKey.string;
        TemporaryTreeEntry entry(key.text, source);
        node = self->items->Insert(entry.key.text, entry.pair, inserted);
    }
    if (inserted && AllocationFailures() == failures) {
        TransferPayload(node->pair.value, value.value);
        node->pair.value.parent = self->items->Owner();
        BindChildren(node->pair.value);
    }
    if (!node) { Release(result->state); result->state = nullptr; }
    else if (result->state) result->state->position = node;
    return result;
}

ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEi(ArrayIterator* self, int) {
    return _ZN3sce4Json5Array8iteratorppEv(self);
}

ArrayIterator* APS5_VABI _ZN3sce4Json5Array8iteratoraSERKS2_(ArrayIterator* self, const ArrayIterator* other) {
    if (self->state) {
        if (other->state && other->state->position) *self->state = *other->state;
        else { Release(self->state); self->state = nullptr; }
    }
    return self;
}

void APS5_VABI _ZN3sce4Json5Array8iteratorC1Ev(ArrayIterator* self) {
    self->state = static_cast<ArrayIteratorState*>(Allocate(sizeof(ArrayIteratorState)));
    if (self->state) self->state->position = nullptr;
}

ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEi(ObjectIterator* self, int) {
    return _ZN3sce4Json6Object8iteratorppEv(self);
}

ObjectIterator* APS5_VABI _ZN3sce4Json6Object8iteratoraSERKS2_(ObjectIterator* self, const ObjectIterator* other) {
    if (self->state) {
        if (other->state && other->state->position) *self->state = *other->state;
        else { Release(self->state); self->state = nullptr; }
    }
    return self;
}

void APS5_VABI _ZN3sce4Json6Object8iteratorC1Ev(ObjectIterator* self) {
    self->state = static_cast<ObjectIteratorState*>(Allocate(sizeof(ObjectIteratorState)));
    if (self->state) self->state->position = nullptr;
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
    BindChildren(self->value);
}
}
