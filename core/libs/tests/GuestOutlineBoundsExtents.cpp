#include "prx/libSceFreeType/include/Api.hpp"
#include <ft2build.h>
#include FT_BBOX_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "boundary check failed at line %u\n", location.line());
        std::abort();
    }
}

class Guarded {
    unsigned char* memory;
    std::size_t page;
    std::size_t writable;
public:
    explicit Guarded(std::size_t capacity = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
#endif
        writable = ((capacity + 128 + page - 1) / page) * page;
#ifdef _WIN32
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, writable + page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr);
#else
        memory = static_cast<unsigned char*>(mmap(nullptr, writable + page * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
#endif
        Reset();
    }
    ~Guarded() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, writable + page * 2) == 0);
#endif
    }
    Guarded(const Guarded&) = delete;
    Guarded& operator=(const Guarded&) = delete;
    void Protect(bool readOnly) {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory + page, writable, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(memory + page, writable, readOnly ? PROT_READ : PROT_READ | PROT_WRITE) == 0);
#endif
    }
    void Reset() {
        Protect(false);
        std::memset(memory + page, 0xa5, writable);
    }
    unsigned char* End(std::size_t count) {
        Require(count + 64 <= writable);
        return memory + page + writable - (std::max)(count, std::size_t{1});
    }
    void Prefix(const void* address) const {
        const auto* bytes = static_cast<const unsigned char*>(address);
        for (int index = 1; index <= 32; ++index) Require(bytes[-index] == 0xa5);
    }
};

using namespace FreeType;
struct Shape { std::vector<GuestVector> points; std::vector<unsigned char> tags; std::vector<std::uint16_t> contours; };
int main() {
    const Shape shapes[]{
        {{{0,0},{64,0},{64,64},{0,64}},{1,1,1,1},{3}},
        {{{0,0},{32,128},{64,0}},{1,0,1},{2}},
        {{{0,0},{16,160},{48,-160},{64,0}},{1,2,2,1},{3}},
        {{{0,0},{16,160},{48,-160},{64,0},{128,0},{192,0},{192,64},{128,64}},{1,2,2,1,1,1,1,1},{3,7}}};
    std::size_t cases = 0;
    for (const auto& shape : shapes) for (int scale : {1,2,4,8}) for (int shift : {-193,0,157}) {
        std::vector<GuestVector> points = shape.points;
        for (auto& p : points) { p.x = p.x * scale + shift; p.y = p.y * scale - shift; }
        Guarded pointMemory(points.size()*sizeof(GuestVector)), tagMemory(shape.tags.size()), contourMemory(shape.contours.size()*2);
        auto* guestPoints = reinterpret_cast<GuestVector*>(pointMemory.End(points.size()*sizeof(GuestVector)));
        auto* guestTags = tagMemory.End(shape.tags.size());
        auto* guestContours = reinterpret_cast<std::uint16_t*>(contourMemory.End(shape.contours.size()*2));
        std::uninitialized_copy(points.begin(),points.end(),guestPoints);
        std::copy(shape.tags.begin(),shape.tags.end(),guestTags);
        std::uninitialized_copy(shape.contours.begin(),shape.contours.end(),guestContours);
        pointMemory.Protect(true);tagMemory.Protect(true);contourMemory.Protect(true);
        Guarded outlineMemory(sizeof(GuestOutline)), output(sizeof(GuestBBox));
        auto* outline = std::construct_at(reinterpret_cast<GuestOutline*>(outlineMemory.End(sizeof(GuestOutline))),
            GuestOutline{static_cast<std::uint16_t>(shape.contours.size()),static_cast<std::uint16_t>(points.size()),guestPoints,guestTags,guestContours,0});
        outlineMemory.Protect(true);
        auto* box = std::construct_at(reinterpret_cast<GuestBBox*>(output.End(sizeof(GuestBBox))));
        std::vector<FT_Vector> nativePoints;
        for (const auto& p : points) nativePoints.push_back({static_cast<FT_Pos>(p.x),static_cast<FT_Pos>(p.y)});
        auto tags = shape.tags;
        auto contours = shape.contours;
        FT_Outline native{static_cast<FT_UShort>(contours.size()),static_cast<FT_UShort>(nativePoints.size()),nativePoints.data(),tags.data(),contours.data(),0};
        FT_BBox expected{};
        Require(FT_Outline_Get_BBox(&native,&expected) == 0);
        Require(FT_Outline_Get_BBox_nid_postfix(outline,box) == 0);
        Require(box->xMin == expected.xMin && box->yMin == expected.yMin && box->xMax == expected.xMax && box->yMax == expected.yMax);
        for (std::size_t i = 0; i < points.size(); ++i) Require(guestPoints[i].x == points[i].x && guestPoints[i].y == points[i].y);
        Require(std::equal(shape.tags.begin(),shape.tags.end(),guestTags));
        Require(std::equal(shape.contours.begin(),shape.contours.end(),guestContours));
        pointMemory.Prefix(guestPoints);tagMemory.Prefix(guestTags);contourMemory.Prefix(guestContours);
        outlineMemory.Prefix(outline);output.Prefix(box);
        ++cases;
    }
    std::printf("%zu guarded outline array and bounding-box cases passed\n", cases);
}
