#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace FreeType;

static bool countLargeAllocations = false;
static std::size_t largeAllocationCount = 0;

void* operator new(std::size_t size) {
    if (countLargeAllocations && size >= 32768) ++largeAllocationCount;
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType outline transform check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Shape {
    std::vector<FT_Vector> points;
    std::vector<unsigned char> tags;
    std::vector<unsigned short> contours;
};

static Shape Square(bool clockwise) {
    Shape shape{{{0, 0}, {640, 0}, {640, 640}, {0, 640}}, {1, 1, 1, 1}, {3}};
    if (clockwise) std::swap(shape.points[1], shape.points[3]);
    return shape;
}

static Shape Frame() {
    return {{{0, 0}, {960, 0}, {960, 960}, {0, 960}, {240, 240}, {240, 720}, {720, 720}, {720, 240}}, {1, 1, 1, 1, 1, 1, 1, 1}, {3, 7}};
}

static Shape Curved() {
    return {{{0, 0}, {320, 0}, {640, 320}, {320, 640}, {0, 640}}, {1, 0, 1, 0, 1}, {4}};
}

struct Pair {
    std::vector<GuestVector> guestPoints;
    Shape shape;
    GuestOutline guest{};
    FT_Outline native{};
    std::vector<FT_Vector> nativePoints;

    explicit Pair(Shape input) : shape(std::move(input)), nativePoints(shape.points) {
        for (const FT_Vector& point : shape.points) guestPoints.push_back({point.x, point.y});
        const auto points = static_cast<std::uint16_t>(shape.points.size());
        const auto contours = static_cast<std::uint16_t>(shape.contours.size());
        guest = {contours, points, guestPoints.data(), shape.tags.data(), shape.contours.data(), 0};
        native = {static_cast<unsigned short>(contours), static_cast<unsigned short>(points), nativePoints.data(), shape.tags.data(), shape.contours.data(), 0};
    }

    void Matches() const {
        Require(guest.n_points == native.n_points && guest.n_contours == native.n_contours);
        for (std::size_t index = 0; index < nativePoints.size(); ++index) {
            Require(guestPoints[index].x == nativePoints[index].x && guestPoints[index].y == nativePoints[index].y);
        }
    }
};

static void TestEmbolden() {
    for (const auto& make : {+[] { return Square(false); }, +[] { return Square(true); }, +[] { return Frame(); }, +[] { return Curved(); }}) {
        for (const std::int64_t strength : {0, 1, 32, 64, 128, 512, -32, -64}) {
            Pair pair(make());
            const int guestError = FT_Outline_Embolden_nid_postfix(&pair.guest, strength);
            const int nativeError = FT_Outline_Embolden(&pair.native, static_cast<FT_Pos>(strength));
            Require(guestError == nativeError);
            pair.Matches();
        }
    }
    Require(FT_Outline_Embolden_nid_postfix(nullptr, 64) == FT_Outline_Embolden(nullptr, 64));
    GuestOutline empty{};
    FT_Outline nativeEmpty{};
    Require(FT_Outline_Embolden_nid_postfix(&empty, 64) == FT_Outline_Embolden(&nativeEmpty, 64));
}

static void TestEmboldenXY() {
    for (const auto& make : {+[] { return Square(false); }, +[] { return Frame(); }, +[] { return Curved(); }}) {
        for (const std::int64_t x : {0, 32, 64, -32}) {
            for (const std::int64_t y : {0, 64, 128, -64}) {
                Pair pair(make());
                const int guestError = FT_Outline_EmboldenXY_nid_postfix(&pair.guest, x, y);
                const int nativeError = FT_Outline_EmboldenXY(&pair.native, static_cast<FT_Pos>(x), static_cast<FT_Pos>(y));
                Require(guestError == nativeError);
                pair.Matches();
            }
        }
    }
    Require(FT_Outline_EmboldenXY_nid_postfix(nullptr, 64, 32) == FT_Outline_EmboldenXY(nullptr, 64, 32));
}

static void TestTransform() {
    const GuestMatrix matrices[] = {{0x10000, 0, 0, 0x10000}, {0x8000, 0, 0, 0x20000}, {0, -0x10000, 0x10000, 0}, {0x10000, 0x4000, 0, 0x10000},
                                    {-0x12345, 0x6789, -0x2468, 0x13579}};
    for (const GuestMatrix& matrix : matrices) {
        for (const auto& make : {+[] { return Square(false); }, +[] { return Frame(); }, +[] { return Curved(); }}) {
            Pair pair(make());
            const FT_Matrix native{static_cast<FT_Fixed>(matrix.xx), static_cast<FT_Fixed>(matrix.xy), static_cast<FT_Fixed>(matrix.yx),
                                   static_cast<FT_Fixed>(matrix.yy)};
            FT_Outline_Transform_nid_postfix(&pair.guest, &matrix);
            FT_Outline_Transform(&pair.native, &native);
            pair.Matches();
        }
    }
    Pair pair(Square(false));
    FT_Outline_Transform_nid_postfix(nullptr, &matrices[0]);
    FT_Outline_Transform_nid_postfix(&pair.guest, nullptr);
    pair.Matches();
    GuestOutline noPoints{};
    FT_Outline_Transform_nid_postfix(&noPoints, &matrices[0]);
}

static void TestNoopEmbolden() {
    constexpr std::int64_t big = std::int64_t{1} << 40;
    std::array<GuestVector, 4> points{{{big, -big}, {big + 640, -big}, {big + 640, -big + 640}, {big, -big + 640}}};
    const auto original = points;
    unsigned char tags[] = {1, 1, 1, 1};
    unsigned short end = 3;
    GuestOutline outline{1, 4, points.data(), tags, &end, FT_OUTLINE_EVEN_ODD_FILL};
    FT_Outline native{};
    const auto unchanged = [&] {
        for (std::size_t index = 0; index < points.size(); ++index) Require(points[index].x == original[index].x && points[index].y == original[index].y);
        Require(outline.n_contours == 1 && outline.n_points == 4 && outline.flags == FT_OUTLINE_EVEN_ODD_FILL);
        Require(outline.points == points.data() && outline.tags == tags && outline.contours == &end);
        Require(tags[0] == 1 && tags[1] == 1 && tags[2] == 1 && tags[3] == 1 && end == 3);
    };
    for (const std::int64_t x : {-1, 0, 1}) {
        Require(FT_Outline_Embolden_nid_postfix(&outline, x) == FT_Outline_Embolden(&native, static_cast<FT_Pos>(x)));
        unchanged();
        Require(FT_Outline_Embolden_nid_postfix(nullptr, x) == FT_Outline_Embolden(nullptr, static_cast<FT_Pos>(x)));
        for (const std::int64_t y : {-1, 0, 1}) {
            Require(FT_Outline_EmboldenXY_nid_postfix(&outline, x, y) == FT_Outline_EmboldenXY(&native, static_cast<FT_Pos>(x), static_cast<FT_Pos>(y)));
            unchanged();
            Require(FT_Outline_EmboldenXY_nid_postfix(nullptr, x, y) == FT_Outline_EmboldenXY(nullptr, static_cast<FT_Pos>(x), static_cast<FT_Pos>(y)));
        }
    }
}

static void TestWideTransformSums() {
    GuestVector points[] = {{0x40000000, 0x40000000}, {-0x40000001, -0x40000001}, {7, -3}};
    unsigned char tags[] = {1, 1, 1};
    unsigned short contours[] = {2};
    GuestOutline outline{1, 3, points, tags, contours, FT_OUTLINE_EVEN_ODD_FILL};
    const GuestMatrix matrix{0x10000, 0x10000, 0x10000, 0x10000};
    FT_Outline_Transform_nid_postfix(&outline, &matrix);
    Require(points[0].x == 0x80000000ll && points[0].y == 0x80000000ll);
    Require(points[1].x == -0x80000002ll && points[1].y == -0x80000002ll);
    Require(points[2].x == 4 && points[2].y == 4);
    Require(outline.n_points == 3 && outline.n_contours == 1 && outline.flags == FT_OUTLINE_EVEN_ODD_FILL);
    Require(outline.points == points && outline.tags == tags && outline.contours == contours);
    Require(tags[0] == 1 && tags[1] == 1 && tags[2] == 1 && contours[0] == 2);
}

static void TestUnreadable() {
    auto* unmapped = reinterpret_cast<GuestOutline*>(std::uintptr_t{0x10});
    const GuestMatrix scale{0x20000, 0, 0, 0x20000};
    Require(FT_Outline_Embolden_nid_postfix(unmapped, 64) == FT_Outline_Embolden(nullptr, 64));
    Require(FT_Outline_EmboldenXY_nid_postfix(unmapped, 64, 32) == FT_Outline_EmboldenXY(nullptr, 64, 32));
    FT_Outline_Transform_nid_postfix(unmapped, &scale);
    for (int field = 0; field < 3; ++field) {
        Pair pair(Frame());
        GuestOutline bad = pair.guest;
        if (field == 0) bad.points = reinterpret_cast<GuestVector*>(std::uintptr_t{0x10});
        if (field == 1) bad.tags = reinterpret_cast<unsigned char*>(std::uintptr_t{0x10});
        if (field == 2) bad.contours = reinterpret_cast<std::uint16_t*>(std::uintptr_t{0x10});
        Require(FT_Outline_Embolden_nid_postfix(&bad, 64) == FT_Outline_Embolden(nullptr, 64));
        Require(FT_Outline_EmboldenXY_nid_postfix(&bad, 64, 32) == FT_Outline_EmboldenXY(nullptr, 64, 32));
        FT_Outline_Transform_nid_postfix(&bad, &scale);
        pair.Matches();
    }
    Pair pair(Square(false));
    FT_Outline_Transform_nid_postfix(&pair.guest, reinterpret_cast<const GuestMatrix*>(std::uintptr_t{0x10}));
    pair.Matches();
    static const GuestVector readOnly[] = {{0, 0}, {640, 0}, {640, 640}, {0, 640}};
    unsigned char tags[] = {1, 1, 1, 1};
    unsigned short end = 3;
    GuestOutline outline{1, 4, const_cast<GuestVector*>(readOnly), tags, &end, 0};
    Require(FT_Outline_Embolden_nid_postfix(&outline, 64) == FT_Outline_Embolden(nullptr, 64));
    Require(FT_Outline_EmboldenXY_nid_postfix(&outline, 64, 32) == FT_Outline_EmboldenXY(nullptr, 64, 32));
    FT_Outline_Transform_nid_postfix(&outline, &scale);
    Require(readOnly[1].x == 640 && readOnly[2].y == 640);
}

class OutlineGuardPage {
public:
    OutlineGuardPage() {
#ifdef _WIN32
        SYSTEM_INFO information;
        GetSystemInfo(&information);
        page = information.dwPageSize;
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous;
        Require(VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous) != 0);
#else
        const long nativePage = sysconf(_SC_PAGESIZE);
        Require(nativePage > 0);
        page = static_cast<std::size_t>(nativePage);
        void* mapped = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        memory = static_cast<unsigned char*>(mapped);
        Require(mprotect(memory + page, page, PROT_NONE) == 0);
#endif
    }

    ~OutlineGuardPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, page * 2);
#endif
    }

    OutlineGuardPage(const OutlineGuardPage&) = delete;
    OutlineGuardPage& operator=(const OutlineGuardPage&) = delete;

    template <typename TElement>
    TElement* Tail(std::size_t count) {
        return reinterpret_cast<TElement*>(memory + page - count * sizeof(TElement));
    }

private:
    std::size_t page = 0;
    unsigned char* memory = nullptr;
};

template <typename TCall>
static void RequireNoLargeAllocation(TCall call) {
    largeAllocationCount = 0;
    countLargeAllocations = true;
    const bool rejected = call();
    countLargeAllocations = false;
    Require(rejected);
    Require(largeAllocationCount == 0);
}

static bool OrientationThrows(GuestOutline outline) {
    try {
        FT_Outline_Get_Orientation_nid_postfix(&outline);
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static void TestHugeCountsRejectedBeforeAllocation() {
    constexpr std::uint16_t huge = 0xFFFF;
    OutlineGuardPage guard;
    auto* shortPoints = guard.Tail<GuestVector>(4);
    auto* shortTags = guard.Tail<unsigned char>(4);
    auto* shortContours = guard.Tail<std::uint16_t>(4);
    auto* unmappedPoints = reinterpret_cast<GuestVector*>(std::uintptr_t{0x10});
    std::vector<GuestVector> bigPoints(huge);
    std::vector<unsigned char> bigTags(huge, 1);
    std::vector<std::uint16_t> bigContours(huge, 3);
    std::vector<GuestVector> bigTargetPoints(huge);
    std::vector<unsigned char> bigTargetTags(huge);
    std::vector<std::uint16_t> bigTargetContours(huge);
    GuestVector smallPoints[]{{0, 0}, {640, 0}, {640, 640}, {0, 640}};
    unsigned char smallTags[]{1, 1, 1, 1};
    std::uint16_t end = huge - 1;
    const GuestMatrix scale{0x20000, 0, 0, 0x20000};
    const std::array<GuestOutline, 5> sources{{
        {1, huge, shortPoints, bigTags.data(), &end, 0},
        {1, huge, unmappedPoints, bigTags.data(), &end, 0},
        {1, huge, bigPoints.data(), shortTags, &end, 0},
        {huge, 4, smallPoints, smallTags, shortContours, 0},
        {huge, huge, bigPoints.data(), bigTags.data(), shortContours, 0},
    }};
    for (std::size_t index = 0; index < sources.size(); ++index) {
        GuestOutline outline = sources[index];
        RequireNoLargeAllocation([&] { return FT_Outline_Embolden_nid_postfix(&outline, 64) == FT_Outline_Embolden(nullptr, 64); });
        RequireNoLargeAllocation([&] { return FT_Outline_EmboldenXY_nid_postfix(&outline, 64, 32) == FT_Outline_EmboldenXY(nullptr, 64, 32); });
        RequireNoLargeAllocation([&] {
            FT_Outline_Transform_nid_postfix(&outline, &scale);
            return true;
        });
        if (outline.tags != shortTags) RequireNoLargeAllocation([&] { return OrientationThrows(outline); });
        GuestOutline target{outline.n_contours, outline.n_points, bigTargetPoints.data(), bigTargetTags.data(), bigTargetContours.data(), 0};
        RequireNoLargeAllocation([&] { return FT_Outline_Copy_nid_postfix(&outline, &target) == FT_Err_Invalid_Outline; });
    }
    for (int field = 0; field < 3; ++field) {
        GuestOutline source{huge, huge, bigPoints.data(), bigTags.data(), bigContours.data(), 0};
        GuestOutline target{huge, huge, bigTargetPoints.data(), bigTargetTags.data(), bigTargetContours.data(), 0};
        if (field == 0) target.points = shortPoints;
        if (field == 1) target.tags = shortTags;
        if (field == 2) target.contours = shortContours;
        RequireNoLargeAllocation([&] { return FT_Outline_Copy_nid_postfix(&source, &target) == FT_Err_Invalid_Outline; });
    }
    for (std::uint16_t index = 0; index < huge; ++index) bigPoints[index] = {index, 1};
    GuestOutline valid{1, huge, bigPoints.data(), bigTags.data(), &end, 0};
    FT_Outline_Transform_nid_postfix(&valid, &scale);
    for (std::uint16_t index = 0; index < huge; ++index) Require(bigPoints[index].x == 2 * index && bigPoints[index].y == 2);
    GuestOutline target{1, huge, bigTargetPoints.data(), bigTargetTags.data(), bigTargetContours.data(), 0};
    Require(FT_Outline_Copy_nid_postfix(&valid, &target) == FT_Err_Ok);
    for (std::uint16_t index = 0; index < huge; ++index) Require(bigTargetPoints[index].x == 2 * index && bigTargetTags[index] == 1);
    Require(bigTargetContours[0] == huge - 1);
}

int main() {
    TestEmbolden();
    TestEmboldenXY();
    TestTransform();
    TestNoopEmbolden();
    TestWideTransformSums();
    TestUnreadable();
    TestHugeCountsRejectedBeforeAllocation();
}
