#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libSceFreeType/libSceFreeType.hpp"
#include "tests/FontFixture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H

#include <array>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include "prx/libkernel/libkernel.hpp"
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Legacy FreeType cache manager check failed at line %u\n", location.line());
        std::abort();
    }
}

static auto Load(const char* path) {
#ifdef _WIN32
    auto module = LoadLibraryW(std::filesystem::path(path).c_str());
    if (!module) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
    auto module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    return module;
}

template <typename TFunction, typename TModule>
static TFunction Import(TModule module, const char* name) {
#ifdef _WIN32
    const auto address = GetProcAddress(module, name);
#else
    const auto address = dlsym(module, name);
#endif
    if (!address) std::fprintf(stderr, "Missing symbol: %s\n", name);
    Require(address != nullptr);
    return std::bit_cast<TFunction>(address);
}

struct Api {
    decltype(&FT_Init_FreeType_nid_postfix) initialize;
    decltype(&FT_Done_FreeType_nid_postfix) doneLibrary;
    decltype(&FT_New_Memory_Face_nid_postfix) newMemoryFace;
    decltype(&freetypeLegacyFtcManagerNew_nid_no_patch) createManager;
};

static int APS5_VABI GuestDummyRequester(void*, FreeType::GuestLibrary*, void*, FreeType::GuestFaceRec**) {
    return FT_Err_Cannot_Open_Resource;
}

static FT_Error NativeDummyRequester(FTC_FaceID, FT_Library, FT_Pointer, FT_Face*) {
    return FT_Err_Cannot_Open_Resource;
}

#ifdef _WIN32
static bool RefusesAsNotImplemented(const char* function, auto call) {
    errno = 71;
    try {
        call();
    } catch (const std::runtime_error& error) {
        return errno == 71 && std::string(error.what()) == std::string(function) + " not implemented";
    }
    return false;
}
#endif

static void CheckErrors(const Api& api) {
    FreeType::GuestLibrary* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(api.initialize(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    void* const sentinel = reinterpret_cast<void*>(std::uintptr_t{0x12345678u});
    void* guestManager = sentinel;
    FTC_Manager nativeManager = reinterpret_cast<FTC_Manager>(sentinel);
    int cookie = 42;

    errno = 71;
    Require(api.createManager(nullptr, 0, 0, 0, nullptr, nullptr, nullptr) ==
            FTC_Manager_New(nullptr, 0, 0, 0, nullptr, nullptr, nullptr));
    Require(errno == 71);

    errno = 71;
    Require(api.createManager(nullptr, 2, 4, 200000, GuestDummyRequester, &cookie, &guestManager) ==
            FTC_Manager_New(nullptr, 2, 4, 200000, NativeDummyRequester, &cookie, &nativeManager));
    Require(errno == 71 && guestManager == sentinel && nativeManager == reinterpret_cast<FTC_Manager>(sentinel));

    errno = 71;
    Require(api.createManager(nullptr, 0, 0, 0, nullptr, nullptr, &guestManager) == FT_Err_Invalid_Library_Handle);
    Require(errno == 71 && guestManager == sentinel);

    errno = 71;
    Require(api.createManager(reinterpret_cast<FreeType::GuestLibrary*>(std::uintptr_t{0x10}), 2, 4, 200000, GuestDummyRequester, nullptr, &guestManager) == FT_Err_Invalid_Library_Handle);
    Require(errno == 71 && guestManager == sentinel);

    alignas(FreeType::GuestLibrary) std::array<std::byte, sizeof(FreeType::GuestLibrary) + alignof(FreeType::GuestLibrary)> storage{};
    auto* misaligned = reinterpret_cast<FreeType::GuestLibrary*>(storage.data() + 1);
    errno = 71;
    Require(api.createManager(misaligned, 2, 4, 200000, GuestDummyRequester, nullptr, &guestManager) == FT_Err_Invalid_Library_Handle);
    Require(errno == 71 && guestManager == sentinel);

    alignas(FreeType::GuestLibrary) std::array<std::byte, sizeof(FreeType::GuestLibrary)> unknownLibrary{};
    errno = 71;
    Require(api.createManager(reinterpret_cast<FreeType::GuestLibrary*>(unknownLibrary.data()), 2, 4, 200000,
                              GuestDummyRequester, nullptr, &guestManager) == FT_Err_Invalid_Library_Handle);
    Require(errno == 71 && guestManager == sentinel);

    errno = 71;
    Require(api.createManager(library, 0, 0, 0, GuestDummyRequester, nullptr, nullptr) ==
            FTC_Manager_New(nativeLibrary, 0, 0, 0, NativeDummyRequester, nullptr, nullptr));
    Require(errno == 71);

    errno = 71;
    Require(api.createManager(library, 0, 0, 0, nullptr, nullptr, &guestManager) ==
            FTC_Manager_New(nativeLibrary, 0, 0, 0, nullptr, nullptr, &nativeManager));
    Require(errno == 71 && guestManager == sentinel && nativeManager == reinterpret_cast<FTC_Manager>(sentinel));

    errno = 71;
    Require(api.createManager(library, 0, 0, 0, nullptr, nullptr, nullptr) ==
            FTC_Manager_New(nativeLibrary, 0, 0, 0, nullptr, nullptr, nullptr));
    Require(errno == 71);

    errno = 71;
    Require(api.createManager(library, 2, 4, 200000, GuestDummyRequester, nullptr, reinterpret_cast<void**>(std::uintptr_t{0x10})) == FT_Err_Invalid_Argument);
    Require(errno == 71);

    static void* const readOnlyTarget = sentinel;
    errno = 71;
    Require(api.createManager(library, 2, 4, 200000, GuestDummyRequester, nullptr, const_cast<void**>(&readOnlyTarget)) == FT_Err_Invalid_Argument);
    Require(errno == 71 && readOnlyTarget == sentinel);

    Require(api.doneLibrary(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}

struct FaceRequest {
    const Api& api;
    FreeType::GuestLibrary* library;
    std::vector<unsigned char> font = FontFixture::BuildFont(2, {});
    void* lastId = nullptr;
    unsigned calls = 0;
    unsigned finalizations = 0;
    FT_Error result = FT_Err_Ok;
    FreeType::GuestFaceRec* substitute = nullptr;
    bool useSubstitute = false;
};

static void APS5_VABI FinalizeRequestedFace(void* record) {
    auto* face = static_cast<FreeType::GuestFaceRec*>(record);
    auto* request = static_cast<FaceRequest*>(face->generic.data);
    ++request->finalizations;
}

static FT_Error APS5_VABI GuestRequester(void* faceId, FreeType::GuestLibrary* library, void* reqData, FreeType::GuestFaceRec** target) {
    auto& request = *static_cast<FaceRequest*>(reqData);
    Require(library == request.library);
    request.lastId = faceId;
    ++request.calls;
    if (request.result) return request.result;
    if (request.useSubstitute) {
        *target = request.substitute;
        return FT_Err_Ok;
    }
    const FT_Error error = request.api.newMemoryFace(library, request.font.data(), static_cast<std::int64_t>(request.font.size()), 0, target);
    if (!error) (*target)->generic = {&request, FinalizeRequestedFace};
    return error;
}

static void CheckRequester(const Api& api) {
    FreeType::GuestLibrary* library = nullptr;
    Require(api.initialize(&library) == 0);
    FaceRequest request{api, library};
    void* handle = nullptr;
    Require(api.createManager(library, 1, 1, 4096, GuestRequester, &request, &handle) == 0);
    auto* manager = static_cast<FTC_Manager>(handle);
    Require(request.calls == 0);

    int firstId = 1;
    int secondId = 2;
    FT_Face face = nullptr;
    Require(FTC_Manager_LookupFace(manager, &firstId, &face) == 0);
    Require(request.calls == 1 && request.lastId == &firstId && face && face->num_glyphs == 2);
    FT_Face cached = nullptr;
    Require(FTC_Manager_LookupFace(manager, &firstId, &cached) == 0);
    Require(cached == face && request.calls == 1);

    FTC_ScalerRec scaler{&firstId, 12, 12, 1, 0, 0};
    FT_Size size = nullptr;
    Require(FTC_Manager_LookupSize(manager, &scaler, &size) == 0);
    Require(size && size->face == face && size->metrics.x_ppem == 12 && request.calls == 1);
    Require(FTC_Manager_LookupFace(manager, &secondId, &face) == 0);
    Require(request.calls == 2 && request.lastId == &secondId && request.finalizations == 1 && library->faces.size() == 1);

    scaler.face_id = &secondId;
    Require(FTC_Manager_LookupSize(manager, &scaler, &size) == 0 && size && size->face == face);

    void* errorHandle = nullptr;
    Require(api.createManager(library, 0, 0, 0, GuestRequester, &request, &errorHandle) == 0);
    auto* errorManager = static_cast<FTC_Manager>(errorHandle);
    request.result = FT_Err_Cannot_Open_Resource;
    Require(FTC_Manager_LookupFace(errorManager, &firstId, &face) == FT_Err_Cannot_Open_Resource && !face);
    request.result = FT_Err_Ok;
    request.useSubstitute = true;
    Require(FTC_Manager_LookupFace(errorManager, &firstId, &face) == FT_Err_Invalid_Face_Handle && !face);

    FreeType::GuestLibrary* otherLibrary = nullptr;
    Require(api.initialize(&otherLibrary) == 0);
    Require(api.newMemoryFace(otherLibrary, request.font.data(), static_cast<std::int64_t>(request.font.size()), 0, &request.substitute) == 0);
    Require(FTC_Manager_LookupFace(errorManager, &firstId, &face) == FT_Err_Invalid_Face_Handle && !face);
    Require(api.doneLibrary(otherLibrary) == 0);
    Require(FTC_Manager_LookupFace(errorManager, &firstId, &face) == FT_Err_Invalid_Face_Handle && !face);
    Require(api.doneLibrary(library) == 0 && request.finalizations == 2);
    Require(api.createManager(library, 0, 0, 0, GuestRequester, &request, &handle) == FT_Err_Invalid_Library_Handle);
}

static void CheckValidCreationAndCleanup(const Api& api) {
    FreeType::GuestLibrary* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(api.initialize(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    void* const sentinel = reinterpret_cast<void*>(std::uintptr_t{0xCAFEBABEu});
    void* manager1 = sentinel;
    void* manager2 = sentinel;
    void* manager3 = sentinel;
    FTC_Manager nativeManager1 = nullptr;
    FTC_Manager nativeManager2 = nullptr;
    int cookie = 77;

    errno = 71;
    Require(api.createManager(library, 0, 0, 0, GuestDummyRequester, nullptr, &manager1) == 0);
    Require(FTC_Manager_New(nativeLibrary, 0, 0, 0, NativeDummyRequester, nullptr, &nativeManager1) == 0);
    Require(errno == 71 && manager1 != nullptr && manager1 != sentinel && nativeManager1 != nullptr);

    errno = 71;
    Require(api.createManager(library, 1, 1, 1, GuestDummyRequester, &cookie, &manager2) == 0);
    Require(FTC_Manager_New(nativeLibrary, 1, 1, 1, NativeDummyRequester, &cookie, &nativeManager2) == 0);
    Require(errno == 71 && manager2 != nullptr && manager2 != sentinel && manager2 != manager1);

    errno = 71;
    Require(api.createManager(library, 16, 32, 0xFFFFFFFFull, GuestDummyRequester, &cookie, &manager3) == 0);
    Require(errno == 71 && manager3 != nullptr && manager3 != sentinel && manager3 != manager1 && manager3 != manager2);

    void* largeManager = sentinel;
#ifdef _WIN32
    Require(RefusesAsNotImplemented("freetypeLegacyFtcManagerNew_nid_no_patch", [&] {
        api.createManager(library, 2, 4, 0x100000000ull, GuestDummyRequester, nullptr, &largeManager);
    }));
    Require(largeManager == sentinel);
#else
    errno = 71;
    Require(api.createManager(library, 2, 4, 0x100000000ull, GuestDummyRequester, nullptr, &largeManager) == 0);
    Require(errno == 71 && largeManager != nullptr && largeManager != sentinel);
#endif

    FTC_Manager_Done(nativeManager2);
    FTC_Manager_Done(nativeManager1);
    Require(api.doneLibrary(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);

    for (int round = 0; round < 10; ++round) {
        FreeType::GuestLibrary* roundLibrary = nullptr;
        void* roundA = nullptr;
        void* roundB = nullptr;
        Require(api.initialize(&roundLibrary) == 0);
        Require(api.createManager(roundLibrary, 2, 4, 200000, GuestDummyRequester, nullptr, &roundA) == 0);
        Require(api.createManager(roundLibrary, 4, 8, 400000, GuestDummyRequester, &round, &roundB) == 0);
        Require(roundA != nullptr && roundB != nullptr && roundA != roundB);
        Require(api.doneLibrary(roundLibrary) == 0);
    }
}

#ifdef _WIN32
struct KernelApi {
    decltype(&sceKernelAllocateDirectMemory) allocate;
    decltype(&sceKernelMapDirectMemory) map;
    decltype(&sceKernelMprotect) protect;
    decltype(&sceKernelMunmap) unmap;
    decltype(&sceKernelReleaseDirectMemory) release;
};

class GuestPages {
public:
    static constexpr std::size_t Page = 0x4000;

    explicit GuestPages(const KernelApi& api) : api(api) {
        Require(api.allocate(0, 0x7fffffffffll, Page * 2, 0, 0, &physical) == 0);
        Require(api.map(&mapping, Page * 2, 3, 0, physical, 0) == 0);
        std::memset(mapping, 0xA5, Page * 2);
    }

    ~GuestPages() {
        Require(api.unmap(reinterpret_cast<std::uint64_t>(mapping), Page * 2) == 0);
        Require(api.release(physical, Page * 2) == 0);
    }

    template <typename TValue>
    TValue* At(unsigned page, std::size_t offset = 64) const {
        return reinterpret_cast<TValue*>(static_cast<unsigned char*>(mapping) + Page * page + offset);
    }

    void Protect(unsigned page, int protection) const {
        Require(api.protect(static_cast<unsigned char*>(mapping) + Page * page, Page, protection) == 0);
        if (protection == 1) {
            MEMORY_BASIC_INFORMATION information{};
            Require(VirtualQuery(At<unsigned char>(page), &information, sizeof(information)) == sizeof(information));
            Require(information.Protect == PAGE_READONLY);
        }
    }

    std::vector<unsigned char> Snapshot() const {
        const auto* bytes = static_cast<const unsigned char*>(mapping);
        return {bytes, bytes + Page * 2};
    }

private:
    const KernelApi& api;
    std::int64_t physical = 0;
    void* mapping = nullptr;
};

static void CheckGuestMemory(const Api& api, const KernelApi& kernel) {
    GuestPages pages(kernel);
    FreeType::GuestLibrary* library = nullptr;
    Require(api.initialize(&library) == 0);

    auto* managerOut = pages.At<void*>(1);
    void* const sentinel = reinterpret_cast<void*>(std::uintptr_t{0xDEAD0001u});
    *managerOut = sentinel;

    pages.Protect(0, 1);
    pages.Protect(1, 1);
    const auto protectedBefore = pages.Snapshot();
    errno = 73;
    Require(api.createManager(library, 2, 4, 200000, GuestDummyRequester, nullptr, managerOut) == FT_Err_Invalid_Argument);
    Require(errno == 73 && *managerOut == sentinel && pages.Snapshot() == protectedBefore);

    pages.Protect(1, 3);
    void* writableLocal = sentinel;
    const auto unmodified = pages.Snapshot();
    errno = 73;
    Require(api.createManager(library, 2, 4, 200000, GuestDummyRequester, nullptr, &writableLocal) == 0);
    Require(errno == 73 && writableLocal != nullptr && writableLocal != sentinel && pages.Snapshot() == unmodified);

    errno = 73;
    Require(api.createManager(library, 3, 6, 300000, GuestDummyRequester, nullptr, managerOut) == 0);
    Require(errno == 73 && *managerOut != nullptr && *managerOut != sentinel && *managerOut != writableLocal);

    Require(api.doneLibrary(library) == 0);
}
#endif

int main(int argc, char** argv) {
    Require(argc == 4);
    const bool patched = std::strcmp(argv[3], "patched") == 0;
    Require(patched || std::strcmp(argv[3], "unpatched") == 0);
    auto base = Load(argv[2]);
    auto module = Load(argv[1]);
#ifdef _WIN32
    auto kernel = Load((std::filesystem::path(argv[2]).parent_path() / "libkernel.prx").string().c_str());
    const KernelApi kernelApi{
        Import<decltype(KernelApi::allocate)>(kernel, patched ? "rTXw65xmLIA" : "sceKernelAllocateDirectMemory"),
        Import<decltype(KernelApi::map)>(kernel, patched ? "L-Q3LEjIbgA" : "sceKernelMapDirectMemory"),
        Import<decltype(KernelApi::protect)>(kernel, patched ? "vSMAm3cxYTY" : "sceKernelMprotect"),
        Import<decltype(KernelApi::unmap)>(kernel, patched ? "cQke9UuBQOk" : "sceKernelMunmap"),
        Import<decltype(KernelApi::release)>(kernel, patched ? "MBuItvba6z8" : "sceKernelReleaseDirectMemory")
    };
#endif
    Api api{};
    api.initialize = Import<decltype(api.initialize)>(base, patched ? "GNsiLzm4SH8" : "FT_Init_FreeType_nid_postfix");
    api.doneLibrary = Import<decltype(api.doneLibrary)>(base, patched ? "dH02CpEEcYk" : "FT_Done_FreeType_nid_postfix");
    api.newMemoryFace = Import<decltype(api.newMemoryFace)>(base, patched ? "40Wa2neSLM0" : "FT_New_Memory_Face_nid_postfix");
    api.createManager = Import<decltype(api.createManager)>(module, patched ? "DYbLD-s1oiQ" : "DYbLD-s1oiQ_nid_no_patch_cut");
    CheckErrors(api);
    CheckValidCreationAndCleanup(api);
    CheckRequester(api);
#ifdef _WIN32
    CheckGuestMemory(api, kernelApi);
    Require(FreeLibrary(module) != 0 && FreeLibrary(base) != 0 && FreeLibrary(kernel) != 0);
#else
    Require(dlclose(module) == 0 && dlclose(base) == 0);
#endif
}
