#include "prx/libSceJson/include/Api.hpp"
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <system_error>
#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

using namespace SceJson;

extern "C" {

const std::int64_t _ZN3sce4Json5Value13s_nullintegerE_nid_no_patch = 0;
APS5_EXPORT("dJSD90mnj1w", _ZN3sce4Json5Value13s_nullintegerE_nid_no_patch);
const std::uint64_t _ZN3sce4Json5Value14s_nulluintegerE_nid_no_patch = 0;
APS5_EXPORT("xxBnIBiUVzY", _ZN3sce4Json5Value14s_nulluintegerE_nid_no_patch);
const double _ZN3sce4Json5Value10s_nullrealE_nid_no_patch = 0.0;
APS5_EXPORT("GpEG1IcP86s", _ZN3sce4Json5Value10s_nullrealE_nid_no_patch);
const bool _ZN3sce4Json5Value10s_nullboolE_nid_no_patch = false;
APS5_EXPORT("bUw2Go-rxtE", _ZN3sce4Json5Value10s_nullboolE_nid_no_patch);
String* _ZN3sce4Json5Value12s_nullstringE_nid_no_patch = nullptr;
APS5_EXPORT("24kDIV0aDzs", _ZN3sce4Json5Value12s_nullstringE_nid_no_patch);
Array* _ZN3sce4Json5Value11s_nullarrayE_nid_no_patch = nullptr;
APS5_EXPORT("zwDiN3-CRTA", _ZN3sce4Json5Value11s_nullarrayE_nid_no_patch);
Object* _ZN3sce4Json5Value12s_nullobjectE_nid_no_patch = nullptr;
APS5_EXPORT("3YODFwSqgtc", _ZN3sce4Json5Value12s_nullobjectE_nid_no_patch);
InitParameter* _ZN3sce4Json11s_initparamE_nid_no_patch = nullptr;
APS5_EXPORT("7Xc95EMbk9I", _ZN3sce4Json11s_initparamE_nid_no_patch);

}

namespace {

constexpr int ErrorParse = static_cast<int>(0x80848101);
constexpr int ErrorNoMemory = static_cast<int>(0x80848102);
constexpr int ErrorFileOpen = static_cast<int>(0x80848103);
constexpr int ErrorNotRoot = static_cast<int>(0x80848104);
constexpr int ErrorFileBufferSize = static_cast<int>(0x80848105);
constexpr int ErrorNotInitialized = static_cast<int>(0x80848110);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0x80848111);
constexpr std::size_t InlineCapacity = 15;
constexpr std::size_t MaxStringSize = std::numeric_limits<std::size_t>::max() - 2;
constexpr char Red = 0;
constexpr char Black = 1;

using AllocateFunction = void* (APS5_VABI*)(MemAllocator*, std::size_t, void*);
using DeallocateFunction = void (APS5_VABI*)(MemAllocator*, void*, void*);

Value fallbackValue {};

[[noreturn]] void AllocationFailed() {
    throw -1;
}

void* AllocateFrom(const InitParameter& parameter, std::size_t size) {
    const auto allocate = reinterpret_cast<AllocateFunction>(parameter.allocator->vtable[2]);
    return allocate(parameter.allocator, size, parameter.userData);
}

void* Allocate(std::size_t size) {
    const InitParameter* parameter = _ZN3sce4Json11s_initparamE_nid_no_patch;
    if (parameter == nullptr || parameter->allocator == nullptr) return nullptr;
    void* result = AllocateFrom(*parameter, size);
    if (result == nullptr) AllocationFailed();
    return result;
}

void* AllocateRequired(std::size_t size) {
    void* result = Allocate(size);
    if (result == nullptr) throw std::runtime_error("sce::Json: allocation before Initializer::initialize, where the console dereferences a null pointer");
    return result;
}

void Release(void* pointer) {
    const InitParameter* parameter = _ZN3sce4Json11s_initparamE_nid_no_patch;
    if (parameter == nullptr || parameter->allocator == nullptr) AllocationFailed();
    const auto deallocate = reinterpret_cast<DeallocateFunction>(parameter->allocator->vtable[3]);
    deallocate(parameter->allocator, pointer, parameter->userData);
}

template <typename T>
T* Required(T* pointer) {
    if (pointer == nullptr) throw std::runtime_error("sce::Json: null container, where the console dereferences a null pointer");
    return pointer;
}

char* Data(String& text) {
    return text.capacity > InlineCapacity ? text.storage.pointer : text.storage.buffer;
}

const char* Data(const String& text) {
    return text.capacity > InlineCapacity ? text.storage.pointer : text.storage.buffer;
}

void ConstructString(String& text) {
    text.capacity = InlineCapacity;
    text.size = 0;
    text.storage.buffer[0] = '\0';
}

void DestroyString(String& text) {
    if (text.capacity > InlineCapacity) Release(text.storage.pointer);
    text.capacity = InlineCapacity;
    text.size = 0;
    text.storage.buffer[0] = '\0';
}

void Reserve(String& text, std::size_t required) {
    if (required > MaxStringSize) throw std::length_error("sce::Json: string too long");
    std::size_t capacity = required | InlineCapacity;
    if (capacity > MaxStringSize) {
        capacity = required;
    } else if (text.capacity / 2 > capacity / 3) {
        capacity = text.capacity <= MaxStringSize - text.capacity / 2 ? text.capacity + text.capacity / 2 : MaxStringSize;
    }
    auto* fresh = static_cast<char*>(AllocateRequired(capacity + 1));
    std::memcpy(fresh, Data(text), text.size);
    fresh[text.size] = '\0';
    if (text.capacity > InlineCapacity) Release(text.storage.pointer);
    text.storage.pointer = fresh;
    text.capacity = capacity;
}

void Append(String& text, const char* source, std::size_t count) {
    if (count == 0) return;
    if (count > MaxStringSize - text.size) throw std::length_error("sce::Json: string too long");
    const auto base = reinterpret_cast<std::uintptr_t>(Data(text));
    const auto address = reinterpret_cast<std::uintptr_t>(source);
    const bool inside = address >= base && address < base + text.size;
    const std::size_t offset = inside ? address - base : 0;
    const std::size_t size = text.size + count;
    if (size > text.capacity) Reserve(text, size);
    if (inside) source = Data(text) + offset;
    std::memmove(Data(text) + text.size, source, count);
    text.size = size;
    Data(text)[size] = '\0';
}

void Append(String& text, const char* source) {
    Append(text, source, std::strlen(source));
}

void Append(String& text, char character) {
    Append(text, &character, 1);
}

void Assign(String& text, const String& source) {
    if (&text == &source) return;
    const std::size_t size = source.size;
    if (size > MaxStringSize) throw std::length_error("sce::Json: string too long");
    if (size > text.capacity) Reserve(text, size);
    std::memmove(Data(text), Data(source), size);
    text.size = size;
    Data(text)[size] = '\0';
}

void Truncate(String& text) {
    text.size = 0;
    Data(text)[0] = '\0';
}

bool Less(const String& left, const String& right) {
    const std::size_t count = left.size < right.size ? left.size : right.size;
    const int result = count == 0 ? 0 : std::memcmp(Data(left), Data(right), count);
    if (result != 0) return result < 0;
    return left.size < right.size;
}

struct LocalString {
    String text {};

    LocalString() { ConstructString(text); }
    LocalString(const LocalString&) = delete;
    LocalString& operator=(const LocalString&) = delete;
    ~LocalString() noexcept(false) { DestroyString(text); }
};

void InitValue(Value& value) {
    value.parent = nullptr;
    value.root = nullptr;
    value.payload.uinteger = 0;
    value.type = TypeNull;
}

void DestroyValue(Value& value);
void SetValue(Value& value, const Value& source);
void SetParent(Value& value, const Value* parent);

void CopyValue(Value& value, const Value& source) {
    InitValue(value);
    SetValue(value, source);
}

ListNode* InsertBefore(Array& array, ListNode* where, const Value& value) {
    auto* node = static_cast<ListNode*>(AllocateRequired(sizeof(ListNode)));
    try {
        CopyValue(node->value, value);
    } catch (...) {
        Release(node);
        throw;
    }
    node->next = where;
    node->previous = where->previous;
    ++array.size;
    where->previous = node;
    node->previous->next = node;
    return node;
}

ListNode* Erase(Array& array, ListNode* node) {
    ListNode* next = node->next;
    node->previous->next = next;
    next->previous = node->previous;
    DestroyValue(node->value);
    Release(node);
    --array.size;
    return next;
}

void ConstructArray(Array& array) {
    array.head = nullptr;
    array.size = 0;
    auto* head = static_cast<ListNode*>(AllocateRequired(sizeof(ListNode)));
    head->next = head;
    head->previous = head;
    array.head = head;
}

void ClearArray(Array& array) {
    ListNode* node = array.head->next;
    array.head->next = array.head;
    array.head->previous = array.head;
    array.size = 0;
    while (node != array.head) {
        ListNode* next = node->next;
        DestroyValue(node->value);
        Release(node);
        node = next;
    }
}

void DestroyArray(Array& array) {
    ClearArray(array);
    Release(array.head);
    array.head = nullptr;
}

void AssignArray(Array& array, const Array& source) {
    if (&array == &source) return;
    ListNode* target = array.head->next;
    const ListNode* item = source.head->next;
    for (; item != source.head && target != array.head; item = item->next, target = target->next) SetValue(target->value, item->value);
    if (item == source.head) {
        while (target != array.head) target = Erase(array, target);
    } else {
        for (; item != source.head; item = item->next) InsertBefore(array, array.head, item->value);
    }
}

ListNode* ArrayAt(const Array& array, std::size_t index) {
    ListNode* node = array.head->next;
    for (; index != 0; --index) node = node->next;
    return node;
}

TreeNode* Root(const Object& object) {
    return object.head->parent;
}

TreeNode* Minimum(TreeNode* node) {
    while (!node->left->isNil) node = node->left;
    return node;
}

TreeNode* Maximum(TreeNode* node) {
    while (!node->right->isNil) node = node->right;
    return node;
}

TreeNode* Next(TreeNode* node) {
    if (!node->right->isNil) return Minimum(node->right);
    TreeNode* parent = node->parent;
    while (!parent->isNil && node == parent->right) {
        node = parent;
        parent = node->parent;
    }
    return parent;
}

TreeNode* ObjectAt(const Object& object, std::size_t index) {
    TreeNode* node = object.head->left;
    for (; index != 0; --index) node = Next(node);
    return node;
}

void ConstructObject(Object& object) {
    object.head = nullptr;
    object.size = 0;
    auto* head = static_cast<TreeNode*>(AllocateRequired(sizeof(TreeNode)));
    head->left = head;
    head->parent = head;
    head->right = head;
    head->color = Black;
    head->isNil = 1;
    object.head = head;
}

TreeNode* BuyNode(Object& object, const String& key, const Value* value, char color) {
    auto* node = static_cast<TreeNode*>(AllocateRequired(sizeof(TreeNode)));
    node->left = object.head;
    node->parent = object.head;
    node->right = object.head;
    node->color = color;
    node->isNil = 0;
    try {
        ConstructString(node->key);
        Assign(node->key, key);
        try {
            if (value == nullptr) InitValue(node->value);
            else CopyValue(node->value, *value);
        } catch (...) {
            DestroyString(node->key);
            throw;
        }
    } catch (...) {
        Release(node);
        throw;
    }
    return node;
}

void RotateLeft(Object& object, TreeNode* where) {
    TreeNode* node = where->right;
    where->right = node->left;
    if (!node->left->isNil) node->left->parent = where;
    node->parent = where->parent;
    if (where == Root(object)) object.head->parent = node;
    else if (where == where->parent->left) where->parent->left = node;
    else where->parent->right = node;
    node->left = where;
    where->parent = node;
}

void RotateRight(Object& object, TreeNode* where) {
    TreeNode* node = where->left;
    where->left = node->right;
    if (!node->right->isNil) node->right->parent = where;
    node->parent = where->parent;
    if (where == Root(object)) object.head->parent = node;
    else if (where == where->parent->right) where->parent->right = node;
    else where->parent->left = node;
    node->right = where;
    where->parent = node;
}

void Link(Object& object, bool addLeft, TreeNode* where, TreeNode* node) {
    TreeNode* head = object.head;
    node->parent = where;
    ++object.size;
    if (where == head) {
        head->parent = node;
        head->left = node;
        head->right = node;
    } else if (addLeft) {
        where->left = node;
        if (where == head->left) head->left = node;
    } else {
        where->right = node;
        if (where == head->right) head->right = node;
    }
    for (TreeNode* current = node; current->parent->color == Red;) {
        TreeNode* parent = current->parent;
        TreeNode* grandparent = parent->parent;
        if (parent == grandparent->left) {
            TreeNode* uncle = grandparent->right;
            if (uncle->color == Red) {
                parent->color = Black;
                uncle->color = Black;
                grandparent->color = Red;
                current = grandparent;
            } else {
                if (current == parent->right) {
                    current = parent;
                    RotateLeft(object, current);
                }
                current->parent->color = Black;
                current->parent->parent->color = Red;
                RotateRight(object, current->parent->parent);
            }
        } else {
            TreeNode* uncle = grandparent->left;
            if (uncle->color == Red) {
                parent->color = Black;
                uncle->color = Black;
                grandparent->color = Red;
                current = grandparent;
            } else {
                if (current == parent->left) {
                    current = parent;
                    RotateRight(object, current);
                }
                current->parent->color = Black;
                current->parent->parent->color = Red;
                RotateLeft(object, current->parent->parent);
            }
        }
    }
    Root(object)->color = Black;
}

TreeNode* LowerBound(const Object& object, const String& key) {
    TreeNode* where = object.head;
    TreeNode* node = Root(object);
    while (!node->isNil) {
        if (Less(node->key, key)) {
            node = node->right;
        } else {
            where = node;
            node = node->left;
        }
    }
    return where;
}

TreeNode* Find(const Object& object, const String& key) {
    TreeNode* where = LowerBound(object, key);
    if (where == object.head || Less(key, where->key)) return object.head;
    return where;
}

Value& Subscript(Object& object, const String& key) {
    TreeNode* where = LowerBound(object, key);
    if (where != object.head && !Less(key, where->key)) return where->value;
    TreeNode* node = BuyNode(object, key, nullptr, Red);
    TreeNode* parent = object.head;
    TreeNode* current = Root(object);
    bool addLeft = true;
    while (!current->isNil) {
        parent = current;
        addLeft = Less(key, current->key);
        current = addLeft ? current->left : current->right;
    }
    Link(object, addLeft, parent, node);
    return node->value;
}

void EraseSubtree(TreeNode* node) {
    while (!node->isNil) {
        EraseSubtree(node->right);
        TreeNode* left = node->left;
        DestroyString(node->key);
        DestroyValue(node->value);
        Release(node);
        node = left;
    }
}

void ClearObject(Object& object) {
    EraseSubtree(Root(object));
    object.head->parent = object.head;
    object.head->left = object.head;
    object.head->right = object.head;
    object.size = 0;
}

void DestroyObject(Object& object) {
    ClearObject(object);
    Release(object.head);
    object.head = nullptr;
}

TreeNode* CopySubtree(Object& object, const TreeNode* source, TreeNode* where) {
    TreeNode* result = object.head;
    if (source->isNil) return result;
    TreeNode* node = BuyNode(object, source->key, &source->value, source->color);
    node->parent = where;
    result = node;
    try {
        node->left = CopySubtree(object, source->left, node);
        node->right = CopySubtree(object, source->right, node);
    } catch (...) {
        EraseSubtree(result);
        throw;
    }
    return result;
}

void AssignObject(Object& object, const Object& source) {
    if (&object == &source) return;
    ClearObject(object);
    try {
        TreeNode* root = CopySubtree(object, Root(source), object.head);
        object.head->parent = root;
        object.size = source.size;
        if (!root->isNil) {
            object.head->left = Minimum(root);
            object.head->right = Maximum(root);
        } else {
            object.head->left = object.head;
            object.head->right = object.head;
        }
    } catch (...) {
        object.head->parent = object.head;
        object.head->left = object.head;
        object.head->right = object.head;
        object.size = 0;
        throw;
    }
}

void SetType(Value& value, ValueType type) {
    if (value.type == type) return;
    if (value.type == TypeString && value.payload.string != nullptr) {
        DestroyString(*value.payload.string);
        Release(value.payload.string);
    } else if (value.type == TypeArray && value.payload.array != nullptr) {
        DestroyArray(*value.payload.array);
        Release(value.payload.array);
    } else if (value.type == TypeObject && value.payload.object != nullptr) {
        DestroyObject(*value.payload.object);
        Release(value.payload.object);
    }
    value.type = TypeNull;
    value.payload.uinteger = 0;
    switch (type) {
    case TypeBoolean:
        value.payload.boolean = false;
        break;
    case TypeInteger:
        value.payload.integer = 0;
        break;
    case TypeUInteger:
        value.payload.uinteger = 0;
        break;
    case TypeReal:
        value.payload.real = 0.0;
        break;
    case TypeString:
        if (auto* text = static_cast<String*>(Allocate(sizeof(String)))) {
            ConstructString(*text);
            value.payload.string = text;
        }
        break;
    case TypeArray:
        if (auto* array = static_cast<Array*>(Allocate(sizeof(Array)))) {
            try {
                ConstructArray(*array);
            } catch (...) {
                Release(array);
                throw;
            }
            value.payload.array = array;
        }
        break;
    case TypeObject:
        if (auto* object = static_cast<Object*>(Allocate(sizeof(Object)))) {
            try {
                ConstructObject(*object);
            } catch (...) {
                Release(object);
                throw;
            }
            value.payload.object = object;
        }
        break;
    default:
        break;
    }
    value.type = type;
}

void SetBoolean(Value& value, bool source) {
    SetType(value, TypeBoolean);
    value.payload.boolean = source;
}

void SetInteger(Value& value, std::int64_t source) {
    SetType(value, TypeInteger);
    value.payload.integer = source;
}

void SetUInteger(Value& value, std::uint64_t source) {
    SetType(value, TypeUInteger);
    value.payload.uinteger = source;
}

void SetReal(Value& value, double source) {
    SetType(value, TypeReal);
    value.payload.real = source;
}

void SetString(Value& value, const String& source) {
    SetType(value, TypeString);
    Assign(*Required(value.payload.string), source);
}

void SetArray(Value& value, const Array& source) {
    SetType(value, TypeArray);
    AssignArray(*Required(value.payload.array), source);
}

void SetObject(Value& value, const Object& source) {
    SetType(value, TypeObject);
    AssignObject(*Required(value.payload.object), source);
}

void SetValue(Value& value, const Value& source) {
    switch (source.type) {
    case TypeNull:
        SetType(value, TypeNull);
        break;
    case TypeBoolean:
        SetBoolean(value, source.payload.boolean);
        break;
    case TypeInteger:
        SetInteger(value, source.payload.integer);
        break;
    case TypeUInteger:
        SetUInteger(value, source.payload.uinteger);
        break;
    case TypeReal:
        SetReal(value, source.payload.real);
        break;
    case TypeString:
        SetString(value, *Required(source.payload.string));
        break;
    case TypeArray:
        SetArray(value, *Required(source.payload.array));
        break;
    case TypeObject:
        SetObject(value, *Required(source.payload.object));
        break;
    default:
        break;
    }
}

void ConstructRootParam(RootParam& root) {
    root.callback = nullptr;
    root.context = nullptr;
    auto* value = static_cast<Value*>(AllocateRequired(sizeof(Value)));
    InitValue(*value);
    SetType(*value, TypeNull);
    root.nullValue = value;
}

void DestroyRootParam(RootParam& root) {
    if (root.nullValue == nullptr) return;
    DestroyValue(*root.nullValue);
    Release(root.nullValue);
    root.nullValue = nullptr;
}

void DestroyValue(Value& value) {
    SetType(value, TypeNull);
    if (value.root == nullptr) return;
    DestroyRootParam(*value.root);
    Release(value.root);
    value.root = nullptr;
}

void SetParent(Value& value, const Value* parent) {
    value.parent = parent;
    if (value.parent == nullptr) {
        if (value.root == nullptr) {
            auto* root = static_cast<RootParam*>(AllocateRequired(sizeof(RootParam)));
            try {
                ConstructRootParam(*root);
            } catch (...) {
                Release(root);
                throw;
            }
            value.root = root;
            SetParent(*root->nullValue, &value);
        }
        value.root->context = nullptr;
        value.root->callback = nullptr;
    } else if (value.root != nullptr) {
        DestroyRootParam(*value.root);
        Release(value.root);
        value.root = nullptr;
    }
}

const Value* RootOf(const Value& value) {
    const Value* current = &value;
    while (current->parent != nullptr) current = current->parent;
    return current;
}

const Value* NullAccess(const Value& value, ValueType requested) {
    if (value.type != TypeNull) return nullptr;
    const Value* root = RootOf(value);
    if (root->root == nullptr || root->root->callback == nullptr) return nullptr;
    return &root->root->callback(requested, value.parent, root->root->context);
}

const Value* Missing(const Value& value) {
    const Value* root = RootOf(value);
    if (root != nullptr && root->root != nullptr) return root->root->nullValue;
    return &fallbackValue;
}

const Value* ValueAt(const Value& value, std::size_t index) {
    if (value.type == TypeArray && index < Required(value.payload.array)->size) return &ArrayAt(*value.payload.array, index)->value;
    if (value.type == TypeObject && index < Required(value.payload.object)->size) return &ObjectAt(*value.payload.object, index)->value;
    return nullptr;
}

const Value* ValueFor(const Value& value, const String& key) {
    if (value.type != TypeObject) return nullptr;
    TreeNode* node = Find(*Required(value.payload.object), key);
    if (node == value.payload.object->head) return nullptr;
    return &node->value;
}

void AppendReal(String& out, double value) {
    char buffer[512];
    std::size_t length = 0;
    if (std::isnan(value)) {
        std::memcpy(buffer, "nan", 3);
        length = 3;
    } else {
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, 6);
        if (result.ec != std::errc()) throw std::runtime_error("sce::Json: real formatting failed");
        length = static_cast<std::size_t>(result.ptr - buffer);
    }
    if (length > 255) length = 255;
    buffer[length] = '\0';
    char* point = std::strchr(buffer, '.');
    if (point != nullptr) {
        for (const char* digit = point + 1; *digit != '\0'; ++digit) {
            if (*digit != '0') point = nullptr;
        }
        if (point != nullptr) *point = '\0';
    }
    Append(out, buffer);
}

template <typename T>
void AppendInteger(String& out, T value) {
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    Append(out, buffer, static_cast<std::size_t>(result.ptr - buffer));
}

void ToString(const Value& value, String& out) {
    switch (value.type) {
    case TypeNull:
        Append(out, "null");
        break;
    case TypeBoolean:
        Append(out, value.payload.boolean ? "true" : "false");
        break;
    case TypeInteger:
        AppendInteger(out, value.payload.integer);
        break;
    case TypeUInteger:
        AppendInteger(out, value.payload.uinteger);
        break;
    case TypeReal:
        AppendReal(out, value.payload.real);
        break;
    case TypeString: {
        const String& text = *Required(value.payload.string);
        Append(out, Data(text), text.size);
        break;
    }
    case TypeArray:
        Append(out, "array");
        break;
    case TypeObject:
        Append(out, "object");
        break;
    default:
        break;
    }
}

void AppendEscaped(String& out, const String& text) {
    for (std::size_t index = 0; index < text.size; ++index) {
        const auto character = static_cast<unsigned char>(Data(text)[index]);
        switch (character) {
        case '\b':
            Append(out, "\\b");
            break;
        case '\t':
            Append(out, "\\t");
            break;
        case '\n':
            Append(out, "\\n");
            break;
        case '\f':
            Append(out, "\\f");
            break;
        case '\r':
            Append(out, "\\r");
            break;
        case '"':
            Append(out, "\\\"");
            break;
        case '/':
            Append(out, "\\/");
            break;
        case '\\':
            Append(out, "\\\\");
            break;
        default:
            if (character < 0x20 || character == 0x7f) {
                static constexpr char Hex[] = "0123456789abcdef";
                const char escape[6] = {'\\', 'u', '0', '0', Hex[character >> 4], Hex[character & 0xf]};
                Append(out, escape, sizeof(escape));
            } else {
                Append(out, static_cast<char>(character));
            }
            break;
        }
    }
}

void AppendQuoted(String& out, const String& text) {
    Append(out, '"');
    AppendEscaped(out, text);
    Append(out, '"');
}

int APS5_VABI DiscardSerialized(String&, void*) {
    return 0;
}

int Serialize(Value& value, String& out, SerializeCallback callback, void* context, Value* parent) {
    SetParent(value, parent);
    int result = 0;
    switch (value.type) {
    case TypeNull:
    case TypeBoolean:
    case TypeInteger:
    case TypeUInteger:
    case TypeReal:
        ToString(value, out);
        result = callback(out, context);
        break;
    case TypeString:
        AppendQuoted(out, *Required(value.payload.string));
        result = callback(out, context);
        break;
    case TypeArray: {
        Append(out, '[');
        result = callback(out, context);
        if (result < 0) break;
        const Array& array = *Required(value.payload.array);
        for (ListNode* node = array.head->next; node != array.head; node = node->next) {
            if (node != array.head->next) Append(out, ',');
            result = Serialize(node->value, out, callback, context, &value);
            if (result < 0) break;
        }
        if (result < 0) break;
        Append(out, ']');
        result = callback(out, context);
        break;
    }
    case TypeObject: {
        Append(out, '{');
        result = callback(out, context);
        if (result < 0) break;
        const Object& object = *Required(value.payload.object);
        for (TreeNode* node = object.head->left; node != object.head; node = Next(node)) {
            if (node != object.head->left) Append(out, ',');
            AppendQuoted(out, node->key);
            Append(out, ':');
            result = Serialize(node->value, out, callback, context, &value);
            if (result < 0) break;
        }
        if (result < 0) break;
        Append(out, '}');
        result = callback(out, context);
        break;
    }
    default:
        break;
    }
    return result;
}

struct BufferReader {
    const char* current;
    const char* end;
};

int APS5_VABI ReadBuffer(char& character, void* context) {
    auto* reader = static_cast<BufferReader*>(context);
    if (reader->current == reader->end) return 0;
    character = *reader->current++;
    return 0;
}

struct FileReader {
    FileStream* file;
    char* buffer;
    std::size_t bufferSize;
    char* data;
    std::size_t filled;
    std::size_t position;
};

int APS5_VABI ReadFile(char& character, void* context) {
    auto* reader = static_cast<FileReader*>(context);
    if (reader->file == nullptr) return ErrorFileOpen;
    if (reader->position == reader->filled) {
        const std::size_t count = fread_nid_postfix(reader->buffer, 1, reader->bufferSize, reader->file);
        if (count == 0) {
            character = '\0';
            return 0;
        }
        reader->position = 0;
        reader->filled = count;
    }
    character = reader->data[reader->position++];
    return 0;
}

int Get(InputStream& stream) {
    if (stream.pushedBack) {
        stream.pushedBack = false;
        return stream.character;
    }
    char character = static_cast<char>(0xff);
    stream.status = stream.read(character, stream.context);
    stream.character = static_cast<unsigned char>(character);
    return stream.character;
}

void Unget(InputStream& stream) {
    stream.pushedBack = true;
}

bool BufferExhausted(const InputStream& stream) {
    if (stream.read != ReadBuffer || stream.pushedBack) return false;
    const auto* reader = static_cast<const BufferReader*>(stream.context);
    return reader->current == reader->end;
}

void SkipWhitespace(InputStream& stream) {
    for (;;) {
        const int character = Get(stream);
        if (stream.status < 0) return;
        if (character != ' ' && character != '\t' && character != '\n' && character != '\r') {
            Unget(stream);
            return;
        }
    }
}

bool Expect(InputStream& stream, int expected) {
    SkipWhitespace(stream);
    if (stream.status < 0) return false;
    const int character = Get(stream);
    if (stream.status < 0) return false;
    if (character != expected) {
        Unget(stream);
        return false;
    }
    return true;
}

bool ExpectLiteral(InputStream& stream, const char* literal) {
    for (; *literal != '\0'; ++literal) {
        const int character = Get(stream);
        if (stream.status < 0) return false;
        if (character != *literal) {
            Unget(stream);
            return false;
        }
    }
    return true;
}

int ParseQuadHex(InputStream& stream) {
    int result = 0;
    for (int index = 0; index < 4; ++index) {
        int digit = Get(stream);
        if (stream.status < 0) return -1;
        if (digit == -1) return -1;
        if (digit >= '0' && digit <= '9') {
            digit -= '0';
        } else if (digit >= 'A' && digit <= 'F') {
            digit -= 'A' - 10;
        } else if (digit >= 'a' && digit <= 'f') {
            digit -= 'a' - 10;
        } else {
            Unget(stream);
            return -1;
        }
        result = result * 16 + digit;
    }
    return result;
}

bool ParseCodePoint(String& out, InputStream& stream) {
    int codePoint = ParseQuadHex(stream);
    if (codePoint == -1) return false;
    if (codePoint >= 0xd800 && codePoint <= 0xdfff) {
        if (codePoint >= 0xdc00) return false;
        if (Get(stream) != '\\' || Get(stream) != 'u') {
            Unget(stream);
            return false;
        }
        const int low = ParseQuadHex(stream);
        if (low < 0xdc00 || low > 0xdfff) return false;
        codePoint = ((codePoint & 0x3ff) << 10) + 0x10000 + (low & 0x3ff);
    }
    if (codePoint < 0x80) {
        Append(out, static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        Append(out, static_cast<char>(0xc0 | ((codePoint >> 6) & 0x1f)));
        Append(out, static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else if (codePoint < 0x10000) {
        Append(out, static_cast<char>(0xe0 | ((codePoint >> 12) & 0xf)));
        Append(out, static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        Append(out, static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else {
        Append(out, static_cast<char>(0xf0 | ((codePoint >> 18) & 0x7)));
        Append(out, static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
        Append(out, static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        Append(out, static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
    return true;
}

bool ParseString(String& out, InputStream& stream) {
    for (;;) {
        if (BufferExhausted(stream)) AllocationFailed();
        const int character = Get(stream);
        if (stream.status < 0) return false;
        if (character < 0x20) {
            Unget(stream);
            return false;
        }
        if (character == '"') return true;
        if (character != '\\') {
            Append(out, static_cast<char>(character));
            continue;
        }
        const int escape = Get(stream);
        if (stream.status < 0) return false;
        switch (escape) {
        case '"':
            Append(out, '"');
            break;
        case '\\':
            Append(out, '\\');
            break;
        case '/':
            Append(out, '/');
            break;
        case 'b':
            Append(out, '\b');
            break;
        case 'f':
            Append(out, '\f');
            break;
        case 'n':
            Append(out, '\n');
            break;
        case 'r':
            Append(out, '\r');
            break;
        case 't':
            Append(out, '\t');
            break;
        case 'u':
            if (!ParseCodePoint(out, stream)) return false;
            break;
        default:
            return false;
        }
    }
}

bool ParseValue(Value& value, InputStream& stream, Value* parent);

bool ParseStringValue(Value& value, InputStream& stream, Value* parent) {
    SetType(value, TypeString);
    SetParent(value, parent);
    return ParseString(*Required(value.type == TypeString ? value.payload.string : nullptr), stream);
}

bool ParseArray(Value& value, InputStream& stream, Value* parent) {
    SetType(value, TypeArray);
    SetParent(value, parent);
    Array* array = value.type == TypeArray ? value.payload.array : nullptr;
    if (Expect(stream, ']')) return true;
    for (;;) {
        Value element {};
        InitValue(element);
        try {
            InsertBefore(*Required(array), array->head, element);
        } catch (...) {
            DestroyValue(element);
            throw;
        }
        DestroyValue(element);
        if (!ParseValue(array->head->previous->value, stream, &value)) return false;
        if (!Expect(stream, ',')) return Expect(stream, ']');
    }
}

bool ParseObject(Value& value, InputStream& stream, Value* parent) {
    SetType(value, TypeObject);
    SetParent(value, parent);
    Object* object = value.type == TypeObject ? value.payload.object : nullptr;
    if (Expect(stream, '}')) return true;
    LocalString key;
    for (;;) {
        if (!Expect(stream, '"') || !ParseString(key.text, stream) || !Expect(stream, ':')) return false;
        Value& member = Subscript(*Required(object), key.text);
        if (!ParseValue(member, stream, &value)) return false;
        Truncate(key.text);
        if (!Expect(stream, ',')) return Expect(stream, '}');
    }
}

bool Overflows(const char* begin, const char* end) {
    const char* cursor = begin;
    if (cursor != end && (*cursor == '-' || *cursor == '+')) ++cursor;
    long long magnitude = 0;
    bool significant = false;
    bool fraction = false;
    for (; cursor != end && ((*cursor >= '0' && *cursor <= '9') || *cursor == '.'); ++cursor) {
        if (*cursor == '.') {
            fraction = true;
        } else if (!significant && *cursor == '0') {
            if (fraction) --magnitude;
        } else {
            if (!significant) {
                significant = true;
                if (fraction) --magnitude;
            }
            if (!fraction) ++magnitude;
        }
    }
    long long exponent = 0;
    if (cursor != end && (*cursor == 'e' || *cursor == 'E')) {
        ++cursor;
        bool negative = false;
        if (cursor != end && (*cursor == '-' || *cursor == '+')) negative = *cursor++ == '-';
        for (; cursor != end && *cursor >= '0' && *cursor <= '9'; ++cursor) {
            if (exponent < 1000000000) exponent = exponent * 10 + (*cursor - '0');
        }
        if (negative) exponent = -exponent;
    }
    return magnitude + exponent > 0;
}

double ParseReal(const String& text) {
    const char* begin = Data(text);
    const char* end = begin + text.size;
    double result = 0.0;
    const auto parsed = std::from_chars(begin, end, result);
    if (parsed.ec == std::errc::invalid_argument) return 0.0;
    if (parsed.ec == std::errc::result_out_of_range) {
        const bool negative = begin != end && *begin == '-';
        if (Overflows(begin, parsed.ptr)) return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
        return negative ? -0.0 : 0.0;
    }
    return result;
}

bool ParseNumber(Value& value, InputStream& stream, Value* parent) {
    LocalString text;
    bool real = false;
    for (;;) {
        const int character = Get(stream);
        if (stream.status < 0) return false;
        if (character >= '0' && character <= '9') {
            Append(text.text, static_cast<char>(character));
        } else if (character == '-' || character == '+' || character == '.' || character == 'e' || character == 'E') {
            if (text.text.size != 0) real = true;
            Append(text.text, static_cast<char>(character));
        } else {
            Unget(stream);
            break;
        }
    }
    if (!real) {
        const char* digit = Data(text.text);
        if (*digit == '-') ++digit;
        std::uint64_t magnitude = 0;
        for (; *digit != '\0'; ++digit) {
            const std::uint64_t next = magnitude * 10 + static_cast<std::uint64_t>(static_cast<std::int64_t>(*digit - '0'));
            if (next < magnitude) {
                real = true;
                break;
            }
            magnitude = next;
        }
        if (!real) {
            if (Data(text.text)[0] == '-') {
                if (magnitude < 0x8000000000000000ull) {
                    SetInteger(value, static_cast<std::int64_t>(0 - magnitude));
                    SetParent(value, parent);
                    return true;
                }
            } else {
                SetUInteger(value, magnitude);
                SetParent(value, parent);
                return true;
            }
        }
    }
    SetReal(value, ParseReal(text.text));
    SetParent(value, parent);
    return true;
}

bool ParseValue(Value& value, InputStream& stream, Value* parent) {
    SkipWhitespace(stream);
    if (stream.status < 0) return false;
    const int character = Get(stream);
    if (stream.status < 0) return false;
    switch (character) {
    case 'n':
        if (!ExpectLiteral(stream, "ull")) return false;
        SetType(value, TypeNull);
        SetParent(value, parent);
        return true;
    case 'f':
        if (!ExpectLiteral(stream, "alse")) return false;
        SetBoolean(value, false);
        SetParent(value, parent);
        return true;
    case 't':
        if (!ExpectLiteral(stream, "rue")) return false;
        SetBoolean(value, true);
        SetParent(value, parent);
        return true;
    case '"':
        return ParseStringValue(value, stream, parent);
    case '[':
        return ParseArray(value, stream, parent);
    case '{':
        return ParseObject(value, stream, parent);
    default:
        Unget(stream);
        if ((character >= '0' && character <= '9') || character == '-') return ParseNumber(value, stream, parent);
        return false;
    }
}

int Parse(Value& value, ParserReadCallback read, void* context) {
    if (_ZN3sce4Json11s_initparamE_nid_no_patch == nullptr) return ErrorNotInitialized;
    InputStream stream {read, context, -1, 0, false};
    try {
        if (ParseValue(value, stream, nullptr)) return 0;
        return stream.status < 0 ? stream.status : ErrorParse;
    } catch (...) {
        SetType(value, TypeNull);
        return ErrorNoMemory;
    }
}

void ReleaseShared() {
    if (_ZN3sce4Json5Value12s_nullstringE_nid_no_patch != nullptr) {
        DestroyString(*_ZN3sce4Json5Value12s_nullstringE_nid_no_patch);
        Release(_ZN3sce4Json5Value12s_nullstringE_nid_no_patch);
        _ZN3sce4Json5Value12s_nullstringE_nid_no_patch = nullptr;
    }
    if (_ZN3sce4Json5Value11s_nullarrayE_nid_no_patch != nullptr) {
        DestroyArray(*_ZN3sce4Json5Value11s_nullarrayE_nid_no_patch);
        Release(_ZN3sce4Json5Value11s_nullarrayE_nid_no_patch);
        _ZN3sce4Json5Value11s_nullarrayE_nid_no_patch = nullptr;
    }
    if (_ZN3sce4Json5Value12s_nullobjectE_nid_no_patch != nullptr) {
        DestroyObject(*_ZN3sce4Json5Value12s_nullobjectE_nid_no_patch);
        Release(_ZN3sce4Json5Value12s_nullobjectE_nid_no_patch);
        _ZN3sce4Json5Value12s_nullobjectE_nid_no_patch = nullptr;
    }
    Release(_ZN3sce4Json11s_initparamE_nid_no_patch);
    _ZN3sce4Json11s_initparamE_nid_no_patch = nullptr;
}

int ParseFile(Value& value, const char* path) {
    if (_ZN3sce4Json11s_initparamE_nid_no_patch == nullptr) return ErrorNotInitialized;
    if (_ZN3sce4Json11s_initparamE_nid_no_patch->fileBufferSize == 0) return ErrorFileBufferSize;
    try {
        FileReader reader {};
        const std::size_t size = _ZN3sce4Json11s_initparamE_nid_no_patch->fileBufferSize;
        reader.buffer = static_cast<char*>(Allocate(size));
        if (reader.buffer != nullptr) {
            reader.bufferSize = size;
            reader.data = reader.buffer;
            reader.file = fopen_nid_postfix(path, "rb");
        }
        int result = 0;
        try {
            result = Parse(value, ReadFile, &reader);
        } catch (...) {
            if (reader.file != nullptr) fclose_nid_postfix(reader.file);
            Release(reader.buffer);
            throw;
        }
        if (reader.file != nullptr) fclose_nid_postfix(reader.file);
        Release(reader.buffer);
        return result;
    } catch (...) {
        return ErrorNoMemory;
    }
}

}

extern "C" {

void* APS5_VABI _ZN3sce4Json6MallocEm_nid_no_patch(std::size_t size) {
    return Allocate(size);
}
APS5_EXPORT("Xzy0onDzSAc", _ZN3sce4Json6MallocEm_nid_no_patch);

void APS5_VABI _ZN3sce4Json4FreeEPv_nid_no_patch(void* pointer) {
    Release(pointer);
}
APS5_EXPORT("bnk+edDbqMk", _ZN3sce4Json4FreeEPv_nid_no_patch);

void APS5_VABI _ZN3sce4Json9RootParamC1Ev_nid_no_patch(RootParam* self) {
    ConstructRootParam(*self);
}
APS5_EXPORT("Xkq8nvo4tKg", _ZN3sce4Json9RootParamC1Ev_nid_no_patch);

void APS5_VABI _ZN3sce4Json9RootParamC2Ev_nid_no_patch(RootParam* self) {
    ConstructRootParam(*self);
}
APS5_EXPORT("qDu+t28zExY", _ZN3sce4Json9RootParamC2Ev_nid_no_patch);

void APS5_VABI _ZN3sce4Json9RootParamD1Ev_nid_no_patch(RootParam* self) {
    DestroyRootParam(*self);
}
APS5_EXPORT("vOTH9Iec7Yc", _ZN3sce4Json9RootParamD1Ev_nid_no_patch);

void APS5_VABI _ZN3sce4Json9RootParamD2Ev_nid_no_patch(RootParam* self) {
    DestroyRootParam(*self);
}
APS5_EXPORT("gyz287utQ6c", _ZN3sce4Json9RootParamD2Ev_nid_no_patch);

void APS5_VABI _ZN3sce4Json11InitializerC1Ev_nid_no_patch(Initializer* self) {
    self->initialized = false;
}
APS5_EXPORT("cK6bYHf-Q5E", _ZN3sce4Json11InitializerC1Ev_nid_no_patch);

void APS5_VABI _ZN3sce4Json11InitializerC2Ev_nid_no_patch(Initializer* self) {
    self->initialized = false;
}
APS5_EXPORT("6qFqND4iwPA", _ZN3sce4Json11InitializerC2Ev_nid_no_patch);

int APS5_VABI _ZN3sce4Json11Initializer9terminateEv_nid_no_patch(Initializer* self) {
    if (_ZN3sce4Json11s_initparamE_nid_no_patch == nullptr || !self->initialized) return ErrorNotInitialized;
    ReleaseShared();
    self->initialized = false;
    return 0;
}
APS5_EXPORT("PR5k1penBLM", _ZN3sce4Json11Initializer9terminateEv_nid_no_patch);

void APS5_VABI _ZN3sce4Json11InitializerD1Ev_nid_no_patch(Initializer* self) {
    _ZN3sce4Json11Initializer9terminateEv_nid_no_patch(self);
}
APS5_EXPORT("RujUxbr3haM", _ZN3sce4Json11InitializerD1Ev_nid_no_patch);

void APS5_VABI _ZN3sce4Json11InitializerD2Ev_nid_no_patch(Initializer* self) {
    _ZN3sce4Json11Initializer9terminateEv_nid_no_patch(self);
}
APS5_EXPORT("qQZGdYkEytk", _ZN3sce4Json11InitializerD2Ev_nid_no_patch);

int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE_nid_no_patch(Initializer* self, const InitParameter* parameter) {
    if (_ZN3sce4Json11s_initparamE_nid_no_patch != nullptr || self->initialized) return ErrorAlreadyInitialized;
    auto* stored = static_cast<InitParameter*>(AllocateFrom(*parameter, sizeof(InitParameter)));
    *Required(stored) = {};
    _ZN3sce4Json11s_initparamE_nid_no_patch = stored;
    *stored = *parameter;
    void* pending = nullptr;
    try {
        auto* text = static_cast<String*>(pending = AllocateFrom(*parameter, sizeof(String)));
        ConstructString(*Required(text));
        _ZN3sce4Json5Value12s_nullstringE_nid_no_patch = text;
        pending = nullptr;
        auto* array = static_cast<Array*>(pending = AllocateFrom(*parameter, sizeof(Array)));
        ConstructArray(*Required(array));
        _ZN3sce4Json5Value11s_nullarrayE_nid_no_patch = array;
        pending = nullptr;
        auto* object = static_cast<Object*>(pending = AllocateFrom(*parameter, sizeof(Object)));
        ConstructObject(*Required(object));
        _ZN3sce4Json5Value12s_nullobjectE_nid_no_patch = object;
        pending = nullptr;
    } catch (...) {
        if (pending != nullptr) Release(pending);
        ReleaseShared();
        throw;
    }
    self->initialized = true;
    return 0;
}
APS5_EXPORT("Cxwy7wHq4J0", _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueD1Ev_nid_no_patch(Value* self) {
    DestroyValue(*self);
}
APS5_EXPORT("WTtYf+cNnXI", _ZN3sce4Json5ValueD1Ev_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueD2Ev_nid_no_patch(Value* self) {
    DestroyValue(*self);
}
APS5_EXPORT("0eUrW9JAxM0", _ZN3sce4Json5ValueD2Ev_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC1Ev_nid_no_patch(Value* self) {
    InitValue(*self);
}
APS5_EXPORT("qBMjqyBn3OM", _ZN3sce4Json5ValueC1Ev_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC2Ev_nid_no_patch(Value* self) {
    InitValue(*self);
}
APS5_EXPORT("-wa17B7TGnw", _ZN3sce4Json5ValueC2Ev_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE_nid_no_patch(Value* self, ValueType type) {
    InitValue(*self);
    SetType(*self, type);
}
APS5_EXPORT("CbrT3dwDILo", _ZN3sce4Json5ValueC1ENS0_9ValueTypeE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC2ENS0_9ValueTypeE_nid_no_patch(Value* self, ValueType type) {
    InitValue(*self);
    SetType(*self, type);
}
APS5_EXPORT("PWm9MyJJVqU", _ZN3sce4Json5ValueC2ENS0_9ValueTypeE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC1Eb_nid_no_patch(Value* self, bool value) {
    InitValue(*self);
    SetBoolean(*self, value);
}
APS5_EXPORT("UeuWT+yNdCQ", _ZN3sce4Json5ValueC1Eb_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC2Eb_nid_no_patch(Value* self, bool value) {
    InitValue(*self);
    SetBoolean(*self, value);
}
APS5_EXPORT("OK4Ot0ue7J0", _ZN3sce4Json5ValueC2Eb_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC1El_nid_no_patch(Value* self, std::int64_t value) {
    InitValue(*self);
    SetInteger(*self, value);
}
APS5_EXPORT("0lLK8+kDqmE", _ZN3sce4Json5ValueC1El_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC2El_nid_no_patch(Value* self, std::int64_t value) {
    InitValue(*self);
    SetInteger(*self, value);
}
APS5_EXPORT("EjxUpzR2Yx8", _ZN3sce4Json5ValueC2El_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC1Em_nid_no_patch(Value* self, std::uint64_t value) {
    InitValue(*self);
    SetUInteger(*self, value);
}
APS5_EXPORT("x4AUdbhpRB0", _ZN3sce4Json5ValueC1Em_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC2Em_nid_no_patch(Value* self, std::uint64_t value) {
    InitValue(*self);
    SetUInteger(*self, value);
}
APS5_EXPORT("cqucBPSeVDA", _ZN3sce4Json5ValueC2Em_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC1Ed_nid_no_patch(Value* self, double value) {
    InitValue(*self);
    SetReal(*self, value);
}
APS5_EXPORT("sOmU4vnx3s0", _ZN3sce4Json5ValueC1Ed_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC2Ed_nid_no_patch(Value* self, double value) {
    InitValue(*self);
    SetReal(*self, value);
}
APS5_EXPORT("23nm4oXHlfI", _ZN3sce4Json5ValueC2Ed_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC1ERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(Value* self, const String* value) {
    InitValue(*self);
    SetString(*self, *value);
}
APS5_EXPORT("geskGcdamVE", _ZN3sce4Json5ValueC1ERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC2ERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(Value* self, const String* value) {
    InitValue(*self);
    SetString(*self, *value);
}
APS5_EXPORT("QaLXUjWq5uw", _ZN3sce4Json5ValueC2ERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC1ERKSt4listIS1_NS0_8StlAllocIS1_EEE_nid_no_patch(Value* self, const Array* value) {
    InitValue(*self);
    SetArray(*self, *value);
}
APS5_EXPORT("JGF+59cTHl0", _ZN3sce4Json5ValueC1ERKSt4listIS1_NS0_8StlAllocIS1_EEE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC2ERKSt4listIS1_NS0_8StlAllocIS1_EEE_nid_no_patch(Value* self, const Array* value) {
    InitValue(*self);
    SetArray(*self, *value);
}
APS5_EXPORT("qwP9U9HsZ6k", _ZN3sce4Json5ValueC2ERKSt4listIS1_NS0_8StlAllocIS1_EEE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC1ERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE_nid_no_patch(Value* self, const Object* value) {
    InitValue(*self);
    SetObject(*self, *value);
}
APS5_EXPORT("xxZjYrZssiI", _ZN3sce4Json5ValueC1ERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC2ERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE_nid_no_patch(Value* self, const Object* value) {
    InitValue(*self);
    SetObject(*self, *value);
}
APS5_EXPORT("iATcGLFMLw4", _ZN3sce4Json5ValueC2ERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC1ERKS1__nid_no_patch(Value* self, const Value* value) {
    CopyValue(*self, *value);
}
APS5_EXPORT("fSb2oQTNrgA", _ZN3sce4Json5ValueC1ERKS1__nid_no_patch);

void APS5_VABI _ZN3sce4Json5ValueC2ERKS1__nid_no_patch(Value* self, const Value* value) {
    CopyValue(*self, *value);
}
APS5_EXPORT("TZyuFeGuw9Y", _ZN3sce4Json5ValueC2ERKS1__nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value3setENS0_9ValueTypeE_nid_no_patch(Value* self, ValueType type) {
    SetType(*self, type);
}
APS5_EXPORT("IKQimvG9Wqs", _ZN3sce4Json5Value3setENS0_9ValueTypeE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value3setEb_nid_no_patch(Value* self, bool value) {
    SetBoolean(*self, value);
}
APS5_EXPORT("5yHuiWXo2gg", _ZN3sce4Json5Value3setEb_nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value3setEl_nid_no_patch(Value* self, std::int64_t value) {
    SetInteger(*self, value);
}
APS5_EXPORT("QxVVYhP-mvg", _ZN3sce4Json5Value3setEl_nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value3setEm_nid_no_patch(Value* self, std::uint64_t value) {
    SetUInteger(*self, value);
}
APS5_EXPORT("SIe1ZmW7e7s", _ZN3sce4Json5Value3setEm_nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value3setEd_nid_no_patch(Value* self, double value) {
    SetReal(*self, value);
}
APS5_EXPORT("BSmWDIkV4w4", _ZN3sce4Json5Value3setEd_nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value3setERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(Value* self, const String* value) {
    SetString(*self, *value);
}
APS5_EXPORT("pcLFw6F9XHo", _ZN3sce4Json5Value3setERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value3setERKSt4listIS1_NS0_8StlAllocIS1_EEE_nid_no_patch(Value* self, const Array* value) {
    SetArray(*self, *value);
}
APS5_EXPORT("I8H3EdOu8cw", _ZN3sce4Json5Value3setERKSt4listIS1_NS0_8StlAllocIS1_EEE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value3setERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE_nid_no_patch(Value* self, const Object* value) {
    SetObject(*self, *value);
}
APS5_EXPORT("kLzq1bVqeWg", _ZN3sce4Json5Value3setERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE_nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value3setERKS1__nid_no_patch(Value* self, const Value* value) {
    SetValue(*self, *value);
}
APS5_EXPORT("XL8+BUqjB1w", _ZN3sce4Json5Value3setERKS1__nid_no_patch);

Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1__nid_no_patch(Value* self, const Value* value) {
    SetValue(*self, *value);
    return self;
}
APS5_EXPORT("4zrm6VrgIAw", _ZN3sce4Json5ValueaSERKS1__nid_no_patch);

bool APS5_VABI _ZNK3sce4Json5ValuecvbEv_nid_no_patch(const Value* self) {
    switch (self->type) {
    case TypeNull:
        return false;
    case TypeBoolean:
        return self->payload.boolean;
    case TypeInteger:
        return self->payload.integer != 0;
    case TypeUInteger:
        return self->payload.uinteger != 0;
    case TypeReal:
        return self->payload.real != 0.0;
    case TypeString:
        return Required(self->payload.string)->size != 0;
    default:
        return true;
    }
}
APS5_EXPORT("a-aMMUXqrN0", _ZNK3sce4Json5ValuecvbEv_nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value4swapERS1__nid_no_patch(Value* self, Value* other) {
    const ValueType type = self->type;
    self->type = other->type;
    other->type = type;
    const auto payload = self->payload;
    self->payload = other->payload;
    other->payload = payload;
}
APS5_EXPORT("9PqrCLfKZZI", _ZN3sce4Json5Value4swapERS1__nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value5clearEv_nid_no_patch(Value* self) {
    SetType(*self, TypeNull);
}
APS5_EXPORT("FIjXN2TkuTs", _ZN3sce4Json5Value5clearEv_nid_no_patch);

int APS5_VABI _ZNK3sce4Json5Value5countEv_nid_no_patch(const Value* self) {
    if (self->type == TypeArray) return static_cast<int>(Required(self->payload.array)->size);
    if (self->type == TypeObject) return static_cast<int>(Required(self->payload.object)->size);
    return 0;
}
APS5_EXPORT("RBw+4NukeGQ", _ZNK3sce4Json5Value5countEv_nid_no_patch);

ValueType APS5_VABI _ZNK3sce4Json5Value7getTypeEv_nid_no_patch(const Value* self) {
    return self->type;
}
APS5_EXPORT("SHtAad20YYM", _ZNK3sce4Json5Value7getTypeEv_nid_no_patch);

const Value* APS5_VABI _ZNK3sce4Json5Value7getRootEv_nid_no_patch(const Value* self) {
    return RootOf(*self);
}
APS5_EXPORT("47s0O9-6E-0", _ZNK3sce4Json5Value7getRootEv_nid_no_patch);

void APS5_VABI _ZN3sce4Json5Value9setParentEPKS1__nid_no_patch(Value* self, const Value* parent) {
    SetParent(*self, parent);
}
APS5_EXPORT("M-tWavI4cgg", _ZN3sce4Json5Value9setParentEPKS1__nid_no_patch);

int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6__nid_no_patch(Value* self, NullAccessCallback callback, void* context) {
    if (self->parent != nullptr || self->root == nullptr) return ErrorNotRoot;
    self->root->callback = callback;
    self->root->context = context;
    return 0;
}
APS5_EXPORT("Xbl-LYVFNEE", _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6__nid_no_patch);

void APS5_VABI _ZNK3sce4Json5Value8toStringERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(const Value* self, String* out) {
    ToString(*self, *out);
}
APS5_EXPORT("RQpBshscViM", _ZNK3sce4Json5Value8toStringERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch);

int APS5_VABI _ZN3sce4Json5Value18serialize_internalERSbIcSt11char_traitsIcENS0_8StlAllocIcEEEPFiS7_PvES8_PS1__nid_no_patch(Value* self, String* out, SerializeCallback callback, void* context, Value* parent) {
    return Serialize(*self, *out, callback, context, parent);
}
APS5_EXPORT("bkVdWPESKhQ", _ZN3sce4Json5Value18serialize_internalERSbIcSt11char_traitsIcENS0_8StlAllocIcEEEPFiS7_PvES8_PS1__nid_no_patch);

int APS5_VABI _ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEEPFiS7_PvES8__nid_no_patch(Value* self, String* out, SerializeCallback callback, void* context) {
    if (_ZN3sce4Json11s_initparamE_nid_no_patch == nullptr) return ErrorNotInitialized;
    return Serialize(*self, *out, callback, context, nullptr);
}
APS5_EXPORT("HlDiu7tCnh8", _ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEEPFiS7_PvES8__nid_no_patch);

int APS5_VABI _ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(Value* self, String* out) {
    if (_ZN3sce4Json11s_initparamE_nid_no_patch == nullptr) return ErrorNotInitialized;
    return Serialize(*self, *out, DiscardSerialized, nullptr, nullptr);
}
APS5_EXPORT("GAccTNWvZ8o", _ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch);

String* APS5_VABI _ZN3sce4Json5Value11referStringEv_nid_no_patch(Value* self) {
    return self->type == TypeString ? self->payload.string : nullptr;
}
APS5_EXPORT("m5J28iw-QPs", _ZN3sce4Json5Value11referStringEv_nid_no_patch);

Array* APS5_VABI _ZN3sce4Json5Value10referArrayEv_nid_no_patch(Value* self) {
    return self->type == TypeArray ? self->payload.array : nullptr;
}
APS5_EXPORT("nM5XqdeXFPw", _ZN3sce4Json5Value10referArrayEv_nid_no_patch);

Object* APS5_VABI _ZN3sce4Json5Value11referObjectEv_nid_no_patch(Value* self) {
    return self->type == TypeObject ? self->payload.object : nullptr;
}
APS5_EXPORT("-NxEk7XLkDY", _ZN3sce4Json5Value11referObjectEv_nid_no_patch);

std::int64_t* APS5_VABI _ZN3sce4Json5Value12referIntegerEv_nid_no_patch(Value* self) {
    return self->type == TypeInteger || self->type == TypeUInteger ? &self->payload.integer : nullptr;
}
APS5_EXPORT("R0ac5QOdlpo", _ZN3sce4Json5Value12referIntegerEv_nid_no_patch);

std::uint64_t* APS5_VABI _ZN3sce4Json5Value13referUIntegerEv_nid_no_patch(Value* self) {
    return self->type == TypeUInteger || self->type == TypeInteger ? &self->payload.uinteger : nullptr;
}
APS5_EXPORT("Nx5tez4siL4", _ZN3sce4Json5Value13referUIntegerEv_nid_no_patch);

double* APS5_VABI _ZN3sce4Json5Value9referRealEv_nid_no_patch(Value* self) {
    return self->type == TypeReal ? &self->payload.real : nullptr;
}
APS5_EXPORT("t7UEJrhojhk", _ZN3sce4Json5Value9referRealEv_nid_no_patch);

bool* APS5_VABI _ZN3sce4Json5Value12referBooleanEv_nid_no_patch(Value* self) {
    return self->type == TypeBoolean ? &self->payload.boolean : nullptr;
}
APS5_EXPORT("QxjfcfBhEdc", _ZN3sce4Json5Value12referBooleanEv_nid_no_patch);

Value* APS5_VABI _ZN3sce4Json5Value10referValueEm_nid_no_patch(Value* self, std::size_t index) {
    return const_cast<Value*>(ValueAt(*self, index));
}
APS5_EXPORT("gLzCc67aTbw", _ZN3sce4Json5Value10referValueEm_nid_no_patch);

Value* APS5_VABI _ZN3sce4Json5Value10referValueERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(Value* self, const String* key) {
    return const_cast<Value*>(ValueFor(*self, *key));
}
APS5_EXPORT("Z3mrBp7y1Lc", _ZN3sce4Json5Value10referValueERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch);

const String* APS5_VABI _ZNK3sce4Json5Value9getStringEv_nid_no_patch(const Value* self) {
    if (const Value* redirected = NullAccess(*self, TypeString)) return _ZNK3sce4Json5Value9getStringEv_nid_no_patch(redirected);
    return self->type == TypeString ? self->payload.string : _ZN3sce4Json5Value12s_nullstringE_nid_no_patch;
}
APS5_EXPORT("epJ6x2LV0kU", _ZNK3sce4Json5Value9getStringEv_nid_no_patch);

const Array* APS5_VABI _ZNK3sce4Json5Value8getArrayEv_nid_no_patch(const Value* self) {
    if (const Value* redirected = NullAccess(*self, TypeArray)) return _ZNK3sce4Json5Value8getArrayEv_nid_no_patch(redirected);
    return self->type == TypeArray ? self->payload.array : _ZN3sce4Json5Value11s_nullarrayE_nid_no_patch;
}
APS5_EXPORT("ONT8As5R1ug", _ZNK3sce4Json5Value8getArrayEv_nid_no_patch);

const Object* APS5_VABI _ZNK3sce4Json5Value9getObjectEv_nid_no_patch(const Value* self) {
    if (const Value* redirected = NullAccess(*self, TypeObject)) return _ZNK3sce4Json5Value9getObjectEv_nid_no_patch(redirected);
    return self->type == TypeObject ? self->payload.object : _ZN3sce4Json5Value12s_nullobjectE_nid_no_patch;
}
APS5_EXPORT("IlsmvBtMkak", _ZNK3sce4Json5Value9getObjectEv_nid_no_patch);

const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv_nid_no_patch(const Value* self) {
    if (const Value* redirected = NullAccess(*self, TypeInteger)) return _ZNK3sce4Json5Value10getIntegerEv_nid_no_patch(redirected);
    return self->type == TypeInteger || self->type == TypeUInteger ? &self->payload.integer : &_ZN3sce4Json5Value13s_nullintegerE_nid_no_patch;
}
APS5_EXPORT("DIxvoy7Ngvk", _ZNK3sce4Json5Value10getIntegerEv_nid_no_patch);

const std::uint64_t* APS5_VABI _ZNK3sce4Json5Value11getUIntegerEv_nid_no_patch(const Value* self) {
    if (const Value* redirected = NullAccess(*self, TypeUInteger)) return _ZNK3sce4Json5Value11getUIntegerEv_nid_no_patch(redirected);
    return self->type == TypeUInteger || self->type == TypeInteger ? &self->payload.uinteger : &_ZN3sce4Json5Value14s_nulluintegerE_nid_no_patch;
}
APS5_EXPORT("sn4HNCtNRzY", _ZNK3sce4Json5Value11getUIntegerEv_nid_no_patch);

const double* APS5_VABI _ZNK3sce4Json5Value7getRealEv_nid_no_patch(const Value* self) {
    if (const Value* redirected = NullAccess(*self, TypeReal)) return _ZNK3sce4Json5Value7getRealEv_nid_no_patch(redirected);
    return self->type == TypeReal ? &self->payload.real : &_ZN3sce4Json5Value10s_nullrealE_nid_no_patch;
}
APS5_EXPORT("3qrge7L-AU4", _ZNK3sce4Json5Value7getRealEv_nid_no_patch);

const bool* APS5_VABI _ZNK3sce4Json5Value10getBooleanEv_nid_no_patch(const Value* self) {
    if (const Value* redirected = NullAccess(*self, TypeBoolean)) return _ZNK3sce4Json5Value10getBooleanEv_nid_no_patch(redirected);
    return self->type == TypeBoolean ? &self->payload.boolean : &_ZN3sce4Json5Value10s_nullboolE_nid_no_patch;
}
APS5_EXPORT("zTwZdI8AZ5Y", _ZNK3sce4Json5Value10getBooleanEv_nid_no_patch);

const Value* APS5_VABI _ZNK3sce4Json5Value8getValueEm_nid_no_patch(const Value* self, std::size_t index) {
    if (const Value* found = ValueAt(*self, index)) return found;
    return Missing(*self);
}
APS5_EXPORT("0YqYAoO-+Uo", _ZNK3sce4Json5Value8getValueEm_nid_no_patch);

const Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(const Value* self, const String* key) {
    if (const Value* found = ValueFor(*self, *key)) return found;
    return Missing(*self);
}
APS5_EXPORT("1sKGLPtkdow", _ZNK3sce4Json5Value8getValueERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch);

const Value* APS5_VABI _ZNK3sce4Json5ValueixEm_nid_no_patch(const Value* self, std::size_t index) {
    return _ZNK3sce4Json5Value8getValueEm_nid_no_patch(self, index);
}
APS5_EXPORT("XlWbvieLj2M", _ZNK3sce4Json5ValueixEm_nid_no_patch);

const Value* APS5_VABI _ZNK3sce4Json5ValueixERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(const Value* self, const String* key) {
    return _ZNK3sce4Json5Value8getValueERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(self, key);
}
APS5_EXPORT("F-bQmUqmTpI", _ZNK3sce4Json5ValueixERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch);

const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc_nid_no_patch(const Value* self, const char* key) {
    LocalString text;
    Append(text.text, key);
    return _ZNK3sce4Json5Value8getValueERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(self, &text.text);
}
APS5_EXPORT("HwDt5lD9Bfo", _ZNK3sce4Json5ValueixEPKc_nid_no_patch);

int APS5_VABI _ZN3sce4Json6Parser12parseQuadHexERNS0_11InputStreamE_nid_no_patch(InputStream* stream) {
    return ParseQuadHex(*stream);
}
APS5_EXPORT("hjDFaEWCV20", _ZN3sce4Json6Parser12parseQuadHexERNS0_11InputStreamE_nid_no_patch);

bool APS5_VABI _ZN3sce4Json6Parser14parseCodePointERSbIcSt11char_traitsIcENS0_8StlAllocIcEEERNS0_11InputStreamE_nid_no_patch(String* out, InputStream* stream) {
    return ParseCodePoint(*out, *stream);
}
APS5_EXPORT("6mTvUfQJa1I", _ZN3sce4Json6Parser14parseCodePointERSbIcSt11char_traitsIcENS0_8StlAllocIcEEERNS0_11InputStreamE_nid_no_patch);

bool APS5_VABI _ZN3sce4Json6Parser11parseStringERSbIcSt11char_traitsIcENS0_8StlAllocIcEEERNS0_11InputStreamE_nid_no_patch(String* out, InputStream* stream) {
    return ParseString(*out, *stream);
}
APS5_EXPORT("QXUHGgXn4JA", _ZN3sce4Json6Parser11parseStringERSbIcSt11char_traitsIcENS0_8StlAllocIcEEERNS0_11InputStreamE_nid_no_patch);

bool APS5_VABI _ZN3sce4Json6Parser11parseStringERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch(Value* value, InputStream* stream, Value* parent) {
    return ParseStringValue(*value, *stream, parent);
}
APS5_EXPORT("bglw6PJL4nw", _ZN3sce4Json6Parser11parseStringERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch);

bool APS5_VABI _ZN3sce4Json6Parser10parseArrayERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch(Value* value, InputStream* stream, Value* parent) {
    return ParseArray(*value, *stream, parent);
}
APS5_EXPORT("cYDA0oEX5Y4", _ZN3sce4Json6Parser10parseArrayERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch);

bool APS5_VABI _ZN3sce4Json6Parser10parseValueERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch(Value* value, InputStream* stream, Value* parent) {
    return ParseValue(*value, *stream, parent);
}
APS5_EXPORT("vR3+bfUo-Qo", _ZN3sce4Json6Parser10parseValueERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch);

bool APS5_VABI _ZN3sce4Json6Parser11parseObjectERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch(Value* value, InputStream* stream, Value* parent) {
    return ParseObject(*value, *stream, parent);
}
APS5_EXPORT("HwBrtLl-5IY", _ZN3sce4Json6Parser11parseObjectERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch);

bool APS5_VABI _ZN3sce4Json6Parser11parseNumberERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch(Value* value, InputStream* stream, Value* parent) {
    return ParseNumber(*value, *stream, parent);
}
APS5_EXPORT("PpR7I7RCsY0", _ZN3sce4Json6Parser11parseNumberERNS0_5ValueERNS0_11InputStreamEPS2__nid_no_patch);

int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5__nid_no_patch(Value* value, ParserReadCallback read, void* context) {
    return Parse(*value, read, context);
}
APS5_EXPORT("itqj2YmuAa8", _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5__nid_no_patch);

int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm_nid_no_patch(Value* value, const char* text, std::size_t size) {
    if (_ZN3sce4Json11s_initparamE_nid_no_patch == nullptr) return ErrorNotInitialized;
    BufferReader reader {text, text + size};
    return Parse(*value, ReadBuffer, &reader);
}
APS5_EXPORT("S5JxQnoGF3E", _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm_nid_no_patch);

int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKc_nid_no_patch(Value* value, const char* path) {
    return ParseFile(*value, path);
}
APS5_EXPORT("LB3jxppxyKU", _ZN3sce4Json6Parser5parseERNS0_5ValueEPKc_nid_no_patch);

}
