#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

std::unordered_map<void*, std::size_t> allocations;
std::size_t allocated = 0;
std::size_t freed = 0;
std::size_t failAllocationOfSize = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc I420Buffer::Create check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failAllocationOfSize && size == failAllocationOfSize) {
        failAllocationOfSize = 0;
        return nullptr;
    }
    auto* pointer = std::malloc(size);
    if (pointer) {
        allocations.emplace(pointer, size);
        ++allocated;
        std::memset(pointer, 0xa5, size);
    }
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    ++freed;
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

template<typename TAction>
void RequireRuntimeError(TAction action, const char* function, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::strstr(error.what(), function) != nullptr, location);
        return;
    }
    Require(false, location);
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

template<typename TResult, std::size_t TSlot, typename... TArguments>
TResult* CallIndirect(const void* object, TResult* result, TArguments... arguments) {
    using Function = TResult*(APS5_VABI*)(TResult*, const void*, TArguments...);
    return reinterpret_cast<Function>(const_cast<void*>(Entry(object, TSlot)))(result, object, arguments...);
}

WebRtcRefCountedI420Buffer* Create(int width, int height, int strideY, int strideU, int strideV) {
    WebRtcRefCountedI420Buffer* buffer = nullptr;
    Require(_ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, width, height, strideY, strideU, strideV) == &buffer);
    Require(buffer != nullptr);
    return buffer;
}

void* AllocationStart(const std::uint8_t* data) {
    std::uintptr_t start;
    std::memcpy(&start, data - sizeof(start), sizeof(start));
    return reinterpret_cast<void*>(start);
}

void RequireLayout(int width, int height, int strideY, int strideU, int strideV) {
    const std::size_t before = allocations.size();
    auto* buffer = Create(width, height, strideY, strideU, strideV);
    Require(allocations.size() == before + 2);
    Require(allocations.at(buffer) == sizeof(WebRtcRefCountedI420Buffer));
    const auto chromaHeight = (static_cast<std::int64_t>(height) + 1) / 2;
    const auto size = static_cast<std::size_t>(static_cast<std::int64_t>(strideY) * height + (static_cast<std::int64_t>(strideU) + strideV) * chromaHeight);
    auto* data = buffer->buffer.data;
    Require(reinterpret_cast<std::uintptr_t>(data) % 64 == 0);
    auto* start = static_cast<std::uint8_t*>(AllocationStart(data));
    Require(allocations.at(start) == size + sizeof(std::uintptr_t) + 63);
    Require(reinterpret_cast<std::uintptr_t>(data) == ((reinterpret_cast<std::uintptr_t>(start) + sizeof(std::uintptr_t) + 63) & ~std::uintptr_t {63}));
    Require(buffer->buffer.width == width && buffer->buffer.height == height);
    Require(buffer->buffer.strideY == strideY && buffer->buffer.strideU == strideU && buffer->buffer.strideV == strideV);
    Require(buffer->refCount.load() == 1);
    Require(Call<std::int32_t, 4>(buffer) == 1);
    Require(Call<int, 5>(buffer) == width && Call<int, 6>(buffer) == height);
    Require(Call<int, 12>(buffer) == (width + 1) / 2 && Call<int, 13>(buffer) == (height + 1) / 2);
    Require(Call<int, 14>(buffer) == strideY && Call<int, 15>(buffer) == strideU && Call<int, 16>(buffer) == strideV);
    Require(Call<const std::uint8_t*, 17>(buffer) == data);
    Require(Call<const std::uint8_t*, 18>(buffer) == data + strideY * height);
    Require(Call<const std::uint8_t*, 19>(buffer) == data + strideY * height + strideU * chromaHeight);
    auto* plain = &buffer->buffer;
    Require(_ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(plain) == data);
    Require(_ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(plain) == data + strideY * height);
    Require(_ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(plain) == data + strideY * height + strideU * chromaHeight);
    std::memset(data, 0x5a, size);
    Require(Call<std::int32_t, 1>(buffer) == 0);
    Require(allocations.size() == before);
}

void TestCreate() {
    RequireLayout(640, 480, 640, 320, 320);
    RequireLayout(1, 1, 1, 1, 1);
    RequireLayout(5, 3, 7, 4, 5);
    RequireLayout(1921, 1081, 1984, 960, 1024);
    RequireLayout(7, 9, 7, 1, 2);
}

void TestVirtuals() {
    auto* buffer = Create(6, 5, 8, 3, 4);
    Require(Call<const void*, 8>(buffer) == buffer);
    Require(Call<bool, 20>(buffer));
    void* converted = nullptr;
    Require(CallIndirect<void*, 7>(buffer, &converted) == &converted && converted == buffer);
    Require(buffer->refCount.load() == 2 && !Call<bool, 20>(buffer));
    Require(Call<std::int32_t, 1>(buffer) == 1);
    Require(Call<bool, 20>(buffer));
    void* mapped = nullptr;
    const std::int32_t types[] = {1};
    RequireRuntimeError([&] { CallIndirect<void*, 10>(buffer, &mapped, WebRtcVideoFrameBufferTypes{types, 1}); }, "GetMappedFrameBuffer");
    void* cropped = nullptr;
    RequireRuntimeError([&] { CallIndirect<void*, 9>(buffer, &cropped, 0, 0, 6, 5, 3, 2); }, "CropAndScale");
    Require(mapped == nullptr && cropped == nullptr && buffer->refCount.load() == 1);
    WebRtcGuestString representation;
    std::memset(&representation, 0xcc, sizeof(representation));
    Require(CallIndirect<WebRtcGuestString, 11>(buffer, &representation) == &representation);
    Require(representation.reserved == 0 && representation.size == 1 && representation.capacity == WebRtcGuestStringInlineCapacity);
    Require(std::strcmp(WebRtcGuestStringData(representation), "?") == 0);
    Call<void, 0>(buffer);
    Call<void, 0>(buffer);
    Require(buffer->refCount.load() == 3);
    Require(Call<std::int32_t, 1>(buffer) == 1);
    Require(Call<std::int32_t, 1>(buffer) == 1);
    Require(Call<std::int32_t, 1>(buffer) == 0);
    Require(allocations.empty());

    buffer = Create(4, 4, 4, 2, 2);
    Call<void, 2>(buffer);
    Require(allocations.size() == 1 && allocations.count(buffer) == 1);
    _ZdlPv_nid_postfix(buffer);
    buffer = Create(4, 4, 4, 2, 2);
    Call<void, 3>(buffer);
    Require(allocations.empty());
}

void TestInvalid() {
    constexpr int max = std::numeric_limits<int>::max();
    WebRtcRefCountedI420Buffer* buffer = nullptr;
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(nullptr, 2, 2, 2, 1, 1); });
    const int cases[][5] = {
        {0, 2, 2, 1, 1}, {-1, 2, 2, 1, 1}, {2, 0, 2, 1, 1}, {2, -3, 2, 1, 1}, {4, 2, 3, 2, 2},
        {2, 2, 2, 0, 1}, {2, 2, 2, 1, 0}, {2, 2, 2, -1, 1}, {1, 1, max - 1, 1, 1}, {1, 2, max / 2, 1, 0},
        {46341, 46341, 46341, 1, 1}, {1, max, 1, 1, 1},
    };
    for (const auto& c : cases) {
        RequireThrows<std::runtime_error>([&] { _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, c[0], c[1], c[2], c[3], c[4]); });
        Require(buffer == nullptr && allocations.empty());
    }
    failAllocationOfSize = 2 * 2 + 2 + sizeof(std::uintptr_t) + 63;
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(&buffer, 2, 2, 2, 1, 1); });
    Require(buffer == nullptr && allocations.empty() && failAllocationOfSize == 0);
    buffer = Create(3, 3, 3, 1, 1);
    Require(Call<int, 12>(buffer) == 2 && Call<const std::uint8_t*, 19>(buffer) == buffer->buffer.data + 9 + 2);
    Require(Call<std::int32_t, 1>(buffer) == 0);
}

}

int main(int argc, char** argv) {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        Require(argc == 2);
        if (std::strcmp(argv[1], "create") == 0) TestCreate();
        else if (std::strcmp(argv[1], "virtuals") == 0) TestVirtuals();
        else if (std::strcmp(argv[1], "invalid") == 0) TestInvalid();
        else Require(false);
        Require(allocations.empty() && allocated == freed);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "I420Buffer::Create exception: %s\n", error.what());
        return 1;
    }
}
