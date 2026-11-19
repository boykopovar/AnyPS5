#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include FT_CACHE_H

#include <algorithm>
#include <cerrno>
#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>

static_assert(sizeof(FT_UInt) == sizeof(std::uint32_t));

namespace FreeType {

namespace {

struct CacheManager {
    GuestLibrary* library;
    GuestFaceRequester requester;
    void* requestData;
    FTC_Manager native = nullptr;
};

std::mutex cacheMutex;
std::unordered_map<GuestLibrary*, std::list<CacheManager>> cacheManagers;

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

FT_Error RequestFace(FTC_FaceID faceId, FT_Library, FT_Pointer requestData, FT_Face* target) {
    const auto& manager = *static_cast<const CacheManager*>(requestData);
    GuestFaceRec* face = nullptr;
    const FT_Error error = manager.requester(faceId, manager.library, manager.requestData, &face);
    if (error) return error;

    const auto& faces = manager.library->faces;
    const auto owner = std::find_if(faces.begin(), faces.end(), [face](const auto& candidate) { return &candidate->rec == face; });
    if (owner == faces.end() || (*owner)->finalized || !(*owner)->host) return FT_Err_Invalid_Face_Handle;
    *target = (*owner)->host;
    return FT_Err_Ok;
}

}

bool RegisterCacheLibrary_nid_no_patch(GuestLibrary* library) {
    try {
        std::lock_guard lock(cacheMutex);
        return cacheManagers.try_emplace(library).second;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

bool DestroyCacheManagers_nid_no_patch(GuestLibrary* library) {
    std::list<CacheManager> managers;
    {
        std::lock_guard lock(cacheMutex);
        const auto entry = cacheManagers.find(library);
        if (entry == cacheManagers.end()) return false;
        managers.splice(managers.end(), entry->second);
        cacheManagers.erase(entry);
    }
    for (auto& manager : managers) FTC_Manager_Done(manager.native);
    return true;
}

extern "C" int FreeTypeNewCacheManager_nid_no_patch(GuestLibrary* library, std::uint32_t maxFaces, std::uint32_t maxSizes,
                                                  std::uint64_t maxBytes, GuestFaceRequester requester, void* reqData, void** target, const char* function) {
    const ErrnoGuard errnoGuard;
    std::lock_guard lock(cacheMutex);
    const auto entry = cacheManagers.find(library);
    if (entry == cacheManagers.end()) return FT_Err_Invalid_Library_Handle;
    if (!target || !requester) return FT_Err_Invalid_Argument;
    GuestArena::HostWrite write(target, sizeof(*target));
    if (!write.Open() || !GuestWritable_nid_no_patch(target, sizeof(*target))) return FT_Err_Invalid_Argument;
    if (!std::in_range<FT_ULong>(maxBytes)) NotImplemented_nid_no_patch(function);

    try {
        std::list<CacheManager> pending;
        auto& manager = pending.emplace_back(library, requester, reqData);
        const FT_Error error = FTC_Manager_New(library->host, maxFaces, maxSizes, static_cast<FT_ULong>(maxBytes), RequestFace, &manager, &manager.native);
        if (error) return error;
        std::unique_ptr<FTC_ManagerRec_, decltype(&FTC_Manager_Done)> native(manager.native, &FTC_Manager_Done);
        void* handle = native.get();
        if (!GuestCopyOut_nid_no_patch(target, &handle, sizeof(handle))) return FT_Err_Invalid_Argument;
        entry->second.splice(entry->second.end(), pending);
        native.release();
        return FT_Err_Ok;
    } catch (const std::bad_alloc&) {
        return FT_Err_Out_Of_Memory;
    }
}

}
