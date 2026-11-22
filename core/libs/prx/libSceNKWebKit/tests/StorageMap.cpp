#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using NKWebKit::WebCoreAbi::CreateStorageMapImpl;
using NKWebKit::WebCoreAbi::DerefStorageMapImpl;
using NKWebKit::WebCoreAbi::StorageMap;
using NKWebKit::WebCoreAbi::StorageMapImpl;
using NKWebKit::WebCoreAbi::StorageMapNoQuota;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringHashMap;
using NKWebKit::WtfAbi::StringHashMapAddResult;
using NKWebKit::WtfAbi::StringHashMapIterator;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringPair;

namespace {

namespace Wtf = NKWebKit::WtfAbi;

struct ModelEntry {
    std::string value;
    bool nullValue;
    unsigned keyBytes;
    unsigned valueBytes;
};

using Model = std::map<std::string, ModelEntry>;

struct SetOutcome {
    std::string oldValue;
    bool hadOldValue;
    bool quotaException;
};

struct RemoveOutcome {
    std::string oldValue;
    bool hadOldValue;
};

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "StorageMap check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename Exception, typename Function>
bool Throws(Function&& function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

String Make8(const std::string& text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

String Make16(const std::string& text) {
    std::u16string wide(text.begin(), text.end());
    return _ZN3WTF10StringImpl6createEPKDsj(wide.data(), static_cast<unsigned>(wide.size()));
}

bool Is8Bit(const StringImpl* impl) {
    return (impl->hashAndFlags & 4U) != 0;
}

unsigned Bytes(const StringImpl* impl) {
    if (!impl) return 0U;
    return impl->length * (Is8Bit(impl) ? 1U : 2U);
}

std::string Read(const StringImpl* impl) {
    if (!impl) return "<null>";
    std::string result;
    for (unsigned i = 0; i < impl->length; ++i) {
        if (Is8Bit(impl))
            result.push_back(static_cast<char>(static_cast<const unsigned char*>(impl->data)[i]));
        else
            result.push_back(static_cast<char>(static_cast<const char16_t*>(impl->data)[i]));
    }
    return result;
}

StringImpl* DeletedKey() {
    return reinterpret_cast<StringImpl*>(~std::uintptr_t(0));
}

unsigned TableWord(const StringHashMap& map, int offset) {
    return reinterpret_cast<const unsigned*>(map.table)[offset];
}

StorageMap NewStorage(unsigned quota) {
    return StorageMap{CreateStorageMapImpl(), quota};
}

void DestroyStorage(StorageMap& storage) {
    DerefStorageMapImpl(storage.impl);
    storage.impl = nullptr;
}

SetOutcome Set(StorageMap& storage, const String& key, const String& value) {
    String oldValue = Make8("stale");
    bool quotaException = true;
    _ZN7WebCore10StorageMap7setItemERKN3WTF6StringES4_RS2_Rb(&storage, &key, &value, &oldValue, &quotaException);
    return SetOutcome{oldValue.impl ? Read(oldValue.impl) : std::string(), oldValue.impl != nullptr, quotaException};
}

RemoveOutcome Remove(StorageMap& storage, const String& key) {
    String oldValue = Make8("stale");
    _ZN7WebCore10StorageMap10removeItemERKN3WTF6StringERS2_(&storage, &key, &oldValue);
    return RemoveOutcome{oldValue.impl ? Read(oldValue.impl) : std::string(), oldValue.impl != nullptr};
}

String Get(const StorageMap& storage, const String& key) {
    return _ZNK7WebCore10StorageMap7getItemERKN3WTF6StringE(&storage, &key);
}

bool Contains(const StorageMap& storage, const String& key) {
    return _ZNK7WebCore10StorageMap8containsERKN3WTF6StringE(&storage, &key);
}

StringPair* GuestLookup(const StringHashMap& map, const StringImpl* key) {
    if (!map.table) return nullptr;
    unsigned sizeMask = TableWord(map, -2);
    unsigned hash = key->hashAndFlags >> 8;
    if (!hash) hash = _ZNK3WTF10StringImpl12hashSlowCaseEv(key);
    Require(hash != 0);
    unsigned i = hash & sizeMask;
    unsigned probeCount = 0;
    for (unsigned step = 0; step <= sizeMask; ++step) {
        StringPair* entry = map.table + i;
        if (!entry->key) return nullptr;
        if (entry->key != DeletedKey() && _ZN3WTF5equalEPKNS_10StringImplES2_(entry->key, key)) return entry;
        ++probeCount;
        i = (i + probeCount) & sizeMask;
    }
    Require(false);
    return nullptr;
}

std::vector<std::string> TableOrder(const StringHashMap& map) {
    std::vector<std::string> order;
    if (!map.table) return order;
    unsigned tableSize = TableWord(map, -1);
    for (unsigned i = 0; i < tableSize; ++i) {
        const StringPair& bucket = map.table[i];
        if (!bucket.key || bucket.key == DeletedKey()) continue;
        order.push_back(Read(bucket.key));
    }
    return order;
}

void CheckTable(const StorageMap& storage, const Model& model, int line) {
    const StorageMapImpl& impl = *storage.impl;
    const StringHashMap& map = impl.map;
    unsigned expectedBytes = 0;
    for (const auto& item : model) expectedBytes += item.second.keyBytes + item.second.valueBytes;
    Check(impl.currentSize == expectedBytes, line);
    if (!map.table) {
        Check(model.empty(), line);
        return;
    }
    unsigned tableSize = TableWord(map, -1);
    Check(tableSize >= 8 && tableSize <= 1024 && (tableSize & (tableSize - 1)) == 0, line);
    Check(TableWord(map, -2) == tableSize - 1, line);
    unsigned live = 0;
    unsigned deleted = 0;
    for (unsigned i = 0; i < tableSize; ++i) {
        const StringPair& bucket = map.table[i];
        if (!bucket.key) {
            Check(bucket.value == nullptr, line);
            continue;
        }
        if (bucket.key == DeletedKey()) {
            ++deleted;
            Check(bucket.value == nullptr, line);
            continue;
        }
        ++live;
        auto found = model.find(Read(bucket.key));
        Check(found != model.end(), line);
        if (found->second.nullValue)
            Check(bucket.value == nullptr, line);
        else
            Check(bucket.value != nullptr && Read(bucket.value) == found->second.value, line);
        Check(Bytes(bucket.key) == found->second.keyBytes && Bytes(bucket.value) == found->second.valueBytes, line);
        Check((bucket.key->hashAndFlags >> 8) != 0, line);
        Check(GuestLookup(map, bucket.key) == &bucket, line);
    }
    Check(live == model.size(), line);
    Check(TableWord(map, -3) == live, line);
    Check(TableWord(map, -4) == deleted, line);
    Check((std::uint64_t(live) + deleted) * 4 < std::uint64_t(tableSize) * 3, line);
}

void CheckEnumeration(StorageMap& storage, const Model& model, int line) {
    std::vector<std::string> order = TableOrder(storage.impl->map);
    Check(order.size() == model.size(), line);
    for (unsigned i = 0; i < order.size(); ++i) {
        String key = _ZN7WebCore10StorageMap3keyEj(&storage, i);
        Check(key.impl != nullptr && Read(key.impl) == order[i], line);
        Check(storage.impl->iteratorIndex == i, line);
        Check(storage.impl->iterator.position->key == key.impl, line);
    }
    String past = _ZN7WebCore10StorageMap3keyEj(&storage, static_cast<unsigned>(order.size()));
    Check(past.impl == nullptr, line);
}

void TestBestTableSize() {
    const unsigned expected[][2] = {
        {0, 8},   {1, 8},    {4, 8},      {5, 16},     {9, 16},     {10, 32},    {19, 32},    {20, 64},
        {38, 64}, {39, 128}, {618, 1024}, {619, 2048}, {853, 2048}, {854, 4096}, {1025, 4096},
    };
    for (const auto& row : expected) Require(Wtf::StringHashMapBestTableSize(row[0]) == row[1]);
}

void TestHashMap() {
    StringHashMap map{nullptr};
    Require(Wtf::StringHashMapKeyCount(map) == 0);
    Require(Wtf::StringHashMapTableSize(map) == 0);
    Require(Wtf::StringHashMapDeletedCount(map) == 0);
    StringHashMapIterator emptyEnd = Wtf::StringHashMapEnd(map);
    Require(emptyEnd.position == nullptr && emptyEnd.endPosition == nullptr);
    StringHashMapIterator emptyBegin = Wtf::StringHashMapBegin(map);
    Require(emptyBegin.position == nullptr && emptyBegin.endPosition == nullptr);

    String alpha = Make8("alpha");
    String beta = Make8("beta");
    String one = Make8("1");
    String two = Make16("2");
    Require(alpha.impl->refCount == 2 && beta.impl->refCount == 2 && one.impl->refCount == 2 && two.impl->refCount == 2);
    Require(Wtf::StringHashMapFind(map, alpha.impl) == nullptr);
    Require(Wtf::StringHashMapFind(map, nullptr) == nullptr);

    StringHashMapAddResult added = Wtf::StringHashMapAdd(map, alpha.impl, one.impl);
    Require(added.isNewEntry && added.entry->key == alpha.impl && added.entry->value == one.impl);
    Require(alpha.impl->refCount == 4 && one.impl->refCount == 4);
    Require((alpha.impl->hashAndFlags >> 8) != 0);
    Require(Wtf::StringHashMapTableSize(map) == 8 && Wtf::StringHashMapKeyCount(map) == 1);
    Require(TableWord(map, -1) == 8 && TableWord(map, -2) == 7 && TableWord(map, -3) == 1 && TableWord(map, -4) == 0);
    Require(Wtf::StringHashMapFind(map, alpha.impl) == added.entry);
    Require(GuestLookup(map, alpha.impl) == added.entry);
    Require(Wtf::StringHashMapFind(map, beta.impl) == nullptr);

    StringHashMapAddResult again = Wtf::StringHashMapAdd(map, alpha.impl, two.impl);
    Require(!again.isNewEntry && again.entry == added.entry && again.entry->value == one.impl);
    Require(two.impl->refCount == 2 && alpha.impl->refCount == 4);

    StringHashMapAddResult replaced = Wtf::StringHashMapSet(map, alpha.impl, two.impl);
    Require(!replaced.isNewEntry && replaced.entry == added.entry && replaced.entry->value == two.impl);
    Require(one.impl->refCount == 2 && two.impl->refCount == 4 && alpha.impl->refCount == 4);

    Require(Throws<std::invalid_argument>([&] { Wtf::StringHashMapFind(map, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { Wtf::StringHashMapAdd(map, nullptr, one.impl); }));
    Require(Wtf::StringHashMapKeyCount(map) == 1);

    StringHashMapAddResult nullValue = Wtf::StringHashMapSet(map, beta.impl, nullptr);
    Require(nullValue.isNewEntry && nullValue.entry->key == beta.impl && nullValue.entry->value == nullptr);
    Require(beta.impl->refCount == 4 && Wtf::StringHashMapKeyCount(map) == 2);

    StringHashMapIterator iterator = Wtf::StringHashMapBegin(map);
    unsigned visited = 0;
    while (iterator.position != iterator.endPosition) {
        Require(iterator.position->key == alpha.impl || iterator.position->key == beta.impl);
        ++visited;
        Wtf::StringHashMapAdvance(iterator);
    }
    Require(visited == 2);
    Require(iterator.endPosition == map.table + 8);
    Require(Throws<std::out_of_range>([&] { Wtf::StringHashMapAdvance(iterator); }));

    StringPair stray{alpha.impl, one.impl};
    Require(Throws<std::invalid_argument>([&] { Wtf::StringHashMapRemove(map, &stray); }));
    Wtf::StringHashMapRemove(map, nullptr);
    Require(Wtf::StringHashMapKeyCount(map) == 2);

    StringHashMap copy{nullptr};
    Wtf::StringHashMapAssign(copy, map);
    Require(copy.table != nullptr && copy.table != map.table);
    Require(Wtf::StringHashMapKeyCount(copy) == 2 && Wtf::StringHashMapTableSize(copy) == 8);
    Require(alpha.impl->refCount == 6 && beta.impl->refCount == 6 && two.impl->refCount == 6);
    Require(Wtf::StringHashMapFind(copy, alpha.impl)->value == two.impl);
    Require(Wtf::StringHashMapFind(copy, beta.impl)->value == nullptr);

    StringPair* removed = Wtf::StringHashMapFind(copy, alpha.impl);
    Wtf::StringHashMapRemove(copy, removed);
    Require(removed->key == DeletedKey() && removed->value == nullptr);
    Require(Wtf::StringHashMapKeyCount(copy) == 1 && Wtf::StringHashMapDeletedCount(copy) == 1);
    Require(TableWord(copy, -3) == 1 && TableWord(copy, -4) == 1);
    Require(Wtf::StringHashMapFind(copy, alpha.impl) == nullptr);
    Require(Wtf::StringHashMapFind(map, alpha.impl) != nullptr && Wtf::StringHashMapKeyCount(map) == 2);
    Require(alpha.impl->refCount == 4 && two.impl->refCount == 4 && beta.impl->refCount == 6);
    Require(Throws<std::invalid_argument>([&] { Wtf::StringHashMapRemove(copy, removed); }));

    String gamma = Make8("gamma");
    String delta = Make16("delta");
    String epsilon = Make8("epsilon");
    Wtf::StringHashMapAdd(map, gamma.impl, one.impl);
    Wtf::StringHashMapAdd(map, delta.impl, one.impl);
    Wtf::StringHashMapAdd(map, epsilon.impl, one.impl);
    Require(Wtf::StringHashMapKeyCount(map) == 5 && Wtf::StringHashMapTableSize(map) == 8);
    Wtf::StringHashMapAssign(copy, map);
    Require(Wtf::StringHashMapKeyCount(copy) == 5 && Wtf::StringHashMapTableSize(copy) == 16);
    Require(Wtf::StringHashMapDeletedCount(copy) == 0);
    Require(GuestLookup(copy, delta.impl) != nullptr && GuestLookup(copy, delta.impl)->value == one.impl);
    Require(alpha.impl->refCount == 6 && gamma.impl->refCount == 6 && one.impl->refCount == 2 + 2 * 6);

    Wtf::StringHashMapAssign(map, map);
    Require(Wtf::StringHashMapKeyCount(map) == 5 && Wtf::StringHashMapTableSize(map) == 16);
    Require(alpha.impl->refCount == 6 && one.impl->refCount == 2 + 2 * 6);

    Wtf::StringHashMapClear(map);
    Require(map.table == nullptr && Wtf::StringHashMapKeyCount(map) == 0);
    Wtf::StringHashMapClear(map);
    Wtf::StringHashMapClear(copy);
    Require(alpha.impl->refCount == 2 && beta.impl->refCount == 2 && gamma.impl->refCount == 2);
    Require(delta.impl->refCount == 2 && epsilon.impl->refCount == 2 && one.impl->refCount == 2 && two.impl->refCount == 2);
}

void TestDeletedBucketReuse() {
    std::vector<String> sameHome;
    unsigned home = 0;
    for (unsigned candidate = 0; sameHome.size() < 4 && candidate < 4096; ++candidate) {
        String key = Make8("t" + std::to_string(candidate));
        const unsigned bucket = Wtf::StringHashMapKeyHash(key.impl) & 7U;
        if (sameHome.empty()) home = bucket;
        if (bucket == home) sameHome.push_back(key);
    }
    Require(sameHome.size() == 4);
    const String value = Make8("v");
    StringImpl* a = sameHome[0].impl;
    StringImpl* b = sameHome[1].impl;
    StringImpl* c = sameHome[2].impl;
    StringImpl* d = sameHome[3].impl;
    const unsigned first = home;
    const unsigned second = (home + 1U) & 7U;
    const unsigned third = (home + 3U) & 7U;
    const unsigned fourth = (home + 6U) & 7U;

    StringHashMap map{nullptr};
    Require(Wtf::StringHashMapAdd(map, a, value.impl).isNewEntry);
    Require(Wtf::StringHashMapAdd(map, b, value.impl).isNewEntry);
    Require(Wtf::StringHashMapAdd(map, c, value.impl).isNewEntry);
    Require(TableWord(map, -1) == 8);
    Require(map.table[first].key == a && map.table[second].key == b && map.table[third].key == c);
    Require(map.table[fourth].key == nullptr);

    Wtf::StringHashMapRemove(map, Wtf::StringHashMapFind(map, a));
    Wtf::StringHashMapRemove(map, Wtf::StringHashMapFind(map, b));
    Require(TableWord(map, -1) == 8 && TableWord(map, -3) == 1 && TableWord(map, -4) == 2);
    Require(map.table[first].key == DeletedKey() && map.table[second].key == DeletedKey());
    Require(Wtf::StringHashMapFind(map, c) == map.table + third);

    const StringHashMapAddResult addedD = Wtf::StringHashMapAdd(map, d, value.impl);
    Require(addedD.isNewEntry && addedD.entry == map.table + second);
    Require(map.table[first].key == DeletedKey() && map.table[second].key == d && map.table[fourth].key == nullptr);
    Require(TableWord(map, -3) == 2 && TableWord(map, -4) == 1);

    const StringHashMapAddResult addedA = Wtf::StringHashMapAdd(map, a, value.impl);
    Require(addedA.isNewEntry && addedA.entry == map.table + first);
    Require(map.table[first].key == a && map.table[fourth].key == nullptr);
    Require(TableWord(map, -3) == 3 && TableWord(map, -4) == 0);

    const StringHashMapAddResult addedB = Wtf::StringHashMapAdd(map, b, value.impl);
    Require(addedB.isNewEntry && addedB.entry == map.table + fourth);
    Require(TableWord(map, -1) == 8 && TableWord(map, -3) == 4 && TableWord(map, -4) == 0);
    Require(Wtf::StringHashMapFind(map, a) == map.table + first);
    Require(Wtf::StringHashMapFind(map, d) == map.table + second);
    Require(Wtf::StringHashMapFind(map, c) == map.table + third);
    Require(Wtf::StringHashMapFind(map, b) == map.table + fourth);
    Require(!Wtf::StringHashMapAdd(map, d, value.impl).isNewEntry);

    Wtf::StringHashMapClear(map);
    Require(map.table == nullptr);
    for (const String& key : sameHome) Require(key.impl->refCount == 2);
    Require(value.impl->refCount == 2);
}

void TestEmptyMap() {
    StorageMap storage = NewStorage(StorageMapNoQuota);
    Require(storage.impl->refCount == 1 && storage.impl->map.table == nullptr);
    Require(storage.impl->iterator.position == nullptr && storage.impl->iterator.endPosition == nullptr);
    Require(storage.impl->iteratorIndex == 0xFFFFFFFFU && storage.impl->currentSize == 0);

    String key = Make8("missing");
    String nullKey;
    Require(Get(storage, key).impl == nullptr);
    Require(Get(storage, nullKey).impl == nullptr);
    Require(!Contains(storage, key));
    Require(!Contains(storage, nullKey));
    Require(_ZN7WebCore10StorageMap3keyEj(&storage, 0).impl == nullptr);
    Require(_ZN7WebCore10StorageMap3keyEj(&storage, 0xFFFFFFFFU).impl == nullptr);
    RemoveOutcome removed = Remove(storage, key);
    Require(!removed.hadOldValue);
    RemoveOutcome removedNull = Remove(storage, nullKey);
    Require(!removedNull.hadOldValue);
    Require(storage.impl->map.table == nullptr && storage.impl->currentSize == 0);

    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore10StorageMap3keyEj(nullptr, 0); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore10StorageMap7getItemERKN3WTF6StringE(nullptr, &key); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore10StorageMap7getItemERKN3WTF6StringE(&storage, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore10StorageMap8containsERKN3WTF6StringE(&storage, nullptr); }));
    StorageMap noImpl{nullptr, 0};
    Require(Throws<std::invalid_argument>([&] { _ZNK7WebCore10StorageMap8containsERKN3WTF6StringE(&noImpl, &key); }));

    DestroyStorage(storage);
    Require(key.impl->refCount == 2);
}

void TestSetGetRemove() {
    StorageMap storage = NewStorage(StorageMapNoQuota);
    Model model;
    String name = Make8("name");
    String wide = Make16("wide");
    String first = Make8("first");
    String second = Make16("second");

    SetOutcome created = Set(storage, name, first);
    Require(!created.hadOldValue && !created.quotaException);
    model["name"] = ModelEntry{"first", false, 4, 5};
    CheckTable(storage, model, __LINE__);
    Require(storage.impl->currentSize == 9);
    Require(TableWord(storage.impl->map, -1) == 8 && TableWord(storage.impl->map, -3) == 1);
    Require(name.impl->refCount == 4 && first.impl->refCount == 4);
    Require(storage.impl->iteratorIndex == 0xFFFFFFFFU);
    Require(storage.impl->iterator.position == storage.impl->map.table + 8);
    Require(storage.impl->iterator.endPosition == storage.impl->map.table + 8);

    {
        String fetched = Get(storage, name);
        Require(fetched.impl == first.impl && first.impl->refCount == 6);
    }
    Require(first.impl->refCount == 4);
    Require(Contains(storage, name));
    Require(!Contains(storage, wide));

    SetOutcome replaced = Set(storage, name, second);
    Require(replaced.hadOldValue && replaced.oldValue == "first" && !replaced.quotaException);
    model["name"] = ModelEntry{"second", false, 4, 12};
    CheckTable(storage, model, __LINE__);
    Require(storage.impl->currentSize == 16);
    Require(first.impl->refCount == 2 && second.impl->refCount == 4 && name.impl->refCount == 4);

    SetOutcome widened = Set(storage, wide, first);
    Require(!widened.hadOldValue && !widened.quotaException);
    model["wide"] = ModelEntry{"first", false, 8, 5};
    CheckTable(storage, model, __LINE__);
    Require(storage.impl->currentSize == 29);
    CheckEnumeration(storage, model, __LINE__);

    String sameName16 = Make16("name");
    Require(Contains(storage, sameName16));
    Require(Get(storage, sameName16).impl == second.impl);
    SetOutcome viaOtherWidth = Set(storage, sameName16, first);
    Require(viaOtherWidth.hadOldValue && viaOtherWidth.oldValue == "second");
    model["name"] = ModelEntry{"first", false, 4, 5};
    CheckTable(storage, model, __LINE__);
    Require(sameName16.impl->refCount == 2 && name.impl->refCount == 4);
    Require(GuestLookup(storage.impl->map, sameName16.impl)->key == name.impl);

    String nullValue;
    SetOutcome cleared = Set(storage, wide, nullValue);
    Require(cleared.hadOldValue && cleared.oldValue == "first");
    model["wide"] = ModelEntry{"", true, 8, 0};
    CheckTable(storage, model, __LINE__);
    Require(Contains(storage, wide) && Get(storage, wide).impl == nullptr);
    SetOutcome afterNull = Set(storage, wide, second);
    Require(!afterNull.hadOldValue);
    model["wide"] = ModelEntry{"second", false, 8, 12};
    CheckTable(storage, model, __LINE__);

    String nullKey;
    Require(Throws<std::invalid_argument>([&] { Get(storage, nullKey); }));
    Require(Throws<std::invalid_argument>([&] { Contains(storage, nullKey); }));
    Require(Throws<std::invalid_argument>([&] { Set(storage, nullKey, first); }));
    Require(Throws<std::invalid_argument>([&] { Remove(storage, nullKey); }));
    Require(Throws<std::invalid_argument>([&] {
        _ZN7WebCore10StorageMap7setItemERKN3WTF6StringES4_RS2_Rb(&storage, &name, &first, nullptr, nullptr);
    }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore10StorageMap10removeItemERKN3WTF6StringERS2_(&storage, &name, nullptr); }));
    CheckTable(storage, model, __LINE__);

    String missing = Make8("missing");
    RemoveOutcome nothing = Remove(storage, missing);
    Require(!nothing.hadOldValue);
    CheckTable(storage, model, __LINE__);

    RemoveOutcome removed = Remove(storage, sameName16);
    Require(removed.hadOldValue && removed.oldValue == "first");
    model.erase("name");
    CheckTable(storage, model, __LINE__);
    Require(storage.impl->currentSize == 20);
    Require(name.impl->refCount == 2 && first.impl->refCount == 2);
    Require(!Contains(storage, name));
    Require(TableWord(storage.impl->map, -3) == 1 && TableWord(storage.impl->map, -4) == 1);
    Require(storage.impl->iteratorIndex == 0xFFFFFFFFU);

    RemoveOutcome last = Remove(storage, wide);
    Require(last.hadOldValue && last.oldValue == "second");
    model.erase("wide");
    CheckTable(storage, model, __LINE__);
    Require(storage.impl->currentSize == 0 && storage.impl->map.table != nullptr);
    Require(TableWord(storage.impl->map, -3) == 0 && TableWord(storage.impl->map, -4) == 2);
    Require(_ZN7WebCore10StorageMap3keyEj(&storage, 0).impl == nullptr);

    DestroyStorage(storage);
    Require(name.impl->refCount == 2 && wide.impl->refCount == 2 && first.impl->refCount == 2 && second.impl->refCount == 2);
}

void TestTableSizes() {
    StorageMap storage = NewStorage(StorageMapNoQuota);
    Model model;
    std::vector<String> keys;
    String value = Make8("v");
    for (unsigned i = 0; i < 12; ++i) keys.push_back(Make8("size" + std::to_string(i)));
    const unsigned grown[12] = {8, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 32};
    for (unsigned i = 0; i < 12; ++i) {
        Set(storage, keys[i], value);
        model[Read(keys[i].impl)] = ModelEntry{"v", false, Bytes(keys[i].impl), 1};
        CheckTable(storage, model, __LINE__);
        Require(TableWord(storage.impl->map, -1) == grown[i]);
        Require(TableWord(storage.impl->map, -4) == 0);
    }
    CheckEnumeration(storage, model, __LINE__);
    for (unsigned i = 0; i < 12; ++i) {
        String backwards = _ZN7WebCore10StorageMap3keyEj(&storage, 11 - i);
        Require(backwards.impl != nullptr && Read(backwards.impl) == TableOrder(storage.impl->map)[11 - i]);
        Require(storage.impl->iteratorIndex == 11 - i);
    }
    {
        String repeated = _ZN7WebCore10StorageMap3keyEj(&storage, 0);
        StringPair* position = storage.impl->iterator.position;
        String repeatedAgain = _ZN7WebCore10StorageMap3keyEj(&storage, 0);
        Require(repeated.impl == repeatedAgain.impl && storage.impl->iterator.position == position);
        Require(repeated.impl->refCount == 8 && storage.impl->iteratorIndex == 0);
    }

    const unsigned shrunk[12] = {32, 32, 32, 32, 32, 32, 16, 16, 16, 8, 8, 8};
    const unsigned tombstones[12] = {1, 2, 3, 4, 5, 6, 0, 1, 2, 0, 1, 2};
    for (unsigned i = 0; i < 12; ++i) {
        RemoveOutcome removed = Remove(storage, keys[i]);
        Require(removed.hadOldValue && removed.oldValue == "v");
        model.erase(Read(keys[i].impl));
        CheckTable(storage, model, __LINE__);
        Require(TableWord(storage.impl->map, -1) == shrunk[i]);
        Require(TableWord(storage.impl->map, -4) == tombstones[i]);
        Require(keys[i].impl->refCount == 2);
        CheckEnumeration(storage, model, __LINE__);
    }
    Set(storage, keys[0], value);
    model[Read(keys[0].impl)] = ModelEntry{"v", false, Bytes(keys[0].impl), 1};
    CheckTable(storage, model, __LINE__);
    DestroyStorage(storage);
    for (const String& key : keys) Require(key.impl->refCount == 2);
    Require(value.impl->refCount == 2);
}

void TestQuota() {
    StorageMap storage = NewStorage(10);
    Model model;
    String ab = Make8("ab");
    String gh = Make8("gh");
    String wideKey = Make16("xy");
    String cdef = Make8("cdef");
    String ijk = Make8("ijk");
    String ij = Make8("ij");
    String cdefg = Make8("cdefg");
    String c = Make8("c");

    SetOutcome first = Set(storage, ab, cdef);
    Require(!first.quotaException && !first.hadOldValue);
    model["ab"] = ModelEntry{"cdef", false, 2, 4};
    CheckTable(storage, model, __LINE__);

    SetOutcome tooBig = Set(storage, gh, ijk);
    Require(tooBig.quotaException && !tooBig.hadOldValue);
    CheckTable(storage, model, __LINE__);
    Require(!Contains(storage, gh) && gh.impl->refCount == 2 && ijk.impl->refCount == 2);

    SetOutcome exact = Set(storage, gh, ij);
    Require(!exact.quotaException && !exact.hadOldValue);
    model["gh"] = ModelEntry{"ij", false, 2, 2};
    CheckTable(storage, model, __LINE__);
    Require(storage.impl->currentSize == 10 && storage.quotaSize == 10);

    SetOutcome longer = Set(storage, ab, cdefg);
    Require(longer.quotaException && longer.hadOldValue && longer.oldValue == "cdef");
    CheckTable(storage, model, __LINE__);
    Require(Get(storage, ab).impl == cdef.impl);

    SetOutcome shorter = Set(storage, ab, c);
    Require(!shorter.quotaException && shorter.hadOldValue && shorter.oldValue == "cdef");
    model["ab"] = ModelEntry{"c", false, 2, 1};
    CheckTable(storage, model, __LINE__);
    Require(storage.impl->currentSize == 7);

    SetOutcome wideTooBig = Set(storage, wideKey, c);
    Require(wideTooBig.quotaException);
    CheckTable(storage, model, __LINE__);
    RemoveOutcome dropped = Remove(storage, gh);
    Require(dropped.hadOldValue && dropped.oldValue == "ij");
    model.erase("gh");
    SetOutcome wideFits = Set(storage, wideKey, c);
    Require(!wideFits.quotaException);
    model["xy"] = ModelEntry{"c", false, 4, 1};
    CheckTable(storage, model, __LINE__);
    Require(storage.impl->currentSize == 8);

    _ZN7WebCore10StorageMap20setItemIgnoringQuotaERKN3WTF6StringES4_(&storage, &gh, &cdefg);
    model["gh"] = ModelEntry{"cdefg", false, 2, 5};
    CheckTable(storage, model, __LINE__);
    Require(storage.impl->currentSize == 15 && storage.quotaSize == 10);
    SetOutcome stillOver = Set(storage, gh, cdef);
    Require(stillOver.quotaException && stillOver.hadOldValue && stillOver.oldValue == "cdefg");
    CheckTable(storage, model, __LINE__);

    String nullKey;
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore10StorageMap20setItemIgnoringQuotaERKN3WTF6StringES4_(&storage, &nullKey, &c); }));
    Require(storage.quotaSize == 10);
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore10StorageMap20setItemIgnoringQuotaERKN3WTF6StringES4_(&storage, nullptr, &c); }));
    Require(storage.quotaSize == 10);
    CheckTable(storage, model, __LINE__);
    DestroyStorage(storage);

    StorageMap zero = NewStorage(0);
    Model zeroModel;
    String empty = Make8("");
    Require(empty.impl != nullptr && empty.impl->length == 0);
    SetOutcome refused = Set(zero, ab, empty);
    Require(refused.quotaException);
    Require(zero.impl->map.table == nullptr);
    SetOutcome emptyPair = Set(zero, empty, empty);
    Require(!emptyPair.quotaException && !emptyPair.hadOldValue);
    zeroModel[""] = ModelEntry{"", false, 0, 0};
    CheckTable(zero, zeroModel, __LINE__);
    Require(Contains(zero, empty) && Get(zero, empty).impl == empty.impl);
    DestroyStorage(zero);

    Require(ab.impl->refCount == 2 && gh.impl->refCount == 2 && wideKey.impl->refCount == 2 && cdef.impl->refCount == 2);
    Require(ijk.impl->refCount == 2 && ij.impl->refCount == 2 && cdefg.impl->refCount == 2 && c.impl->refCount == 2);
}

void TestSizeOverflow() {
    String key = Make8("overflow");
    String value = Make8("sixteen-bytes-ok");
    Require(Bytes(key.impl) == 8 && Bytes(value.impl) == 16);

    StorageMap limited = NewStorage(0xFFFFFFFEU);
    limited.impl->currentSize = 0xFFFFFFF0U;
    SetOutcome refused = Set(limited, key, value);
    Require(refused.quotaException && !refused.hadOldValue);
    Require(limited.impl->map.table == nullptr && limited.impl->currentSize == 0xFFFFFFF0U);
    limited.impl->currentSize = 0;
    DestroyStorage(limited);

    StorageMap unlimited = NewStorage(StorageMapNoQuota);
    unlimited.impl->currentSize = 0xFFFFFFF0U;
    Require(Throws<std::overflow_error>([&] { Set(unlimited, key, value); }));
    Require(unlimited.impl->map.table == nullptr && unlimited.impl->currentSize == 0xFFFFFFF0U);
    unlimited.impl->currentSize = 0;
    SetOutcome stored = Set(unlimited, key, value);
    Require(!stored.quotaException && unlimited.impl->currentSize == 24);
    unlimited.impl->currentSize = 1;
    Require(Throws<std::overflow_error>([&] { Remove(unlimited, key); }));
    Require(Contains(unlimited, key) && unlimited.impl->currentSize == 1);
    unlimited.impl->currentSize = 24;
    RemoveOutcome removed = Remove(unlimited, key);
    Require(removed.hadOldValue && unlimited.impl->currentSize == 0);
    DestroyStorage(unlimited);
    Require(key.impl->refCount == 2 && value.impl->refCount == 2);
}

void TestCopyOnWrite() {
    String one = Make8("one");
    String two = Make8("two");
    String three = Make16("three");
    String value = Make8("value");
    String other = Make8("other");

    StorageMap original = NewStorage(StorageMapNoQuota);
    Model originalModel;
    Set(original, one, value);
    Set(original, two, value);
    originalModel["one"] = ModelEntry{"value", false, 3, 5};
    originalModel["two"] = ModelEntry{"value", false, 3, 5};
    CheckTable(original, originalModel, __LINE__);
    StorageMapImpl* sharedImpl = original.impl;
    StringPair* sharedTable = sharedImpl->map.table;

    StorageMap sharer{sharedImpl, 20};
    sharedImpl->refCount = 2;

    {
        String listed = _ZN7WebCore10StorageMap3keyEj(&sharer, 1);
        Require(listed.impl == one.impl || listed.impl == two.impl);
    }
    Require(sharer.impl == sharedImpl && sharedImpl->refCount == 2 && sharedImpl->iteratorIndex == 1);
    Require(Get(sharer, one).impl == value.impl && Contains(sharer, two) && sharer.impl == sharedImpl);

    SetOutcome overQuota = Set(sharer, three, other);
    Require(overQuota.quotaException && sharer.impl == sharedImpl && sharedImpl->refCount == 2);
    RemoveOutcome missing = Remove(sharer, three);
    Require(!missing.hadOldValue && sharer.impl == sharedImpl && sharedImpl->refCount == 2);
    Require(sharedImpl->iteratorIndex == 1);

    sharer.quotaSize = StorageMapNoQuota;
    SetOutcome copied = Set(sharer, three, other);
    Require(!copied.quotaException && !copied.hadOldValue);
    Require(sharer.impl != sharedImpl && sharedImpl->refCount == 1 && sharer.impl->refCount == 1);
    Require(original.impl == sharedImpl && sharedImpl->map.table == sharedTable && sharedImpl->iteratorIndex == 1);
    Model sharerModel = originalModel;
    sharerModel["three"] = ModelEntry{"other", false, 10, 5};
    CheckTable(original, originalModel, __LINE__);
    CheckTable(sharer, sharerModel, __LINE__);
    Require(!Contains(original, three));
    Require(one.impl->refCount == 6 && two.impl->refCount == 6 && three.impl->refCount == 4);
    Require(value.impl->refCount == 2 + 2 * 4 && other.impl->refCount == 4);
    Require(sharer.impl->iteratorIndex == 0xFFFFFFFFU);

    StorageMap remover{sharer.impl, StorageMapNoQuota};
    sharer.impl->refCount = 2;
    StorageMapImpl* beforeRemove = sharer.impl;
    RemoveOutcome removed = Remove(remover, one);
    Require(removed.hadOldValue && removed.oldValue == "value");
    Require(remover.impl != beforeRemove && beforeRemove->refCount == 1 && remover.impl->refCount == 1 && sharer.impl == beforeRemove);
    Model removerModel = sharerModel;
    removerModel.erase("one");
    CheckTable(sharer, sharerModel, __LINE__);
    CheckTable(remover, removerModel, __LINE__);
    Require(Contains(sharer, one) && !Contains(remover, one));
    Require(TableWord(remover.impl->map, -4) == 1);

    StorageMap replacer{original.impl, StorageMapNoQuota};
    original.impl->refCount = 2;
    SetOutcome replaced = Set(replacer, one, other);
    Require(replaced.hadOldValue && replaced.oldValue == "value" && replacer.impl != original.impl && original.impl->refCount == 1);
    Model replacerModel = originalModel;
    replacerModel["one"] = ModelEntry{"other", false, 3, 5};
    CheckTable(original, originalModel, __LINE__);
    CheckTable(replacer, replacerModel, __LINE__);

    DestroyStorage(original);
    DestroyStorage(sharer);
    DestroyStorage(remover);
    DestroyStorage(replacer);
    Require(one.impl->refCount == 2 && two.impl->refCount == 2 && three.impl->refCount == 2);
    Require(value.impl->refCount == 2 && other.impl->refCount == 2);
}

void TestOutputAliases() {
    StorageMap storage = NewStorage(StorageMapNoQuota);
    String key = Make8("alias");
    String value = Make8("held");
    Set(storage, key, value);
    String previous = Make8("previous");
    StringImpl* previousImpl = previous.impl;
    previousImpl->refCount += 2;
    bool quotaException = true;
    String replacement = Make8("replacement");
    _ZN7WebCore10StorageMap7setItemERKN3WTF6StringES4_RS2_Rb(&storage, &key, &replacement, &previous, &quotaException);
    Require(!quotaException && previous.impl == value.impl && previousImpl->refCount == 2);
    NKWebKit::WtfAbi::Release(previousImpl);
    Require(value.impl->refCount == 4);
    String missing = Make8("missing");
    _ZN7WebCore10StorageMap10removeItemERKN3WTF6StringERS2_(&storage, &missing, &previous);
    Require(previous.impl == nullptr && value.impl->refCount == 2);
    DestroyStorage(storage);
    Require(key.impl->refCount == 2 && replacement.impl->refCount == 2);
}

void TestRandomOperations() {
    StorageMap storage = NewStorage(StorageMapNoQuota);
    Model model;
    std::vector<String> keys;
    for (unsigned i = 0; i < 48; ++i) {
        std::string text = "key" + std::to_string(i);
        keys.push_back(i % 2 ? Make16(text) : Make8(text));
    }
    std::uint32_t state = 0x5EED1234U;
    auto next = [&state]() {
        state = state * 1664525U + 1013904223U;
        return state >> 8;
    };
    unsigned sets = 0;
    unsigned removals = 0;
    unsigned maximumTableSize = 0;
    for (unsigned step = 0; step < 900; ++step) {
        const String& key = keys[next() % keys.size()];
        std::string keyText = Read(key.impl);
        auto existing = model.find(keyText);
        bool grow = step < 300 || (step >= 600 && step < 750);
        if (next() % 10 < (grow ? 8U : 3U)) {
            std::size_t length = next() % 9;
            char letter = static_cast<char>('a' + next() % 26);
            std::string text(length, letter);
            bool wideValue = next() % 2 != 0;
            String value = wideValue ? Make16(text) : Make8(text);
            SetOutcome outcome = Set(storage, key, value);
            Require(!outcome.quotaException);
            if (existing == model.end())
                Require(!outcome.hadOldValue);
            else
                Require(outcome.hadOldValue && outcome.oldValue == existing->second.value);
            model[keyText] = ModelEntry{text, false, Bytes(key.impl), Bytes(value.impl)};
            ++sets;
        } else {
            RemoveOutcome outcome = Remove(storage, key);
            if (existing == model.end())
                Require(!outcome.hadOldValue);
            else
                Require(outcome.hadOldValue && outcome.oldValue == existing->second.value);
            model.erase(keyText);
            ++removals;
        }
        CheckTable(storage, model, __LINE__);
        CheckEnumeration(storage, model, __LINE__);
        const String& probe = keys[next() % keys.size()];
        auto probed = model.find(Read(probe.impl));
        String fetched = Get(storage, probe);
        Require(Contains(storage, probe) == (probed != model.end()));
        if (probed == model.end())
            Require(fetched.impl == nullptr);
        else
            Require(fetched.impl != nullptr && Read(fetched.impl) == probed->second.value);
        Require((GuestLookup(storage.impl->map, probe.impl) != nullptr) == (probed != model.end()));
        for (const String& candidate : keys)
            Require(candidate.impl->refCount == (model.count(Read(candidate.impl)) ? 4U : 2U));
        if (storage.impl->map.table && TableWord(storage.impl->map, -1) > maximumTableSize)
            maximumTableSize = TableWord(storage.impl->map, -1);
    }
    Require(sets > 300 && removals > 200 && maximumTableSize >= 64);
    DestroyStorage(storage);
    for (const String& key : keys) Require(key.impl->refCount == 2);
}

}  // namespace

int main() {
    TestBestTableSize();
    TestHashMap();
    TestDeletedBucketReuse();
    TestEmptyMap();
    TestSetGetRemove();
    TestTableSizes();
    TestQuota();
    TestSizeOverflow();
    TestCopyOnWrite();
    TestOutputAliases();
    TestRandomOperations();
    return 0;
}
