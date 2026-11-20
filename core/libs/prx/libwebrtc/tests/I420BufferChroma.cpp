#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc I420 chroma check failed at line %u\n", location.line());
    std::abort();
}

struct Buffer {
    const void* const* vtable;
    int width;
    int height;
};

const void* expectedBuffer;
int widthCalls;
int heightCalls;

int APS5_VABI Width(const void* buffer) {
    Require(buffer == expectedBuffer);
    ++widthCalls;
    return static_cast<const Buffer*>(buffer)->width;
}

int APS5_VABI Height(const void* buffer) {
    Require(buffer == expectedBuffer);
    ++heightCalls;
    return static_cast<const Buffer*>(buffer)->height;
}

template<typename TAction> void RequireInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

void Test(bool width) {
    const auto function = width ? &_ZNK6webrtc19I420BufferInterface11ChromaWidthEv_nid_postfix
                                : &_ZNK6webrtc19I420BufferInterface12ChromaHeightEv_nid_postfix;
    std::array<const void*, 7> vtable{};
    vtable[5] = reinterpret_cast<const void*>(&Width);
    vtable[6] = reinterpret_cast<const void*>(&Height);
    Buffer buffer{vtable.data(), 1919, 1081};
    expectedBuffer = &buffer;
    Require(function(&buffer) == (width ? 960 : 541));
    const std::array dimensions{0, 1, 2, 3, 1919, 1081, -1, -2, -3, std::numeric_limits<int>::min(), std::numeric_limits<int>::max() - 1};
    const std::array expected{0, 1, 1, 2, 960, 541, 0, 0, -1, -1073741823, 1073741823};
    for (std::size_t i = 0; i < dimensions.size(); ++i) {
        buffer.width = width ? dimensions[i] : 44;
        buffer.height = width ? 66 : dimensions[i];
        Require(function(&buffer) == expected[i]);
        Require(buffer.vtable == vtable.data());
        Require(buffer.width == (width ? dimensions[i] : 44));
        Require(buffer.height == (width ? 66 : dimensions[i]));
    }
    Require(widthCalls == (width ? 12 : 0));
    Require(heightCalls == (width ? 0 : 12));
    buffer.width = std::numeric_limits<int>::max();
    buffer.height = std::numeric_limits<int>::max();
    RequireInvalidArgument([&] { function(&buffer); });
    RequireInvalidArgument([&] { function(nullptr); });
    buffer.vtable = nullptr;
    RequireInvalidArgument([&] { function(&buffer); });
    buffer.vtable = vtable.data();
    vtable[width ? 5 : 6] = nullptr;
    RequireInvalidArgument([&] { function(&buffer); });
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    try {
        Test(std::string_view(argv[1]) == "width");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    std::puts("libwebrtc I420 chroma tests passed");
}
