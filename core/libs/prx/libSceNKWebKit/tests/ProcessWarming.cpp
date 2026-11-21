#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

using NKWebKit::WebCoreAbi::PrewarmInformation;
using NKWebKit::WtfAbi::String;
using StringVector = NKWebKit::WtfAbi::Vector<String>;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ProcessWarming check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

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

static String* NewBuffer(unsigned count) {
    return static_cast<String*>(_ZN3WTF10fastMallocEm(std::size_t{count} * sizeof(String)));
}

static void Adopt(StringVector& vector, unsigned capacity, unsigned count, const char* const* texts) {
    vector.buffer = NewBuffer(capacity);
    vector.capacity = capacity;
    for (unsigned i = 0; i < count; ++i) new (&vector.buffer[i]) String(Latin1(texts[i]));
    vector.size = count;
}

static bool Is(const String& string, const char* text) {
    return string.impl && string.impl->length == std::strlen(text) && std::memcmp(string.impl->data, text, string.impl->length) == 0;
}

static bool Empty(const StringVector& vector) {
    return !vector.buffer && !vector.capacity && !vector.size;
}

static const char* const Single[] = {"old"};
static const char* const Pair[] = {"alpha", "beta"};

static PrewarmInformation* Constructed(void* storage) {
    return reinterpret_cast<PrewarmInformation*>(storage);
}

static void ConstructsEmpty() {
    for (auto construct : {&_ZN7WebCore18PrewarmInformationC1Ev, &_ZN7WebCore18PrewarmInformationC2Ev}) {
        alignas(PrewarmInformation) unsigned char storage[sizeof(PrewarmInformation)];
        std::memset(storage, 0xAB, sizeof(storage));
        PrewarmInformation* self = Constructed(storage);
        construct(self);
        Require(Empty(self->fontCache.seenFamilies));
        Require(Empty(self->fontCache.fontNamesRequiringSystemFallback));
        _ZN7WebCore18PrewarmInformationD2Ev(self);
    }
}

static void CollectsEmpty() {
    PrewarmInformation collected = _ZN7WebCore14ProcessWarming25collectPrewarmInformationEv();
    Require(Empty(collected.fontCache.seenFamilies));
    Require(Empty(collected.fontCache.fontNamesRequiringSystemFallback));
}

static void CopiesIndependently() {
    const char* families[] = {"Helvetica", "Times"};
    const char* fallback[] = {"serif"};
    PrewarmInformation source;
    Adopt(source.fontCache.seenFamilies, 2, 2, families);
    Adopt(source.fontCache.fontNamesRequiringSystemFallback, 1, 1, fallback);
    const unsigned familyReferences = source.fontCache.seenFamilies.buffer[0].impl->refCount;
    const unsigned fallbackReferences = source.fontCache.fontNamesRequiringSystemFallback.buffer[0].impl->refCount;
    for (auto copy : {&_ZN7WebCore18PrewarmInformationC1ERKS0_, &_ZN7WebCore18PrewarmInformationC2ERKS0_}) {
        alignas(PrewarmInformation) unsigned char storage[sizeof(PrewarmInformation)];
        PrewarmInformation* copied = Constructed(storage);
        copy(copied, &source);
        Require(copied->fontCache.seenFamilies.size == 2 && copied->fontCache.seenFamilies.capacity == 2);
        Require(copied->fontCache.seenFamilies.buffer != source.fontCache.seenFamilies.buffer);
        Require(copied->fontCache.seenFamilies.buffer[0].impl == source.fontCache.seenFamilies.buffer[0].impl);
        Require(Is(copied->fontCache.seenFamilies.buffer[1], "Times"));
        Require(copied->fontCache.fontNamesRequiringSystemFallback.size == 1 && copied->fontCache.fontNamesRequiringSystemFallback.capacity == 1);
        Require(Is(copied->fontCache.fontNamesRequiringSystemFallback.buffer[0], "serif"));
        Require(source.fontCache.seenFamilies.buffer[0].impl->refCount == familyReferences + 2);
        Require(source.fontCache.fontNamesRequiringSystemFallback.buffer[0].impl->refCount == fallbackReferences + 2);
        _ZN7WebCore18PrewarmInformationD1Ev(copied);
        Require(source.fontCache.seenFamilies.buffer[0].impl->refCount == familyReferences);
        Require(source.fontCache.fontNamesRequiringSystemFallback.buffer[0].impl->refCount == fallbackReferences);
    }
    Require(source.fontCache.seenFamilies.size == 2 && Is(source.fontCache.seenFamilies.buffer[0], "Helvetica"));

    alignas(PrewarmInformation) unsigned char emptySourceStorage[sizeof(PrewarmInformation)];
    PrewarmInformation* emptySource = Constructed(emptySourceStorage);
    _ZN7WebCore18PrewarmInformationC1Ev(emptySource);
    alignas(PrewarmInformation) unsigned char emptyCopyStorage[sizeof(PrewarmInformation)];
    PrewarmInformation* emptyCopy = Constructed(emptyCopyStorage);
    _ZN7WebCore18PrewarmInformationC2ERKS0_(emptyCopy, emptySource);
    Require(Empty(emptyCopy->fontCache.seenFamilies) && Empty(emptyCopy->fontCache.fontNamesRequiringSystemFallback));
    _ZN7WebCore18PrewarmInformationD2Ev(emptyCopy);
    _ZN7WebCore18PrewarmInformationD2Ev(emptySource);
}

static void CopiesReservedButEmpty() {
    alignas(PrewarmInformation) unsigned char sourceStorage[sizeof(PrewarmInformation)];
    PrewarmInformation* source = Constructed(sourceStorage);
    _ZN7WebCore18PrewarmInformationC1Ev(source);
    Adopt(source->fontCache.seenFamilies, 3, 0, nullptr);

    alignas(PrewarmInformation) unsigned char copyStorage[sizeof(PrewarmInformation)];
    PrewarmInformation* copied = Constructed(copyStorage);
    _ZN7WebCore18PrewarmInformationC2ERKS0_(copied, source);
    Require(Empty(copied->fontCache.seenFamilies) && Empty(copied->fontCache.fontNamesRequiringSystemFallback));
    _ZN7WebCore18PrewarmInformationD2Ev(copied);
    _ZN7WebCore18PrewarmInformationD2Ev(source);
}

static void AssignsInPlace() {
    PrewarmInformation source;
    Adopt(source.fontCache.seenFamilies, 2, 2, Pair);
    const unsigned alphaReferences = source.fontCache.seenFamilies.buffer[0].impl->refCount;

    PrewarmInformation target;
    Adopt(target.fontCache.seenFamilies, 4, 1, Single);
    const String* targetBuffer = target.fontCache.seenFamilies.buffer;

    auto& assigned = _ZN7WebCore18PrewarmInformationaSERKS0_(&target, &source);
    Require(&assigned == &target);
    Require(target.fontCache.seenFamilies.buffer == targetBuffer);
    Require(target.fontCache.seenFamilies.capacity == 4);
    Require(target.fontCache.seenFamilies.size == 2);
    Require(target.fontCache.seenFamilies.buffer[0].impl == source.fontCache.seenFamilies.buffer[0].impl);
    Require(target.fontCache.seenFamilies.buffer[0].impl->refCount == alphaReferences + 2);
    Require(Is(target.fontCache.seenFamilies.buffer[1], "beta"));
    _ZN7WebCore18PrewarmInformationD2Ev(&target);
}

static void AssignsShrinking() {
    PrewarmInformation source;
    Adopt(source.fontCache.seenFamilies, 1, 1, Single);

    PrewarmInformation target;
    Adopt(target.fontCache.seenFamilies, 4, 2, Pair);
    const String* targetBuffer = target.fontCache.seenFamilies.buffer;

    _ZN7WebCore18PrewarmInformationaSERKS0_(&target, &source);
    Require(target.fontCache.seenFamilies.buffer == targetBuffer);
    Require(target.fontCache.seenFamilies.capacity == 4);
    Require(target.fontCache.seenFamilies.size == 1);
    Require(Is(target.fontCache.seenFamilies.buffer[0], "old"));
    _ZN7WebCore18PrewarmInformationD2Ev(&target);
}

static void AssignsReallocating() {
    const char* triple[] = {"one", "two", "three"};
    PrewarmInformation source;
    Adopt(source.fontCache.seenFamilies, 3, 3, triple);

    PrewarmInformation target;
    Adopt(target.fontCache.seenFamilies, 1, 1, Single);

    _ZN7WebCore18PrewarmInformationaSERKS0_(&target, &source);
    Require(target.fontCache.seenFamilies.capacity == 3);
    Require(target.fontCache.seenFamilies.size == 3);
    Require(Is(target.fontCache.seenFamilies.buffer[0], "one"));
    Require(Is(target.fontCache.seenFamilies.buffer[2], "three"));
    _ZN7WebCore18PrewarmInformationD2Ev(&target);
}

static void AssignsSelf() {
    alignas(PrewarmInformation) unsigned char targetStorage[sizeof(PrewarmInformation)];
    PrewarmInformation* target = Constructed(targetStorage);
    _ZN7WebCore18PrewarmInformationC1Ev(target);
    const char* solo[] = {"solo"};
    Adopt(target->fontCache.seenFamilies, 2, 1, solo);
    const char* fallback[] = {"fallback"};
    Adopt(target->fontCache.fontNamesRequiringSystemFallback, 1, 1, fallback);
    const String* familyBuffer = target->fontCache.seenFamilies.buffer;

    Require(&_ZN7WebCore18PrewarmInformationaSERKS0_(target, target) == target);
    Require(target->fontCache.seenFamilies.buffer == familyBuffer);
    Require(target->fontCache.seenFamilies.size == 1 && Is(target->fontCache.seenFamilies.buffer[0], "solo"));
    Require(target->fontCache.fontNamesRequiringSystemFallback.size == 1 && Is(target->fontCache.fontNamesRequiringSystemFallback.buffer[0], "fallback"));
    _ZN7WebCore18PrewarmInformationD2Ev(target);
}

static void AssignsSameSize() {
    PrewarmInformation source;
    Adopt(source.fontCache.seenFamilies, 2, 2, Pair);
    const unsigned alphaReferences = source.fontCache.seenFamilies.buffer[0].impl->refCount;

    PrewarmInformation target;
    Adopt(target.fontCache.seenFamilies, 2, 2, Pair);

    _ZN7WebCore18PrewarmInformationaSERKS0_(&target, &source);
    Require(target.fontCache.seenFamilies.buffer[0].impl == source.fontCache.seenFamilies.buffer[0].impl);
    Require(target.fontCache.seenFamilies.buffer[0].impl->refCount == alphaReferences + 2);
    Require(Is(target.fontCache.seenFamilies.buffer[1], "beta"));
    _ZN7WebCore18PrewarmInformationD2Ev(&target);
    Require(source.fontCache.seenFamilies.buffer[0].impl->refCount == alphaReferences);
}

static void RefusesCorruptVectors() {
    alignas(PrewarmInformation) unsigned char oversizedStorage[sizeof(PrewarmInformation)];
    std::memset(oversizedStorage, 0, sizeof(oversizedStorage));
    auto* oversizedSource = reinterpret_cast<PrewarmInformation*>(oversizedStorage);
    oversizedSource->fontCache.seenFamilies.size = 2;
    oversizedSource->fontCache.seenFamilies.capacity = 1;
    {
        PrewarmInformation copied;
        Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationC2ERKS0_(&copied, oversizedSource); }));
        Require(Empty(copied.fontCache.seenFamilies));
    }

    alignas(PrewarmInformation) unsigned char nullStorageSourceStorage[sizeof(PrewarmInformation)];
    std::memset(nullStorageSourceStorage, 0, sizeof(nullStorageSourceStorage));
    auto* nullStorageSource = reinterpret_cast<PrewarmInformation*>(nullStorageSourceStorage);
    nullStorageSource->fontCache.fontNamesRequiringSystemFallback.size = 1;
    {
        PrewarmInformation copied;
        Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationC1ERKS0_(&copied, nullStorageSource); }));
        Require(Empty(copied.fontCache.fontNamesRequiringSystemFallback));
    }

    alignas(PrewarmInformation) unsigned char targetStorage[sizeof(PrewarmInformation)];
    PrewarmInformation* target = Constructed(targetStorage);
    _ZN7WebCore18PrewarmInformationC1Ev(target);
    const char* names[] = {"x"};
    Adopt(target->fontCache.seenFamilies, 1, 1, names);
    const String* targetBuffer = target->fontCache.seenFamilies.buffer;
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationaSERKS0_(target, oversizedSource); }));
    Require(target->fontCache.seenFamilies.buffer == targetBuffer);
    Require(target->fontCache.seenFamilies.size == 1 && Is(target->fontCache.seenFamilies.buffer[0], "x"));

    alignas(PrewarmInformation) unsigned char oversizedTargetStorage[sizeof(PrewarmInformation)];
    std::memset(oversizedTargetStorage, 0, sizeof(oversizedTargetStorage));
    auto* oversizedTarget = reinterpret_cast<PrewarmInformation*>(oversizedTargetStorage);
    oversizedTarget->fontCache.seenFamilies.buffer = NewBuffer(1);
    oversizedTarget->fontCache.seenFamilies.capacity = 1;
    new (&oversizedTarget->fontCache.seenFamilies.buffer[0]) String(Latin1("x"));
    oversizedTarget->fontCache.seenFamilies.size = 2;
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationaSERKS0_(oversizedTarget, target); }));
    Require(Empty(oversizedTarget->fontCache.fontNamesRequiringSystemFallback));
    oversizedTarget->fontCache.seenFamilies.size = 1;
    _ZN7WebCore18PrewarmInformationD2Ev(oversizedTarget);

    alignas(PrewarmInformation) unsigned char corruptSelfStorage[sizeof(PrewarmInformation)];
    std::memset(corruptSelfStorage, 0, sizeof(corruptSelfStorage));
    auto* corruptSelf = reinterpret_cast<PrewarmInformation*>(corruptSelfStorage);
    corruptSelf->fontCache.fontNamesRequiringSystemFallback.size = 1;
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationD1Ev(corruptSelf); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationD2Ev(corruptSelf); }));

    _ZN7WebCore18PrewarmInformationD2Ev(target);
}

static void RefusesNull() {
    alignas(PrewarmInformation) unsigned char selfStorage[sizeof(PrewarmInformation)];
    PrewarmInformation* self = Constructed(selfStorage);
    _ZN7WebCore18PrewarmInformationC1Ev(self);
    const char* names[] = {"x"};
    Adopt(self->fontCache.seenFamilies, 1, 1, names);
    Require(ThrowsInvalidArgument([] { _ZN7WebCore18PrewarmInformationC1Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore18PrewarmInformationC2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationC1ERKS0_(self, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationC2ERKS0_(self, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationC1ERKS0_(nullptr, self); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationC2ERKS0_(nullptr, self); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationaSERKS0_(nullptr, self); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18PrewarmInformationaSERKS0_(self, nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore18PrewarmInformationD1Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore18PrewarmInformationD2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore14ProcessWarming22prewarmWithInformationERKNS_18PrewarmInformationE(nullptr); }));
    const unsigned keptReferences = self->fontCache.seenFamilies.buffer[0].impl->refCount;
    _ZN7WebCore14ProcessWarming22prewarmWithInformationERKNS_18PrewarmInformationE(self);
    Require(self->fontCache.seenFamilies.size == 1 && Is(self->fontCache.seenFamilies.buffer[0], "x"));
    Require(self->fontCache.seenFamilies.buffer[0].impl->refCount == keptReferences);
    _ZN7WebCore18PrewarmInformationD2Ev(self);
}

int main() {
    ConstructsEmpty();
    CollectsEmpty();
    CopiesIndependently();
    CopiesReservedButEmpty();
    AssignsInPlace();
    AssignsShrinking();
    AssignsReallocating();
    AssignsSameSize();
    AssignsSelf();
    RefusesNull();
    RefusesCorruptVectors();
    std::puts("ProcessWarming passed");
    return 0;
}
