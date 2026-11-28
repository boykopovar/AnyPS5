#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using NKWebKit::WebCoreAbi::ContentType;
using NKWebKit::WebCoreAbi::MIMETypeCache;
using NKWebKit::WebCoreAbi::MIMETypeCacheVtable;
using NKWebKit::WebCoreAbi::MIMETypeResult;
using NKWebKit::WebCoreAbi::MIMETypeSet;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringVector;
using NKWebKit::WtfAbi::StringView;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "mime type cache check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr auto copyConstruct = &_ZN7WebCore13MIMETypeCacheC1ERKS0_;
constexpr auto destroy = &_ZN7WebCore13MIMETypeCacheD2Ev;
constexpr auto assign = &_ZN7WebCore13MIMETypeCacheaSERKS0_;
constexpr auto canDecodeType = &_ZN7WebCore13MIMETypeCache13canDecodeTypeERKN3WTF6StringE;
constexpr auto supportedTypes = &_ZN7WebCore13MIMETypeCache14supportedTypesEv;
constexpr auto addSupportedTypes = &_ZN7WebCore13MIMETypeCache17addSupportedTypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE;
constexpr auto supportsContainerType = &_ZN7WebCore13MIMETypeCache21supportsContainerTypeERKN3WTF6StringE;

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Text(const std::string& text) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(text.c_str()));
}

static std::string Lower(std::string text) {
    for (char& character : text)
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + 32);
    return text;
}

static unsigned OracleHash(const std::string& key) {
    static std::map<std::string, unsigned> known;
    const std::string folded = Lower(key);
    const auto found = known.find(folded);
    if (found != known.end()) return found->second;
    const String lower = Text(folded);
    const unsigned stored = lower.impl->hashAndFlags >> 8;
    const unsigned hash = stored ? stored : _ZNK3WTF10StringImpl12hashSlowCaseEv(lower.impl);
    known.emplace(folded, hash);
    return hash;
}

static unsigned Meta(const void* table, int slot) {
    return reinterpret_cast<const unsigned*>(table)[slot];
}

struct Model {
    std::vector<std::string> buckets;
    unsigned keyCount = 0;

    unsigned Slot(const std::string& key, bool stopAtEqual, bool& found) const {
        const unsigned mask = static_cast<unsigned>(buckets.size()) - 1;
        unsigned index = OracleHash(key) & mask;
        found = false;
        for (unsigned probe = 1; !buckets[index].empty(); ++probe) {
            if (stopAtEqual && Lower(buckets[index]) == Lower(key)) {
                found = true;
                return index;
            }
            index = (index + probe) & mask;
        }
        return index;
    }

    void Resize(unsigned size) {
        std::vector<std::string> old(size);
        old.swap(buckets);
        bool found = false;
        for (const std::string& key : old)
            if (!key.empty()) buckets[Slot(key, false, found)] = key;
    }

    void Add(const std::string& key) {
        if (buckets.empty()) Resize(8);
        bool found = false;
        const unsigned index = Slot(key, true, found);
        if (found) return;
        buckets[index] = key;
        ++keyCount;
        const unsigned size = static_cast<unsigned>(buckets.size());
        const bool full = size <= 1024 ? keyCount * 4 >= size * 3 : keyCount * 2 >= size;
        if (full) Resize(keyCount * 6 < size * 2 ? size : size * 2);
    }

    static Model CopyOf(const Model& source, unsigned size) {
        Model copy;
        copy.buckets.resize(size);
        copy.keyCount = source.keyCount;
        bool found = false;
        for (const std::string& key : source.buckets)
            if (!key.empty()) copy.buckets[copy.Slot(key, false, found)] = key;
        return copy;
    }
};

static std::string Utf8(const StringImpl* impl) {
    const StringView view = NKWebKit::WtfAbi::ViewOf(impl);
    std::string text;
    for (unsigned index = 0; index < view.length; ++index) text.push_back(static_cast<char>(NKWebKit::WtfAbi::CodeUnitAt(view, index)));
    return text;
}

static void RequireSameAs(const Model& model, StringImpl* const* table) {
    if (model.buckets.empty()) {
        Require(table == nullptr);
        return;
    }
    Require(table != nullptr);
    Require(Meta(table, -1) == model.buckets.size() && Meta(table, -2) == model.buckets.size() - 1);
    Require(Meta(table, -3) == model.keyCount && Meta(table, -4) == 0);
    for (std::size_t index = 0; index < model.buckets.size(); ++index) {
        if (model.buckets[index].empty())
            Require(table[index] == nullptr);
        else
            Require(table[index] != nullptr && Utf8(table[index]) == model.buckets[index]);
    }
}

struct Counters {
    int initialized = 0;
    int available = 0;
    int unsupported = 0;
    int staticType = 0;
    int extended = 0;
    int destroyed = 0;
};

struct Cache {
    MIMETypeCache object{};
    unsigned char guard[24];
    Counters counters;
    bool isAvailable = true;
    std::string unsupportedType;
    std::string staticType;
    bool canDecodeExtended = false;
    std::vector<std::string> initialTypes;
    std::string lastExtended;
};

static Cache& CacheOf(const MIMETypeCache* self) {
    return *reinterpret_cast<Cache*>(const_cast<MIMETypeCache*>(self));
}

static StringVector Vector(std::vector<String>& strings) {
    StringVector vector;
    vector.buffer = strings.data();
    vector.capacity = static_cast<unsigned>(strings.size());
    vector.size = static_cast<unsigned>(strings.size());
    return vector;
}

static void Release(StringVector& vector) {
    vector.buffer = nullptr;
    vector.capacity = 0;
    vector.size = 0;
}

static void AddTypes(MIMETypeCache* self, const std::vector<std::string>& types) {
    std::vector<String> strings;
    for (const std::string& type : types) strings.push_back(Text(type));
    StringVector vector = Vector(strings);
    addSupportedTypes(self, &vector);
    Release(vector);
}

static void APS5_VABI TestCompleteDestructor(MIMETypeCache* self) {
    ++CacheOf(self).counters.destroyed;
    destroy(self);
}
static void APS5_VABI TestDeletingDestructor(MIMETypeCache*) { Require(false); }
static bool APS5_VABI TestIsAvailable(const MIMETypeCache* self) {
    ++CacheOf(self).counters.available;
    return CacheOf(self).isAvailable;
}
static bool APS5_VABI TestIsStaticContainerType(MIMETypeCache* self, StringView type) {
    ++CacheOf(self).counters.staticType;
    std::string text;
    for (unsigned index = 0; index < type.length; ++index) text.push_back(static_cast<char>(NKWebKit::WtfAbi::CodeUnitAt(type, index)));
    return !CacheOf(self).staticType.empty() && text == CacheOf(self).staticType;
}
static bool APS5_VABI TestIsUnsupportedContainerType(MIMETypeCache* self, const String* type) {
    ++CacheOf(self).counters.unsupported;
    return !CacheOf(self).unsupportedType.empty() && Utf8(type->impl) == CacheOf(self).unsupportedType;
}
static void APS5_VABI TestInitializeCache(MIMETypeCache* self, MIMETypeSet* types) {
    ++CacheOf(self).counters.initialized;
    Require(types == &self->supportedTypes && self->hasSupportedTypes && types->table == nullptr);
    if (!CacheOf(self).initialTypes.empty()) AddTypes(self, CacheOf(self).initialTypes);
}
static bool APS5_VABI TestCanDecodeExtendedType(MIMETypeCache* self, const ContentType* type) {
    ++CacheOf(self).counters.extended;
    CacheOf(self).lastExtended = Utf8(type->type.impl);
    return CacheOf(self).canDecodeExtended;
}

static const MIMETypeCacheVtable testVtable = {
    TestCompleteDestructor, TestDeletingDestructor, TestIsAvailable, canDecodeType, supportedTypes,
    TestIsStaticContainerType, TestIsUnsupportedContainerType, TestInitializeCache, TestCanDecodeExtendedType,
};

static void Arm(Cache& cache) {
    std::memset(cache.guard, 0xA5, sizeof(cache.guard));
    cache.object.vtable = &testVtable;
}

static bool GuardIntact(const Cache& cache) {
    for (unsigned char byte : cache.guard)
        if (byte != 0xA5) return false;
    return true;
}

static std::uint8_t Decode(Cache& cache, const std::string& type) {
    const String text = Text(type);
    return canDecodeType(&cache.object, &text);
}

static bool Supports(Cache& cache, const std::string& type) {
    const String text = Text(type);
    return supportsContainerType(&cache.object, &text);
}

static const std::vector<std::string>& ManyTypes() {
    static const std::vector<std::string> types = [] {
        std::vector<std::string> list;
        for (const char* family : {"video", "Audio", "APPLICATION", "image", "text"})
            for (const char* name : {"mp4", "MPEG", "webm", "ogg", "x-matroska", "quicktime", "3gpp", "flac"}) list.push_back(std::string(family) + "/" + name);
        return list;
    }();
    return types;
}

static void TestTheBaseClass() {
    Cache prototype;
    Arm(prototype);
    MIMETypeCache base{};
    copyConstruct(&base, &prototype.object);
    Require(base.vtable != nullptr && base.vtable != &testVtable);
    Require(!base.hasSupportedTypes && !base.hasCachedResults && base.supportedTypes.table == nullptr && base.cachedResults.table == nullptr);
    const String type = Text("video/mp4");
    const ContentType contentType{Text("video/mp4; codecs=avc1")};
    Require(base.vtable->isAvailable(&base));
    Require(!base.vtable->isUnsupportedContainerType(&base, &type));
    Require(!base.vtable->isStaticContainerType(&base, NKWebKit::WtfAbi::ViewOf(type.impl)));
    Require(!base.vtable->canDecodeExtendedType(&base, &contentType));
    MIMETypeSet* set = base.vtable->supportedTypes(&base);
    Require(set == &base.supportedTypes && base.hasSupportedTypes && set->table == nullptr);
    Require(!supportsContainerType(&base, &type));
    Require(base.vtable->canDecodeType(&base, &type) == 0);
    AddTypes(&base, {"video/mp4"});
    Require(supportsContainerType(&base, &type));
    Require(base.vtable->canDecodeType(&base, &type) == 0);
    const String other = Text("VIDEO/MP4; codecs=avc1");
    Require(base.vtable->canDecodeType(&base, &other) == 0);
    const String plain = Text("Video/Mp4 ");
    Require(base.vtable->canDecodeType(&base, &plain) == 2);
    base.vtable->completeDestructor(&base);
}

static void TestTheSetOfTypes() {
    Cache cache;
    Arm(cache);
    Model model;
    const unsigned before = Text("x").impl->refCount;
    std::vector<String> kept;
    for (const std::string& type : ManyTypes()) {
        kept.push_back(Text(type));
        std::vector<String> one{kept.back()};
        StringVector vector = Vector(one);
        addSupportedTypes(&cache.object, &vector);
        Release(vector);
        model.Add(type);
        Require(cache.object.hasSupportedTypes);
        RequireSameAs(model, cache.object.supportedTypes.table);
    }
    Require(model.keyCount == 40 && model.buckets.size() == 64 && cache.counters.initialized == 0);
    for (const String& string : kept) Require(string.impl->refCount == before + 2);
    AddTypes(&cache.object, {"VIDEO/MP4", "audio/mpeg", "video/mp4"});
    RequireSameAs(model, cache.object.supportedTypes.table);
    AddTypes(&cache.object, {});
    RequireSameAs(model, cache.object.supportedTypes.table);
    Require(supportedTypes(&cache.object) == &cache.object.supportedTypes && cache.counters.initialized == 0);
    Require(Supports(cache, "APPLICATION/FLAC") && Supports(cache, "text/3gpp") && !Supports(cache, "text/3gp") && !Supports(cache, "text/3gppp"));

    std::vector<String> withNull{Text("a/b"), String()};
    StringVector vector = Vector(withNull);
    Require(ThrowsInvalidArgument([&] { addSupportedTypes(&cache.object, &vector); }));
    Release(vector);
    RequireSameAs(model, cache.object.supportedTypes.table);

    Cache copy;
    Arm(copy);
    copyConstruct(&copy.object, &cache.object);
    for (const String& string : kept) Require(string.impl->refCount == before + 4);
    RequireSameAs(Model::CopyOf(model, 128), copy.object.supportedTypes.table);
    destroy(&copy.object);
    for (const String& string : kept) Require(string.impl->refCount == before + 2);

    destroy(&cache.object);
    for (const String& string : kept) Require(string.impl->refCount == before);
    Require(GuardIntact(cache));
}

static void TestALargeSet() {
    Cache cache;
    Arm(cache);
    Model model;
    for (unsigned index = 0; index < 1500; ++index) {
        const std::string type = (index % 3 ? "type/" : "TYPE/") + std::to_string(index * 7919U);
        AddTypes(&cache.object, {type});
        model.Add(type);
        if (index == 766 || index == 767 || index == 768 || index == 1022 || index == 1023 || index == 1024 || index == 1499)
            RequireSameAs(model, cache.object.supportedTypes.table);
        if (index == 899) {
            Cache early;
            Arm(early);
            copyConstruct(&early.object, &cache.object);
            RequireSameAs(Model::CopyOf(model, 4096), early.object.supportedTypes.table);
            destroy(&early.object);
        }
    }
    Require(model.buckets.size() == 4096 && Supports(cache, "TYPE/7919") && !Supports(cache, "type/7920"));
    Cache copy;
    Arm(copy);
    copyConstruct(&copy.object, &cache.object);
    RequireSameAs(Model::CopyOf(model, 4096), copy.object.supportedTypes.table);
    destroy(&copy.object);
    destroy(&cache.object);
}

static void TestSupportsContainerType() {
    Cache cache;
    Arm(cache);
    cache.initialTypes = {"video/mp4", "audio/ogg"};
    cache.staticType = "static/type";
    cache.unsupportedType = "audio/ogg";
    const String null;
    Require(!supportsContainerType(&cache.object, &null) && !Supports(cache, ""));
    Require(cache.counters.available == 2 && cache.counters.unsupported == 0 && cache.counters.initialized == 0);
    Require(Supports(cache, "static/type") && cache.counters.staticType == 1 && cache.counters.initialized == 0 && !cache.object.hasSupportedTypes);
    Require(!Supports(cache, "audio/ogg") && cache.counters.staticType == 1 && cache.counters.initialized == 0);
    Require(Supports(cache, "VIDEO/mp4") && cache.counters.initialized == 1 && cache.object.hasSupportedTypes);
    Require(!Supports(cache, "video/webm") && cache.counters.initialized == 1);
    cache.unsupportedType.clear();
    Require(Supports(cache, "Audio/Ogg"));
    cache.isAvailable = false;
    const int unsupportedBefore = cache.counters.unsupported;
    Require(!Supports(cache, "video/mp4") && cache.counters.unsupported == unsupportedBefore);
    cache.object.vtable->completeDestructor(&cache.object);
    Require(cache.counters.destroyed == 1 && GuardIntact(cache));
}

static void TestCanDecodeType() {
    Cache cache;
    Arm(cache);
    cache.initialTypes = {"video/mp4", "audio/mpeg"};
    const String null;
    Require(canDecodeType(&cache.object, &null) == 0 && Decode(cache, "") == 0);
    Require(!cache.object.hasCachedResults && cache.counters.available == 0);

    const String kept = Text("video/mp4");
    const unsigned keptBefore = kept.impl->refCount;
    Require(canDecodeType(&cache.object, &kept) == 2 && cache.counters.extended == 0 && kept.impl->refCount == keptBefore + 2);
    Require(cache.object.hasCachedResults && Meta(cache.object.cachedResults.table, -3) == 1 && Meta(cache.object.cachedResults.table, -1) == 8);
    const int availableBefore = cache.counters.available;
    Require(Decode(cache, "VIDEO/MP4") == 2 && cache.counters.available == availableBefore);
    Require(Meta(cache.object.cachedResults.table, -3) == 1);

    Require(Decode(cache, "video/mp4; codecs=\"avc1.42E01E\"") == 0 && cache.counters.extended == 1);
    Require(cache.lastExtended == "video/mp4; codecs=\"avc1.42E01E\"");
    cache.canDecodeExtended = true;
    Require(Decode(cache, "video/mp4; codecs=\"avc1.42E01E\"") == 0 && cache.counters.extended == 1);
    Require(Decode(cache, "video/mp4; codecs=\"avc1.640028\"") == 1 && cache.counters.extended == 2);
    cache.canDecodeExtended = false;

    Require(Decode(cache, "audio/mpeg; codecs=\"mp3\"") == 1 && cache.counters.extended == 2);
    Require(Decode(cache, "Audio/MPEG; codecs=\" mp3 \"") == 1 && cache.counters.extended == 2);
    Require(Decode(cache, "audio/mpeg; codecs=\"mp3,\"") == 1 && cache.counters.extended == 2);
    Require(Decode(cache, "audio/mpeg; codecs=\"mp3 ,\"") == 1 && cache.counters.extended == 2);
    Require(Decode(cache, "audio/mpeg; codecs=\", mp3\"") == 1 && cache.counters.extended == 2);
    Require(Decode(cache, "audio/mpeg;codecs=\"MP3\"") == 0 && cache.counters.extended == 3);
    Require(Decode(cache, "audio/mpeg; codecs=\"mp3, mp4a\"") == 0 && cache.counters.extended == 4);
    Require(Decode(cache, "audio/mpeg; codecs=\",\"") == 2 && cache.counters.extended == 4);
    Require(Decode(cache, "video/mp4; codecs=\"mp3\"") == 0 && cache.counters.extended == 5);
    Require(Decode(cache, "video/webm") == 0 && Decode(cache, "video/webm; codecs=\"vp9\"") == 0 && cache.counters.extended == 5);

    const unsigned cachedBefore = Meta(cache.object.cachedResults.table, -3);
    Require(cachedBefore == 14 && Meta(cache.object.cachedResults.table, -1) == 32);
    cache.isAvailable = false;
    Require(Decode(cache, "video/mp4") == 2);
    Require(Decode(cache, "audio/mpeg") == 0);
    cache.isAvailable = true;
    Require(Decode(cache, "audio/mpeg") == 0 && Meta(cache.object.cachedResults.table, -3) == cachedBefore + 1);

    const MIMETypeResult* table = cache.object.cachedResults.table;
    unsigned live = 0;
    for (unsigned index = 0; index < Meta(table, -1); ++index) {
        if (!table[index].key) continue;
        ++live;
        if (Utf8(table[index].key) == "video/mp4") Require(table[index].value == 2);
        if (Utf8(table[index].key) == "audio/mpeg; codecs=\"mp3\"") Require(table[index].value == 1);
    }
    Require(live == cachedBefore + 1);
    destroy(&cache.object);
    Require(kept.impl->refCount == keptBefore);
    Require(GuardIntact(cache));
}

static void TestCodecEntries() {
    Cache cache;
    Arm(cache);
    cache.initialTypes = {"audio/mpeg"};
    Require(Decode(cache, "audio/mpeg; codecs=\"mp3, ,\"") == 0 && cache.counters.extended == 1);
    Require(Decode(cache, "audio/mpeg; codecs=\", ,mp3\"") == 0 && cache.counters.extended == 2);
    Require(Decode(cache, "audio/mpeg; codecs=\",\t\r\n,\"") == 0 && cache.counters.extended == 3);
    Require(Decode(cache, "audio/mpeg; codecs=\",\fmp3\f,\"") == 1 && cache.counters.extended == 3);
    Require(Decode(cache, "audio/mpeg; codecs=\",\tmp3\r\n,\"") == 1 && cache.counters.extended == 3);
    Require(Decode(cache, "audio/mpeg; codecs=\",\vmp3,\"") == 0 && cache.counters.extended == 4);
    destroy(&cache.object);
    Require(GuardIntact(cache));
}

static void TestCopies() {
    Cache source;
    Arm(source);
    Model model;
    for (std::size_t index = 0; index < 5; ++index) {
        AddTypes(&source.object, {ManyTypes()[index]});
        model.Add(ManyTypes()[index]);
    }
    Require(Meta(source.object.supportedTypes.table, -1) == 8);
    Require(Decode(source, "video/mp4") == 2 && Decode(source, "video/none") == 0);
    const String probe = Text("video/mp4");

    Cache copy;
    Arm(copy);
    copy.object.vtable = nullptr;
    copyConstruct(&copy.object, &source.object);
    Require(copy.object.vtable != nullptr && copy.object.vtable != &testVtable && GuardIntact(copy));
    Require(copy.object.hasSupportedTypes && copy.object.hasCachedResults);
    Require(copy.object.supportedTypes.table != source.object.supportedTypes.table && copy.object.cachedResults.table != source.object.cachedResults.table);
    RequireSameAs(Model::CopyOf(model, 16), copy.object.supportedTypes.table);
    Require(Meta(copy.object.cachedResults.table, -3) == 2 && Meta(copy.object.cachedResults.table, -1) == 8);
    Require(canDecodeType(&copy.object, &probe) == 2);

    Cache six;
    Arm(six);
    for (std::size_t index = 0; index < 5; ++index) AddTypes(&six.object, {ManyTypes()[index]});
    AddTypes(&six.object, {ManyTypes()[5]});
    model.Add(ManyTypes()[5]);
    Require(Meta(six.object.supportedTypes.table, -1) == 16);

    Cache target;
    Arm(target);
    const String old = Text("old/type");
    const unsigned oldBefore = old.impl->refCount;
    {
        std::vector<String> list{old};
        StringVector vector = Vector(list);
        addSupportedTypes(&target.object, &vector);
        Release(vector);
    }
    Require(canDecodeType(&target.object, &old) == 2 && old.impl->refCount == oldBefore + 4);
    Require(assign(&target.object, &six.object) == &target.object);
    Require(old.impl->refCount == oldBefore);
    Require(target.object.vtable == &testVtable && GuardIntact(target));
    RequireSameAs(Model::CopyOf(model, 16), target.object.supportedTypes.table);
    Require(!target.object.hasCachedResults && target.object.cachedResults.table == nullptr);
    Require(!supportsContainerType(&target.object, &old));

    Require(assign(&target.object, &target.object) == &target.object);
    RequireSameAs(Model::CopyOf(model, 16), target.object.supportedTypes.table);

    Cache empty;
    Arm(empty);
    Require(supportedTypes(&empty.object)->table == nullptr && empty.object.hasSupportedTypes);
    Require(assign(&target.object, &empty.object) == &target.object);
    Require(target.object.hasSupportedTypes && target.object.supportedTypes.table == nullptr && !target.object.hasCachedResults);
    Cache blank;
    Arm(blank);
    Require(assign(&target.object, &blank.object) == &target.object);
    Require(!target.object.hasSupportedTypes && !target.object.hasCachedResults);

    destroy(&copy.object);
    destroy(&source.object);
    destroy(&six.object);
    destroy(&target.object);
}

static void TestADeletedBucket() {
    Cache cache;
    Arm(cache);
    AddTypes(&cache.object, {"a/one", "a/two", "a/three"});
    StringImpl** table = cache.object.supportedTypes.table;
    unsigned removed = 8;
    for (unsigned index = 0; index < 8; ++index)
        if (table[index] && Utf8(table[index]) == "a/two") removed = index;
    Require(removed < 8);
    NKWebKit::WtfAbi::Release(table[removed]);
    table[removed] = reinterpret_cast<StringImpl*>(~std::uintptr_t(0));
    reinterpret_cast<unsigned*>(table)[-3] = 2;
    reinterpret_cast<unsigned*>(table)[-4] = 1;
    Require(Supports(cache, "a/one") && Supports(cache, "a/three") && !Supports(cache, "a/two"));

    Cache copy;
    Arm(copy);
    copyConstruct(&copy.object, &cache.object);
    Require(Meta(copy.object.supportedTypes.table, -3) == 2 && Meta(copy.object.supportedTypes.table, -4) == 0);
    const String one = Text("a/one");
    Require(supportsContainerType(&copy.object, &one));
    destroy(&copy.object);

    AddTypes(&cache.object, {"a/two"});
    Require(cache.object.supportedTypes.table == table && Meta(table, -3) == 3 && Meta(table, -4) == 0 && Supports(cache, "A/TWO"));
    destroy(&cache.object);

    Cache sparse;
    Arm(sparse);
    AddTypes(&sparse.object, {"b/one", "b/two", "b/three", "b/four", "b/five"});
    StringImpl** old = sparse.object.supportedTypes.table;
    Require(Meta(old, -1) == 8);
    bool keptOne = false;
    for (unsigned index = 0; index < 8; ++index) {
        if (!old[index]) continue;
        if (!keptOne && Utf8(old[index]) == "b/one") {
            keptOne = true;
            continue;
        }
        NKWebKit::WtfAbi::Release(old[index]);
        old[index] = reinterpret_cast<StringImpl*>(~std::uintptr_t(0));
    }
    reinterpret_cast<unsigned*>(old)[-3] = 1;
    reinterpret_cast<unsigned*>(old)[-4] = 4;
    std::string fresh;
    for (unsigned candidate = 0; fresh.empty(); ++candidate) {
        const std::string name = "c/" + std::to_string(candidate);
        if (!old[OracleHash(name) & 7]) fresh = name;
    }
    AddTypes(&sparse.object, {fresh});
    StringImpl** rebuilt = sparse.object.supportedTypes.table;
    Require(rebuilt != old && Meta(rebuilt, -1) == 8 && Meta(rebuilt, -3) == 2 && Meta(rebuilt, -4) == 0);
    Require(Supports(sparse, "b/one") && Supports(sparse, fresh) && !Supports(sparse, "b/two"));
    for (unsigned index = 0; index < 8; ++index)
        if (rebuilt[index] && Utf8(rebuilt[index]) == fresh) {
            NKWebKit::WtfAbi::Release(rebuilt[index]);
            rebuilt[index] = reinterpret_cast<StringImpl*>(~std::uintptr_t(0));
        }
    reinterpret_cast<unsigned*>(rebuilt)[-3] = 1;
    reinterpret_cast<unsigned*>(rebuilt)[-4] = 1;
    destroy(&sparse.object);
}

static void TestNullArguments() {
    Cache cache;
    Arm(cache);
    MIMETypeCache other{};
    const String type = Text("a/b");
    std::vector<String> none;
    StringVector vector = Vector(none);
    Require(ThrowsInvalidArgument([&] { copyConstruct(nullptr, &cache.object); }));
    Require(ThrowsInvalidArgument([&] { copyConstruct(&other, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore13MIMETypeCacheC2ERKS0_(&other, nullptr); }));
    Require(ThrowsInvalidArgument([&] { destroy(nullptr); }));
    Require(ThrowsInvalidArgument([&] { assign(nullptr, &cache.object); }));
    Require(ThrowsInvalidArgument([&] { assign(&cache.object, nullptr); }));
    Require(ThrowsInvalidArgument([&] { canDecodeType(nullptr, &type); }));
    Require(ThrowsInvalidArgument([&] { canDecodeType(&cache.object, nullptr); }));
    Require(ThrowsInvalidArgument([&] { supportedTypes(nullptr); }));
    Require(ThrowsInvalidArgument([&] { addSupportedTypes(nullptr, &vector); }));
    Require(ThrowsInvalidArgument([&] { addSupportedTypes(&cache.object, nullptr); }));
    Require(ThrowsInvalidArgument([&] { supportsContainerType(nullptr, &type); }));
    Require(ThrowsInvalidArgument([&] { supportsContainerType(&cache.object, nullptr); }));
    Require(!cache.object.hasSupportedTypes && !cache.object.hasCachedResults && GuardIntact(cache));
    _ZN7WebCore13MIMETypeCacheC2ERKS0_(&other, &cache.object);
    Require(other.vtable != nullptr && !other.hasSupportedTypes);
}

int main() {
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(20));
        std::fprintf(stderr, "mime type cache test timed out\n");
        std::fflush(stderr);
        std::_Exit(3);
    }).detach();
    TestTheBaseClass();
    TestTheSetOfTypes();
    TestALargeSet();
    TestSupportsContainerType();
    TestCanDecodeType();
    TestCodecEntries();
    TestCopies();
    TestADeletedBucket();
    TestNullArguments();
    std::puts("libSceNKWebKit mime type cache tests passed");
    return 0;
}
