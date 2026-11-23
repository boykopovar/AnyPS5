#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<void*, std::size_t> allocations;
std::size_t allocationCalls = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoFrameBuffer::CropAndScale check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    ++allocationCalls;
    auto* pointer = std::malloc(size);
    if (pointer) {
        allocations.emplace(pointer, size);
        std::memset(pointer, 0xa5, size);
    }
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

template<typename TException, typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

const void* Entry(const void* object, std::size_t slot) {
    return (*static_cast<const void* const* const*>(object))[slot];
}

template<typename TResult, std::size_t TSlot, typename... TArguments>
TResult Call(const void* object, TArguments... arguments) {
    using Function = TResult(APS5_VABI*)(const void*, TArguments...);
    return reinterpret_cast<Function>(const_cast<void*>(Entry(object, TSlot)))(object, arguments...);
}

using Buffer = WebRtcRefCountedI420Buffer;

Buffer* Create(int width, int height, int strideY, int strideU, int strideV) {
    Buffer* buffer = nullptr;
    _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, width, height, strideY, strideU, strideV);
    Require(buffer != nullptr);
    return buffer;
}

void Release(const void* buffer) {
    Require(Call<std::int32_t, 1>(buffer) == 0);
}

struct Planes {
    const std::uint8_t* y;
    const std::uint8_t* u;
    const std::uint8_t* v;
    int strideY;
    int strideU;
    int strideV;
    int width;
    int height;

    explicit Planes(const void* buffer)
        : y(Call<const std::uint8_t*, 17>(buffer)), u(Call<const std::uint8_t*, 18>(buffer)), v(Call<const std::uint8_t*, 19>(buffer)),
          strideY(Call<int, 14>(buffer)), strideU(Call<int, 15>(buffer)), strideV(Call<int, 16>(buffer)),
          width(Call<int, 5>(buffer)), height(Call<int, 6>(buffer)) {}

    int Y(int column, int row) const { return y[row * strideY + column]; }
    int U(int column, int row) const { return u[row * strideU + column]; }
    int V(int column, int row) const { return v[row * strideV + column]; }
    int ChromaWidth() const { return (width + 1) / 2; }
    int ChromaHeight() const { return (height + 1) / 2; }
};

int PatternY(int column, int row) { return 16 + (column * 7 + row * 13 + column * row) % 220; }
int PatternU(int column, int row) { return 5 + (column * 31 + row * 17) % 240; }
int PatternV(int column, int row) { return 250 - (column * 11 + row * 29) % 240; }

Buffer* CreatePattern(int width, int height, int strideY, int strideU, int strideV) {
    Buffer* buffer = Create(width, height, strideY, strideU, strideV);
    const Planes planes(buffer);
    const int chromaHeight = planes.ChromaHeight();
    std::memset(buffer->buffer.data, 0xee, static_cast<std::size_t>(strideY) * height + static_cast<std::size_t>(strideU + strideV) * chromaHeight);
    for (int row = 0; row < height; ++row)
        for (int column = 0; column < width; ++column) const_cast<std::uint8_t*>(planes.y)[row * strideY + column] = static_cast<std::uint8_t>(PatternY(column, row));
    for (int row = 0; row < chromaHeight; ++row)
        for (int column = 0; column < planes.ChromaWidth(); ++column) {
            const_cast<std::uint8_t*>(planes.u)[row * strideU + column] = static_cast<std::uint8_t>(PatternU(column, row));
            const_cast<std::uint8_t*>(planes.v)[row * strideV + column] = static_cast<std::uint8_t>(PatternV(column, row));
        }
    return buffer;
}

Buffer* CropAndScale(void* source, int offsetX, int offsetY, int cropWidth, int cropHeight, int scaledWidth, int scaledHeight) {
    using Function = void** (APS5_VABI*)(void**, void*, int, int, int, int, int, int);
    void* result = nullptr;
    Require(reinterpret_cast<Function>(const_cast<void*>(Entry(source, 9)))(&result, source, offsetX, offsetY, cropWidth, cropHeight, scaledWidth, scaledHeight) == &result);
    Require(result != nullptr && result != source);
    auto* buffer = static_cast<Buffer*>(result);
    Require(buffer->refCount.load() == 1 && Call<std::int32_t, 4>(buffer) == 1);
    Require(buffer->buffer.width == scaledWidth && buffer->buffer.height == scaledHeight);
    Require(buffer->buffer.strideY == scaledWidth && buffer->buffer.strideU == (scaledWidth + 1) / 2 && buffer->buffer.strideV == (scaledWidth + 1) / 2);
    return buffer;
}

void TestCopy() {
    const int sizes[][5] = {{6, 5, 8, 3, 4}, {5, 3, 7, 4, 5}, {1, 1, 1, 1, 1}, {64, 48, 64, 32, 32}, {33, 17, 40, 20, 17}};
    for (const auto& size : sizes) {
        Buffer* source = CreatePattern(size[0], size[1], size[2], size[3], size[4]);
        const std::size_t before = allocations.size();
        Buffer* copy = CropAndScale(source, 0, 0, size[0], size[1], size[0], size[1]);
        Require(allocations.size() == before + 2 && source->refCount.load() == 1);
        const Planes planes(copy);
        for (int row = 0; row < size[1]; ++row)
            for (int column = 0; column < size[0]; ++column) Require(planes.Y(column, row) == PatternY(column, row));
        for (int row = 0; row < planes.ChromaHeight(); ++row)
            for (int column = 0; column < planes.ChromaWidth(); ++column)
                Require(planes.U(column, row) == PatternU(column, row) && planes.V(column, row) == PatternV(column, row));
        Release(copy);
        Release(source);
    }
}

void TestHalfAndQuarter() {
    Buffer* source = CreatePattern(16, 16, 19, 9, 11);
    const Planes from(source);
    Buffer* half = CropAndScale(source, 0, 0, 16, 16, 8, 8);
    const Planes halved(half);
    for (int row = 0; row < 8; ++row)
        for (int column = 0; column < 8; ++column)
            Require(halved.Y(column, row) == (from.Y(2 * column, 2 * row) + from.Y(2 * column + 1, 2 * row) + from.Y(2 * column, 2 * row + 1) + from.Y(2 * column + 1, 2 * row + 1) + 2) / 4);
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column) {
            Require(halved.U(column, row) == (from.U(2 * column, 2 * row) + from.U(2 * column + 1, 2 * row) + from.U(2 * column, 2 * row + 1) + from.U(2 * column + 1, 2 * row + 1) + 2) / 4);
            Require(halved.V(column, row) == (from.V(2 * column, 2 * row) + from.V(2 * column + 1, 2 * row) + from.V(2 * column, 2 * row + 1) + from.V(2 * column + 1, 2 * row + 1) + 2) / 4);
        }
    Release(half);

    Buffer* quarter = CropAndScale(source, 0, 0, 16, 16, 4, 4);
    const Planes quartered(quarter);
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column) {
            int sum = 0;
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) sum += from.Y(4 * column + x, 4 * row + y);
            Require(quartered.Y(column, row) == (sum + 8) / 16);
        }
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 2; ++column) {
            int sumU = 0;
            int sumV = 0;
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) {
                    sumU += from.U(4 * column + x, 4 * row + y);
                    sumV += from.V(4 * column + x, 4 * row + y);
                }
            Require(quartered.U(column, row) == (sumU + 8) / 16 && quartered.V(column, row) == (sumV + 8) / 16);
        }
    Release(quarter);

    Buffer* cropped = CropAndScale(source, 4, 8, 8, 8, 4, 4);
    const Planes window(cropped);
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            Require(window.Y(column, row) == (from.Y(4 + 2 * column, 8 + 2 * row) + from.Y(5 + 2 * column, 8 + 2 * row) + from.Y(4 + 2 * column, 9 + 2 * row) + from.Y(5 + 2 * column, 9 + 2 * row) + 2) / 4);
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 2; ++column)
            Require(window.U(column, row) == (from.U(2 + 2 * column, 4 + 2 * row) + from.U(3 + 2 * column, 4 + 2 * row) + from.U(2 + 2 * column, 5 + 2 * row) + from.U(3 + 2 * column, 5 + 2 * row) + 2) / 4);
    Release(cropped);
    Release(source);
}

void TestOffsetsAreMadeEven() {
    Buffer* source = CreatePattern(12, 12, 12, 6, 7);
    const int offsets[][4] = {{3, 5, 2, 4}, {2, 4, 2, 4}, {1, 1, 0, 0}, {0, 7, 0, 6}, {7, 0, 6, 0}, {8, 8, 8, 8}};
    for (const auto& offset : offsets) {
        Buffer* cropped = CropAndScale(source, offset[0], offset[1], 4, 4, 4, 4);
        const Planes planes(cropped);
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column) Require(planes.Y(column, row) == PatternY(offset[2] + column, offset[3] + row));
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 2; ++column) {
                Require(planes.U(column, row) == PatternU(offset[2] / 2 + column, offset[3] / 2 + row));
                Require(planes.V(column, row) == PatternV(offset[2] / 2 + column, offset[3] / 2 + row));
            }
        Release(cropped);
    }
    Release(source);
}

void TestANegativeHeightFlips() {
    Buffer* source = CreatePattern(8, 8, 10, 5, 4);
    Buffer* flipped = CropAndScale(source, 0, 0, 8, -8, 8, 8);
    const Planes planes(flipped);
    for (int row = 0; row < 8; ++row)
        for (int column = 0; column < 8; ++column) Require(planes.Y(column, row) == PatternY(column, 7 - row));
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column) Require(planes.U(column, row) == PatternU(column, 3 - row) && planes.V(column, row) == PatternV(column, 3 - row));
    Release(flipped);
    Buffer* window = CropAndScale(source, 0, 2, 8, -4, 8, 4);
    const Planes rows(window);
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 8; ++column) Require(rows.Y(column, row) == PatternY(column, 5 - row));
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 4; ++column) Require(rows.U(column, row) == PatternU(column, 2 - row) && rows.V(column, row) == PatternV(column, 2 - row));
    Release(window);
    Release(source);
}

void TestUniformPlanes() {
    const int cases[][5] = {{7, 5, 3, 9, 0}, {4, 4, 9, 7, 0}, {640, 360, 320, 180, 0}, {10, 10, 7, 7, 0}, {1, 1, 5, 5, 0}, {16, 16, 12, 12, 0}, {64, 64, 16, 16, 0}, {20, 28, 13, 9, 0},
        {1280, 720, 960, 540, 0}, {1280, 720, 854, 480, 0}, {33, 17, 11, 5, 1}, {300, 200, 37, 23, 1}, {9, 9, 3, 3, 1}, {20, 28, 7, 5, 1}, {64, 64, 24, 24, 1}};
    for (const auto& item : cases) {
        Buffer* source = Create(item[0], item[1], item[0] + 3, (item[0] + 1) / 2 + 1, (item[0] + 1) / 2);
        const Planes from(source);
        std::memset(const_cast<std::uint8_t*>(from.y), 77, static_cast<std::size_t>(from.strideY) * item[1]);
        std::memset(const_cast<std::uint8_t*>(from.u), 33, static_cast<std::size_t>(from.strideU) * from.ChromaHeight());
        std::memset(const_cast<std::uint8_t*>(from.v), 201, static_cast<std::size_t>(from.strideV) * from.ChromaHeight());
        Buffer* scaled = CropAndScale(source, 0, 0, item[0], item[1], item[2], item[3]);
        const Planes planes(scaled);
        const int loss = item[4];
        for (int row = 0; row < item[3]; ++row)
            for (int column = 0; column < item[2]; ++column) Require(planes.Y(column, row) <= 77 && planes.Y(column, row) >= 77 - loss);
        for (int row = 0; row < planes.ChromaHeight(); ++row)
            for (int column = 0; column < planes.ChromaWidth(); ++column) {
                Require(planes.U(column, row) <= 33 && planes.U(column, row) >= 33 - loss);
                Require(planes.V(column, row) <= 201 && planes.V(column, row) >= 201 - loss);
            }
        Release(scaled);
        Release(source);
    }
}

std::uint32_t Checksum(const Planes& planes) {
    std::uint32_t hash = 2166136261u;
    for (int row = 0; row < planes.height; ++row)
        for (int column = 0; column < planes.width; ++column) hash = (hash ^ static_cast<std::uint32_t>(planes.Y(column, row))) * 16777619u;
    for (int row = 0; row < planes.ChromaHeight(); ++row)
        for (int column = 0; column < planes.ChromaWidth(); ++column) hash = (hash ^ static_cast<std::uint32_t>(planes.U(column, row))) * 16777619u;
    for (int row = 0; row < planes.ChromaHeight(); ++row)
        for (int column = 0; column < planes.ChromaWidth(); ++column) hash = (hash ^ static_cast<std::uint32_t>(planes.V(column, row))) * 16777619u;
    return hash;
}

void TestThirdAndThreeQuarters() {
    Buffer* source = CreatePattern(18, 18, 21, 9, 12);
    const Planes from(source);
    Buffer* third = CropAndScale(source, 0, 0, 18, 18, 6, 6);
    const Planes thirds(third);
    for (int row = 0; row < 6; ++row)
        for (int column = 0; column < 6; ++column) {
            int sum = 0;
            for (int y = 0; y < 3; ++y)
                for (int x = 0; x < 3; ++x) sum += from.Y(3 * column + x, 3 * row + y);
            Require(thirds.Y(column, row) == (sum * (65536 / 9)) >> 16);
        }
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column) {
            int sumU = 0;
            int sumV = 0;
            for (int y = 0; y < 3; ++y)
                for (int x = 0; x < 3; ++x) {
                    sumU += from.U(3 * column + x, 3 * row + y);
                    sumV += from.V(3 * column + x, 3 * row + y);
                }
            Require(thirds.U(column, row) == (sumU * (65536 / 9)) >> 16 && thirds.V(column, row) == (sumV * (65536 / 9)) >> 16);
        }
    Release(third);
    Release(source);

    Buffer* wide = Create(64, 64, 64, 32, 32);
    std::uint32_t seed = 12345;
    for (int index = 0; index < 64 * 64 + 2 * 32 * 32; ++index) {
        seed = seed * 1103515245u + 12345u;
        wide->buffer.data[index] = static_cast<std::uint8_t>(seed >> 16);
    }
    Buffer* threeQuarters = CropAndScale(wide, 0, 0, 64, 64, 48, 48);
    Require(Checksum(Planes(threeQuarters)) == 0xa07acbb5u);
    Release(threeQuarters);
    Release(wide);
}

void TestScaledValuesStayInsideTheCrop() {
    Buffer* source = Create(40, 28, 40, 20, 20);
    const Planes from(source);
    for (int row = 0; row < 28; ++row)
        for (int column = 0; column < 40; ++column) const_cast<std::uint8_t*>(from.y)[row * 40 + column] = static_cast<std::uint8_t>(column < 20 ? 250 : 100 + (row * 3 + column) % 50);
    for (int row = 0; row < 14; ++row)
        for (int column = 0; column < 20; ++column) {
            const_cast<std::uint8_t*>(from.u)[row * 20 + column] = static_cast<std::uint8_t>(column < 10 ? 3 : 60 + (row + column) % 30);
            const_cast<std::uint8_t*>(from.v)[row * 20 + column] = static_cast<std::uint8_t>(column < 10 ? 240 : 120 + (row * 5 + column) % 40);
        }
    const int sizes[][2] = {{7, 5}, {13, 9}, {40, 56}, {20, 14}, {19, 27}, {5, 7}};
    for (const auto& size : sizes) {
        Buffer* scaled = CropAndScale(source, 20, 0, 20, 28, size[0], size[1]);
        const Planes planes(scaled);
        for (int row = 0; row < size[1]; ++row)
            for (int column = 0; column < size[0]; ++column) Require(planes.Y(column, row) >= 100 && planes.Y(column, row) < 150);
        for (int row = 0; row < planes.ChromaHeight(); ++row)
            for (int column = 0; column < planes.ChromaWidth(); ++column) {
                Require(planes.U(column, row) >= 60 && planes.U(column, row) < 90);
                Require(planes.V(column, row) >= 120 && planes.V(column, row) < 160);
            }
        Release(scaled);
    }
    Release(source);
}

struct GuestBuffer {
    void* const* vtable;
    void* converted = nullptr;
    int conversions = 0;
    int references = 0;
    int releases = 0;
    int width = 0;
    int height = 0;
    int strideY = 0;
    int strideU = 0;
    int strideV = 0;
    std::vector<std::uint8_t> y;
    std::vector<std::uint8_t> u;
    std::vector<std::uint8_t> v;
};

void APS5_VABI GuestAddRef(GuestBuffer* buffer) { ++buffer->references; }
std::int32_t APS5_VABI GuestRelease(GuestBuffer* buffer) {
    ++buffer->releases;
    return 1;
}
void APS5_VABI GuestUnexpected(GuestBuffer*) { Require(false); }
std::int32_t APS5_VABI GuestType(const GuestBuffer*) { return 0; }
std::int32_t APS5_VABI GuestWidth(const GuestBuffer* buffer) { return buffer->width; }
std::int32_t APS5_VABI GuestHeight(const GuestBuffer* buffer) { return buffer->height; }
void** APS5_VABI GuestToI420(void** result, GuestBuffer* buffer) {
    ++buffer->conversions;
    if (buffer->converted) Call<void, 0>(buffer->converted);
    *result = buffer->converted;
    return result;
}
std::int32_t APS5_VABI GuestStrideY(const GuestBuffer* buffer) { return buffer->strideY; }
std::int32_t APS5_VABI GuestStrideU(const GuestBuffer* buffer) { return buffer->strideU; }
std::int32_t APS5_VABI GuestStrideV(const GuestBuffer* buffer) { return buffer->strideV; }
const std::uint8_t* APS5_VABI GuestDataY(const GuestBuffer* buffer) { return buffer->y.data(); }
const std::uint8_t* APS5_VABI GuestDataU(const GuestBuffer* buffer) { return buffer->u.data(); }
const std::uint8_t* APS5_VABI GuestDataV(const GuestBuffer* buffer) { return buffer->v.data(); }

void* const guestVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&GuestAddRef), reinterpret_cast<void*>(&GuestRelease),
    reinterpret_cast<void*>(&GuestUnexpected), reinterpret_cast<void*>(&GuestUnexpected),
    reinterpret_cast<void*>(&GuestType), reinterpret_cast<void*>(&GuestWidth), reinterpret_cast<void*>(&GuestHeight),
    reinterpret_cast<void*>(&GuestToI420), reinterpret_cast<void*>(&GuestUnexpected),
    reinterpret_cast<void*>(&_ZN6webrtc16VideoFrameBuffer12CropAndScaleEiiiiii_nid_postfix),
    reinterpret_cast<void*>(&GuestUnexpected), reinterpret_cast<void*>(&GuestUnexpected),
    reinterpret_cast<void*>(&GuestUnexpected), reinterpret_cast<void*>(&GuestUnexpected),
    reinterpret_cast<void*>(&GuestStrideY), reinterpret_cast<void*>(&GuestStrideU), reinterpret_cast<void*>(&GuestStrideV),
    reinterpret_cast<void*>(&GuestDataY), reinterpret_cast<void*>(&GuestDataU), reinterpret_cast<void*>(&GuestDataV),
    reinterpret_cast<void*>(&GuestUnexpected),
};

void TestAGuestBufferIsConverted() {
    Buffer* converted = CreatePattern(8, 8, 9, 5, 6);
    GuestBuffer native{guestVtable + 2};
    native.converted = converted;
    native.width = 1000;
    native.height = 1000;
    Buffer* scaled = CropAndScale(&native, 2, 2, 4, 4, 4, 4);
    Require(native.conversions == 1 && native.references == 0 && native.releases == 0 && converted->refCount.load() == 1);
    const Planes planes(scaled);
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column) Require(planes.Y(column, row) == PatternY(2 + column, 2 + row));
    Require(planes.U(1, 1) == PatternU(2, 2) && planes.V(0, 1) == PatternV(1, 2));
    Release(scaled);
    void* untouched = &native;
    using Function = void** (APS5_VABI*)(void**, void*, int, int, int, int, int, int);
    const auto cropAndScale = reinterpret_cast<Function>(const_cast<void*>(Entry(&native, 9)));
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &native, 0, 0, 9, 8, 4, 4); });
    Require(native.conversions == 2 && converted->refCount.load() == 1 && untouched == &native);
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &native, 0, 0, 8, 8, 0, 4); });
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &native, 0, 0, 8, 8, 4, -1); });
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &native, 0, 0, 8, 8, INT_MAX, 4); });
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &native, 0, 0, 8, 8, 4, INT_MAX); });
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &native, 0, 0, 8, 8, INT_MAX, INT_MAX); });
    for (const int width : {0, -4, INT_MIN, 32769, INT_MAX}) RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &native, 0, 0, width, 4, 4, 4); });
    for (const int height : {0, INT_MIN, 32769, -32769, INT_MAX}) RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &native, 0, 0, 4, height, 4, 4); });
    Require(native.conversions == 2 && untouched == &native);
    native.converted = nullptr;
    const std::size_t before = allocations.size();
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &native, 0, 0, 8, 8, 4, 4); });
    Require(native.conversions == 3 && untouched == &native && allocations.size() == before);
    Release(converted);

    GuestBuffer planar{guestVtable + 2};
    planar.converted = &planar;
    planar.width = 6;
    planar.height = 4;
    planar.strideY = 11;
    planar.strideU = 7;
    planar.strideV = 5;
    planar.y.assign(11 * 4, 0xee);
    planar.u.assign(7 * 2, 0xee);
    planar.v.assign(5 * 2, 0xee);
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 6; ++column) planar.y[row * 11 + column] = static_cast<std::uint8_t>(PatternY(column, row));
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 3; ++column) {
            planar.u[row * 7 + column] = static_cast<std::uint8_t>(PatternU(column, row));
            planar.v[row * 5 + column] = static_cast<std::uint8_t>(PatternV(column, row));
        }
    Buffer* copy = CropAndScale(&planar, 0, 0, 6, 4, 6, 4);
    Require(planar.conversions == 1 && planar.references == 1 && planar.releases == 1);
    const Planes copied(copy);
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 6; ++column) Require(copied.Y(column, row) == PatternY(column, row));
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 3; ++column) Require(copied.U(column, row) == PatternU(column, row) && copied.V(column, row) == PatternV(column, row));
    Release(copy);
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &planar, 0, 0, 6, 5, 6, 4); });
    Require(planar.references == 2 && planar.releases == 2);
    for (int plane = 0; plane < 3; ++plane) {
        GuestBuffer partial = planar;
        partial.converted = &partial;
        partial.references = 0;
        partial.releases = 0;
        (plane == 0 ? partial.y : plane == 1 ? partial.u : partial.v) = std::vector<std::uint8_t>();
        const std::size_t calls = allocationCalls;
        RequireThrows<std::invalid_argument>([&] { cropAndScale(&untouched, &partial, 0, 0, 6, 4, 6, 4); });
        Require(partial.references == 1 && partial.releases == 1 && allocationCalls == calls && untouched == &native);
    }
}

void TestTheScalerLimits() {
    Buffer* wide = Create(40000, 2, 40000, 20000, 20000);
    Buffer* tall = Create(2, 40000, 2, 1, 1);
    using Function = void** (APS5_VABI*)(void**, void*, int, int, int, int, int, int);
    const auto cropAndScale = reinterpret_cast<Function>(const_cast<void*>(Entry(wide, 9)));
    void* result = wide;
    const std::size_t calls = allocationCalls;
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&result, wide, 0, 0, 32769, 2, 4, 2); });
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&result, wide, 0, 0, 40000, 2, 4, 2); });
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&result, tall, 0, 0, 2, 32769, 2, 4); });
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&result, tall, 0, 0, 2, -32769, 2, 4); });
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&result, tall, 0, 0, 2, 40000, 2, 4); });
    Require(result == wide && allocationCalls == calls && wide->refCount.load() == 1 && tall->refCount.load() == 1);
    Release(CropAndScale(wide, 0, 0, 32768, 2, 4, 2));
    Release(CropAndScale(wide, 7232, 0, 32768, 2, 4, 2));
    Release(CropAndScale(tall, 0, 0, 2, 32768, 2, 4));
    Release(CropAndScale(tall, 0, 7232, 2, -32768, 2, 4));
    Release(wide);
    Release(tall);
}

void TestInvalid() {
    Buffer* source = CreatePattern(8, 6, 8, 4, 4);
    using Function = void** (APS5_VABI*)(void**, void*, int, int, int, int, int, int);
    const auto cropAndScale = reinterpret_cast<Function>(const_cast<void*>(Entry(source, 9)));
    void* result = source;
    const std::size_t before = allocations.size();
    const std::size_t calls = allocationCalls;
    RequireThrows<std::invalid_argument>([&] { cropAndScale(nullptr, source, 0, 0, 8, 6, 8, 6); });
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&result, nullptr, 0, 0, 8, 6, 8, 6); });
    WebRtcVideoFrameBuffer empty{nullptr};
    RequireThrows<std::invalid_argument>([&] { cropAndScale(&result, &empty, 0, 0, 8, 6, 8, 6); });
    const int rejected[][4] = {{0, 0, 9, 6}, {0, 0, 8, 7}, {1, 0, 8, 6}, {0, 1, 8, 6}, {2, 0, 7, 6}, {0, 2, 8, 5}, {-1, 0, 8, 6}, {0, -1, 8, 6}, {-2, -2, 4, 4},
        {1, 0, INT_MAX, 6}, {0, 1, 8, INT_MAX}, {INT_MAX, 0, 1, 6}, {0, INT_MAX, 8, 1}, {INT_MAX, INT_MAX, INT_MAX, INT_MAX},
        {0, 0, 8, -7}, {0, 4, 8, -4}, {0, 1, 8, -6}, {0, 0, 8, INT_MIN}, {0, 1, 8, INT_MIN}, {0, 0, 8, -INT_MAX}, {0, 6, 8, -1}, {0, INT_MAX, 8, INT_MIN},
        {0, 0, 0, 6}, {0, 0, 8, 0}, {8, 6, 0, 0}, {0, 0, -8, 6}, {0, 0, INT_MIN, 6}, {8, 0, -1, 6}, {0, 0, -1, -1}, {0, 0, 0, 0}, {INT_MAX, 0, INT_MIN, 6}};
    for (const auto& crop : rejected) {
        RequireThrows<std::invalid_argument>([&] { cropAndScale(&result, source, crop[0], crop[1], crop[2], crop[3], 4, 4); });
        Require(result == source && source->refCount.load() == 1 && allocations.size() == before && allocationCalls == calls);
    }
    const int sizes[][2] = {{0, 4}, {4, 0}, {-4, 4}, {4, -4}, {0, 0}, {INT_MAX, 4}, {4, INT_MAX}, {INT_MAX, INT_MAX}, {INT_MAX - 1, 2}, {INT_MIN, 4}, {4, INT_MIN}, {-1, -1}, {40000, 40000}, {60000, 30000}, {46342, 46342}};
    for (const auto& size : sizes) {
        RequireThrows<std::invalid_argument>([&] { cropAndScale(&result, source, 0, 0, 8, 6, size[0], size[1]); });
        Require(result == source && source->refCount.load() == 1 && allocations.size() == before && allocationCalls == calls);
    }
    const int accepted[][4] = {{0, 0, 8, 6}, {2, 2, 6, 4}, {7, 5, 1, 1}, {0, 0, 8, -6}, {0, 2, 8, -4}, {0, 5, 8, -1}};
    for (const auto& crop : accepted) {
        Buffer* scaled = CropAndScale(source, crop[0], crop[1], crop[2], crop[3], 4, 4);
        Require(source->refCount.load() == 1);
        Release(scaled);
        Require(allocations.size() == before);
    }
    Release(source);
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        TestCopy();
        TestHalfAndQuarter();
        TestOffsetsAreMadeEven();
        TestANegativeHeightFlips();
        TestUniformPlanes();
        TestThirdAndThreeQuarters();
        TestScaledValuesStayInsideTheCrop();
        TestAGuestBufferIsConverted();
        TestTheScalerLimits();
        TestInvalid();
        Require(allocations.empty());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "VideoFrameBuffer::CropAndScale exception: %s\n", error.what());
        return 1;
    }
    std::puts("libwebrtc VideoFrameBuffer::CropAndScale tests passed");
    return 0;
}
