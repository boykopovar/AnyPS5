#include "prx/libSceVideoNativeExtEssential/libSceVideoNativeExtEssential.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int notInitialized = static_cast<int>(0xF0000000u);
constexpr int invalidArgument = static_cast<int>(0xF0000003u);
constexpr int unknownType = static_cast<int>(0xF0000008u);

}

int main() {
    Require(sceVideoNativeExtEssential_IsInit() == 0);

    void* instance = reinterpret_cast<void*>(0x1234);
    Require(sceVideoNativeExtEssential_CreateDecInstance(nullptr, &instance) == notInitialized);
    Require(instance == nullptr);
    void* existing = reinterpret_cast<void*>(0x5678);
    Require(sceVideoNativeExtEssential_DestroyDecInstance(nullptr) == notInitialized);
    Require(sceVideoNativeExtEssential_DestroyDecInstance(&existing) == notInitialized);
    Require(existing == reinterpret_cast<void*>(0x5678));

    std::array<std::uint8_t, 16> param{};
    Require(sceVideoNativeExtEssential_Initialize(nullptr) == invalidArgument);
    Require(sceVideoNativeExtEssential_Initialize(param.data()) == invalidArgument);
    const std::uint64_t pointer = 0x1000;
    std::memcpy(param.data(), &pointer, sizeof(pointer));
    for (const std::uint32_t kind : std::array<std::uint32_t, 4>{0, 2, 0x100, 0xFFFFFFFFu}) {
        std::memcpy(param.data() + 8, &kind, sizeof(kind));
        Require(sceVideoNativeExtEssential_Initialize(param.data()) == invalidArgument);
    }
    Require(sceVideoNativeExtEssential_IsInit() == 0);

    std::uint32_t output = 0;
    for (const std::int32_t type : std::array<std::int32_t, 6>{-1, 0, 1, 2, 3, 4}) {
        Require(libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch(type, 1, 2, 3, 4, 5, nullptr) == invalidArgument);
    }
    for (const std::int32_t type : std::array<std::int32_t, 5>{-1, 0, 1, 5, 0x7FFFFFFF}) {
        Require(libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch(type, 1, 2, 3, 4, 5, &output) == unknownType);
    }
    Require(output == 0);
    return 0;
}
