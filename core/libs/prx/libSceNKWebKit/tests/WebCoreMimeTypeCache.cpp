#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/MimeTypeCacheExports.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

using namespace NKWebKit::MimeTypeCacheAbi;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCoreMimeTypeCache check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Destructor = void(APS5_VABI*)(MIMETypeCache*);

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

static StringImpl* const DeletedKey = reinterpret_cast<StringImpl*>(~std::uintptr_t(0));

template <typename Bucket>
static Bucket* AllocateTable(unsigned size, unsigned keyCount, unsigned deletedCount) {
    auto* raw = static_cast<unsigned*>(_ZN3WTF16fastZeroedMallocEm(4 * sizeof(unsigned) + size * sizeof(Bucket)));
    raw[0] = deletedCount;
    raw[1] = keyCount;
    raw[2] = size - 1;
    raw[3] = size;
    return reinterpret_cast<Bucket*>(raw + 4);
}

static StringImpl* Retain(const String& string) {
    string.impl->refCount += 2;
    return string.impl;
}

static MIMETypeCache* NewCache() {
    return new (_ZN3WTF10fastMallocEm(sizeof(MIMETypeCache))) MIMETypeCache{};
}

struct Held {
    String video = Latin1("video/mp4");
    String audio = Latin1("audio/mpeg");
    String text = Latin1("text/vtt");
};

static void FillBoth(MIMETypeCache& cache, const Held& held) {
    auto* types = AllocateTable<StringImpl*>(8, 2, 1);
    types[1] = Retain(held.video);
    types[4] = DeletedKey;
    types[6] = Retain(held.audio);
    cache.hasSupportedTypes = true;
    cache.supportedTypes = types;
    auto* results = AllocateTable<SupportsTypeBucket>(8, 2, 1);
    results[0] = {Retain(held.text), SupportsType::IsSupported};
    results[3] = {DeletedKey, SupportsType::IsNotSupported};
    results[7] = {Retain(held.video), SupportsType::MayBeSupported};
    cache.hasCachedResults = true;
    cache.cachedResults = results;
}

static bool RefCounts(const Held& held, unsigned video, unsigned audio, unsigned text) {
    return held.video.impl->refCount == video && held.audio.impl->refCount == audio && held.text.impl->refCount == text;
}

static void BaseDestructorReleasesBothTables() {
    Held held;
    auto* cache = NewCache();
    FillBoth(*cache, held);
    Require(RefCounts(held, 6, 4, 4));
    _ZN7WebCore13MIMETypeCacheD2Ev(cache);
    Require(RefCounts(held, 2, 2, 2));
    _ZN3WTF8fastFreeEPv(cache);
}

static void CompleteDestructorForwardsToBase() {
    Held held;
    auto* cache = NewCache();
    FillBoth(*cache, held);
    reinterpret_cast<Destructor>(&_ZN7WebCore13MIMETypeCacheD1Ev)(cache);
    Require(RefCounts(held, 2, 2, 2));
    _ZN3WTF8fastFreeEPv(cache);
}

static void DeletingDestructorReleasesAndFrees() {
    Held held;
    auto* cache = NewCache();
    FillBoth(*cache, held);
    _ZN7WebCore13MIMETypeCacheD0Ev(cache);
    Require(RefCounts(held, 2, 2, 2));
}

static void DisengagedOptionalsAreIgnored() {
    Held held;
    auto* cache = NewCache();
    cache->supportedTypes = reinterpret_cast<StringImpl**>(std::uintptr_t(0x10));
    cache->cachedResults = reinterpret_cast<SupportsTypeBucket*>(std::uintptr_t(0x20));
    _ZN7WebCore13MIMETypeCacheD0Ev(cache);
    auto* empty = NewCache();
    empty->hasSupportedTypes = true;
    empty->hasCachedResults = true;
    _ZN7WebCore13MIMETypeCacheD0Ev(empty);
    Require(RefCounts(held, 2, 2, 2));
}

static void IsEmptyNeedsAnEngagedEmptySet() {
    Held held;
    auto* cache = NewCache();
    Require(!_ZNK7WebCore13MIMETypeCache7isEmptyEv(cache));
    cache->hasSupportedTypes = true;
    Require(_ZNK7WebCore13MIMETypeCache7isEmptyEv(cache));
    auto* types = AllocateTable<StringImpl*>(8, 0, 1);
    types[2] = DeletedKey;
    cache->supportedTypes = types;
    Require(_ZNK7WebCore13MIMETypeCache7isEmptyEv(cache));
    types[5] = Retain(held.audio);
    reinterpret_cast<unsigned*>(types)[-3] = 1;
    Require(!_ZNK7WebCore13MIMETypeCache7isEmptyEv(cache));
    cache->hasSupportedTypes = false;
    Require(!_ZNK7WebCore13MIMETypeCache7isEmptyEv(cache));
    cache->hasSupportedTypes = true;
    _ZN7WebCore13MIMETypeCacheD0Ev(cache);
    Require(RefCounts(held, 2, 2, 2));
}

static void NullThisThrows() {
    Require(ThrowsInvalidArgument([] { _ZN7WebCore13MIMETypeCacheD2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore13MIMETypeCacheD0Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { reinterpret_cast<Destructor>(&_ZN7WebCore13MIMETypeCacheD1Ev)(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore13MIMETypeCache7isEmptyEv(nullptr); }));
}

int main() {
    BaseDestructorReleasesBothTables();
    CompleteDestructorForwardsToBase();
    DeletingDestructorReleasesAndFrees();
    DisengagedOptionalsAreIgnored();
    IsEmptyNeedsAnEngagedEmptySet();
    NullThisThrows();
    std::puts("WebCoreMimeTypeCache tests passed");
    return 0;
}
