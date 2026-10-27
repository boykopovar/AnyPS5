#include "prx/libSceFreeType/include/Api.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

using FreeType::GuestOutline;
using FreeType::GuestVector;
using Query = decltype(&FT_Outline_GetInsideBorder_nid_postfix);
static_assert(std::is_same_v<Query, decltype(&FT_Outline_GetOutsideBorder_nid_postfix)>);
static_assert(sizeof(GuestOutline) == 40 && alignof(GuestOutline) == 8);
static_assert(offsetof(GuestOutline, points) == 8 && offsetof(GuestOutline, contours) == 24);
static_assert(offsetof(GuestOutline, flags) == 32 && sizeof(GuestVector) == 16);
static_assert(sizeof(FT_StrokerBorder) == 4);

namespace {

bool nativeInside;

void Require(bool result, const char* message) {
    if (result) return;
    std::fprintf(stderr, "FreeType outline border: %s\n", message);
    std::exit(1);
}

int APS5_VABI Native(const GuestOutline* input) {
    if (!input) return nativeInside ? FT_Outline_GetInsideBorder(nullptr) : FT_Outline_GetOutsideBorder(nullptr);
    FT_Outline outline{};
    outline.n_points = input->n_points;
    outline.n_contours = input->n_contours;
    outline.flags = input->flags;
    std::vector<FT_Vector> points;
    std::vector<unsigned short> contours;
    if (outline.n_points > 0) {
        for (int i = 0; i < outline.n_points; ++i) {
            Require(input->points[i].x >= std::numeric_limits<FT_Pos>::min() && input->points[i].x <= std::numeric_limits<FT_Pos>::max(), "reference x range");
            Require(input->points[i].y >= std::numeric_limits<FT_Pos>::min() && input->points[i].y <= std::numeric_limits<FT_Pos>::max(), "reference y range");
            points.push_back({static_cast<FT_Pos>(input->points[i].x), static_cast<FT_Pos>(input->points[i].y)});
        }
        if (outline.n_contours > 0)
            for (int i = 0; i < outline.n_contours; ++i) contours.push_back(input->contours[i]);
    }
    outline.points = points.data();
    outline.contours = contours.data();
    return nativeInside ? FT_Outline_GetInsideBorder(&outline) : FT_Outline_GetOutsideBorder(&outline);
}

int Expected(bool inside, bool clockwise) { return inside == clockwise ? 1 : 0; }

class ReadOnlyTail {
    void* memory;
    std::size_t page;
    void* data;
public:
    ReadOnlyTail(const void* source, std::size_t bytes) {
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(memory != nullptr, "allocate guard pages");
#else
        page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        memory = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(memory != MAP_FAILED, "allocate guard pages");
#endif
        Require(bytes <= page, "guard fixture size");
        data = static_cast<unsigned char*>(memory) + page - bytes;
        std::memcpy(data, source, bytes);
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory, page, PAGE_READONLY, &previous) != 0, "read-only input");
        Require(VirtualProtect(static_cast<unsigned char*>(memory) + page, page, PAGE_NOACCESS, &previous) != 0, "inaccessible neighbor");
#else
        Require(mprotect(memory, page, PROT_READ) == 0, "read-only input");
        Require(mprotect(static_cast<unsigned char*>(memory) + page, page, PROT_NONE) == 0, "inaccessible neighbor");
#endif
    }
    ~ReadOnlyTail() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, page * 2);
#endif
    }
    template<class T> T* As() const { return static_cast<T*>(data); }
    void* End() const { return static_cast<unsigned char*>(memory) + page; }
};

void Cases(Query query, bool inside, bool guest) {
    Require(query(nullptr) == Expected(inside, true), "null outline");
    GuestOutline outline{};
    outline.points = reinterpret_cast<GuestVector*>(std::uintptr_t{1});
    outline.contours = reinterpret_cast<std::uint16_t*>(std::uintptr_t{1});
    Require(query(&outline) == Expected(inside, true), "empty outline does not read points or contours");
    std::array<GuestVector, 4> points{{{0, 0}, {0, 64}, {64, 64}, {64, 0}}};
    std::uint16_t end = 3;
    outline = {1, 4, points.data(), nullptr, &end, 0};
    Require(query(&outline) == Expected(inside, true), "clockwise polygon");
    const auto original = points;
    for (const int flags : {0, 1, 2, -1}) {
        outline.flags = flags;
        Require(query(&outline) == Expected(inside, true), "orientation ignores outline flags");
    }
    Require(std::memcmp(points.data(), original.data(), sizeof(points)) == 0, "input points unchanged");
    std::reverse(points.begin(), points.end());
    Require(query(&outline) == Expected(inside, false), "counterclockwise polygon");
    for (auto& point : points) point.y = 0;
    Require(query(&outline) == Expected(inside, false), "collapsed polygon uses no-orientation fallback");
    points = original;
    outline.n_contours = 0;
    Require(query(&outline) == Expected(inside, false), "no contours");
    outline.n_contours = 1;
    for (const std::int64_t edge : {std::int64_t{0x1000000}, std::int64_t{0x1000001}}) {
        points = {{{-edge, -edge}, {-edge, edge}, {edge, edge}, {edge, -edge}}};
        Require(query(&outline) == (edge == 0x1000000 ? Expected(inside, true) : Expected(inside, false)), "coordinate-limit boundary");
    }
    std::array<GuestVector, 8> mixed{};
    std::copy(original.begin(), original.end(), mixed.begin());
    std::copy(original.rbegin(), original.rend(), mixed.begin() + 4);
    std::array<std::uint16_t, 2> mixedEnds{{3, 7}};
    GuestOutline cancelled{2, 8, mixed.data(), nullptr, mixedEnds.data(), 0};
    Require(query(&cancelled) == Expected(inside, false), "non-collapsed opposite contours cancel area");
    std::vector<GuestVector> manyPoints(65535);
    std::copy(original.rbegin(), original.rend(), manyPoints.begin());
    std::uint16_t manyEnd = 65534;
    GuestOutline many{1, 65535, manyPoints.data(), nullptr, &manyEnd, 0};
    Require(query(&many) == Expected(inside, false), "unsigned point count above 32767");
    std::fill(manyPoints.begin(), manyPoints.end(), GuestVector{});
    std::copy(original.begin(), original.end(), manyPoints.end() - 4);
    std::vector<std::uint16_t> manyEnds(65532);
    for (unsigned i = 0; i < manyEnds.size() - 1; ++i) manyEnds[i] = static_cast<std::uint16_t>(i);
    manyEnds.back() = 65534;
    many.n_contours = 65532;
    many.contours = manyEnds.data();
    Require(query(&many) == Expected(inside, true), "unsigned contour count above 32767");
    if (guest) {
        for (const auto edge : {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}) {
            points = original;
            points[0].x = edge;
            Require(query(&outline) == Expected(inside, false), "64-bit out-of-range coordinate");
        }
        points = original;
        std::array<GuestVector, 20> repeated{};
        std::array<std::uint16_t, 5> ends{};
        const std::array<GuestVector, 4> large{{{0, 0}, {0, 16384}, {16384, 16384}, {16384, 0}}};
        for (unsigned i = 0; i < 5; ++i) {
            std::copy(large.begin(), large.end(), repeated.begin() + i * 4);
            ends[i] = static_cast<std::uint16_t>(i * 4 + 3);
        }
        GuestOutline wide{5, 20, repeated.data(), nullptr, ends.data(), 0};
        Require(query(&wide) == Expected(inside, true), "LP64 area must not wrap at 32 bits");
        ReadOnlyTail guardedPoints(original.data(), sizeof(original));
        ReadOnlyTail guardedEnds(&end, sizeof(end));
        GuestOutline guarded{1, 4, guardedPoints.As<GuestVector>(), static_cast<unsigned char*>(guardedPoints.End()), guardedEnds.As<std::uint16_t>(), -1};
        ReadOnlyTail guardedOutline(&guarded, sizeof(guarded));
        Require(query(guardedOutline.As<GuestOutline>()) == Expected(inside, true), "read-only bounded inputs and unused inaccessible tags");
        outline = {1, 4, nullptr, nullptr, &end, 0};
        bool threw = false;
        try { query(&outline); } catch (const std::invalid_argument&) { threw = true; }
        Require(threw, "null positive-count points are explicitly unsupported");
        outline.points = points.data();
        outline.contours = nullptr;
        threw = false;
        try { query(&outline); } catch (const std::invalid_argument&) { threw = true; }
        Require(threw, "null positive-count contours are explicitly unsupported");
        for (const std::uint16_t badEnd : {std::uint16_t{4}, std::uint16_t{0xffff}}) {
            auto invalidEnd = badEnd;
            outline.contours = &invalidEnd;
            threw = false;
            try { query(&outline); } catch (const std::invalid_argument&) { threw = true; }
            Require(threw, "invalid contour endpoint is explicitly unsupported");
        }
        for (const std::uint16_t nextEnd : {std::uint16_t{2}, std::uint16_t{3}}) {
            std::array<std::uint16_t, 2> invalidEnds{{3, nextEnd}};
            outline.n_contours = 2;
            outline.contours = invalidEnds.data();
            threw = false;
            try { query(&outline); } catch (const std::invalid_argument&) { threw = true; }
            Require(threw, "non-increasing contour endpoints are explicitly unsupported");
        }
    }
    std::uint32_t state = 7;
    for (unsigned iteration = 0; iteration < 512; ++iteration) {
        for (auto& point : points) {
            state = state * 1664525u + 1013904223u;
            point.x = static_cast<int>((state >> 16) & 255) - 128;
            state = state * 1664525u + 1013904223u;
            point.y = static_cast<int>((state >> 16) & 255) - 128;
        }
        outline = {1, 4, points.data(), nullptr, &end, 0};
        nativeInside = inside;
        Require(query(&outline) == Native(&outline), "bounded polygon matches bundled FreeType");
    }
    if (guest) {
        points = original;
        outline = {1, 4, points.data(), nullptr, &end, 0};
        std::array<std::thread, 4> threads;
        for (auto& thread : threads) thread = std::thread([&] {
            for (unsigned i = 0; i < 100; ++i) Require(query(&outline) == Expected(inside, true), "concurrent independent query");
        });
        for (auto& thread : threads) thread.join();
    }
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    if (argc == 2 && std::strcmp(argv[1], "reference") == 0) {
        for (const bool inside : {false, true}) {
            nativeInside = inside;
            Cases(Native, inside, false);
        }
        std::puts("Bundled FreeType reference cases passed");
        return 0;
    }
    Require(argc == 4, "expected library, border and export mode");
    const bool inside = std::strcmp(argv[2], "inside") == 0;
    const bool patched = std::strcmp(argv[3], "patched") == 0;
    const char* symbol = patched ? (inside ? "Pz262plj20w" : "sKneJjUXWkQ") :
        (inside ? "FT_Outline_GetInsideBorder_nid_postfix" : "FT_Outline_GetOutsideBorder_nid_postfix");
#ifdef _WIN32
    const auto library = LoadLibraryA(argv[1]);
    Require(library != nullptr, "load requested library");
    const auto checkPath = [](HMODULE module, const std::filesystem::path& expected) {
        Require(module != nullptr, "load matching runtime module");
        std::array<wchar_t, 32768> path{};
        const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        Require(length != 0 && length < path.size(), "read loaded module path");
        Require(std::filesystem::equivalent(expected, std::filesystem::path(path.data())), "loaded module variant path");
    };
    const auto requested = std::filesystem::absolute(argv[1]);
    checkPath(library, requested);
    checkPath(GetModuleHandleW(L"libc.prx"), requested.parent_path() / "libc.prx");
    auto query = reinterpret_cast<Query>(GetProcAddress(library, symbol));
#else
    const auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) std::fprintf(stderr, "%s\n", dlerror());
    Require(library != nullptr, "load requested library");
    auto query = reinterpret_cast<Query>(dlsym(library, symbol));
#endif
    if (!query) std::fprintf(stderr, "Missing %s\n", symbol);
    Require(query != nullptr, "missing outline-border export");
    Cases(query, inside, true);
    std::puts("Guest outline-border cases passed");
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
}
