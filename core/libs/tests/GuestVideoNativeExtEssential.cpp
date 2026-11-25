#include "prx/libSceVideoNativeExtEssential/libSceVideoNativeExtEssential.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) { errno = 99; }

void Require(bool value) {
    if (!value) std::abort();
}

void RequireAcceptedThrowsAndZeroes(std::int32_t type, std::uint32_t profile, std::uint32_t level) {
    std::uint64_t queryOut = 0xA5A5A5A5A5A5A5A5ull;
    errno = 71;
    bool threw = false;
    try {
        libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch(type, 1920, 1080, 0, profile, level, &queryOut);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && queryOut == 0 && errno == 71);
}

constexpr int notInitialized = static_cast<int>(0xF0000000u);
constexpr int alreadyInitialized = static_cast<int>(0xF0000002u);
constexpr int invalidArgument = static_cast<int>(0xF0000003u);
constexpr int unknownType = static_cast<int>(0xF0000008u);

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    Require(sceVideoNativeExtEssential_IsInit() == 0);
    Require(sceVideoNativeExtEssential_Finalize() == 0);
    Require(sceVideoNativeExtEssential_IsInit() == 0);

    void* instance = reinterpret_cast<void*>(0x1234);
    errno = 71;
    Require(sceVideoNativeExtEssential_CreateDecInstance(nullptr, &instance) == notInitialized);
    Require(errno == 71);
    Require(instance == nullptr);
    void* existing = reinterpret_cast<void*>(0x5678);
    Require(sceVideoNativeExtEssential_DestroyDecInstance(nullptr) == notInitialized);
    Require(sceVideoNativeExtEssential_DestroyDecInstance(&existing) == notInitialized);
    Require(existing == reinterpret_cast<void*>(0x5678));

    std::array<std::uint8_t, 16> param{};
    Require(sceVideoNativeExtEssential_Initialize(nullptr) == invalidArgument);
    errno = 71;
    Require(sceVideoNativeExtEssential_Initialize(param.data()) == invalidArgument);
    Require(errno == 71);
    const std::uint64_t pointer = 0x1000;
    std::memcpy(param.data(), &pointer, sizeof(pointer));
    for (const std::uint32_t kind : std::array<std::uint32_t, 4>{0, 2, 0x100, 0xFFFFFFFFu}) {
        std::memcpy(param.data() + 8, &kind, sizeof(kind));
        errno = 71;
        Require(sceVideoNativeExtEssential_Initialize(param.data()) == invalidArgument);
        Require(errno == 71);
    }
    Require(sceVideoNativeExtEssential_IsInit() == 0);

    alignas(16) std::array<std::uint8_t, 0x58> allocator{};
    const auto allocatorPtr = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(allocator.data()));
    const std::uint32_t validKind = 1;
    std::memcpy(param.data(), &allocatorPtr, sizeof(allocatorPtr));
    std::memcpy(param.data() + 8, &validKind, sizeof(validKind));
    errno = 71;
    Require(sceVideoNativeExtEssential_Initialize(param.data()) == 0);
    Require(errno == 71);
    Require(sceVideoNativeExtEssential_IsInit() == 1);
    errno = 71;
    Require(sceVideoNativeExtEssential_Initialize(param.data()) == alreadyInitialized);
    Require(errno == 71);

    instance = reinterpret_cast<void*>(0x1234);
    errno = 71;
    Require(sceVideoNativeExtEssential_CreateDecInstance(nullptr, &instance) == unknownType);
    Require(instance == nullptr && errno == 71);
    instance = reinterpret_cast<void*>(0x1234);
    errno = 71;
    Require(sceVideoNativeExtEssential_CreateDecInstance(
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0xFFFFFFFFu)), &instance) == unknownType);
    Require(instance == nullptr && errno == 71);

    void* nullHandle = nullptr;
    errno = 71;
    Require(sceVideoNativeExtEssential_DestroyDecInstance(nullptr) == invalidArgument);
    Require(errno == 71);
    Require(sceVideoNativeExtEssential_DestroyDecInstance(&nullHandle) == invalidArgument);
    Require(errno == 71);
    Require(sceVideoNativeExtEssential_DestroyDecInstance(&existing) == invalidArgument);
    Require(existing == reinterpret_cast<void*>(0x5678) && errno == 71);

    Require(sceVideoNativeExtEssential_Finalize() == 0);
    Require(sceVideoNativeExtEssential_IsInit() == 0);
    Require(sceVideoNativeExtEssential_DestroyDecInstance(&existing) == notInitialized);

    Require(sceVideoNativeExtEssential_Initialize(param.data()) == 0);
    Require(sceVideoNativeExtEssential_IsInit() == 1);
    Require(sceVideoNativeExtEssential_Finalize() == 0);
    Require(sceVideoNativeExtEssential_IsInit() == 0);

    std::uint32_t output = 0;
    for (const std::int32_t type : std::array<std::int32_t, 6>{-1, 0, 1, 2, 3, 4}) {
        Require(libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch(type, 1, 2, 3, 4, 5, nullptr) == invalidArgument);
    }
    for (const std::int32_t type : std::array<std::int32_t, 5>{-1, 0, 1, 5, 0x7FFFFFFF}) {
        Require(libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch(type, 1, 2, 3, 4, 5, &output) == unknownType);
    }
    Require(output == 0);

    std::uint64_t queryOut = 0;
    for (const std::uint32_t badProfile : std::array<std::uint32_t, 4>{0, 3, 6, 0xFFFFu}) {
        queryOut = 0xA5A5A5A5A5A5A5A5ull;
        errno = 71;
        Require(libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch(2, 1920, 1080, 0, badProfile, 4, &queryOut) == invalidArgument);
        Require(queryOut == 0 && errno == 71);
    }
    for (const std::uint32_t goodProfile : std::array<std::uint32_t, 3>{4, 5, 0x10004u}) {
        for (const std::uint32_t badLevel : std::array<std::uint32_t, 7>{0, 3, 5, 7, 9, 11, 12}) {
            queryOut = 0xA5A5A5A5A5A5A5A5ull;
            errno = 71;
            Require(libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch(2, 1920, 1080, 0, goodProfile, badLevel, &queryOut) == invalidArgument);
            Require(queryOut == 0 && errno == 71);
        }
    }
    for (const std::uint32_t badProfile : std::array<std::uint32_t, 7>{0, 0x41, 0x4E, 0x63, 0x65, 0x7F, 0x81}) {
        queryOut = 0xA5A5A5A5A5A5A5A5ull;
        errno = 71;
        Require(libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch(4, 1920, 1080, 0, badProfile, 40, &queryOut) == invalidArgument);
        Require(queryOut == 0 && errno == 71);
    }
    for (const std::uint32_t goodProfile : std::array<std::uint32_t, 5>{0x42, 0x4D, 0x64, 0x80, 0x20064u}) {
        for (const std::uint32_t badLevel : std::array<std::uint32_t, 10>{0, 9, 14, 19, 23, 33, 43, 52, 110, 112}) {
            queryOut = 0xA5A5A5A5A5A5A5A5ull;
            errno = 71;
            Require(libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch(4, 1920, 1080, 0, goodProfile, badLevel, &queryOut) == invalidArgument);
            Require(queryOut == 0 && errno == 71);
        }
    }

    for (const std::uint32_t goodProfile : std::array<std::uint32_t, 3>{4, 5, 0x10004u}) {
        for (const std::uint32_t goodLevel : std::array<std::uint32_t, 5>{4, 6, 8, 10, 0x20004u}) {
            RequireAcceptedThrowsAndZeroes(2, goodProfile, goodLevel);
        }
    }
    for (const std::uint32_t goodProfile : std::array<std::uint32_t, 5>{0x42, 0x4D, 0x64, 0x80, 0x20064u}) {
        for (const std::uint32_t goodLevel : std::array<std::uint32_t, 17>{10, 11, 12, 13, 20, 21, 22, 30, 31, 32, 40, 41, 42, 50, 51, 111, 0x30028u}) {
            RequireAcceptedThrowsAndZeroes(4, goodProfile, goodLevel);
        }
    }
    RequireAcceptedThrowsAndZeroes(3, 0, 0);
    RequireAcceptedThrowsAndZeroes(3, 0xFFFFu, 0xFFFFu);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
