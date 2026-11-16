#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::ObjectBase;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::StringValuePair;
using NKWebKit::JsonAbi::Value;
using NKWebKit::JsonAbi::ValueType;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static StringImpl* const DeletedKey = reinterpret_cast<StringImpl*>(~std::uintptr_t(0));

static String Make(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::char_traits<char>::length(text)));
}

static unsigned* Meta(StringValuePair* table) {
    return reinterpret_cast<unsigned*>(table);
}

static unsigned TableSize(const ObjectBase& object) {
    return object.map.table ? Meta(object.map.table)[-1] : 0U;
}

static unsigned KeyCount(const ObjectBase& object) {
    return object.map.table ? Meta(object.map.table)[-3] : 0U;
}

static unsigned DeletedCount(const ObjectBase& object) {
    return Meta(object.map.table)[-4];
}

static unsigned Hash(const String& key) {
    unsigned hash = key.impl->hashAndFlags >> 8;
    return hash ? hash : _ZNK3WTF10StringImpl12hashSlowCaseEv(key.impl);
}

static void AllocateTable(ObjectBase& object, unsigned size) {
    auto* storage = static_cast<unsigned char*>(_ZN3WTF16fastZeroedMallocEm(16U + std::size_t(size) * sizeof(StringValuePair)));
    object.map.table = reinterpret_cast<StringValuePair*>(storage + 16U);
    Meta(object.map.table)[-1] = size;
    Meta(object.map.table)[-2] = size - 1U;
    Meta(object.map.table)[-3] = 0U;
    Meta(object.map.table)[-4] = 0U;
}

static void Set(ObjectBase& object, const String& key, Ref<Value> value) {
    if (!object.map.table) AllocateTable(object, 8U);
    StringValuePair* table = object.map.table;
    unsigned mask = Meta(table)[-2];
    unsigned i = Hash(key) & mask;
    unsigned probe = 0;
    while (table[i].key.impl) {
        Require(table[i].key.impl == DeletedKey || !_ZN3WTF5equalEPKNS_10StringImplES2_(table[i].key.impl, key.impl));
        ++probe;
        i = (i + probe) & mask;
    }
    key.impl->refCount += 2U;
    table[i].key.impl = key.impl;
    table[i].value.ptr = value.Leak();
    Meta(table)[-3] += 1U;
    auto& order = object.order;
    if (order.size == order.capacity) {
        unsigned capacity = order.capacity ? order.capacity * 2U : 16U;
        auto* buffer = static_cast<String*>(_ZN3WTF10fastMallocEm(std::size_t(capacity) * sizeof(String)));
        for (unsigned j = 0U; j < order.size; ++j) buffer[j].impl = order.buffer[j].impl;
        if (order.buffer) _ZN3WTF8fastFreeEPv(order.buffer);
        order.buffer = buffer;
        order.capacity = capacity;
    }
    key.impl->refCount += 2U;
    order.buffer[order.size++].impl = key.impl;
}

static bool GetString(const ObjectBase& object, const String& key, String& out) {
    return _ZNK3WTF8JSONImpl10ObjectBase9getStringERKNS_6StringERS2_(&object, key, out);
}

static StringImpl* BucketKey(const ObjectBase& object, const String& key) {
    StringValuePair* table = object.map.table;
    for (unsigned i = 0U; i < TableSize(object); ++i) {
        if (table[i].key.impl == key.impl) return table[i].key.impl;
    }
    return nullptr;
}

static std::vector<String> CollidingKeys(unsigned mask, unsigned count) {
    std::vector<String> keys;
    unsigned target = 0;
    bool haveTarget = false;
    for (int n = 0; keys.size() < count; ++n) {
        std::string text = "key" + std::to_string(n);
        String key = Make(text.c_str());
        unsigned slot = Hash(key) & mask;
        if (!haveTarget) {
            target = slot;
            haveTarget = true;
        }
        if (slot == target) keys.push_back(std::move(key));
    }
    return keys;
}

static void TestCreate() {
    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    Require(object.Get() != nullptr);
    Require(object->refCount == 1U);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(object.Get()) == ValueType::Object);
    Require(object->map.table == nullptr);
    Require(object->order.buffer == nullptr);
    Require(object->order.size == 0U);
    Require(object->order.capacity == 0U);
    Require(_ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(object.Get()) == sizeof(Value*));
    Require(object->vptr->memoryCost(object.Get()) == sizeof(Value*));

    auto asObject = object->vptr->asObject(object.Get());
    Require(asObject.Get() == object.Get());
    Require(object->refCount == 2U);
    auto asConst = object->vptr->asObjectConst(object.Get());
    Require(asConst.Get() == object.Get());
    Require(!object->vptr->asArray(object.Get()));

    String missing = Make("missing");
    String out = Make("keep");
    StringImpl* before = out.impl;
    Require(!GetString(*object, missing, out));
    Require(out.impl == before);
    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), missing);
    Require(object->map.table == nullptr);
}

static void TestGetStringAndMemoryCost() {
    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    String nameKey = Make("name");
    String countKey = Make("count");
    String nullKey = Make("nothing");
    String value = Make("hello");
    Set(*object, nameKey, _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(value));
    Set(*object, countKey, _ZN3WTF8JSONImpl5Value6createEi(7));
    String nullString;
    Set(*object, nullKey, _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(nullString));

    String out = Make("old");
    unsigned valueRefs = value.impl->refCount;
    Require(GetString(*object, nameKey, out));
    Require(out.impl == value.impl);
    Require(value.impl->refCount == valueRefs + 2U);

    String lookup = Make("name");
    String out2;
    Require(GetString(*object, lookup, out2));
    Require(out2.impl == value.impl);

    String keep = Make("keep");
    StringImpl* before = keep.impl;
    Require(!GetString(*object, countKey, keep));
    Require(keep.impl == before);
    Require(!GetString(*object, nullKey, keep));
    Require(keep.impl == before);
    String absent = Make("absent");
    Require(!GetString(*object, absent, keep));
    Require(keep.impl == before);

    std::size_t expected = sizeof(Value*);
    expected += 4U + sizeof(Value*) + sizeof(StringImpl) + 5U;
    expected += 5U + sizeof(Value*);
    expected += 7U + sizeof(Value*);
    Require(_ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(object.Get()) == expected);
    Require(object->vptr->memoryCost(object.Get()) == expected);

    auto nested = _ZN3WTF8JSONImpl6Object6createEv();
    String innerKey = Make("inner");
    Set(*nested, innerKey, _ZN3WTF8JSONImpl5Value6createEb(true));
    std::size_t nestedCost = sizeof(Value*) + 5U + sizeof(Value*);
    Require(_ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(nested.Get()) == nestedCost);
    String nestedKey = Make("nested");
    Set(*object, nestedKey, Ref<Value>(nested));
    Require(_ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(object.Get()) == expected + 6U + nestedCost);
    Require(!GetString(*object, nestedKey, keep));

    bool threw = false;
    try {
        GetString(*object, nullString, keep);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestRemoveCollisions() {
    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    std::vector<String> keys = CollidingKeys(7U, 3U);
    std::vector<String> values;
    for (unsigned i = 0U; i < keys.size(); ++i) {
        values.push_back(Make(("v" + std::to_string(i)).c_str()));
        Set(*object, keys[i], _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(values[i]));
    }
    Require(KeyCount(*object) == 3U);
    Require(object->order.size == 3U);

    unsigned keyRefs = keys[1].impl->refCount;
    unsigned valueRefs = values[1].impl->refCount;
    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), keys[1]);
    Require(TableSize(*object) == 8U);
    Require(KeyCount(*object) == 2U);
    Require(DeletedCount(*object) == 1U);
    Require(keys[1].impl->refCount == keyRefs - 4U);
    Require(values[1].impl->refCount == valueRefs - 2U);
    Require(BucketKey(*object, keys[1]) == nullptr);
    unsigned deletedBuckets = 0U;
    for (unsigned i = 0U; i < TableSize(*object); ++i) {
        if (object->map.table[i].key.impl == DeletedKey) {
            ++deletedBuckets;
            Require(object->map.table[i].value.ptr == nullptr);
        }
    }
    Require(deletedBuckets == 1U);
    Require(object->order.size == 2U);
    Require(object->order.buffer[0].impl == keys[0].impl);
    Require(object->order.buffer[1].impl == keys[2].impl);

    String out;
    Require(!GetString(*object, keys[1], out));
    Require(GetString(*object, keys[0], out) && out.impl == values[0].impl);
    Require(GetString(*object, keys[2], out) && out.impl == values[2].impl);

    String missing = Make("not-there");
    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), missing);
    Require(KeyCount(*object) == 2U);
    Require(DeletedCount(*object) == 1U);
    Require(object->order.size == 2U);
    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), keys[1]);
    Require(KeyCount(*object) == 2U);
    Require(DeletedCount(*object) == 1U);

    std::string copy(reinterpret_cast<const char*>(keys[2].impl->data), keys[2].impl->length);
    String equalKey = Make(copy.c_str());
    Require(equalKey.impl != keys[2].impl);
    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), equalKey);
    Require(KeyCount(*object) == 1U);
    Require(object->order.size == 1U);
    Require(object->order.buffer[0].impl == keys[0].impl);
    Require(GetString(*object, keys[0], out) && out.impl == values[0].impl);

    bool threw = false;
    try {
        String nullString;
        _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), nullString);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestRemoveShrinks() {
    auto object = _ZN3WTF8JSONImpl6Object6createEv();
    AllocateTable(*object, 32U);
    std::vector<String> keys;
    std::vector<String> values;
    for (unsigned i = 0U; i < 6U; ++i) {
        keys.push_back(Make(("shrink" + std::to_string(i)).c_str()));
        values.push_back(Make(("value" + std::to_string(i)).c_str()));
        Set(*object, keys[i], _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(values[i]));
    }
    Require(TableSize(*object) == 32U);
    StringValuePair* original = object->map.table;

    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), keys[0]);
    Require(TableSize(*object) == 16U);
    Require(object->map.table != original);
    Require(KeyCount(*object) == 5U);
    Require(DeletedCount(*object) == 0U);
    Require(Meta(object->map.table)[-2] == 15U);

    String out;
    for (unsigned i = 1U; i < 6U; ++i) {
        Require(GetString(*object, keys[i], out) && out.impl == values[i].impl);
        unsigned slot = Hash(keys[i]) & 15U;
        unsigned probe = 0U;
        while (object->map.table[slot].key.impl != keys[i].impl) {
            Require(object->map.table[slot].key.impl != nullptr);
            ++probe;
            slot = (slot + probe) & 15U;
        }
    }

    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), keys[1]);
    Require(TableSize(*object) == 16U);
    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), keys[2]);
    Require(TableSize(*object) == 16U);
    Require(KeyCount(*object) == 3U);
    Require(DeletedCount(*object) == 2U);
    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), keys[3]);
    Require(TableSize(*object) == 8U);
    Require(KeyCount(*object) == 2U);
    Require(DeletedCount(*object) == 0U);
    _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), keys[4]);
    Require(TableSize(*object) == 8U);
    Require(KeyCount(*object) == 1U);
    Require(DeletedCount(*object) == 1U);
    Require(GetString(*object, keys[5], out) && out.impl == values[5].impl);
    Require(object->order.size == 1U);
    Require(object->order.buffer[0].impl == keys[5].impl);

    std::size_t expected = sizeof(Value*) + 7U + sizeof(Value*) + sizeof(StringImpl) + 6U;
    Require(_ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(object.Get()) == expected);
}

static void TestRelease() {
    String key = Make("held");
    String text = Make("text");
    String other = Make("other");
    Ref<Value> held = _ZN3WTF8JSONImpl5Value6createEi(3);
    unsigned keyRefs = key.impl->refCount;
    unsigned textRefs = text.impl->refCount;
    {
        auto object = _ZN3WTF8JSONImpl6Object6createEv();
        Set(*object, key, Ref<Value>(held));
        Set(*object, other, _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(text));
        Require(held->refCount == 2U);
        Require(key.impl->refCount == keyRefs + 4U);
        Require(text.impl->refCount == textRefs + 2U);
        _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(object.Get(), other);
        Require(text.impl->refCount == textRefs);
    }
    Require(held->refCount == 1U);
    Require(key.impl->refCount == keyRefs);
    Require(text.impl->refCount == textRefs);
}

static void TestNullObject() {
    String key = Make("k");
    String out;
    bool threw = false;
    try {
        _ZNK3WTF8JSONImpl10ObjectBase9getStringERKNS_6StringERS2_(nullptr, key, out);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        _ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(nullptr, key);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    TestCreate();
    TestGetStringAndMemoryCost();
    TestRemoveCollisions();
    TestRemoveShrinks();
    TestRelease();
    TestNullObject();
    return 0;
}
