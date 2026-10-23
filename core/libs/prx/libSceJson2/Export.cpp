#include <algorithm>
#include <bit>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <mutex>
#include <new>
#include <type_traits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"

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
struct LegacyHandlerMap;
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
    bool paddedStringGrowth = false;
    bool initialized = false;
    bool terminating = false;
    AllocParamRtti rtti{};
    Handlers handlers{};
    AllocationContext* allocation = nullptr;
    Value* fallback = nullptr;
    LegacyHandlerMap* legacyHandlers = nullptr;
    PthreadMutex* internalMutex = nullptr;
    PthreadMutex* recoveryMutex = nullptr;
    std::recursive_mutex callbackMutex;
    bool callbackActive = false;
    std::uint32_t allocationDepth = 0;
    bool allocationFailed = false;
};

Globals& State() {
    static Globals globals;
    return globals;
}

thread_local std::uint32_t allocatorCallbackDepth = 0;

struct AllocatorCallbackScope {
    AllocatorCallbackScope() { ++allocatorCallbackDepth; }
    ~AllocatorCallbackScope() { --allocatorCallbackDepth; }
    AllocatorCallbackScope(const AllocatorCallbackScope&) = delete;
    AllocatorCallbackScope& operator=(const AllocatorCallbackScope&) = delete;
};

struct TerminationScope {
    bool& active;
    explicit TerminationScope(bool& value) : active(value) { active = true; }
    ~TerminationScope() { active = false; }
    TerminationScope(const TerminationScope&) = delete;
    TerminationScope& operator=(const TerminationScope&) = delete;
};

struct KernelMutexLock {
    PthreadMutex* mutex;
    explicit KernelMutexLock(PthreadMutex* value) : mutex(value) {
        if (mutex && scePthreadMutexLock(mutex)) NotImplemented_nid_no_patch(__func__);
    }
    ~KernelMutexLock() { if (mutex) scePthreadMutexUnlock(mutex); }
    KernelMutexLock(const KernelMutexLock&) = delete;
    KernelMutexLock& operator=(const KernelMutexLock&) = delete;
};

class AllocationOperation {
public:
    AllocationOperation() {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        KernelMutexLock kernelLock(state.internalMutex);
        ++state.allocationDepth;
    }
    ~AllocationOperation() { if (active) Finish(); }
    AllocationOperation(const AllocationOperation&) = delete;
    AllocationOperation& operator=(const AllocationOperation&) = delete;
    static bool Failed() {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        KernelMutexLock kernelLock(state.internalMutex);
        return state.allocationFailed;
    }
    bool Finish() {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        KernelMutexLock kernelLock(state.internalMutex);
        const bool failed = state.allocationFailed;
        if (--state.allocationDepth == 0) state.allocationFailed = false;
        active = false;
        return failed;
    }
private:
    bool active = true;
};

struct CallbackScope {
    Globals& state = State();
    std::unique_lock<std::recursive_mutex> lock{state.callbackMutex};
    KernelMutexLock kernelLock{state.recoveryMutex};
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
    void* memory;
    {
        AllocatorCallbackScope scope;
        memory = allocate(context.allocator, size, context.userData);
    }
    if (!memory) {
        const auto notify = reinterpret_cast<void (APS5_VABI*)(void*, int, std::size_t, void*)>(table[4]);
        {
            AllocatorCallbackScope scope;
            notify(context.allocator, JsonErrorOutOfMemory, size, context.userData);
        }
        auto& state = State();
        std::lock_guard lock(state.mutex);
        KernelMutexLock kernelLock(state.internalMutex);
        if (state.allocationDepth) state.allocationFailed = true;
    }
    return memory;
}

void Release(const AllocationContext& context, void* memory) {
    auto* const* table = *static_cast<void* const* const*>(context.allocator);
    const auto free = reinterpret_cast<void (APS5_VABI*)(void*, void*, void*)>(table[3]);
    AllocatorCallbackScope scope;
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
    return state.allocation ? *state.allocation : AllocationContext{};
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

bool SearchEqual(const char* left, const char* right, std::size_t count) {
    const auto bounded = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(count));
    return bounded <= 0 || std::strncmp(left, right, static_cast<std::size_t>(bounded)) == 0;
}

std::size_t FindText(const String& string, const char* needle, std::size_t position, std::size_t count) {
    constexpr auto missing = std::numeric_limits<std::size_t>::max();
    if (!string.text) return missing;
    const std::size_t length = string.text->length;
    if (!count) return position <= length ? position : missing;
    if (position >= length || count > length - position) return missing;
    if (!needle) { NotImplemented_nid_no_patch(__func__); return missing; }
    const char* begin = string.text->characters;
    const char* cursor = begin + position;
    while (cursor < begin + length) {
        const char* candidate = std::strchr(cursor, static_cast<unsigned char>(*needle));
        if (!candidate) return missing;
        if (SearchEqual(candidate, needle, count)) return static_cast<std::size_t>(candidate - begin);
        cursor = candidate + 1;
    }
    return missing;
}

std::size_t ReverseFindText(const String& string, const char* needle, std::size_t position, std::size_t count) {
    constexpr auto missing = std::numeric_limits<std::size_t>::max();
    if (!string.text) return missing;
    const std::size_t length = string.text->length;
    if (!count) return std::min(position, length);
    if (count > length) return missing;
    if (!needle) { NotImplemented_nid_no_patch(__func__); return missing; }
    std::size_t index = std::min(position, length - count);
    for (;;) {
        const char* candidate = string.text->characters + index;
        if (*candidate == *needle && SearchEqual(candidate, needle, count)) return index;
        if (!index) return missing;
        --index;
    }
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

bool Grow(Text& text, std::size_t requested) {
    if (requested > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("sce::Json::String: capacity exceeds 32 bits");
    if (requested <= text.capacity) return true;
    std::uint32_t capacity = static_cast<std::uint32_t>(requested);
    bool padded;
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        padded = state.paddedStringGrowth;
    }
    if (padded) {
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

template<typename Node>
void RefreshTreeEndpoints(Node* head) {
    auto* first = head->parent;
    auto* last = first;
    if (!first->nil) {
        while (!first->left->nil) first = first->left;
        while (!last->right->nil) last = last->right;
    }
    head->left = first;
    head->right = last;
}

template<typename Node>
void RotateTreeLeft(Node* head, Node* node) {
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

template<typename Node>
void RotateTreeRight(Node* head, Node* node) {
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

template<typename Node>
void BalanceTreeInsertion(Node* head, Node* node) {
    while (node->parent->color == 1) {
        auto* parent = node->parent;
        auto* grandparent = parent->parent;
        if (parent == grandparent->left) {
            auto* uncle = grandparent->right;
            if (uncle->color == 1) { parent->color = 0; uncle->color = 0; grandparent->color = 1; node = grandparent; }
            else {
                if (node == parent->right) { node = parent; RotateTreeLeft(head, node); parent = node->parent; grandparent = parent->parent; }
                parent->color = 0; grandparent->color = 1; RotateTreeRight(head, grandparent);
            }
        } else {
            auto* uncle = grandparent->left;
            if (uncle->color == 1) { parent->color = 0; uncle->color = 0; grandparent->color = 1; node = grandparent; }
            else {
                if (node == parent->left) { node = parent; RotateTreeRight(head, node); parent = node->parent; grandparent = parent->parent; }
                parent->color = 0; grandparent->color = 1; RotateTreeLeft(head, grandparent);
            }
        }
    }
    head->parent->color = 0;
}

template<typename Node>
void ReplaceTreeNode(Node* head, Node* node, Node* replacement) {
    if (node->parent->nil) head->parent = replacement;
    else if (node == node->parent->left) node->parent->left = replacement;
    else node->parent->right = replacement;
    if (!replacement->nil) replacement->parent = node->parent;
}

template<typename Node>
void BalanceTreeErasure(Node* head, Node* node, Node* parent) {
    while (node != head->parent && node->color == 0) {
        if (node == parent->left) {
            auto* sibling = parent->right;
            if (sibling->color == 1) { sibling->color = 0; parent->color = 1; RotateTreeLeft(head, parent); sibling = parent->right; }
            if (sibling->left->color == 0 && sibling->right->color == 0) {
                if (!sibling->nil) sibling->color = 1;
                node = parent; parent = node->parent;
            } else {
                if (sibling->right->color == 0) { sibling->left->color = 0; sibling->color = 1; RotateTreeRight(head, sibling); sibling = parent->right; }
                sibling->color = parent->color; parent->color = 0; sibling->right->color = 0;
                RotateTreeLeft(head, parent); node = head->parent;
            }
        } else {
            auto* sibling = parent->left;
            if (sibling->color == 1) { sibling->color = 0; parent->color = 1; RotateTreeRight(head, parent); sibling = parent->left; }
            if (sibling->right->color == 0 && sibling->left->color == 0) {
                if (!sibling->nil) sibling->color = 1;
                node = parent; parent = node->parent;
            } else {
                if (sibling->left->color == 0) { sibling->right->color = 0; sibling->color = 1; RotateTreeLeft(head, sibling); sibling = parent->left; }
                sibling->color = parent->color; parent->color = 0; sibling->left->color = 0;
                RotateTreeRight(head, parent); node = head->parent;
            }
        }
    }
    node->color = 0;
}

template<typename Node>
void UnlinkTreeNode(Node* head, Node* removed) {
    if (!removed || removed->nil) return;
    auto* successor = removed;
    auto color = successor->color;
    Node* child;
    Node* parent;
    if (removed->left->nil) { child = removed->right; parent = removed->parent; ReplaceTreeNode(head, removed, child); }
    else if (removed->right->nil) { child = removed->left; parent = removed->parent; ReplaceTreeNode(head, removed, child); }
    else {
        successor = removed->right;
        while (!successor->left->nil) successor = successor->left;
        color = successor->color;
        child = successor->right;
        if (successor->parent == removed) parent = successor;
        else {
            parent = successor->parent;
            ReplaceTreeNode(head, successor, child);
            successor->right = removed->right;
            successor->right->parent = successor;
        }
        ReplaceTreeNode(head, removed, successor);
        successor->left = removed->left;
        successor->left->parent = successor;
        successor->color = removed->color;
    }
    if (color == 0) BalanceTreeErasure(head, child, parent);
    RefreshTreeEndpoints(head);
}

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
    void RefreshEndpoints() { RefreshTreeEndpoints(head); }

    void BalanceInsertion(ObjectLink* node) { BalanceTreeInsertion(head, node); }
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

    void Erase(ObjectLink* removed) {
        if (!removed || removed->nil) return;
        UnlinkTreeNode(head, removed);
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

extern "C" void* APS5_VABI _Znwm_nid_postfix(std::size_t bytes);
extern "C" void APS5_VABI _ZdlPv_nid_postfix(void* memory);

struct LegacyHandlerNode {
    LegacyHandlerNode* parent;
    LegacyHandlerNode* left;
    LegacyHandlerNode* right;
    const Value* key;
    HandlerRecord<TypeMismatchHandler> record;
    std::uint8_t color;
    std::uint8_t nil;
    std::uint8_t reserved[6];
};
static_assert(sizeof(LegacyHandlerNode) == 56 && offsetof(LegacyHandlerNode, record) == 32 && offsetof(LegacyHandlerNode, nil) == 49);

struct LegacyHandlerMap {
    LegacyHandlerNode* head;
    std::size_t count;
    std::uint64_t reserved;
    LegacyHandlerNode* Find(const Value* key) const {
        auto* node = head->parent;
        while (!node->nil) {
            const auto requested = reinterpret_cast<std::uintptr_t>(key);
            const auto stored = reinterpret_cast<std::uintptr_t>(node->key);
            if (requested == stored) return node;
            node = requested < stored ? node->left : node->right;
        }
        return head;
    }
    void RefreshEndpoints() { RefreshTreeEndpoints(head); }

    void BalanceInsertion(LegacyHandlerNode* node) { BalanceTreeInsertion(head, node); }
    LegacyHandlerNode* Insert(const Value* key) {
        auto* parent = head;
        auto* position = head->parent;
        bool left = false;
        while (!position->nil) {
            parent = position;
            const auto requested = reinterpret_cast<std::uintptr_t>(key);
            const auto stored = reinterpret_cast<std::uintptr_t>(position->key);
            if (requested == stored) return position;
            left = requested < stored;
            position = left ? position->left : position->right;
        }
        auto* node = static_cast<LegacyHandlerNode*>(_Znwm_nid_postfix(sizeof(LegacyHandlerNode)));
        node->parent = parent; node->left = head; node->right = head;
        node->key = key; node->record = {};
        node->color = 1; node->nil = 0;
        if (parent->nil) head->parent = node;
        else if (left) parent->left = node;
        else parent->right = node;
        ++count;
        BalanceInsertion(node);
        RefreshEndpoints();
        return node;
    }

    void Erase(LegacyHandlerNode* removed) {
        if (!removed || removed->nil) return;
        UnlinkTreeNode(head, removed);
        _ZdlPv_nid_postfix(removed);
        --count;
    }
    void ReleaseTree(LegacyHandlerNode* node) {
        if (node->nil) return;
        ReleaseTree(node->left); ReleaseTree(node->right);
        _ZdlPv_nid_postfix(node);
    }
};
static_assert(sizeof(LegacyHandlerMap) == 24);

HandlerRecord<TypeMismatchHandler> LegacyHandlerOf(const Value& value) {
    auto& state = State();
    KernelMutexLock lock(state.recoveryMutex);
    if (!state.legacyHandlers) return {};
    const auto* node = state.legacyHandlers->Find(&value);
    return node->nil ? HandlerRecord<TypeMismatchHandler>{} : node->record;
}

int RegisterLegacyHandler(Value& value, TypeMismatchHandler callback, void* context) {
    auto& state = State();
    KernelMutexLock lock(state.recoveryMutex);
    if (!state.legacyHandlers) {
        auto* map = static_cast<LegacyHandlerMap*>(Allocate(sizeof(LegacyHandlerMap)));
        if (!map) return JsonErrorOutOfMemory;
        map->count = 0;
        try { map->head = static_cast<LegacyHandlerNode*>(_Znwm_nid_postfix(sizeof(LegacyHandlerNode))); }
        catch (const std::bad_alloc&) { Release(map); NotImplemented_nid_no_patch(__func__); }
        map->head->parent = map->head; map->head->left = map->head; map->head->right = map->head;
        map->head->color = 0; map->head->nil = 1;
        state.legacyHandlers = map;
    }
    AllocationOperation operation;
    auto* node = state.legacyHandlers->Insert(&value);
    if (operation.Finish()) return JsonErrorOutOfMemory;
    node->record = {callback, context};
    return 0;
}

void EraseLegacyHandler(const Value& value) {
    auto& state = State();
    KernelMutexLock lock(state.recoveryMutex);
    if (state.legacyHandlers) state.legacyHandlers->Erase(state.legacyHandlers->Find(&value));
}

void CopyLegacyHandler(Value& destination, const Value& source) {
    auto& state = State();
    KernelMutexLock lock(state.recoveryMutex);
    if (!state.legacyHandlers) return;
    const auto* sourceNode = state.legacyHandlers->Find(&source);
    if (!sourceNode->nil) {
        const auto record = sourceNode->record;
        RegisterLegacyHandler(destination, record.callback, record.context);
    }
}

void ReleaseLegacyHandlers() {
    auto& state = State();
    auto* map = state.legacyHandlers;
    if (!map) return;
    map->ReleaseTree(map->head->parent);
    _ZdlPv_nid_postfix(map->head);
    map->head = reinterpret_cast<LegacyHandlerNode*>(std::uintptr_t{0xffffffff});
    map->count = std::numeric_limits<std::size_t>::max();
    Release(map);
    state.legacyHandlers = nullptr;
}

void Construct(Object& object, bool releaseMissingHeader = false) {
    object.items = static_cast<ObjectStorage*>(Allocate(ModernValues() ? 32 : 24));
    if (!object.items) { if (releaseMissingHeader) Release(nullptr); return; }
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
    if (a.items->head) Release(a.items->head);
    a.items->head = reinterpret_cast<ArrayLink*>(std::uintptr_t{0xffffffff});
    a.items->count = std::numeric_limits<std::size_t>::max();
    Release(a.items);
    a.items = nullptr;
}
void Destroy(Object& object) {
    if (!object.items) return;
    object.items->clear();
    if (object.items->head) Release(object.items->head);
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
    if (!ModernValues()) {
        const auto record = LegacyHandlerOf(RootOf(n));
        handlers.typeMismatch = record.callback;
        handlers.typeMismatchContext = record.context;
        return handlers;
    }
    if (!auxiliary) return handlers;
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

void ReleaseValueAuxiliary(Value& v) {
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

void Destroy(Value& v) {
    Clear(v);
    ReleaseValueAuxiliary(v);
    if (!ModernValues()) EraseLegacyHandler(v);
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

template<typename TLink, typename TNext, typename TValue>
void TransferChildren(Value& destination, TLink* first, TLink* head, TNext next, TValue value) {
    auto* begin = static_cast<TLink**>(Allocate(sizeof(TLink*)));
    if (begin) *begin = first;
    for (;;) {
        auto* end = static_cast<TLink**>(Allocate(sizeof(TLink*)));
        if (end) *end = head;
        const bool more = begin && end && *begin != *end;
        Release(end);
        if (!more) break;
        if (!*begin) { Release(begin); NotImplemented_nid_no_patch(__func__); }
        value(*begin).parent = &destination;
        *begin = next(*begin);
    }
    Release(begin);
}

void TransferPayload(Node& destination, Node& source) {
    if (destination.auxiliary || source.auxiliary) return;
    Clear(destination);
    if (source.type <= TypeReal) AssignNode(destination, source);
    else if (source.type <= TypeObject) {
        destination.payload = source.payload;
        source.payload = 0;
        if (source.type == TypeArray && destination.array) {
            auto* items = destination.array->items;
            auto* head = items ? items->head : nullptr;
            TransferChildren(destination, head ? head->next : nullptr, head,
                [](ArrayLink* link) { return link->next; }, [](ArrayLink* link) -> Value& { return link->value; });
        } else if (source.type == TypeObject && destination.object) {
            auto* items = destination.object->items;
            auto* head = items ? items->head : nullptr;
            TransferChildren(destination, head ? head->left : nullptr, head,
                [](ObjectLink* link) { return NextObjectLink(link); }, [](ObjectLink* link) -> Value& { return link->pair.value; });
        }
    } else return;
    destination.parent = source.parent;
    source.parent = nullptr;
    destination.type = source.type;
    Clear(source);
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

void CopyArrayStorage(ArrayStorage& destination, const ArrayStorage& source) {
    ArrayStorage pending{NewArrayHead(), 0, 0};
    try {
        if (pending.head) {
            for (const auto& item : source) {
                try { CopyInto(pending.emplace_back(), item); }
                catch (const GuestOutOfMemory&) { break; }
            }
        }
    } catch (...) { pending.clear(); if (pending.head) Release(pending.head); throw; }
    destination.clear();
    if (destination.head) Release(destination.head);
    destination.head = pending.head;
    destination.count = pending.count;
}

void CopyArray(Array& destination, const Array& source) {
    Array pending{};
    if (!source.items) { destination = pending; return; }
    Construct(pending);
    if (!pending.items) { destination = pending; return; }
    try { CopyArrayStorage(*pending.items, *source.items); }
    catch (...) { Destroy(pending); throw; }
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

void AssignReal(Node& n, double value);

void SetType(Node& n, ValueType type);
void AssignArray(Array& destination, const Array& source);
void AssignObject(Object& destination, const Object& source);

void AssignPayload(Node& destination, const Node& source) {
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
    if (source.type == TypeArray) {
        const auto* array = source.array;
        if (!array) NotImplemented_nid_no_patch(__func__);
        if (!array->items) return;
        SetType(destination, TypeArray);
        if (destination.array) AssignArray(*destination.array, *array);
        return;
    }
    if (source.type == TypeObject) {
        const auto* object = source.object;
        if (!object) NotImplemented_nid_no_patch(__func__);
        if (!object->items) return;
        SetType(destination, TypeObject);
        if (destination.object) AssignObject(*destination.object, *object);
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void AssignNode(Node& destination, const Node& source) {
    AssignPayload(destination, source);
    if (!ModernValues()) CopyLegacyHandler(destination, source);
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
    if (type == TypeNull && state.fallback) return *state.fallback;
    auto& defaults = SharedDefaults();
    if (!defaults.ready[type]) {
        Construct(defaults.values[type]);
        SetType(defaults.values[type], type);
        defaults.ready[type] = true;
    }
    return defaults.values[type];
}

void ResetDefaults(bool releaseMissing = false) {
    auto& defaults = SharedDefaults();
    for (unsigned type = 0; type < 8; ++type) {
        if (defaults.ready[type]) {
            const bool missing = type >= TypeString && !defaults.values[type].payload;
            Destroy(defaults.values[type]);
            if (releaseMissing && missing) Release(nullptr);
        }
        defaults.values[type] = {};
        defaults.ready[type] = false;
    }
}

PthreadMutex* CreateKernelMutex() {
    auto* mutex = static_cast<PthreadMutex*>(Allocate(sizeof(PthreadMutex)));
    if (!mutex) return nullptr;
    *mutex = nullptr;
    PthreadMutexattr attribute = nullptr;
    try {
        if (scePthreadMutexattrInit(&attribute)) NotImplemented_nid_no_patch(__func__);
        const auto typeStatus = scePthreadMutexattrSettype(&attribute, 2);
        const auto status = typeStatus ? typeStatus : scePthreadMutexInit(mutex, &attribute, "SceJson_internal");
        scePthreadMutexattrDestroy(&attribute);
        if (status) NotImplemented_nid_no_patch(__func__);
        return mutex;
    } catch (...) {
        if (attribute) scePthreadMutexattrDestroy(&attribute);
        if (*mutex) scePthreadMutexDestroy(mutex);
        Release(mutex);
        throw;
    }
}

void ReleaseInitialization() {
    auto& state = State();
    if (!ModernValues() && !state.legacyHandlers) Release(nullptr);
    ReleaseLegacyHandlers();
    if (state.fallback) Destroy(*state.fallback);
    Release(state.fallback);
    state.fallback = nullptr;
    if (ModernValues()) {
        if (state.recoveryMutex) scePthreadMutexDestroy(state.recoveryMutex);
        Release(state.recoveryMutex);
    }
    state.recoveryMutex = nullptr;
    if (state.internalMutex) scePthreadMutexDestroy(state.internalMutex);
    Release(state.internalMutex);
    state.internalMutex = nullptr;
    ResetDefaults(true);
    if (state.allocation) {
        auto* allocation = state.allocation;
        const auto context = *allocation;
        Release(context, allocation);
        state.allocation = nullptr;
    }
    state.floatFormat = 2;
    state.floatFormatEnabled = false;
    state.paddedStringGrowth = false;
    state.initialized = false;
}

template<typename T>
void CreateDefault(ValueType type) {
    auto& defaults = SharedDefaults();
    auto& value = defaults.values[type];
    Construct(value);
    auto* handle = static_cast<T*>(Allocate(sizeof(T)));
    value.payload = reinterpret_cast<std::uintptr_t>(handle);
    value.type = type;
    defaults.ready[type] = true;
    if (handle) {
        *handle = {};
        if constexpr (std::is_same_v<T, String>) Construct(*handle, std::string_view{});
        else Construct(*handle);
    }
}

int InitializeResources(void* self, const void* parameter, std::int32_t format, bool modern) {
    auto& state = State();
    AllocationContext context;
    std::memcpy(&context, parameter, offsetof(AllocationContext, fileBufferSize));
    auto* const* table = *static_cast<void* const* const*>(context.allocator);
    const auto allocate = reinterpret_cast<void* (APS5_VABI*)(void*, std::size_t, void*)>(table[2]);
    auto* allocation = static_cast<AllocationContext*>(allocate(context.allocator, sizeof(AllocationContext), context.userData));
    if (!allocation) return JsonErrorOutOfMemory;
    std::memcpy(&context.fileBufferSize, static_cast<const std::byte*>(parameter) + offsetof(AllocationContext, fileBufferSize), sizeof(context.fileBufferSize));
    *allocation = context;
    state.allocation = allocation;
    state.floatFormat = format;
    state.floatFormatEnabled = modern;
    try {
        CreateDefault<String>(TypeString);
        CreateDefault<Array>(TypeArray);
        CreateDefault<Object>(TypeObject);
        state.fallback = static_cast<Value*>(Allocate(sizeof(Value)));
        if (state.fallback) Construct(*state.fallback);
        state.internalMutex = CreateKernelMutex();
        const auto& defaults = SharedDefaults();
        const bool admitted = defaults.values[TypeString].string && defaults.values[TypeArray].array &&
            defaults.values[TypeObject].object && state.fallback && state.internalMutex;
        if (admitted) state.recoveryMutex = ModernValues() ? CreateKernelMutex() : state.internalMutex;
        if (!admitted || !state.recoveryMutex) { ReleaseInitialization(); return JsonErrorOutOfMemory; }
    } catch (...) { ReleaseInitialization(); throw; }
    state.paddedStringGrowth = CompiledSdk() > 0x044fffff;
    state.initialized = true;
    *static_cast<std::uint8_t*>(self) = 1;
    return 0;
}

const Node& Typed(const Value* self, ValueType requested) {
    if (self->type == requested ||
            ((requested == TypeInteger || requested == TypeUInteger) &&
             (self->type == TypeInteger || self->type == TypeUInteger))) return *self;
    if (!ModernValues() && self->type == TypeNull) {
        const auto* auxiliary = RootOf(*self).auxiliary;
        if (auxiliary && auxiliary->nullAccess)
            return Typed(&auxiliary->nullAccess(requested, self->parent, auxiliary->nullAccessContext), requested);
    }
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
    if (!ModernValues()) {
        const auto record = LegacyHandlerOf(RootOf(*self));
        if (record.callback) return Typed(&record.callback(requested, self, self->parent, record.context), requested);
        return Defaults(requested);
    }
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

const Value* LookupKey(const Value* self, const String* key) {
    if (!key) { NotImplemented_nid_no_patch(__func__); return nullptr; }
    auto moduleFallback = [] {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        return state.fallback;
    };
    if (!key->text) return moduleFallback();
    struct IteratorState {
        ObjectIteratorState* state = static_cast<ObjectIteratorState*>(Allocate(sizeof(ObjectIteratorState)));
        IteratorState() { if (state) state->position = nullptr; }
        ~IteratorState() { Release(state); }
    };
    if (self->type == TypeObject) {
        IteratorState found;
        if (!self->object) { NotImplemented_nid_no_patch(__func__); return nullptr; }
        auto* items = self->object->items;
        if (found.state) found.state->position = items ? items->Find(Characters(*key)) : nullptr;
        if (items && !items->head) { Release(found.state); found.state = nullptr; }
        bool present;
        {
            IteratorState end;
            if (end.state) end.state->position = items ? items->head : nullptr;
            if (items && !items->head) { Release(end.state); end.state = nullptr; }
            present = found.state && end.state && found.state->position != end.state->position;
        }
        if (present) return &found.state->position->pair.value;
        if (ModernValues()) {
            const auto* result = &MissingElement(self, static_cast<int>(0x80848115), key, nullptr);
            if (moduleFallback() || RootOf(*self).auxiliary) return result;
        }
    } else if (ModernValues()) {
        const auto* result = &MissingElement(self, static_cast<int>(0x80848117), key, nullptr);
        if (moduleFallback() || RootOf(*self).auxiliary) return result;
    }
    const auto* auxiliary = RootOf(*self).auxiliary;
    return auxiliary && auxiliary->fallback ? auxiliary->fallback : moduleFallback();
}

template <typename TCallback>
int SetHandler(Node& n, HandlerRecord<TCallback>* Auxiliary::* member, TCallback callback, void* context, bool modernOnly = false) {
    if (n.parent) return static_cast<int>(0x80848104);
    const bool legacyMismatch = !ModernValues() && std::is_same_v<TCallback, TypeMismatchHandler>;
    if (legacyMismatch && !n.auxiliary) return static_cast<int>(0x80848104);
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        if (!state.allocation) return JsonErrorNotInitialized;
        if (modernOnly && !state.floatFormatEnabled) return static_cast<int>(0x80848113);
    }
    if (!ModernValues()) {
        if constexpr (std::is_same_v<TCallback, TypeMismatchHandler>) return RegisterLegacyHandler(n, callback, context);
        else NotImplemented_nid_no_patch(__func__);
    }
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

void AssignArray(Array& destination, const Array& source) {
    if (!destination.items || !source.items) return;
    if (!ModernValues()) {
        CopyArrayStorage(*destination.items, *source.items);
        return;
    }
    Array pending{};
    Construct(pending);
    if (!pending.items) { Release(nullptr); return; }
    if (!pending.items->head) { Destroy(pending); return; }
    pending.items->SetOwner(destination.items->Owner());
    try {
        for (const auto& item : *source.items) {
            ScopedValue placeholder;
            SetType(placeholder.value, item.type);
            AllocationOperation operation;
            try { CopyInto(pending.items->emplace_back(), placeholder.value); }
            catch (const GuestOutOfMemory&) {}
            if (!operation.Finish()) {
                auto& value = pending.items->back();
                value.parent = destination.items->Owner();
                AssignNode(value, item);
            }
        }
    } catch (...) { Destroy(pending); throw; }
    auto previous = destination;
    destination = pending;
    Destroy(previous);
}

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
    Construct(pending, true);
    if (!pending.items) return;
    pending.items->SetOwner(destination.items->Owner());
    try {
        for (auto position = source.items->begin(), end = source.items->end(); position != end; ++position) {
            const auto& pair = *position;
            ScopedValue placeholder;
            SetType(placeholder.value, pair.value.type);
            ScopedPair copied;
            {
                ScopedString key{std::string_view{Characters(pair.key)}};
                Construct(copied.pair.key, key.string);
                CopyInto(copied.pair.value, placeholder.value);
            }
            AllocationOperation operation;
            ObjectLink* node;
            bool inserted;
            {
                InlineText key; key.Assign(position.node->key.characters);
                TemporaryTreeEntry entry(key.text, copied.pair);
                node = pending.items->Insert(entry.key.text, entry.pair, inserted);
            }
            const bool failed = operation.Finish();
            if (inserted && !failed) {
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
    AllocationOperation operation;
    ObjectLink* node;
    {
        InlineText key; key.Assign(Characters(input));
        ScopedString defaultKey{std::string_view{}};
        Pair source{}; source.key = defaultKey.string;
        TemporaryTreeEntry entry(key.text, source);
        bool inserted;
        node = object.items->Insert(entry.key.text, entry.pair, inserted);
    }
    if (operation.Finish()) return const_cast<Value*>(&Defaults(TypeNull));
    if (!node || node->nil) NotImplemented_nid_no_patch(__func__);
    AssignText(node->pair.key, View(input), true);
    node->pair.value.parent = object.items->Owner();
    return &node->pair.value;
}

void AppendArray(Array* self, const Value* value) {
    if (!self->items || !self->items->head) return;
    AllocationOperation operation;
    if (!ModernValues()) {
        try { CopyInto(self->items->emplace_back(), *value); }
        catch (const GuestOutOfMemory&) {}
        if (operation.Finish()) self->items->PopBack();
        return;
    }
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
    if (operation.Finish()) self->items->PopBack();
}

ArrayLink* InsertArrayLink(ArrayStorage& items, ArrayLink* position, const Value& value) {
    auto* link = static_cast<ArrayLink*>(Allocate(sizeof(ArrayLink)));
    if (!link) return items.head;
    try { CopyInto(link->value, value); }
    catch (...) { Release(link); throw; }
    link->previous = position->previous;
    link->next = position;
    position->previous->next = link;
    position->previous = link;
    ++items.count;
    return link;
}

void PrependArray(Array* self, const Value* value) {
    if (!self->items) return;
    AllocationOperation operation;
    if (!ModernValues()) {
        if (self->items->head) InsertArrayLink(*self->items, self->items->head->next, *value);
    } else {
        ScopedValue copy;
        copy.value.parent = self->items->Owner();
        AssignNode(copy.value, *value);
        Value empty{};
        if (self->items->head) InsertArrayLink(*self->items, self->items->head->next, empty);
        if (self->items->count) TransferPayload(self->items->head->next->value, copy.value);
    }
    if (operation.Finish() && self->items->head && self->items->count) {
        auto* link = self->items->head->next;
        link->next->previous = self->items->head;
        self->items->head->next = link->next;
        Destroy(link->value);
        Release(link);
        --self->items->count;
    }
}

using SerializationCallback = int (APS5_VABI*)(String&, void*);

template<typename TLink>
struct SerializationIterator {
    TLink** state;
    SerializationIterator(TLink* position, bool invalid) : state(static_cast<TLink**>(Allocate(sizeof(TLink*)))) {
        if (state) *state = position;
        if (invalid) { Release(state); state = nullptr; }
    }
    ~SerializationIterator() { Release(state); }
    bool Different(TLink* position, bool invalid) const {
        SerializationIterator other(position, invalid);
        return state && other.state && *state != *other.state;
    }
};

template<typename TLink>
struct SerializationPosition { TLink* first; TLink* head; bool invalid; };

void PrepareValueAdmission(Value& value, const Value* parent) {
    value.parent = const_cast<Value*>(parent);
    if (parent) ReleaseValueAuxiliary(value);
    else {
        EnsureAuxiliary(value);
        if (!ModernValues() && value.auxiliary) { value.auxiliary->nullAccess = nullptr; value.auxiliary->nullAccessContext = nullptr; }
    }
}

void AppendLegacyReal(String& output, double value) {
    float magnitude = static_cast<float>(value);
    const bool negative = magnitude < 0;
    if (negative) magnitude = -magnitude;
    const bool unordered = std::isnan(magnitude);
    const std::uint64_t integer = unordered ? UINT64_C(0x8000000000000000) :
        magnitude <= 2147483648.0f ? static_cast<std::uint64_t>(magnitude) : UINT64_C(2147483647);
    float fraction = unordered || magnitude > 2147483648.0f ? 0.0f : magnitude - static_cast<float>(integer);
    char digits[6];
    bool nonzero = false;
    for (char& digit : digits) {
        fraction *= 10.0f;
        const auto number = static_cast<std::uint32_t>(fraction) % 10;
        digit = static_cast<char>('0' + number);
        nonzero = nonzero || number != 0;
    }
    char text[32];
    char* cursor = text;
    if (negative) *cursor++ = '-';
    char whole[20];
    unsigned length = 0;
    auto remaining = integer;
    do { whole[length++] = static_cast<char>('0' + remaining % 10); remaining /= 10; } while (remaining);
    while (length) *cursor++ = whole[--length];
    if (nonzero) { *cursor++ = '.'; std::memcpy(cursor, digits, sizeof(digits)); cursor += sizeof(digits); }
    AppendText(output, std::string_view(text, cursor - text));
}

class CallbackSerializer {
public:
    CallbackSerializer(String& buffer, String* destination, SerializationCallback callback, void* context, bool collect)
        : buffer(buffer), destination(destination), callback(callback), context(context), collect(collect) {}
    int SerializeValue(const Value& value, const Value* parent = nullptr) {
        if (!ModernValues()) PrepareValueAdmission(const_cast<Value&>(value), parent);
        if (value.type == TypeReal && !std::isfinite(value.real) && FloatConfiguration().enabled) {
            CallbackScope scope;
            const auto handlers = HandlersOf(value);
            if (scope.entered && handlers.specialFloat) {
                ScopedValue replacement;
                bool error = false;
                RecoverSpecial(value, handlers, 0, value.real, replacement.value, &error);
                return error ? JsonErrorSpecialFloat : SerializeValue(replacement.value, parent);
            }
        }
        if (value.type == TypeReal && !FloatConfiguration().enabled) { AppendLegacyReal(buffer, value.real); return Emit(); }
        if (value.type >= TypeNull && value.type <= TypeReal) {
            std::string scalar;
            const int status = Serialize(scalar, value);
            if (status < 0) return status;
            AppendText(buffer, scalar);
            return Emit();
        }
        if (value.type == TypeString) {
            if (!value.string) { NotImplemented_nid_no_patch(__func__); return 0; }
            Escape(*value.string);
            return Emit();
        }
        if (value.type == TypeArray) {
            if (!value.array) { NotImplemented_nid_no_patch(__func__); return 0; }
            auto position = [&value] {
                if (value.type != TypeArray || !value.array) NotImplemented_nid_no_patch(__func__);
                auto* items = value.array->items;
                auto* head = items ? items->head : nullptr;
                return SerializationPosition<ArrayLink>{head ? head->next : nullptr, head, items && !head};
            };
            return Collection(position, '[', ']',
                [](ArrayLink* link) { return link->next; },
                [this, &value](ArrayLink* link) { return SerializeValue(link->value, &value); });
        }
        if (value.type == TypeObject) {
            if (!value.object) { NotImplemented_nid_no_patch(__func__); return 0; }
            auto position = [&value] {
                if (value.type != TypeObject || !value.object) NotImplemented_nid_no_patch(__func__);
                auto* items = value.object->items;
                auto* head = items ? items->head : nullptr;
                return SerializationPosition<ObjectLink>{head ? head->left : nullptr, head, items && !head};
            };
            return Collection(position, '{', '}',
                [](ObjectLink* link) { return NextObjectLink(link); },
                [this, &value](ObjectLink* link) { Escape(link->pair.key); Character(':'); return SerializeValue(link->pair.value, &value); });
        }
        return 0;
    }
private:
    String& buffer;
    String* destination;
    SerializationCallback callback;
    void* context;
    bool collect;
    void Character(char character) { if (character) AppendText(buffer, std::string_view(&character, 1)); }
    int Emit() {
        if (collect) {
            if (destination) { AppendText(*destination, Characters(buffer)); ClearText(buffer); }
            return callback(*destination, context);
        }
        const int result = callback(buffer, context);
        ClearText(buffer);
        return result;
    }
    void Escape(const String& string) {
        Character('"');
        for (const unsigned char character : View(string)) {
            switch (character) {
                case '\b': AppendText(buffer, "\\b"); break;
                case '\t': AppendText(buffer, "\\t"); break;
                case '\n': AppendText(buffer, "\\n"); break;
                case '\f': AppendText(buffer, "\\f"); break;
                case '\r': AppendText(buffer, "\\r"); break;
                case '"': AppendText(buffer, "\\\""); break;
                case '/': AppendText(buffer, "\\/"); break;
                case '\\': AppendText(buffer, "\\\\"); break;
                default:
                    if (character < 0x20 || character == 0x7f) {
                        char escaped[7];
                        std::snprintf(escaped, sizeof(escaped), "\\u%04x", character);
                        AppendText(buffer, escaped);
                    } else Character(static_cast<char>(character));
            }
        }
        Character('"');
    }
    template<typename TPosition, typename TNext, typename TSerialize>
    int Collection(TPosition position, char opening, char closing, TNext next, TSerialize serialize) {
        Character(opening);
        int status = Emit();
        if (status < 0) return status;
        {
            auto current = position();
            SerializationIterator iterator(current.first, current.invalid);
            current = position();
            if (iterator.Different(current.head, current.invalid)) {
                do {
                    current = position();
                    if (iterator.Different(current.first, current.invalid)) Character(',');
                    if (!*iterator.state) { NotImplemented_nid_no_patch(__func__); return 0; }
                    status = serialize(*iterator.state);
                    if (status < 0) break;
                    *iterator.state = next(*iterator.state);
                    current = position();
                } while (iterator.Different(current.head, current.invalid));
            }
        }
        if (status < 0) return status;
        Character(closing);
        return Emit();
    }
};

int APS5_VABI SerializationSucceeded(String&, void*) { return 0; }

int SerializeCallbackValue(Value* value, String* destination, SerializationCallback callback, void* context, bool collect) {
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        if (!state.allocation) return JsonErrorNotInitialized;
    }
    if (!callback || (collect && !destination)) { NotImplemented_nid_no_patch(__func__); return 0; }
    ScopedString buffer(std::string_view{});
    CallbackSerializer serializer(buffer.string, destination, callback, context, collect);
    return serializer.SerializeValue(*value);
}

using ParserReadCallback = int (APS5_VABI*)(char&, void*);

double ParseDecimal(std::string_view text) {
    std::size_t cursor = 0;
    while (cursor < text.size() && (text[cursor] == ' ' || (text[cursor] >= '\t' && text[cursor] <= '\r'))) ++cursor;
    double sign = 1.0;
    while (cursor < text.size() && (text[cursor] == '+' || text[cursor] == '-')) {
        if (text[cursor++] == '-') sign = -sign;
    }
    auto digit = [&](std::size_t index) { return index < text.size() && text[index] >= '0' && text[index] <= '9'; };
    double magnitude = 0;
    bool admitted = false;
    while (digit(cursor)) {
        volatile double scaled = magnitude * 10.0;
        magnitude = scaled + (text[cursor++] - '0');
        admitted = true;
    }
    if (cursor < text.size() && text[cursor] == '.') {
        ++cursor;
        double divisor = 10.0;
        while (digit(cursor)) {
            magnitude += (text[cursor++] - '0') / divisor;
            divisor *= 10.0;
            admitted = true;
        }
    }
    if (!admitted) return 0;
    if (cursor < text.size() && (text[cursor] == 'e' || text[cursor] == 'E' || text[cursor] == 'd' || text[cursor] == 'D')) {
        ++cursor;
        bool divide = false;
        if (cursor < text.size() && (text[cursor] == '+' || text[cursor] == '-')) divide = text[cursor++] == '-';
        if (!digit(cursor)) return 0;
        std::uint32_t exponent = 0;
        while (digit(cursor)) exponent = exponent * 10 + static_cast<unsigned>(text[cursor++] - '0');
        while (exponent-- && magnitude != 0 && !std::isinf(magnitude)) {
            if (divide) magnitude /= 10.0;
            else magnitude *= 10.0;
        }
    }
    return magnitude * sign;
}

class Parser {
public:
    Parser(const char* text, std::size_t size) : text(text), end(text + size) {}
    Parser(ParserReadCallback read, void* context) : read(read), context(context) {}

    bool Parse(Node& out) { return value(out); }

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
    template<class Output>
    static void appendUtf8(Output& out, std::uint32_t code) {
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
    struct ParsedText {
        String& string;
        void operator+=(char character) { AppendText(string, {&character, 1}); }
    };
    template<class Output>
    bool string(Output& out) {
        if (AllocationOperation::Failed()) return false;
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
    void Admit(Node& out, const Value* parent) { PrepareValueAdmission(out, parent); }
    bool number(Node& out, const Value* parent) {
        ScopedString token{std::string_view{}};
        auto append = [&](char character) { AppendText(token.string, {&character, 1}); };
        if (peek() >= 0 && peek() == '-') append(static_cast<char>(take()));
        if (peek() < 0 || peek() < '0' || peek() > '9') return false;
        while (peek() >= '0' && peek() <= '9') append(static_cast<char>(take()));
        bool integral = true;
        if (peek() >= 0 && peek() == '.') {
            integral = false;
            append(static_cast<char>(take()));
            if (peek() < 0 || peek() < '0' || peek() > '9') return false;
            while (peek() >= 0 && peek() >= '0' && peek() <= '9') append(static_cast<char>(take()));
        }
        if (peek() >= 0 && (peek() == 'e' || peek() == 'E')) {
            integral = false;
            append(static_cast<char>(take()));
            if (peek() >= 0 && (peek() == '+' || peek() == '-')) append(static_cast<char>(take()));
            if (peek() < 0 || peek() < '0' || peek() > '9') return false;
            while (peek() >= 0 && peek() >= '0' && peek() <= '9') append(static_cast<char>(take()));
        }
        const auto text = View(token.string);
        if (text.empty()) {
            SetType(out, TypeUInteger);
            out.uinteger = 0;
            Admit(out, parent);
            return true;
        }
        if (integral) {
            errno = 0;
            if (text[0] == '-') {
                const long long value = std::strtoll(Characters(token.string), nullptr, 10);
                if (errno == 0) { SetType(out, TypeInteger); out.integer = value; Admit(out, parent); return true; }
            } else {
                const unsigned long long value = std::strtoull(Characters(token.string), nullptr, 10);
                if (errno == 0) {
                    SetType(out, TypeUInteger);
                    out.uinteger = value;
                    Admit(out, parent);
                    return true;
                }
            }
        }
        const double real = ParseDecimal(text);
        Admit(out, parent);
        AssignReal(out, real);
        return true;
    }
    bool value(Node& out, const Value* parent = nullptr) {
        if (AllocationOperation::Failed()) return false;
        skip();
        if (peek() < 0) return false;
        switch (peek()) {
            case 'n': if (!literal("null")) return false; SetType(out, TypeNull); Admit(out, parent); return true;
            case 't': if (!literal("true")) return false; SetType(out, TypeBoolean); out.boolean = true; Admit(out, parent); return true;
            case 'f': if (!literal("false")) return false; SetType(out, TypeBoolean); out.boolean = false; Admit(out, parent); return true;
            case '"': {
                SetType(out, TypeString);
                Admit(out, parent);
                if (out.type != TypeString) throw GuestOutOfMemory{};
                if (AllocationOperation::Failed()) return false;
                ParsedText text{*out.string};
                return string(text);
            }
            case '[': {
                if (depth == MaxNestingDepth) return false;
                take();
                ++depth;
                SetType(out, TypeArray);
                Admit(out, parent);
                if (out.type != TypeArray) throw GuestOutOfMemory{};
                skip();
                if (peek() >= 0 && peek() == ']') { take(); --depth; return true; }
                for (;;) {
                    if (!out.array->items) throw GuestOutOfMemory{};
                    if (!out.array->items->head) NotImplemented_nid_no_patch(__func__);
                    Value placeholder{};
                    AppendArray(out.array, &placeholder);
                    if (AllocationOperation::Failed()) return false;
                    auto& item = out.array->items->back();
                    item.parent = &out;
                    skip();
                    if (!value(item, &out)) return false;
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
                Admit(out, parent);
                if (out.type != TypeObject) throw GuestOutOfMemory{};
                skip();
                if (peek() >= 0 && peek() == '}') { take(); --depth; return true; }
                ScopedString key{std::string_view{}};
                for (;;) {
                    skip();
                    ParsedText text{key.string};
                    if (!string(text)) return false;
                    skip();
                    if (peek() < 0 || peek() != ':') return false;
                    take();
                    skip();
                    Value& entry = *ObjectAccess(*out.object, key.string);
                    if (!value(entry, &out)) return false;
                    ClearText(key.string);
                    skip();
                    if (peek() >= 0 && peek() == ',') { take(); continue; }
                    if (peek() >= 0 && peek() == '}') { take(); --depth; return true; }
                    return false;
                }
            }
            default:
                if (peek() != '-' && (peek() < '0' || peek() > '9')) return false;
                return number(out, parent);
        }
    }
};

int ParseValue(Value* out, Parser& parser) {
    AllocationOperation operation;
    bool parsedOk = false;
    try {
        parsedOk = parser.Parse(NodeOf(*out));
    } catch (const GuestOutOfMemory&) {
        operation.Finish();
        return JsonErrorOutOfMemory;
    }
    if (operation.Finish()) return JsonErrorOutOfMemory;
    return parsedOk ? 0 : parser.Error();
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
    std::lock_guard callbackLock(state.callbackMutex);
    std::lock_guard lock(state.mutex);
    if (state.allocation || state.initialized || *static_cast<const std::uint8_t*>(self)) return JsonErrorAlreadyInitialized;
    if (!parameter) return JsonErrorInvalidArgument;
    if (!*static_cast<void* const*>(parameter)) return JsonErrorOutOfMemory;
    ResetDefaults();
    return InitializeResources(self, parameter, 2, false);
}

void APS5_VABI _ZN3sce4Json19InternalInitializerC1Ev(void* self) { _ZN3sce4Json11InitializerC1Ev(self); }

void APS5_VABI _ZN3sce4Json19InternalInitializerC2Ev(void* self) { _ZN3sce4Json11InitializerC1Ev(self); }

void APS5_VABI _ZN3sce4Json19InternalInitializerD1Ev(void* self) {
    _ZN3sce4Json11Initializer9terminateEv(self);
    _ZN3sce4Json11Initializer9terminateEv(self);
}

void APS5_VABI _ZN3sce4Json19InternalInitializerD2Ev(void* self) { _ZN3sce4Json19InternalInitializerD1Ev(self); }

int APS5_VABI _ZN3sce4Json19InternalInitializer19terminate_staticlibEv(void* self) { return _ZN3sce4Json11Initializer9terminateEv(self); }

int APS5_VABI _ZN3sce4Json19InternalInitializer20initialize_staticlibEPKNS0_13InitParameterENS1_10InitOptionE(void* self, const void* parameter, std::int32_t option) {
    auto& state = State();
    std::lock_guard callbackLock(state.callbackMutex);
    std::lock_guard lock(state.mutex);
    state.paddedStringGrowth = option == 1;
    return _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(self, parameter);
}

int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void* self) {
    auto& state = State();
    std::lock_guard callbackLock(state.callbackMutex);
    std::lock_guard lock(state.mutex);
    if (!state.initialized || !*static_cast<const std::uint8_t*>(self)) return JsonErrorNotInitialized;
    if (state.terminating || state.callbackActive || (allocatorCallbackDepth && state.allocationDepth)) NotImplemented_nid_no_patch(__func__);
    TerminationScope termination(state.terminating);
    state.callback = nullptr;
    state.context = nullptr;
    state.handlers = {};
    ReleaseInitialization();
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

void APS5_VABI _ZN3sce4Json12MemAllocatorD2Ev(void*) {}

void APS5_VABI _ZN3sce4Json12MemAllocatorD1Ev(void*) {}

void* APS5_VABI _ZN3sce4Json6MallocEm(std::size_t size) { return Allocate(size); }

void APS5_VABI _ZN3sce4Json4FreeEPv(void* memory) { Release(memory); }

int APS5_VABI _ZN3sce4Json11Initializer24setAllocatorInfoCallBackEPFviNS0_9ValueTypeEPvES3_(void*, void (APS5_VABI*)(int, ValueType, void*), void*) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return state.allocation ? 0 : JsonErrorNotInitialized;
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
char APS5_VABI _ZNK3sce4Json6String2atEm(const String* self, std::size_t index) {
    return !self->text ? 0 : index < self->text->length ? self->text->characters[index] : emptyCharacters[0];
}
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcm(const String* self, const char* needle, std::size_t position) { return FindText(*self, needle, position, needle ? std::strlen(needle) : 0); }
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcmm(const String* self, const char* needle, std::size_t position, std::size_t count) { return FindText(*self, needle, position, count); }
std::size_t APS5_VABI _ZNK3sce4Json6String4findERKS1_m(const String* self, const String* needle, std::size_t position) { return FindText(*self, Characters(*needle), position, std::strlen(Characters(*needle))); }
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcm(const String* self, const char* needle, std::size_t position) { return ReverseFindText(*self, needle, position, needle ? std::strlen(needle) : 0); }
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcmm(const String* self, const char* needle, std::size_t position, std::size_t count) { return ReverseFindText(*self, needle, position, count); }
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindERKS1_m(const String* self, const String* needle, std::size_t position) { return ReverseFindText(*self, Characters(*needle), position, std::strlen(Characters(*needle))); }
std::size_t APS5_VABI _ZNK3sce4Json6String4findEcm(const String* self, char character, std::size_t position) { return FindText(*self, &character, position, 1); }
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEcm(const String* self, char character, std::size_t position) { return ReverseFindText(*self, &character, position, 1); }
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
Array* APS5_VABI _ZN3sce4Json5ArrayaSERKS1_(Array* self, const Array* other) { AssignArray(*self, *other); return self; }
void APS5_VABI _ZN3sce4Json5Array10push_frontERKNS0_5ValueE(Array* self, const Value* value) { PrependArray(self, value); }

ArrayIterator* APS5_VABI _ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(ArrayIterator* result, Array* self, const ArrayIterator* position, const Value* value) {
    result->state = static_cast<ArrayIteratorState*>(Allocate(sizeof(ArrayIteratorState)));
    if (result->state) result->state->position = nullptr;
    if (!self->items) return result;
    if (!position || !position->state || !position->state->position || !self->items->head) {
        Release(result->state); result->state = nullptr;
        NotImplemented_nid_no_patch(__func__);
    }
    ArrayLink* inserted;
    if (!ModernValues()) inserted = InsertArrayLink(*self->items, position->state->position, *value);
    else {
        ScopedValue copy;
        copy.value.parent = self->items->Owner();
        AssignNode(copy.value, *value);
        AllocationOperation operation;
        Value empty{};
        inserted = InsertArrayLink(*self->items, position->state->position, empty);
        if (!operation.Finish()) TransferPayload(inserted->value, copy.value);
    }
    if (inserted) { if (result->state) result->state->position = inserted; }
    else { Release(result->state); result->state = nullptr; }
    return result;
}

void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array* self, const Value* value) { AppendArray(self, value); }

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
void APS5_VABI _ZN3sce4Json5Array8pop_backEv(Array* self) { if (self->items) self->items->PopBack(); }

ArrayIterator* APS5_VABI _ZN3sce4Json5Array5eraseERKNS1_8iteratorE(ArrayIterator* result, Array* self, const ArrayIterator* position) {
    result->state = static_cast<ArrayIteratorState*>(Allocate(sizeof(ArrayIteratorState)));
    if (result->state) result->state->position = nullptr;
    if (!self->items) return result;
    if (!position || !position->state || !position->state->position || position->state->position == self->items->head) {
        Release(result->state);
        result->state = nullptr;
        NotImplemented_nid_no_patch(__func__);
    }
    auto* link = position->state->position;
    auto* next = link->next;
    next->previous = link->previous;
    link->previous->next = next;
    Destroy(link->value);
    Release(link);
    --self->items->count;
    if (next) {
        if (result->state) result->state->position = next;
    } else {
        Release(result->state);
        result->state = nullptr;
    }
    return result;
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
    Construct(*self);
    ScopedString temporary{std::string_view{text ? text : ""}};
    if (!temporary.string.text) return;
    SetType(*self, TypeString);
    if (self->type == TypeString) AssignText(*self->string, View(temporary.string), true);
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6StringE(Value* self, const String* text) {
    Construct(*self);
    if (!text->text) return;
    SetType(*self, TypeString);
    if (self->type == TypeString) AssignText(*self->string, View(*text), true);
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(Value* self, const Array* array) {
    Construct(*self);
    if (!array->items) return;
    SetType(*self, TypeArray);
    if (self->array) AssignArray(*self->array, *array);
}
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(Value* self, const Object* object) {
    Construct(*self);
    if (!object->items) return;
    SetType(*self, TypeObject);
    if (self->object) AssignObject(*self->object, *object);
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
        Destroy(temporary);
        throw;
    }
    Destroy(temporary);
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
void APS5_VABI _ZN3sce4Json5Value3setERKNS0_5ArrayE(Value* self, const Array* array) {
    if (!array->items) return;
    SetType(*self, TypeArray);
    if (self->array) AssignArray(*self->array, *array);
}
void APS5_VABI _ZN3sce4Json5Value3setERKNS0_6ObjectE(Value* self, const Object* object) {
    if (!object->items) return;
    SetType(*self, TypeObject);
    if (self->object) AssignObject(*self->object, *object);
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
const Value* APS5_VABI _ZNK3sce4Json5ValueixERKNS0_6StringE(const Value* self, const String* key) { return LookupKey(self, key); }
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value* self, const char* key) {
    ScopedString text(key ? key : "");
    return LookupKey(self, &text.string);
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
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKNS0_6StringE(const Value* self, const String* key) { return LookupKey(self, key); }
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
int APS5_VABI _ZN3sce4Json5Value9serializeEPFiRNS0_6StringEPvES4_(Value* self, SerializationCallback callback, void* context) {
    return SerializeCallbackValue(self, nullptr, callback, context, false);
}
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(Value* self, String* output, SerializationCallback callback, void* context) {
    return SerializeCallbackValue(self, output, callback, context, true);
}
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value* self, String* out) {
    {
        auto& state = State();
        std::lock_guard lock(state.mutex);
        if (!state.allocation) return JsonErrorNotInitialized;
    }
    return SerializeCallbackValue(self, out, SerializationSucceeded, nullptr, true);
}
void APS5_VABI _ZNK3sce4Json5Value8toStringERNS0_6StringE(const Value* self, String* out) {
    std::string text;
    if (self->type == TypeString) AppendText(*out, Characters(*self->string));
    else {
        if (self->type == TypeArray) text = "array";
        else if (self->type == TypeObject) text = "object";
        else if (self->type == TypeReal) {
            if (!FloatConfiguration().enabled) { AppendLegacyReal(*out, self->real); return; }
            AppendReal(text, self->real);
        }
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
        allocation = state.allocation ? *state.allocation : AllocationContext{};
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
    if (state.allocation) return;
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
    if (state.allocation) return;
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
    std::lock_guard callbackLock(state.callbackMutex);
    std::lock_guard lock(state.mutex);
    if (state.allocation || state.initialized || *static_cast<const std::uint8_t*>(self)) return JsonErrorAlreadyInitialized;
    if (!parameter || !parameter->allocator || static_cast<std::uint32_t>(parameter->specialFloatFormatType) > 2)
        return JsonErrorInvalidArgument;
    ResetDefaults();
    return InitializeResources(self, parameter, parameter->specialFloatFormatType, true);
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
        if (ModernValues() && !state.allocation) return JsonErrorNotInitialized;
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
    Construct(self->key, *key);
    try {
        CopyInto(self->value, *value);
    } catch (...) {
        Destroy(self->key);
        throw;
    }
}

void APS5_VABI _ZN3sce4Json6Object4PairD1Ev(Pair* self) {
    Destroy(self->value);
    Destroy(self->key);
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
    AllocationOperation operation;
    ObjectLink* node;
    bool inserted;
    {
        InlineText key; key.Assign(Characters(pair->key));
        ScopedString copiedKey(pair->key);
        Pair source{}; source.key = copiedKey.string;
        TemporaryTreeEntry entry(key.text, source);
        node = self->items->Insert(entry.key.text, entry.pair, inserted);
    }
    const bool failed = operation.Finish();
    if (inserted && !failed) {
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
    Construct(self->key, std::string_view{});
    try {
        Construct(self->value);
    } catch (...) {
        Destroy(self->key);
        throw;
    }
}

void APS5_VABI _ZN3sce4Json11InitializerC2Ev(void* self) { _ZN3sce4Json11InitializerC1Ev(self); }
void APS5_VABI _ZN3sce4Json11InitializerD2Ev(void* self) { _ZN3sce4Json11InitializerD1Ev(self); }
void APS5_VABI _ZN3sce4Json17InitParameterRttiC2EPNS0_14AllocParamRttiEPvm(InitParameterRtti* self, void* allocParam, void* userData, std::size_t fileBufferSize) { _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(self, allocParam, userData, fileBufferSize); }
void APS5_VABI _ZN3sce4Json5Array8iteratorC2ERKS2_(ArrayIterator* self, const ArrayIterator* other) { _ZN3sce4Json5Array8iteratorC1ERKS2_(self, other); }
void APS5_VABI _ZN3sce4Json5Array8iteratorC2Ev(ArrayIterator* self) { _ZN3sce4Json5Array8iteratorC1Ev(self); }
void APS5_VABI _ZN3sce4Json5Array8iteratorD2Ev(ArrayIterator* self) { _ZN3sce4Json5Array8iteratorD1Ev(self); }
void APS5_VABI _ZN3sce4Json5ArrayC2ERKS1_(Array* self, const Array* other) { _ZN3sce4Json5ArrayC1ERKS1_(self, other); }
void APS5_VABI _ZN3sce4Json5ArrayC2Ev(Array* self) { _ZN3sce4Json5ArrayC1Ev(self); }
void APS5_VABI _ZN3sce4Json5ArrayD2Ev(Array* self) { _ZN3sce4Json5ArrayD1Ev(self); }
void APS5_VABI _ZN3sce4Json5ValueC2ENS0_9ValueTypeE(Value* self, ValueType type) { _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(self, type); }
void APS5_VABI _ZN3sce4Json5ValueC2EPKc(Value* self, const char* text) { _ZN3sce4Json5ValueC1EPKc(self, text); }
void APS5_VABI _ZN3sce4Json5ValueC2ERKNS0_5ArrayE(Value* self, const Array* array) { _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(self, array); }
void APS5_VABI _ZN3sce4Json5ValueC2ERKNS0_6ObjectE(Value* self, const Object* object) { _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(self, object); }
void APS5_VABI _ZN3sce4Json5ValueC2ERKNS0_6StringE(Value* self, const String* text) { _ZN3sce4Json5ValueC1ERKNS0_6StringE(self, text); }
void APS5_VABI _ZN3sce4Json5ValueC2ERKS1_(Value* self, const Value* other) { _ZN3sce4Json5ValueC1ERKS1_(self, other); }
void APS5_VABI _ZN3sce4Json5ValueC2Eb(Value* self, bool value) { _ZN3sce4Json5ValueC1Eb(self, value); }
void APS5_VABI _ZN3sce4Json5ValueC2Ed(Value* self, double value) { _ZN3sce4Json5ValueC1Ed(self, value); }
void APS5_VABI _ZN3sce4Json5ValueC2El(Value* self, std::int64_t value) { _ZN3sce4Json5ValueC1El(self, value); }
void APS5_VABI _ZN3sce4Json5ValueC2Em(Value* self, std::uint64_t value) { _ZN3sce4Json5ValueC1Em(self, value); }
void APS5_VABI _ZN3sce4Json5ValueC2Ev(Value* self) { _ZN3sce4Json5ValueC1Ev(self); }
void APS5_VABI _ZN3sce4Json5ValueD2Ev(Value* self) { _ZN3sce4Json5ValueD1Ev(self); }
void APS5_VABI _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(Pair* self, const String* key, const Value* value) { _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(self, key, value); }
void APS5_VABI _ZN3sce4Json6Object4PairC2Ev(Pair* self) { _ZN3sce4Json6Object4PairC1Ev(self); }
void APS5_VABI _ZN3sce4Json6Object4PairD2Ev(Pair* self) { _ZN3sce4Json6Object4PairD1Ev(self); }
void APS5_VABI _ZN3sce4Json6Object8iteratorC2ERKS2_(ObjectIterator* self, const ObjectIterator* other) { _ZN3sce4Json6Object8iteratorC1ERKS2_(self, other); }
void APS5_VABI _ZN3sce4Json6Object8iteratorC2Ev(ObjectIterator* self) { _ZN3sce4Json6Object8iteratorC1Ev(self); }
void APS5_VABI _ZN3sce4Json6Object8iteratorD2Ev(ObjectIterator* self) { _ZN3sce4Json6Object8iteratorD1Ev(self); }
void APS5_VABI _ZN3sce4Json6ObjectC2ERKS1_(Object* self, const Object* other) { _ZN3sce4Json6ObjectC1ERKS1_(self, other); }
void APS5_VABI _ZN3sce4Json6ObjectC2Ev(Object* self) { _ZN3sce4Json6ObjectC1Ev(self); }
void APS5_VABI _ZN3sce4Json6ObjectD2Ev(Object* self) { _ZN3sce4Json6ObjectD1Ev(self); }
void APS5_VABI _ZN3sce4Json6StringC2EPKc(String* self, const char* text) { _ZN3sce4Json6StringC1EPKc(self, text); }
void APS5_VABI _ZN3sce4Json6StringC2ERKS1_(String* self, const String* other) { _ZN3sce4Json6StringC1ERKS1_(self, other); }
void APS5_VABI _ZN3sce4Json6StringC2Ev(String* self) { _ZN3sce4Json6StringC1Ev(self); }
void APS5_VABI _ZN3sce4Json6StringD2Ev(String* self) { _ZN3sce4Json6StringD1Ev(self); }

String* APS5_VABI _ZN3sce4Json5Value11referStringEv(Value* self) { return self->type == TypeString ? self->string : nullptr; }
bool* APS5_VABI _ZN3sce4Json5Value12referBooleanEv(Value* self) { return self->type == TypeBoolean ? &self->boolean : nullptr; }
std::int64_t* APS5_VABI _ZN3sce4Json5Value12referIntegerEv(Value* self) { return self->type == TypeInteger || self->type == TypeUInteger ? &self->integer : nullptr; }
std::uint64_t* APS5_VABI _ZN3sce4Json5Value13referUIntegerEv(Value* self) { return self->type == TypeInteger || self->type == TypeUInteger ? &self->uinteger : nullptr; }
double* APS5_VABI _ZN3sce4Json5Value9referRealEv(Value* self) { return self->type == TypeReal ? &self->real : nullptr; }
void* APS5_VABI _ZNK3sce4Json14InitParameter211getUserDataEv(const InitParameter2* self) { return self->userData; }
void* APS5_VABI _ZNK3sce4Json14InitParameter212getAllocatorEv(const InitParameter2* self) { return self->allocator; }
std::size_t APS5_VABI _ZNK3sce4Json14InitParameter217getFileBufferSizeEv(const InitParameter2* self) { return self->fileBufferSize; }
std::int32_t APS5_VABI _ZNK3sce4Json14InitParameter225getSpecialFloatFormatTypeEv(const InitParameter2* self) { return self->specialFloatFormatType; }
}
