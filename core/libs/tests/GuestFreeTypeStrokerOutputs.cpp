#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "FreeTypeGuestOutputFixture.hpp"

#include FT_STROKER_H

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace FreeType;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "FreeType stroker output check failed at line %u\n", location.line());
    std::abort();
}

template <typename TModule, typename TFunction>
static TFunction Import(TModule module, const char* name, const char* nid, bool patched, bool kernel = false) {
    const std::string symbol = patched ? nid : std::string(name) + (kernel ? "" : "_nid_postfix");
#ifdef _WIN32
    auto address = GetProcAddress(module, symbol.c_str());
#else
    auto address = dlsym(module, symbol.c_str());
#endif
    Require(address != nullptr);
    return std::bit_cast<TFunction>(address);
}

struct Api {
    decltype(&FT_Init_FreeType_nid_postfix) initialize;
    decltype(&FT_Done_FreeType_nid_postfix) doneLibrary;
    decltype(&FT_Stroker_New_nid_postfix) create;
    decltype(&FT_Stroker_Done_nid_postfix) destroy;
    decltype(&FT_Stroker_Set_nid_postfix) set;
    decltype(&FT_Stroker_BeginSubPath_nid_postfix) begin;
    decltype(&FT_Stroker_LineTo_nid_postfix) line;
    decltype(&FT_Stroker_EndSubPath_nid_postfix) end;
    decltype(&FT_Stroker_GetCounts_nid_postfix) counts;
    decltype(&FT_Stroker_GetBorderCounts_nid_postfix) borderCounts;
};

struct Pair {
    const Api& api;
    void* guest = nullptr;
    FT_Stroker native = nullptr;

    Pair(const Api& api, GuestLibrary* library, FT_Library nativeLibrary, unsigned shape) : api(api) {
        Require(api.create(library, &guest) == 0 && FT_Stroker_New(nativeLibrary, &native) == 0);
        api.set(guest, 64, FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 0);
        FT_Stroker_Set(native, 64, FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 0);
        if (shape == 0) return;
        const GuestVector points[] = {{0, 0}, {640, 0}, {640, 640}, {0, 640}};
        FT_Vector point{0, 0};
        Require(api.begin(guest, &points[0], 0) == FT_Stroker_BeginSubPath(native, &point, 0));
        for (unsigned index = 1; index < 4; ++index) {
            point = {static_cast<FT_Pos>(points[index].x), static_cast<FT_Pos>(points[index].y)};
            Require(api.line(guest, &points[index]) == FT_Stroker_LineTo(native, &point));
        }
        if (shape == 1) Require(api.end(guest) == FT_Stroker_EndSubPath(native));
    }

    ~Pair() {
        api.destroy(guest);
        FT_Stroker_Done(native);
    }

    int Query(int border, bool nullStroker, std::uint32_t* points, std::uint32_t* contours) const {
        void* stroker = nullStroker ? nullptr : guest;
        return border == -1 ? api.counts(stroker, points, contours) : api.borderCounts(stroker, border, points, contours);
    }

    int Reference(int border, bool nullStroker, FT_UInt* points, FT_UInt* contours) const {
        FT_Stroker stroker = nullStroker ? nullptr : native;
        return border == -1 ? FT_Stroker_GetCounts(stroker, points, contours) :
            FT_Stroker_GetBorderCounts(stroker, static_cast<FT_StrokerBorder>(border), points, contours);
    }
};

static void CheckWritable(const Pair& pair, int border, bool nullStroker) {
    for (unsigned mask = 0; mask < 4; ++mask) {
        std::uint32_t points = 71, contours = 93;
        FT_UInt nativePoints = points, nativeContours = contours;
        const int expected = pair.Reference(border, nullStroker, mask & 1 ? &nativePoints : nullptr, mask & 2 ? &nativeContours : nullptr);
        Require(pair.Query(border, nullStroker, mask & 1 ? &points : nullptr, mask & 2 ? &contours : nullptr) == expected);
        Require(points == nativePoints && contours == nativeContours);
    }
    std::uint32_t alias = 71;
    FT_UInt nativeAlias = alias;
    const int expected = pair.Reference(border, nullStroker, &nativeAlias, &nativeAlias);
    Require(pair.Query(border, nullStroker, &alias, &alias) == expected && alias == nativeAlias);
}

#ifdef _WIN32
static void CheckProtected(const Pair& pair, int border, bool nullStroker, const FreeTypeTest::KernelApi& kernel) {
    FreeTypeTest::GuestOutput output(kernel);
    for (const bool hostReadOnly : {false, true}) {
        for (unsigned mask = 1; mask < 4; ++mask) {
            for (unsigned protection = 1; protection < 4; ++protection) {
                if (!(mask & protection)) continue;
                output.Reset();
                auto* points = mask & 1 ? output.Record<std::uint32_t>(16) : nullptr;
                auto* contours = mask & 2 ? output.Record<std::uint32_t>(output.Page + 16) : nullptr;
                FT_UInt nativePoints = 0, nativeContours = 0;
                const int nativeError = pair.Reference(border, nullStroker, points ? &nativePoints : nullptr, contours ? &nativeContours : nullptr);
                const auto before = output.Snapshot();
                for (unsigned page = 0; page < 2; ++page) {
                    if (!(protection & (1u << page))) continue;
                    output.Protect(page, false);
                    if (!hostReadOnly) output.PermitNativeWrite(page);
                }
                Require(pair.Query(border, nullStroker, points, contours) == (nativeError != 0 ? nativeError : FT_Err_Invalid_Argument));
                output.Unchanged(before);
                output.Protect(0, true);
                output.Protect(1, true);
                Require(pair.Query(border, nullStroker, points, contours) == nativeError);
                auto expected = before;
                if (points) std::memcpy(expected.data() + 16, &nativePoints, sizeof(nativePoints));
                if (contours) std::memcpy(expected.data() + output.Page + 16, &nativeContours, sizeof(nativeContours));
                output.Unchanged(expected);
            }
        }
        output.Reset();
        auto* alias = output.Record<std::uint32_t>(16);
        FT_UInt nativeAlias = 0;
        const int error = pair.Reference(border, nullStroker, &nativeAlias, &nativeAlias);
        const auto before = output.Snapshot();
        output.Protect(0, false);
        if (!hostReadOnly) output.PermitNativeWrite(0);
        Require(pair.Query(border, nullStroker, alias, alias) == (error != 0 ? error : FT_Err_Invalid_Argument));
        output.Unchanged(before);
        output.Protect(0, true);
        Require(pair.Query(border, nullStroker, alias, alias) == error && *alias == nativeAlias);
    }
}
#endif

int main(int argc, char** argv) {
    Require(argc == 3);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    Require(patched || std::strcmp(argv[2], "unpatched") == 0);
#ifdef _WIN32
    auto module = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
    auto kernel = LoadLibraryW((std::filesystem::path(argv[1]).parent_path() / "libkernel.prx").c_str());
    Require(kernel != nullptr);
#else
    auto module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
#endif
    Require(module != nullptr);
    Api api{};
    api.initialize = Import<decltype(module), decltype(api.initialize)>(module, "FT_Init_FreeType", "GNsiLzm4SH8", patched);
    api.doneLibrary = Import<decltype(module), decltype(api.doneLibrary)>(module, "FT_Done_FreeType", "dH02CpEEcYk", patched);
    api.create = Import<decltype(module), decltype(api.create)>(module, "FT_Stroker_New", "eqGCaeizvWM", patched);
    api.destroy = Import<decltype(module), decltype(api.destroy)>(module, "FT_Stroker_Done", "+RLa-6fbshA", patched);
    api.set = Import<decltype(module), decltype(api.set)>(module, "FT_Stroker_Set", "B3uU5aDZn94", patched);
    api.begin = Import<decltype(module), decltype(api.begin)>(module, "FT_Stroker_BeginSubPath", "xj3tNJptfLI", patched);
    api.line = Import<decltype(module), decltype(api.line)>(module, "FT_Stroker_LineTo", "cKndiZnt0EA", patched);
    api.end = Import<decltype(module), decltype(api.end)>(module, "FT_Stroker_EndSubPath", "pBqh-Uh-gns", patched);
    api.counts = Import<decltype(module), decltype(api.counts)>(module, "FT_Stroker_GetCounts", "rzIWh7FJnmU", patched);
    api.borderCounts = Import<decltype(module), decltype(api.borderCounts)>(module, "FT_Stroker_GetBorderCounts", "LfyUi-LkdCk", patched);
#ifdef _WIN32
    FreeTypeTest::KernelApi kernelApi{
        Import<decltype(kernel), decltype(FreeTypeTest::KernelApi::allocate)>(kernel, "sceKernelAllocateDirectMemory", "rTXw65xmLIA", patched, true),
        Import<decltype(kernel), decltype(FreeTypeTest::KernelApi::map)>(kernel, "sceKernelMapDirectMemory", "L-Q3LEjIbgA", patched, true),
        Import<decltype(kernel), decltype(FreeTypeTest::KernelApi::protect)>(kernel, "sceKernelMprotect", "vSMAm3cxYTY", patched, true),
        Import<decltype(kernel), decltype(FreeTypeTest::KernelApi::unmap)>(kernel, "sceKernelMunmap", "cQke9UuBQOk", patched, true),
        Import<decltype(kernel), decltype(FreeTypeTest::KernelApi::release)>(kernel, "sceKernelReleaseDirectMemory", "MBuItvba6z8", patched, true)
    };
#endif
    GuestLibrary* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(api.initialize(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    for (unsigned shape = 0; shape < 3; ++shape) {
        Pair pair(api, library, nativeLibrary, shape);
        for (const int border : {-1, 0, 1, 2, -2}) {
            for (const bool nullStroker : {false, true}) {
                CheckWritable(pair, border, nullStroker);
#ifdef _WIN32
                CheckProtected(pair, border, nullStroker, kernelApi);
#endif
            }
        }
    }
    Require(api.doneLibrary(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
#ifdef _WIN32
    Require(FreeLibrary(module) && FreeLibrary(kernel));
#else
    Require(dlclose(module) == 0);
#endif
}
