#include "prx/libSceFreeTypeOl/libSceFreeTypeOl.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <utility>

#ifndef ANYPS5_FREETYPEOL_SYSTEM_DEPS
#include <freetype/internal/ftobjs.h>
#include "src/sfnt/ttcmap.h"
#endif

using namespace FreeType;

#ifndef ANYPS5_FREETYPEOL_SYSTEM_DEPS
namespace FreeType {

struct CharmapRemoval {
    GuestFace& owner;
    FT_CMap native;
    std::exception_ptr exception;
    CharmapRemoval* previous;

    CharmapRemoval(GuestFace& face, FT_CMap map)
        : owner(face), native(map), previous(std::exchange(face.charmapRemoval, this)) {}
    ~CharmapRemoval() { owner.charmapRemoval = previous; }
    CharmapRemoval(const CharmapRemoval&) = delete;
    CharmapRemoval& operator=(const CharmapRemoval&) = delete;
};

}

namespace {

struct Bridge {
    FT_CMapRec cmap;
    GuestFace* owner;
    GuestCMap* guest;
    std::exception_ptr* initializationException;
};

struct Initialization {
    GuestFace& owner;
    const GuestCharMapRec& info;
    const GuestCMapClass* clazz;
    FT_Long size;
    void* data;
    GuestCMap* guest;
    std::exception_ptr exception;
};

bool CopyIn(void* destination, const void* source, std::size_t size) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = saved;
    return copied;
}

GuestFace* Owner(const GuestCharMapRec& info) {
    auto* face = static_cast<const GuestFaceRec*>(info.face);
    GuestFace* owner = nullptr;
    if (!face || !CopyIn(&owner, &face->internal, sizeof(owner)) || !owner || &owner->rec != face)
        NotImplemented_nid_no_patch(__func__);
    return owner;
}

FT_Error Initialize(FT_CMap native, FT_Pointer data) {
    auto& source = *static_cast<Initialization*>(data);
    auto& bridge = *reinterpret_cast<Bridge*>(native);
    bridge.owner = &source.owner;
    const auto memory = source.owner.host->memory;
    void* storage = memory->alloc(memory, source.size);
    if (!storage) return FT_Err_Out_Of_Memory;
    std::memset(storage, 0, static_cast<std::size_t>(source.size));
    auto* guest = new (storage) GuestCMap{source.info, source.clazz};
    try {
        bridge.owner->customCharmaps.emplace(&native->charmap, &guest->charmap);
    } catch (const std::bad_alloc&) {
        memory->free(memory, storage);
        return FT_Err_Out_Of_Memory;
    }
    bridge.guest = guest;
    source.guest = bridge.guest;
    bridge.initializationException = &source.exception;
    try {
        const auto init = bridge.guest->clazz->init;
        const auto error = init ? init(bridge.guest, source.data) : FT_Err_Ok;
        native->charmap.encoding = static_cast<FT_Encoding>(bridge.guest->charmap.encoding);
        native->charmap.platform_id = bridge.guest->charmap.platform_id;
        native->charmap.encoding_id = bridge.guest->charmap.encoding_id;
        if (!error) {
            auto& pointers = source.owner.charmapPointers;
            pointers.reserve(pointers.size() + 1);
            source.owner.rec.charmaps = pointers.empty() ? nullptr : pointers.data();
        }
        return error;
    } catch (const std::bad_alloc&) {
        return FT_Err_Out_Of_Memory;
    } catch (...) {
        source.exception = std::current_exception();
        return FT_Err_Invalid_Argument;
    }
}

void Destroy(FT_CMap native) {
    auto& bridge = *reinterpret_cast<Bridge*>(native);
    if (!bridge.guest) return;
    const auto release = [memory = bridge.owner->host->memory](GuestCMap* guest) { memory->free(memory, guest); };
    const std::unique_ptr<GuestCMap, decltype(release)> storage(bridge.guest, release);
    bridge.owner->customCharmaps.erase(&native->charmap);
    auto* pending = bridge.initializationException;
    auto* removal = bridge.owner->charmapRemoval;
    if (removal && removal->native == native) {
        pending = &removal->exception;
        FreeTypeSyncCharmaps_nid_no_patch(*bridge.owner);
    }
    if (const auto done = bridge.guest->clazz->done) {
        if (!pending) {
            done(bridge.guest);
        } else {
            try {
                done(bridge.guest);
            } catch (...) {
                if (!*pending) *pending = std::current_exception();
            }
        }
    }
}

FT_UInt CharIndex(FT_CMap native, FT_UInt32 code) {
    const auto* bridge = reinterpret_cast<const Bridge*>(native);
    if (!bridge->guest->clazz->index) NotImplemented_nid_no_patch(__func__);
    return bridge->guest->clazz->index(bridge->guest, code);
}

FT_UInt CharNext(FT_CMap native, FT_UInt32* code) {
    const auto* bridge = reinterpret_cast<const Bridge*>(native);
    if (!bridge->guest->clazz->next) NotImplemented_nid_no_patch(__func__);
    return bridge->guest->clazz->next(bridge->guest, code);
}

FT_CMap NativeMap(const GuestCMap* guest) {
    auto* owner = Owner(guest->charmap);
    const auto found = std::find(owner->charmapPointers.begin(), owner->charmapPointers.end(), &guest->charmap);
    if (found == owner->charmapPointers.end()) NotImplemented_nid_no_patch(__func__);
    return reinterpret_cast<FT_CMap>(owner->host->charmaps[found - owner->charmapPointers.begin()]);
}

std::uint32_t APS5_VABI NativeIndex(GuestCMap* guest, std::uint32_t code) {
    auto* native = NativeMap(guest);
    return native->clazz->char_index(native, code);
}

std::uint32_t APS5_VABI NativeNext(GuestCMap* guest, std::uint32_t* code) {
    auto* native = NativeMap(guest);
    return native->clazz->char_next(native, code);
}

const GuestCMapClass NativeClass{sizeof(GuestCMap), nullptr, nullptr, NativeIndex, NativeNext};

FT_UInt VariantIndex(FT_CMap native, FT_CMap unicode, FT_UInt32 code, FT_UInt32 selector) {
    const auto& bridge = *reinterpret_cast<Bridge*>(native);
    const auto method = bridge.guest->clazz->variantIndex;
    if (!method) return 0;
    const auto& owner = *bridge.owner;
    const auto maps = owner.host->charmaps;
    const auto found = std::find(maps, maps + owner.host->num_charmaps, &unicode->charmap);
    if (found == maps + owner.host->num_charmaps) NotImplemented_nid_no_patch(__func__);
    return method(bridge.guest, reinterpret_cast<GuestCMap*>(owner.charmapPointers[found - maps]), code, selector);
}

FT_Int VariantDefault(FT_CMap native, FT_UInt32 code, FT_UInt32 selector) {
    const auto& bridge = *reinterpret_cast<Bridge*>(native);
    return bridge.guest->clazz->variantDefault ? bridge.guest->clazz->variantDefault(bridge.guest, code, selector) : -1;
}

void* APS5_VABI Allocate(GuestMemoryRec* guest, std::int64_t size) {
    if (size < 0 || size > std::numeric_limits<FT_Long>::max()) return nullptr;
    auto* host = reinterpret_cast<CharMapMemory*>(guest)->host;
    return host->alloc(host, static_cast<FT_Long>(size));
}

void APS5_VABI Release(GuestMemoryRec* guest, void* block) {
    auto* host = reinterpret_cast<CharMapMemory*>(guest)->host;
    host->free(host, block);
}

void* APS5_VABI Reallocate(GuestMemoryRec* guest, std::int64_t oldSize, std::int64_t size, void* block) {
    if (oldSize < 0 || size < 0 || oldSize > std::numeric_limits<FT_Long>::max() || size > std::numeric_limits<FT_Long>::max())
        return nullptr;
    auto* host = reinterpret_cast<CharMapMemory*>(guest)->host;
    return host->realloc(host, static_cast<FT_Long>(oldSize), static_cast<FT_Long>(size), block);
}

GuestMemoryRec* GuestMemory(GuestFace& owner, FT_Memory memory) {
    if (owner.rec.memory && owner.host->memory == memory) return static_cast<GuestMemoryRec*>(owner.rec.memory);
    auto& view = owner.charmapMemory;
    if (view.host != memory) {
        view.host = memory;
        view.guest = {memory->user, Allocate, Release, Reallocate};
    }
    return &view.guest;
}

FT_UInt32* Variants(FT_CMap native, FT_Memory memory) {
    auto& bridge = *reinterpret_cast<Bridge*>(native);
    const auto method = bridge.guest->clazz->variants;
    return method ? method(bridge.guest, GuestMemory(*bridge.owner, memory)) : nullptr;
}

FT_UInt32* CharVariants(FT_CMap native, FT_Memory memory, FT_UInt32 code) {
    auto& bridge = *reinterpret_cast<Bridge*>(native);
    const auto method = bridge.guest->clazz->charVariants;
    return method ? method(bridge.guest, GuestMemory(*bridge.owner, memory), code) : nullptr;
}

FT_UInt32* VariantChars(FT_CMap native, FT_Memory memory, FT_UInt32 selector) {
    auto& bridge = *reinterpret_cast<Bridge*>(native);
    const auto method = bridge.guest->clazz->variantChars;
    return method ? method(bridge.guest, GuestMemory(*bridge.owner, memory), selector) : nullptr;
}

struct GuestCMapInfo {
    std::uint64_t language;
    std::int64_t format;
};

struct GuestSfntCMapClass {
    GuestCMapClass base;
    std::uint32_t format;
    void* validate;
    int (APS5_VABI *info)(GuestCharMapRec*, GuestCMapInfo*);
};

static_assert(offsetof(GuestSfntCMapClass, info) == 96 && sizeof(GuestCMapInfo) == 16);

FT_Error GetInfo(FT_CharMap native, TT_CMapInfo* info) {
    const auto& bridge = *reinterpret_cast<Bridge*>(native);
    const auto* guestClass = reinterpret_cast<const GuestSfntCMapClass*>(bridge.guest->clazz);
    decltype(guestClass->info) method = nullptr;
    if (!CopyIn(&method, &guestClass->info, sizeof(method))) NotImplemented_nid_no_patch(__func__);
    if (!method) return FT_Err_Invalid_CharMap_Format;
    GuestCMapInfo guestInfo{};
    const auto error = method(&bridge.guest->charmap, &guestInfo);
    if (!error) {
        if (!std::in_range<FT_ULong>(guestInfo.language) || !std::in_range<FT_Long>(guestInfo.format))
            NotImplemented_nid_no_patch(__func__);
        *info = {static_cast<FT_ULong>(guestInfo.language), static_cast<FT_Long>(guestInfo.format)};
    }
    return error;
}

constexpr FT_CMap_ClassRec Methods{sizeof(Bridge), Initialize, Destroy, CharIndex, CharNext, VariantIndex,
                                  VariantDefault, Variants, CharVariants, VariantChars};
const TT_CMap_ClassRec CMapClass{Methods, 0, nullptr, GetInfo};

}
#endif

extern "C" {

int APS5_VABI sceFreeTypeOlCMapNew_nid_no_patch(const GuestCMapClass* clazz, void* initData, const GuestCharMapRec* charmap, GuestCMap** output) {
#ifdef ANYPS5_FREETYPEOL_SYSTEM_DEPS
    NotImplemented_nid_no_patch(__func__);
#else
    if (!clazz || !charmap) return FT_Err_Invalid_Argument;
    GuestCharMapRec info{};
    GuestCMapClass methods{};
    if (!CopyIn(&info, charmap, sizeof(info))) NotImplemented_nid_no_patch(__func__);
    if (!info.face) return FT_Err_Invalid_Argument;
    auto* owner = Owner(info);
    if (!CopyIn(&methods, clazz, sizeof(methods)) || methods.size < sizeof(GuestCMap)) NotImplemented_nid_no_patch(__func__);
    if (!owner->nativeCharmapClass) {
        owner->nativeCharmapClass = &NativeClass;
        FreeTypeSyncCharmaps_nid_no_patch(*owner);
    }
    if (output && !GuestWritable_nid_no_patch(output, sizeof(*output))) NotImplemented_nid_no_patch(__func__);
    if (methods.size > std::numeric_limits<FT_Long>::max()) NotImplemented_nid_no_patch(__func__);
    owner->rec.memory = GuestMemory(*owner, owner->host->memory);
    FT_CharMapRec nativeInfo{owner->host, static_cast<FT_Encoding>(info.encoding), info.platform_id, info.encoding_id};
    Initialization init{*owner, info, clazz, static_cast<FT_Long>(methods.size), initData, nullptr, {}};
    FT_CMap created = nullptr;
    const auto error = FT_CMap_New(&CMapClass.clazz, &init, &nativeInfo, &created);
    if (error) {
        if (output) *output = nullptr;
        if (init.exception) std::rethrow_exception(init.exception);
        return error;
    }
    auto& bridge = *reinterpret_cast<Bridge*>(created);
    bridge.initializationException = nullptr;
    FreeTypeSyncCharmaps_nid_no_patch(*owner);
    if (output) *output = init.guest;
    return FT_Err_Ok;
#endif
}
APS5_EXPORT("wLqQQWyOdpc", sceFreeTypeOlCMapNew_nid_no_patch);

void APS5_VABI sceFreeTypeOlCMapDone_nid_no_patch(GuestCMap* cmap) {
#ifdef ANYPS5_FREETYPEOL_SYSTEM_DEPS
    NotImplemented_nid_no_patch(__func__);
#else
    if (!cmap) return;
    GuestCharMapRec info{};
    if (!CopyIn(&info, cmap, sizeof(info))) NotImplemented_nid_no_patch(__func__);
    auto* owner = Owner(info);
    const auto found = std::find(owner->charmapPointers.begin(), owner->charmapPointers.end(), &cmap->charmap);
    if (found == owner->charmapPointers.end()) return;
    const auto index = static_cast<std::size_t>(found - owner->charmapPointers.begin());
    auto* nativeMap = owner->host->charmaps[index];
    auto* native = reinterpret_cast<FT_CMap>(nativeMap);
    const auto builtin = owner->charmaps.find(nativeMap);
    const auto count = owner->host->num_charmaps;
    CharmapRemoval removal(*owner, native);
    FT_CMap_Done(native);
    if (builtin != owner->charmaps.end() && owner->host->num_charmaps < count)
        owner->charmaps.erase(builtin);
    FreeTypeSyncCharmaps_nid_no_patch(*owner);
    if (removal.exception) std::rethrow_exception(removal.exception);
#endif
}
APS5_EXPORT("h0n4Fb9j2R8", sceFreeTypeOlCMapDone_nid_no_patch);

}
