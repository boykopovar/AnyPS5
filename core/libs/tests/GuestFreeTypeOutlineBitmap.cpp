#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType outline bitmap check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

class GuardedTail {
public:
    explicit GuardedTail(std::size_t bytes) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        memory = VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(memory != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(static_cast<unsigned char*>(memory) + page, page, PAGE_NOACCESS, &previous) != 0);
#else
        page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        memory = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(memory != MAP_FAILED);
        Require(mprotect(static_cast<unsigned char*>(memory) + page, page, PROT_NONE) == 0);
#endif
        Require(bytes <= page);
        data = static_cast<unsigned char*>(memory) + page - bytes;
    }
    ~GuardedTail() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, page * 2);
#endif
    }
    GuardedTail(const GuardedTail&) = delete;
    GuardedTail& operator=(const GuardedTail&) = delete;
    template <class T> T* As() const { return static_cast<T*>(data); }

private:
    void* memory = nullptr;
    std::size_t page = 0;
    void* data = nullptr;
};

struct Shape {
    std::vector<FT_Vector> points;
    std::vector<unsigned char> tags;
    std::vector<unsigned short> contours;
};

static Shape Square() {
    return {{{64, 64}, {576, 64}, {576, 576}, {64, 576}}, {1, 1, 1, 1}, {3}};
}

static Shape Triangle() {
    return {{{32, 32}, {672, 160}, {160, 672}}, {1, 1, 1}, {2}};
}

static Shape Curve() {
    return {{{64, 64}, {448, 64}, {704, 320}, {448, 576}, {64, 576}}, {1, 0, 1, 0, 1}, {4}};
}

static void Compare(GuestLibrary& library, FT_Library native, const Shape& shape, unsigned char mode) {
    std::vector<GuestVector> guestPoints;
    for (const FT_Vector& point : shape.points) guestPoints.push_back({point.x, point.y});
    std::vector<FT_Vector> nativePoints = shape.points;
    std::vector<unsigned char> tags = shape.tags;
    std::vector<unsigned short> contours = shape.contours;
    const auto points = static_cast<std::uint16_t>(shape.points.size());
    GuestOutline guest{1, points, guestPoints.data(), tags.data(), contours.data(), 0};
    FT_Outline expected{1, points, nativePoints.data(), tags.data(), contours.data(), 0};
    const std::uint32_t width = 12;
    const std::uint32_t rows = 12;
    const std::int32_t pitch = mode == FT_PIXEL_MODE_MONO ? 2 : 12;
    std::vector<unsigned char> guestPixels(static_cast<std::size_t>(pitch) * rows, 0);
    std::vector<unsigned char> nativePixels = guestPixels;
    const GuestBitmap guestBitmap{rows, width, pitch, guestPixels.data(), static_cast<std::uint16_t>(mode == FT_PIXEL_MODE_MONO ? 2 : 256), mode, 0, nullptr};
    const FT_Bitmap nativeBitmap{rows, width, pitch, nativePixels.data(), guestBitmap.num_grays, mode, 0, nullptr};
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, &guest, &guestBitmap) == FT_Outline_Get_Bitmap(native, &expected, &nativeBitmap));
    Require(guestPixels == nativePixels);
    bool drawn = false;
    for (const unsigned char value : guestPixels) drawn |= value != 0;
    Require(drawn);
}

int main() {
    GuestLibrary library{};
    FT_Library native = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
    for (const auto& make : {+[] { return Square(); }, +[] { return Triangle(); }, +[] { return Curve(); }}) {
        for (const unsigned char mode : {FT_PIXEL_MODE_GRAY, FT_PIXEL_MODE_MONO}) Compare(library, native, make(), mode);
    }
    Shape square = Square();
    std::vector<GuestVector> points;
    for (const FT_Vector& point : square.points) points.push_back({point.x, point.y});
    GuestOutline outline{1, 4, points.data(), square.tags.data(), square.contours.data(), 0};
    std::array<unsigned char, 144> pixels{};
    const GuestBitmap bitmap{12, 12, 12, pixels.data(), 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    const GuestBitmap unsupported{12, 12, 12, pixels.data(), 256, FT_PIXEL_MODE_BGRA, 0, nullptr};
    const FT_Bitmap nativeUnsupported{12, 12, 12, pixels.data(), 256, FT_PIXEL_MODE_BGRA, 0, nullptr};
    const FT_Bitmap nativeBitmap{12, 12, 12, pixels.data(), 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    FT_Outline nativeOutline{1, 4, square.points.data(), square.tags.data(), square.contours.data(), 0};
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, &outline, &unsupported) == FT_Outline_Get_Bitmap(native, &nativeOutline, &nativeUnsupported));
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, &outline, nullptr) == FT_Outline_Get_Bitmap(native, &nativeOutline, nullptr));
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, nullptr, &bitmap) == FT_Outline_Get_Bitmap(native, nullptr, &nativeBitmap));
    Require(FT_Outline_Get_Bitmap_nid_postfix(nullptr, &outline, &bitmap) == FT_Outline_Get_Bitmap(nullptr, &nativeOutline, &nativeBitmap));
    pixels.fill(0);
    const auto unmapped = std::uintptr_t{0x10};
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, &outline, reinterpret_cast<const GuestBitmap*>(unmapped)) ==
            FT_Outline_Get_Bitmap(native, &nativeOutline, nullptr));
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, reinterpret_cast<GuestOutline*>(unmapped), &bitmap) ==
            FT_Outline_Get_Bitmap(native, nullptr, &nativeBitmap));
    const FT_Error nullOutline = FT_Outline_Get_Bitmap(native, nullptr, &nativeBitmap);
    GuestOutline unmappedPoints{1, 4, reinterpret_cast<GuestVector*>(unmapped), square.tags.data(), square.contours.data(), 0};
    GuestOutline unmappedTags{1, 4, points.data(), reinterpret_cast<unsigned char*>(unmapped), square.contours.data(), 0};
    GuestOutline unmappedContours{1, 4, points.data(), square.tags.data(), reinterpret_cast<std::uint16_t*>(unmapped), 0};
    for (GuestOutline* invalid : {&unmappedPoints, &unmappedTags, &unmappedContours}) {
        Require(FT_Outline_Get_Bitmap_nid_postfix(&library, invalid, &bitmap) == nullOutline);
    }
    for (const unsigned char value : pixels) Require(value == 0);
    GuardedTail tailPoints(points.size() * sizeof(GuestVector));
    std::memcpy(tailPoints.As<GuestVector>(), points.data(), points.size() * sizeof(GuestVector));
    GuardedTail tailTags(square.tags.size());
    std::memcpy(tailTags.As<unsigned char>(), square.tags.data(), square.tags.size());
    GuardedTail tailContours(square.contours.size() * sizeof(std::uint16_t));
    std::memcpy(tailContours.As<std::uint16_t>(), square.contours.data(), square.contours.size() * sizeof(std::uint16_t));
    GuestOutline oversizedPoints{1, 65535, tailPoints.As<GuestVector>(), square.tags.data(), square.contours.data(), 0};
    std::vector<GuestVector> manyPoints(65535, GuestVector{64, 64});
    GuestOutline oversizedTags{1, 65535, manyPoints.data(), tailTags.As<unsigned char>(), square.contours.data(), 0};
    GuestOutline oversizedContours{65535, 4, points.data(), square.tags.data(), tailContours.As<std::uint16_t>(), 0};
    for (GuestOutline* invalid : {&oversizedPoints, &oversizedTags, &oversizedContours}) {
        Require(FT_Outline_Get_Bitmap_nid_postfix(&library, invalid, &bitmap) == nullOutline);
    }
    for (const unsigned char value : pixels) Require(value == 0);
    GuestOutline tail{1, 4, tailPoints.As<GuestVector>(), tailTags.As<unsigned char>(), tailContours.As<std::uint16_t>(), 0};
    std::array<unsigned char, 144> nativePixels{};
    const FT_Bitmap nativeTarget{12, 12, 12, nativePixels.data(), 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, &tail, &bitmap) == FT_Outline_Get_Bitmap(native, &nativeOutline, &nativeTarget));
    Require(pixels == nativePixels && pixels[66] != 0);
    static const std::array<unsigned char, 144> readOnly{};
    const GuestBitmap readOnlyBitmap{12, 12, 12, const_cast<unsigned char*>(readOnly.data()), 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, &outline, &readOnlyBitmap) == FT_Err_Invalid_Argument);
    GuardedTail shortPixels(143);
    std::memset(shortPixels.As<unsigned char>(), 0, 143);
    const GuestBitmap shortBitmap{12, 12, 12, shortPixels.As<unsigned char>(), 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, &outline, &shortBitmap) == FT_Err_Invalid_Argument);
    for (std::size_t index = 0; index < 143; ++index) Require(shortPixels.As<unsigned char>()[index] == 0);
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
}
