#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::WtfAbi::AtomString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using MediaContentType = NKWebKit::WebCoreAbi::ContentType;
using MediaVector = NKWebKit::WtfAbi::Vector<MediaContentType>;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SettingsBase check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

namespace {

constexpr std::size_t kFamiliesSize = 56;
constexpr std::size_t kSettingsSize = 128;
constexpr std::size_t kMediaVectorOffset = 72;
constexpr std::size_t kPageOffset = 8;
constexpr std::size_t kMinimumDOMTimerIntervalOffset = 16;
constexpr std::size_t kStandardMapOffset = 0;
constexpr std::size_t kSerifMapOffset = 8;
constexpr std::size_t kFixedMapOffset = 16;
constexpr std::size_t kSansSerifMapOffset = 24;
constexpr std::size_t kCursiveMapOffset = 32;
constexpr std::size_t kFantasyMapOffset = 40;
constexpr std::size_t kPictographMapOffset = 48;
constexpr std::size_t kTableMetadataBytes = 16;
constexpr int kGuestEmptyKey = -2;
constexpr unsigned kMaxSnapshotEntries = 32;

alignas(8) unsigned char familiesStorage[kFamiliesSize];

bool nonConstGetterUsed = false;
bool constGetterUsed = false;

void* APS5_VABI GetFontGenericFamilies(void*) {
    nonConstGetterUsed = true;
    return familiesStorage;
}

void* APS5_VABI GetFontGenericFamiliesConstant(void*) {
    constGetterUsed = true;
    return familiesStorage;
}

void ResetGetterFlags() {
    nonConstGetterUsed = false;
    constGetterUsed = false;
}

void ResetFamilies() {
    std::memset(familiesStorage, 0, kFamiliesSize);
}

void* settings() {
    alignas(8) static unsigned char storage[kSettingsSize];
    return storage;
}

void ResetSettings() {
    void* object = settings();
    static void* const entries[4] = {reinterpret_cast<void*>(&GetFontGenericFamilies), reinterpret_cast<void*>(&GetFontGenericFamiliesConstant), nullptr, nullptr};
    std::memset(object, 0, kSettingsSize);
    const void* vptr = entries;
    std::memcpy(object, &vptr, sizeof(vptr));
}

String MakeLatin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

AtomString MakeFamily(const char* text) {
    return AtomString{MakeLatin1(text)};
}

bool FamilyEquals(const String& family, const char* text) {
    String other = MakeLatin1(text);
    return _ZN3WTF5equalEPKNS_10StringImplES2_(family.impl, other.impl);
}

const void* MapTable(std::size_t mapOffset) {
    const void* table;
    std::memcpy(&table, familiesStorage + mapOffset, sizeof(table));
    return table;
}

unsigned TableMetadata(const void* table, int slot) {
    if (!table) return 0;
    unsigned value;
    std::memcpy(&value, static_cast<const unsigned char*>(table) - 4 * slot, sizeof(value));
    return value;
}

struct GuestTable {
    unsigned char* storage;
    void* entries;
    unsigned size;
};

struct GuestEntry {
    int key;
    StringImpl* value;
};

GuestTable CreateGuestTable(unsigned size) {
    GuestTable table;
    table.size = size;
    table.storage = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(kTableMetadataBytes + std::size_t{size} * sizeof(GuestEntry)));
    auto* meta = reinterpret_cast<unsigned*>(table.storage);
    meta[0] = 0;
    meta[1] = 0;
    meta[2] = size - 1;
    meta[3] = size;
    auto* entries = reinterpret_cast<GuestEntry*>(table.storage + kTableMetadataBytes);
    for (unsigned i = 0; i < size; ++i) {
        entries[i].key = kGuestEmptyKey;
        entries[i].value = nullptr;
    }
    table.entries = entries;
    return table;
}

unsigned GuestHashScript(int script) {
    std::uint32_t key = static_cast<std::uint32_t>(script);
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

void InsertGuestFamily(const GuestTable& table, int script, StringImpl* impl) {
    auto* entries = static_cast<GuestEntry*>(table.entries);
    const unsigned sizeMask = table.size - 1;
    unsigned i = GuestHashScript(script) & sizeMask;
    unsigned probeCount = 0;
    while (entries[i].key != kGuestEmptyKey) {
        Require(entries[i].key != script);
        ++probeCount;
        i = (i + probeCount) & sizeMask;
    }
    entries[i].key = script;
    entries[i].value = impl;
    ++reinterpret_cast<unsigned*>(table.storage)[1];
}

void InstallMap(std::size_t mapOffset, const GuestTable& table) {
    const void* pointer = table.entries;
    std::memcpy(familiesStorage + mapOffset, &pointer, sizeof(pointer));
}

struct MapState {
    const void* table;
    unsigned size;
    unsigned char bytes[kTableMetadataBytes + kMaxSnapshotEntries * 16];
};

MapState SnapshotMap(std::size_t mapOffset) {
    MapState state;
    std::memset(&state, 0, sizeof(state));
    std::memcpy(&state.table, familiesStorage + mapOffset, sizeof(state.table));
    state.size = state.table ? TableMetadata(state.table, 1) : 0;
    Require(state.size <= kMaxSnapshotEntries);
    if (state.table) {
        std::memcpy(state.bytes, static_cast<const unsigned char*>(state.table) - kTableMetadataBytes, kTableMetadataBytes + std::size_t{state.size} * 16);
    }
    return state;
}

void RequireMapUnchanged(const MapState& before, std::size_t mapOffset, int line) {
    const void* table = MapTable(mapOffset);
    if (table != before.table) {
        std::fprintf(stderr, "SettingsBase table pointer changed at line %d\n", line);
        std::abort();
    }
    if (TableMetadata(table, 1) != before.size || TableMetadata(table, 3) != TableMetadata(before.table, 3) || TableMetadata(table, 4) != TableMetadata(before.table, 4)) {
        std::fprintf(stderr, "SettingsBase table metadata changed at line %d\n", line);
        std::abort();
    }
    if (before.table && std::memcmp(static_cast<const unsigned char*>(table) - kTableMetadataBytes, before.bytes, kTableMetadataBytes + std::size_t{before.size} * 16) != 0) {
        std::fprintf(stderr, "SettingsBase table bytes changed at line %d\n", line);
        std::abort();
    }
}
#define RequireMapUnchangedAt(before, offset) RequireMapUnchanged((before), (offset), __LINE__)

MediaVector& MediaVectorOf() {
    return *reinterpret_cast<MediaVector*>(static_cast<unsigned char*>(settings()) + kMediaVectorOffset);
}

void ResetMediaVector() {
    MediaVector& vector = MediaVectorOf();
    for (unsigned i = 0; i < vector.size; ++i) vector.buffer[i].~MediaContentType();
    if (vector.buffer) _ZN3WTF8fastFreeEPv(vector.buffer);
    std::memset(static_cast<unsigned char*>(settings()) + kMediaVectorOffset, 0, sizeof(MediaVector));
}

template <typename Function>
void RequireThrowsInvalidArgument(Function&& function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

template <typename Function>
void RequireNotImplemented(Function&& function, const char* name) {
    const auto before = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error& error) {
        threw = true;
        Require(std::strstr(error.what(), name) != nullptr);
    }
    Require(threw);
    Require(NotImplementedCalls_nid_no_patch() == before + 1);
}

using FamilySetter = void (APS5_VABI*)(void*, const AtomString*, int);
using FamilyGetter = const String& (APS5_VABI*)(const void*, int);

constexpr FamilySetter familySetters[] = {
    _ZN7WebCore12SettingsBase18setFixedFontFamilyERKN3WTF10AtomStringE11UScriptCode,
    _ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode,
    _ZN7WebCore12SettingsBase20setCursiveFontFamilyERKN3WTF10AtomStringE11UScriptCode,
    _ZN7WebCore12SettingsBase20setFantasyFontFamilyERKN3WTF10AtomStringE11UScriptCode,
    _ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode,
    _ZN7WebCore12SettingsBase22setSansSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode,
    _ZN7WebCore12SettingsBase23setPictographFontFamilyERKN3WTF10AtomStringE11UScriptCode,
};

constexpr FamilyGetter familyGetters[] = {
    _ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode,
    _ZNK7WebCore12SettingsBase15serifFontFamilyE11UScriptCode,
    _ZNK7WebCore12SettingsBase17cursiveFontFamilyE11UScriptCode,
    _ZNK7WebCore12SettingsBase17fantasyFontFamilyE11UScriptCode,
    _ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode,
    _ZNK7WebCore12SettingsBase19sansSerifFontFamilyE11UScriptCode,
    _ZNK7WebCore12SettingsBase20pictographFontFamilyE11UScriptCode,
};

void RequireSetterNotImplemented(FamilySetter setter, const char* name, const AtomString& family, int script) {
    RequireNotImplemented([&] {
        setter(settings(), &family, script);
    }, name);
}

void RequireSetterQuiet(FamilySetter setter, const AtomString& family, int script) {
    const auto before = NotImplementedCalls_nid_no_patch();
    setter(settings(), &family, script);
    Require(NotImplementedCalls_nid_no_patch() == before);
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

void TestGuestPointers() {
    GuardedPages pages;
    ResetSettings();
    ResetFamilies();

    const AtomString validFamily = MakeFamily("Times");
    RequireNotImplemented([&] {
        _ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(pages.Guard(), 0);
    }, "_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode");
    RequireNotImplemented([&] {
        _ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode(pages.Guard(), &validFamily, 0);
    }, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode");
    RequireNotImplemented([&] {
        _ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode(
            settings(), reinterpret_cast<const AtomString*>(pages.Guard()), 0);
    }, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode");

    {
        alignas(8) unsigned char badVtableSettings[kSettingsSize]{};
        void* badVtable = pages.Guard();
        std::memcpy(badVtableSettings, &badVtable, sizeof(badVtable));
        RequireNotImplemented([&] {
            _ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode(badVtableSettings, 0);
        }, "_ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode");
    }

    {
        void* badTable = pages.Guard();
        std::memcpy(familiesStorage + kStandardMapOffset, &badTable, sizeof(badTable));
        RequireNotImplemented([&] {
            _ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 0);
        }, "_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode");
        ResetFamilies();
    }

    {
        AtomString badFamily;
        badFamily.string.impl = reinterpret_cast<StringImpl*>(pages.Guard());
        RequireNotImplemented([&] {
            _ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode(settings(), &badFamily, 0);
        }, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode");
        badFamily.string.impl = nullptr;
    }

    RequireNotImplemented([&] {
        _ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE(pages.Guard(), WTF::Seconds{2.0});
    }, "_ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE");
    RequireNotImplemented([&] {
        _ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE(pages.ReadOnly(), WTF::Seconds{2.0});
    }, "_ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE");

    ResetMediaVector();
    const AtomString initial = MakeFamily("video/mp4");
    _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(settings(), &initial.string);
    MediaVector& target = MediaVectorOf();
    Require(target.size == 1 && FamilyEquals(target.buffer[0].type, "video/mp4"));

    RequireNotImplemented([&] {
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(pages.Guard(), &initial.string);
    }, "_ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE");
    RequireNotImplemented([&] {
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(pages.ReadOnly(), &initial.string);
    }, "_ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE");
    RequireNotImplemented([&] {
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(
            settings(), reinterpret_cast<const String*>(pages.Guard()));
    }, "_ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE");
    Require(target.size == 1 && FamilyEquals(target.buffer[0].type, "video/mp4"));

    {
        String badImpl;
        badImpl.impl = reinterpret_cast<StringImpl*>(pages.Guard());
        RequireNotImplemented([&] {
            _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(settings(), &badImpl);
        }, "_ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE");
        badImpl.impl = nullptr;
        Require(target.size == 1 && FamilyEquals(target.buffer[0].type, "video/mp4"));

        StringImpl badDataImpl{2, 16, pages.Guard(), 4};
        String badData;
        badData.impl = &badDataImpl;
        RequireNotImplemented([&] {
            _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(settings(), &badData);
        }, "_ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE");
        badData.impl = nullptr;
        Require(target.size == 1 && FamilyEquals(target.buffer[0].type, "video/mp4"));
    }

    RequireNotImplemented([&] {
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
            settings(), reinterpret_cast<const MediaVector*>(pages.Guard()));
    }, "_ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE");
    Require(target.size == 1 && FamilyEquals(target.buffer[0].type, "video/mp4"));

    {
        MediaVector badVec(0);
        badVec.buffer = reinterpret_cast<MediaContentType*>(pages.Guard());
        badVec.capacity = 1;
        badVec.size = 1;
        RequireNotImplemented([&] {
            _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
                settings(), &badVec);
        }, "_ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE");
        badVec.buffer = nullptr;
        badVec.capacity = 0;
        badVec.size = 0;
        Require(target.size == 1 && FamilyEquals(target.buffer[0].type, "video/mp4"));

        MediaVector readOnlyTargetSrc(1);
        new (&readOnlyTargetSrc.buffer[0]) MediaContentType{MakeLatin1("video/webm")};
        readOnlyTargetSrc.size = 1;
        RequireNotImplemented([&] {
            _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
                pages.ReadOnly(), &readOnlyTargetSrc);
        }, "_ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE");
    }

    ResetMediaVector();
}

}  // namespace

int main() {
    ResetSettings();
    ResetFamilies();

    {
        RequireThrowsInvalidArgument([] {
            AtomString family = MakeFamily("Times");
            _ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode(nullptr, &family, 0);
        });
        RequireThrowsInvalidArgument([] {
            _ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(nullptr, 0);
        });
        RequireThrowsInvalidArgument([] {
            _ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode(settings(), nullptr, 0);
        });
    }

    {
        for (FamilySetter setter : familySetters) {
            RequireThrowsInvalidArgument([setter] {
                setter(nullptr, nullptr, 0);
            });
            RequireThrowsInvalidArgument([setter] {
                setter(settings(), nullptr, 0);
            });
        }
        for (FamilyGetter getter : familyGetters) {
            RequireThrowsInvalidArgument([getter] {
                getter(nullptr, 0);
            });
        }
        RequireThrowsInvalidArgument([] {
            _ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE(nullptr, WTF::Seconds{1.0});
        });
        RequireThrowsInvalidArgument([] {
            _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(nullptr, nullptr);
        });
        RequireThrowsInvalidArgument([] {
            _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(nullptr, nullptr);
        });
    }

    {
        ResetGetterFlags();
        const String& empty = _ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode(settings(), 0);
        const String& emptyAgain = _ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode(settings(), 0);
        Require(constGetterUsed);
        Require(!nonConstGetterUsed);
        Require(&empty == &emptyAgain);
        Require(empty.impl != nullptr);
        Require(empty.impl->length == 0);
        StringImpl* sharedEmpty = _ZN3WTF10StringImpl6createEPKhj(nullptr, 0).impl;
        Require(empty.impl == sharedEmpty);
        Require(&_ZNK7WebCore12SettingsBase15serifFontFamilyE11UScriptCode(settings(), 0) == &empty);
        Require(&_ZNK7WebCore12SettingsBase17cursiveFontFamilyE11UScriptCode(settings(), 0) == &empty);
        Require(&_ZNK7WebCore12SettingsBase17fantasyFontFamilyE11UScriptCode(settings(), 0) == &empty);
        Require(&_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 0) == &empty);
        Require(&_ZNK7WebCore12SettingsBase19sansSerifFontFamilyE11UScriptCode(settings(), 0) == &empty);
        Require(&_ZNK7WebCore12SettingsBase20pictographFontFamilyE11UScriptCode(settings(), 0) == &empty);
        Require(MapTable(kStandardMapOffset) == nullptr);
    }

    {
        ResetFamilies();
        GuestTable serifTable = CreateGuestTable(8);
        String serifValue = MakeLatin1("Georgia");
        InsertGuestFamily(serifTable, 0, serifValue.impl);
        InstallMap(kSerifMapOffset, serifTable);
        ResetGetterFlags();
        AtomString serif = MakeFamily("Georgia");
        RequireSetterQuiet(_ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode, serif, 0);
        Require(nonConstGetterUsed);
        Require(!constGetterUsed);
        MapState before = SnapshotMap(kSerifMapOffset);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase15serifFontFamilyE11UScriptCode(settings(), 0), "Georgia"));
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase15serifFontFamilyE11UScriptCode(settings(), 2), "Georgia"));
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase15serifFontFamilyE11UScriptCode(settings(), 17), "Georgia"));
        AtomString times = MakeFamily("Times");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode", times, 0);
        RequireMapUnchangedAt(before, kSerifMapOffset);
        AtomString empty;
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode", empty, 0);
        RequireMapUnchangedAt(before, kSerifMapOffset);
        RequireSetterQuiet(_ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode, empty, 5);
        RequireMapUnchangedAt(before, kSerifMapOffset);
        AtomString fallback = MakeFamily("Fallback");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode", fallback, 2);
        RequireMapUnchangedAt(before, kSerifMapOffset);

        GuestTable pictographTable = CreateGuestTable(8);
        String pictographValue = MakeLatin1("NotoColorEmoji");
        InsertGuestFamily(pictographTable, 73, pictographValue.impl);
        InstallMap(kPictographMapOffset, pictographTable);
        MapState beforePictograph = SnapshotMap(kPictographMapOffset);
        Require(setenv_nid_postfix("SYSTEM_LANGUAGE", "zh-cn", 1) == 0);
        AtomString pictograph = MakeFamily("NotoColorEmoji");
        RequireSetterQuiet(_ZN7WebCore12SettingsBase23setPictographFontFamilyERKN3WTF10AtomStringE11UScriptCode, pictograph, 73);
        RequireMapUnchangedAt(beforePictograph, kPictographMapOffset);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase20pictographFontFamilyE11UScriptCode(settings(), 17), "NotoColorEmoji"));
        Require(setenv_nid_postfix("SYSTEM_LANGUAGE", "zh-TW", 1) == 0);
        Require(_ZNK7WebCore12SettingsBase20pictographFontFamilyE11UScriptCode(settings(), 17).impl->length == 0);
        Require(unsetenv_nid_postfix("SYSTEM_LANGUAGE") == 0);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase20pictographFontFamilyE11UScriptCode(settings(), 17), "NotoColorEmoji"));
        AtomString other = MakeFamily("Other");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase23setPictographFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase23setPictographFontFamilyERKN3WTF10AtomStringE11UScriptCode", other, 73);
        RequireMapUnchangedAt(beforePictograph, kPictographMapOffset);
    }

    {
        ResetFamilies();
        GuestTable standardTable = CreateGuestTable(8);
        String timesValue = MakeLatin1("Times");
        InsertGuestFamily(standardTable, 0, timesValue.impl);
        InstallMap(kStandardMapOffset, standardTable);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 0), "Times"));
        MapState before = SnapshotMap(kStandardMapOffset);
        AtomString times = MakeFamily("Times");
        RequireSetterQuiet(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, times, 0);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 1);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 0), "Times"));
        AtomString georgia = MakeFamily("Georgia");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", georgia, 0);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 0), "Times"));
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 1);
    }

    {
        ResetFamilies();
        GuestTable standardTable = CreateGuestTable(8);
        String arabicValue = MakeLatin1("Arabic");
        String commonValue = MakeLatin1("Common");
        InsertGuestFamily(standardTable, 2, arabicValue.impl);
        InsertGuestFamily(standardTable, 0, commonValue.impl);
        InstallMap(kStandardMapOffset, standardTable);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 2), "Arabic"));
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 3), "Common"));
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 17), "Common"));
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 2);
        MapState before = SnapshotMap(kStandardMapOffset);
        AtomString empty;
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", empty, 2);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 2);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 2), "Arabic"));
        StringImpl* zeroLength = _ZN3WTF10StringImpl6createEPKhj(nullptr, 0).impl;
        AtomString zeroLengthFamily{String(zeroLength)};
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", zeroLengthFamily, 0);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 0), "Common"));
        RequireSetterQuiet(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, empty, 5);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 2);
    }

    {
        ResetFamilies();
        const char* names[] = {"Monospace", "Georgia", "Snell", "Papyrus", "Helvetica", "NotoColorEmoji"};
        const char* setterNames[] = {
            "_ZN7WebCore12SettingsBase18setFixedFontFamilyERKN3WTF10AtomStringE11UScriptCode",
            "_ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode",
            "_ZN7WebCore12SettingsBase20setCursiveFontFamilyERKN3WTF10AtomStringE11UScriptCode",
            "_ZN7WebCore12SettingsBase20setFantasyFontFamilyERKN3WTF10AtomStringE11UScriptCode",
            "_ZN7WebCore12SettingsBase22setSansSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode",
            "_ZN7WebCore12SettingsBase23setPictographFontFamilyERKN3WTF10AtomStringE11UScriptCode",
        };
        const std::size_t mapOffsets[] = {kFixedMapOffset, kSerifMapOffset, kCursiveMapOffset, kFantasyMapOffset, kSansSerifMapOffset, kPictographMapOffset};
        const std::size_t setterIndices[] = {0, 1, 2, 3, 5, 6};
        String values[6];
        for (std::size_t i = 0; i < 6; ++i) {
            values[i] = MakeLatin1(names[i]);
            GuestTable table = CreateGuestTable(8);
            InsertGuestFamily(table, 0, values[i].impl);
            InstallMap(mapOffsets[i], table);
        }
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode(settings(), 0), "Monospace"));
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase15serifFontFamilyE11UScriptCode(settings(), 0), "Georgia"));
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase17cursiveFontFamilyE11UScriptCode(settings(), 0), "Snell"));
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase17fantasyFontFamilyE11UScriptCode(settings(), 0), "Papyrus"));
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase19sansSerifFontFamilyE11UScriptCode(settings(), 0), "Helvetica"));
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase20pictographFontFamilyE11UScriptCode(settings(), 0), "NotoColorEmoji"));
        for (std::size_t i = 0; i < 6; ++i) {
            MapState before = SnapshotMap(mapOffsets[i]);
            Require(TableMetadata(MapTable(mapOffsets[i]), 3) == 1);
            AtomString replacement = MakeFamily("Replacement");
            RequireSetterNotImplemented(familySetters[setterIndices[i]], setterNames[i], replacement, 0);
            RequireMapUnchangedAt(before, mapOffsets[i]);
            RequireSetterQuiet(familySetters[setterIndices[i]], AtomString{MakeLatin1(names[i])}, 0);
            RequireMapUnchangedAt(before, mapOffsets[i]);
        }
    }

    {
        ResetFamilies();
        GuestTable fixedTable = CreateGuestTable(8);
        String simplifiedValue = MakeLatin1("Simplified");
        InsertGuestFamily(fixedTable, 73, simplifiedValue.impl);
        InstallMap(kFixedMapOffset, fixedTable);
        Require(setenv_nid_postfix("SYSTEM_LANGUAGE", "zh-cn", 1) == 0);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode(settings(), 17), "Simplified"));
        MapState before = SnapshotMap(kFixedMapOffset);
        AtomString simplified = MakeFamily("Simplified");
        RequireSetterQuiet(_ZN7WebCore12SettingsBase18setFixedFontFamilyERKN3WTF10AtomStringE11UScriptCode, simplified, 73);
        RequireMapUnchangedAt(before, kFixedMapOffset);
        AtomString other = MakeFamily("Other");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase18setFixedFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase18setFixedFontFamilyERKN3WTF10AtomStringE11UScriptCode", other, 73);
        RequireMapUnchangedAt(before, kFixedMapOffset);
        Require(setenv_nid_postfix("SYSTEM_LANGUAGE", "zh-TW", 1) == 0);
        const String& noTraditional = _ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode(settings(), 17);
        Require(noTraditional.impl->length == 0);
        AtomString traditional = MakeFamily("Traditional");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase18setFixedFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase18setFixedFontFamilyERKN3WTF10AtomStringE11UScriptCode", traditional, 74);
        RequireMapUnchangedAt(before, kFixedMapOffset);
        Require(unsetenv_nid_postfix("SYSTEM_LANGUAGE") == 0);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode(settings(), 17), "Simplified"));
    }

    {
        ResetFamilies();
        GuestTable standardTable = CreateGuestTable(8);
        String firstValue = MakeLatin1("First");
        InsertGuestFamily(standardTable, 5, firstValue.impl);
        InstallMap(kStandardMapOffset, standardTable);
        MapState before = SnapshotMap(kStandardMapOffset);
        AtomString empty;
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", empty, 5);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 1);
        Require(TableMetadata(MapTable(kStandardMapOffset), 4) == 0);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 5), "First"));
        AtomString second = MakeFamily("Second");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", second, 5);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 5), "First"));
        Require(TableMetadata(MapTable(kStandardMapOffset), 1) == 8);
    }

    {
        ResetFamilies();
        GuestTable standardTable = CreateGuestTable(16);
        String familyValue = MakeLatin1("Family");
        for (int script = 0; script < 10; ++script) {
            InsertGuestFamily(standardTable, script, familyValue.impl);
        }
        InstallMap(kStandardMapOffset, standardTable);
        MapState before = SnapshotMap(kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 1) == 16);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 10);
        for (int script = 0; script < 10; ++script) {
            Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), script), "Family"));
        }
        for (int script = 0; script < 10; ++script) {
            AtomString empty;
            RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", empty, script);
        }
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 1) == 16);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 10);
        for (int script = 0; script < 10; ++script) {
            Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), script), "Family"));
        }
        AtomString fresh = MakeFamily("Fresh");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", fresh, 42);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 1) == 16);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 10);
    }

    {
        ResetFamilies();
        GuestTable standardTable = CreateGuestTable(8);
        String invalidScriptValue = MakeLatin1("InvalidScript");
        InsertGuestFamily(standardTable, -1, invalidScriptValue.impl);
        InstallMap(kStandardMapOffset, standardTable);
        MapState before = SnapshotMap(kStandardMapOffset);
        AtomString invalidScript = MakeFamily("InvalidScript");
        RequireSetterQuiet(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, invalidScript, -1);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        AtomString other = MakeFamily("Other");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", other, -1);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(FamilyEquals(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), -1), "InvalidScript"));
        Require(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 0).impl->length == 0);
    }

    {
        ResetFamilies();
        MapState before = SnapshotMap(kStandardMapOffset);
        Require(before.table == nullptr);
        AtomString emptySentinel = MakeFamily("EmptySentinel");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", emptySentinel, -2);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(MapTable(kStandardMapOffset) == nullptr);
        AtomString empty;
        RequireSetterQuiet(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, empty, -2);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), -2).impl->length == 0);
        Require(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 0).impl->length == 0);
    }

    {
        ResetFamilies();
        GuestTable standardTable = CreateGuestTable(8);
        InstallMap(kStandardMapOffset, standardTable);
        MapState before = SnapshotMap(kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 0);
        AtomString empty;
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", empty, -2);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 0);
        AtomString sentinel = MakeFamily("Sentinel");
        RequireSetterNotImplemented(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, "_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode", sentinel, -2);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 0);
        RequireSetterQuiet(_ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode, empty, -3);
        RequireMapUnchangedAt(before, kStandardMapOffset);
        Require(TableMetadata(MapTable(kStandardMapOffset), 3) == 0);
        Require(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), -2).impl == nullptr);
        Require(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), -3).impl != nullptr);
        Require(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), -3).impl->length == 0);
        Require(_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(settings(), 0).impl->length == 0);
    }

    {
        _ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE(settings(), WTF::Seconds{2.5});
        double stored;
        std::memcpy(&stored, static_cast<unsigned char*>(settings()) + kMinimumDOMTimerIntervalOffset, sizeof(stored));
        Require(stored == 2.5);
        _ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE(settings(), WTF::Seconds{-1.75});
        std::memcpy(&stored, static_cast<unsigned char*>(settings()) + kMinimumDOMTimerIntervalOffset, sizeof(stored));
        Require(stored == -1.75);
        void* fakePage = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234));
        std::memcpy(static_cast<unsigned char*>(settings()) + kPageOffset, &fakePage, sizeof(fakePage));
        RequireNotImplemented([&] {
            _ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE(settings(), WTF::Seconds{9.0});
        }, "_ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE");
        std::memcpy(&stored, static_cast<unsigned char*>(settings()) + kMinimumDOMTimerIntervalOffset, sizeof(stored));
        Require(stored == -1.75);
        void* nullPage = nullptr;
        std::memcpy(static_cast<unsigned char*>(settings()) + kPageOffset, &nullPage, sizeof(nullPage));
        _ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE(settings(), WTF::Seconds{3.0});
        std::memcpy(&stored, static_cast<unsigned char*>(settings()) + kMinimumDOMTimerIntervalOffset, sizeof(stored));
        Require(stored == 3.0);
    }

    {
        ResetMediaVector();
        MediaVector& vector = MediaVectorOf();
        AtomString contentTypes = MakeFamily("video/mp4:video/webm");
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(settings(), &contentTypes.string);
        Require(vector.size == 2);
        Require(vector.capacity == 16);
        Require(FamilyEquals(vector.buffer[0].type, "video/mp4"));
        Require(FamilyEquals(vector.buffer[1].type, "video/webm"));
        AtomString replacement = MakeFamily("video/x");
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(settings(), &replacement.string);
        Require(vector.size == 1);
        Require(vector.capacity == 16);
        Require(FamilyEquals(vector.buffer[0].type, "video/x"));
        AtomString withEmpties = MakeFamily(":a::b:");
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(settings(), &withEmpties.string);
        Require(vector.size == 2);
        Require(FamilyEquals(vector.buffer[0].type, "a"));
        Require(FamilyEquals(vector.buffer[1].type, "b"));
        AtomString none = MakeFamily("");
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(settings(), &none.string);
        Require(vector.size == 0);
        AtomString nullString;
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(settings(), &nullString.string);
        Require(vector.size == 0);
        String wide = _ZN3WTF10StringImpl6createEPKDsj(reinterpret_cast<const char16_t*>(u"ab:c"), 4);
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(settings(), &wide);
        Require(vector.size == 2);
        Require(FamilyEquals(vector.buffer[0].type, "ab"));
        Require(FamilyEquals(vector.buffer[1].type, "c"));
        const unsigned sizeBefore = vector.size;
        const unsigned capacityBefore = vector.capacity;
        RequireThrowsInvalidArgument([] {
            _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(settings(), nullptr);
        });
        Require(vector.size == sizeBefore);
        Require(vector.capacity == capacityBefore);
        Require(FamilyEquals(vector.buffer[0].type, "ab"));
        Require(FamilyEquals(vector.buffer[1].type, "c"));
    }

    {
        ResetMediaVector();
        MediaVector& target = MediaVectorOf();
        MediaVector source(3);
        new (&source.buffer[0]) MediaContentType{MakeLatin1("a")};
        new (&source.buffer[1]) MediaContentType{MakeLatin1("b")};
        new (&source.buffer[2]) MediaContentType{MakeLatin1("c")};
        source.size = 3;
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(settings(), &source);
        Require(target.size == 3);
        Require(target.capacity == 3);
        Require(FamilyEquals(target.buffer[0].type, "a"));
        Require(FamilyEquals(target.buffer[1].type, "b"));
        Require(FamilyEquals(target.buffer[2].type, "c"));
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(settings(), &target);
        Require(target.size == 3);
        Require(target.capacity == 3);
        Require(FamilyEquals(target.buffer[0].type, "a"));
        MediaVector smaller(1);
        new (&smaller.buffer[0]) MediaContentType{MakeLatin1("z")};
        smaller.size = 1;
        _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(settings(), &smaller);
        Require(target.size == 1);
        Require(target.capacity == 3);
        Require(FamilyEquals(target.buffer[0].type, "z"));
        RequireThrowsInvalidArgument([] {
            _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(settings(), nullptr);
        });
        Require(target.size == 1);
        Require(target.capacity == 3);
        Require(FamilyEquals(target.buffer[0].type, "z"));
    }

    TestGuestPointers();

    return 0;
}
