#include "prx/libSceAddressSanitizer/src/ShadowAddressSpace.hpp"
#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include <exception>
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <atomic>
#include <barrier>
#include <thread>
#include <array>
#include <string_view>

inline constexpr uintptr_t REGION_BASE = 1ULL << 40;
inline constexpr uintptr_t REGION_SIZE = 16ULL << 40;

alignas(8) static std::array<std::uint8_t, 32> reportBuffer{};
static std::uintptr_t reportExpected = 0;

static void APS5_VABI PrintReportCallback(const char* text) {
    std::fprintf(stderr, "callback present=%d type=%d size=%zu desc=%s address=%s text=%s\n", __asan_report_present(), __asan_get_report_access_type(),
        __asan_get_report_access_size(), __asan_get_report_description(),
        reinterpret_cast<std::uintptr_t>(__asan_get_report_address()) == reportExpected ? "ok" : "bad",
        std::string_view(text).find("Shadow byte legend") != std::string_view::npos ? "ok" : "bad");
}

static void APS5_VABI PrintDeathCallback() {
    std::fputs("death callback\n", stderr);
}

int main(int argc, char* argv[]) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (si.dwPageSize != 4096) return 1;
    if (argc > 1 && std::string_view(argv[1]) == "collision") {
        void* prep = VirtualAlloc(reinterpret_cast<void*>(REGION_BASE), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (prep != reinterpret_cast<void*>(REGION_BASE)) return 2;
        constexpr uint32_t SENTINEL = 0xDEADBEEF;
        *reinterpret_cast<volatile uint32_t*>(prep) = SENTINEL;
        if (AsanRuntime::InitializeShadow_nid_no_patch()) return 3;
        if (*reinterpret_cast<volatile uint32_t*>(prep) != SENTINEL) return 4;
        return 0;
    }
    if (argc > 2 && std::string_view(argv[1]) == "report") {
        __asan_set_error_report_callback(PrintReportCallback);
        __sanitizer_set_death_callback(PrintDeathCallback);
        const auto base = reinterpret_cast<std::uintptr_t>(reportBuffer.data());
        __asan_poison_memory_region(reportBuffer.data() + 16, 16);
        const std::string_view mode(argv[2]);
        if (mode == "load4") {
            reportExpected = base + 16;
            __asan_load4(base + 16);
        } else if (mode == "storeN") {
            reportExpected = base + 16;
            __asan_storeN(base + 8, 16);
        } else if (mode == "load16") {
            __asan_load16(base);
            reportExpected = base + 16;
            __asan_load16(base + 4);
        } else if (mode == "unaligned") {
            reportExpected = base + 16;
            __sanitizer_unaligned_store32(reportBuffer.data() + 14, 0);
        } else if (mode == "partial") {
            __asan_poison_memory_region(reportBuffer.data() + 4, 12);
            __asan_load4(base);
            __asan_load1(base + 3);
            reportExpected = base + 4;
            __asan_load1(base + 4);
        } else if (mode == "report_error") {
            reportExpected = base + 20;
            __asan_report_error(reinterpret_cast<void*>(0x1000), reinterpret_cast<void*>(0x2000), reinterpret_cast<void*>(0x3000), reinterpret_cast<void*>(base + 20), 1, 2);
        }
        return 2;
    }
    uintptr_t sharedPage = REGION_BASE + 4096 * 8;
    std::barrier sync(16);
    std::atomic<bool> threadOk{true};
    std::array<std::thread, 16> threads;
    for (int i = 0; i < 16; ++i) {
        threads[i] = std::thread([i, sharedPage, &sync, &threadOk]() {
            sync.arrive_and_wait();
            if (!AsanRuntime::InitializeShadow_nid_no_patch()) threadOk.store(false, std::memory_order_relaxed);
            sync.arrive_and_wait();
            if (!threadOk.load(std::memory_order_relaxed)) return;
            std::atomic_ref<uint32_t> ref(*reinterpret_cast<uint32_t*>(sharedPage));
            ref.fetch_add(1, std::memory_order_relaxed);
            uintptr_t threadPages = REGION_BASE + 4096 * (100 + i * 32);
            for (uintptr_t j = 0; j < 32; ++j) {
                volatile uint32_t* p = reinterpret_cast<volatile uint32_t*>(threadPages + j * 4096);
                if (*p != 0) threadOk.store(false, std::memory_order_relaxed);
                uint32_t val = static_cast<uint32_t>((i + 1) * 1000 + j + 1);
                *p = val;
                if (*p != val) threadOk.store(false, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) t.join();
    if (!threadOk.load(std::memory_order_relaxed)) return 7;
    if (*reinterpret_cast<uint32_t*>(sharedPage) != 16) return 8;
    auto* last = reinterpret_cast<volatile std::uint8_t*>(REGION_BASE + REGION_SIZE - 1);
    if (*last != 0) return 9;
    *last = 0x7a;
    if (*last != 0x7a || !AsanRuntime::InitializeShadow_nid_no_patch()) return 10;
    constexpr std::uintptr_t app = 0x30000000;
    auto* shadow = reinterpret_cast<volatile std::int8_t*>(REGION_BASE + (app >> 3));
    shadow[0] = -8;
    shadow[1] = -7;
    shadow[2] = -9;
    __asan_unpoison_stack_memory(app, 10);
    if (shadow[0] != 0 || shadow[1] != 2 || shadow[2] != -9) return 11;
    __asan_unpoison_stack_memory(app + 1, 0);
    try {
        __asan_unpoison_stack_memory(app + 1, 1);
        return 12;
    } catch (const std::exception&) {
    }
    try {
        __asan_unpoison_stack_memory(REGION_BASE, 8);
        return 13;
    } catch (const std::exception&) {
    }
    {
        __asan_version_mismatch_check_v6();
        __asan_version_mismatch_check_v7();
        __asan_version_mismatch_check_v8();

        __asan_unpoison_stack_memory(app, 24);
        __asan_poison_stack_memory(app, 16);
        if (shadow[0] != -8) return 60;
        if (shadow[1] != -8) return 61;
        if (shadow[2] != 0) return 62;

        __asan_poison_stack_memory(app, 0);
        if (shadow[0] != -8) return 63;
        if (shadow[1] != -8) return 64;
        if (shadow[2] != 0) return 65;

        __asan_unpoison_stack_memory(app, 24);
        if (shadow[0] != 0) return 66;
        if (shadow[1] != 0) return 67;
        if (shadow[2] != 0) return 68;

        try {
            __asan_poison_stack_memory(app + 1, 1);
            return 69;
        } catch (const std::exception&) {}

        try {
            __asan_poison_stack_memory((1ULL << 47) - 8, 16);
            return 70;
        } catch (const std::exception&) {}
    }
    constexpr std::uintptr_t object = 0x40000001;
    auto* objectShadow = reinterpret_cast<volatile std::int8_t*>(REGION_BASE + (object >> 3));
    objectShadow[0] = 0;
    objectShadow[1] = 0;
    objectShadow[2] = -9;
    __asan_poison_intra_object_redzone(object, 15);
    if (objectShadow[0] != 1 || objectShadow[1] != -69 || objectShadow[2] != -9) return 14;
    if (__asan_address_is_poisoned(reinterpret_cast<const volatile void*>(object - 1)) != 0) return 18;
    if (__asan_address_is_poisoned(reinterpret_cast<const volatile void*>(object)) != 1) return 19;
    if (__asan_region_is_poisoned(object - 1, 3) != object) return 20;
    if (__asan_region_is_poisoned(object + 2, 1) != object + 2) return 21;
    if (__asan_region_is_poisoned(object + 16, 0) != 0) return 22;
    __asan_unpoison_intra_object_redzone(object, 15);
    if (objectShadow[0] != 0 || objectShadow[1] != 0 || objectShadow[2] != -9) return 15;
    if (__asan_region_is_poisoned(object - 1, 16) != 0) return 23;
    try {
        __asan_region_is_poisoned(REGION_BASE - 1, 2);
        return 24;
    } catch (const std::exception&) {
    }
    try {
        __asan_poison_intra_object_redzone(object, 0);
        return 16;
    } catch (const std::exception&) {
    }
    try {
        __asan_unpoison_intra_object_redzone(object, 14);
        return 17;
    } catch (const std::exception&) {
    }
    constexpr std::uintptr_t allocaTop = 0x50000003;
    auto* allocaShadow = reinterpret_cast<volatile std::int8_t*>(REGION_BASE + (allocaTop >> 3));
    allocaShadow[0] = -8;
    allocaShadow[1] = -8;
    allocaShadow[2] = -8;
    __asan_allocas_unpoison(allocaTop, allocaTop + 20);
    if (allocaShadow[0] != 0 || allocaShadow[1] != 0 || allocaShadow[2] != -8) return 25;
    __asan_allocas_unpoison(0, allocaTop);
    __asan_allocas_unpoison(allocaTop + 1, allocaTop);
    try {
        __asan_allocas_unpoison(REGION_BASE - 8, REGION_BASE + 8);
        return 26;
    } catch (const std::exception&) {
    }
    {
        constexpr std::uintptr_t base = 0x60000000;
        auto* s = reinterpret_cast<volatile std::int8_t*>(REGION_BASE + (base >> 3));
        __asan_poison_cxx_array_cookie(base);
        if (s[0] != static_cast<std::int8_t>(0xac)) return 100;
        s[0] = 0;
        alignas(8) std::uintptr_t cookie = 0x1234;
        auto* cookieShadow = reinterpret_cast<volatile std::int8_t*>(REGION_BASE + (reinterpret_cast<std::uintptr_t>(&cookie) >> 3));
        const auto cookieSaved = *cookieShadow;
        *cookieShadow = static_cast<std::int8_t>(0xac);
        if (__asan_load_cxx_array_cookie(&cookie) != 0x1234) return 101;
        *cookieShadow = static_cast<std::int8_t>(0xfd);
        if (__asan_load_cxx_array_cookie(&cookie) != 0) return 102;
        *cookieShadow = 0;
        if (__asan_load_cxx_array_cookie(&cookie) != 0x1234) return 103;
        *cookieShadow = cookieSaved;
        try {
            __asan_poison_cxx_array_cookie(REGION_BASE);
            return 104;
        } catch (const std::exception&) {
        }
        __asan_alloca_poison(base + 32, 10);
        const std::array<std::int8_t, 12> expected{-54, -54, -54, -54, 0, 2, -53, -53, -53, -53, -53, -53};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            if (s[i] != expected[i]) return 105;
        }
        if (s[12] != 0) return 106;
        for (std::size_t i = 0; i < 13; ++i) s[i] = 0;
        __asan_alloca_poison(base + 32, 32);
        const std::array<std::int8_t, 12> exact{-54, -54, -54, -54, 0, 0, 0, 0, -53, -53, -53, -53};
        for (std::size_t i = 0; i < exact.size(); ++i) {
            if (s[i] != exact[i]) return 107;
        }
        try {
            __asan_alloca_poison(base + 33, 1);
            return 108;
        } catch (const std::exception&) {
        }
        try {
            __asan_alloca_poison(0, 8);
            return 109;
        } catch (const std::exception&) {
        }
        try {
            __asan_alloca_poison(base + 32, ~std::uintptr_t{0} - base);
            return 110;
        } catch (const std::exception&) {
        }
        for (std::size_t i = 0; i < 13; ++i) s[i] = 0;
    }
    {
        constexpr std::uintptr_t app = 0x70000000;
        volatile std::int8_t* shadow = reinterpret_cast<volatile std::int8_t*>(REGION_BASE + (app >> 3));

        shadow[0] = 0;
        shadow[1] = 0;
        shadow[2] = 0;
        shadow[3] = 0;
        shadow[4] = 0;

        __asan_poison_memory_region(reinterpret_cast<const volatile void*>(app), 32);
        if (shadow[0] != -9) return 80;
        if (shadow[1] != -9) return 81;
        if (shadow[2] != -9) return 82;
        if (shadow[3] != -9) return 83;
        if (shadow[4] != 0) return 84;

        __asan_unpoison_memory_region(reinterpret_cast<const volatile void*>(app), 32);
        if (shadow[0] != 0) return 85;
        if (shadow[1] != 0) return 86;
        if (shadow[2] != 0) return 87;
        if (shadow[3] != 0) return 88;

        __asan_poison_memory_region(reinterpret_cast<const volatile void*>(app + 3), 13);
        if (shadow[0] != 3) return 89;
        if (shadow[1] != -9) return 90;

        __asan_unpoison_memory_region(reinterpret_cast<const volatile void*>(app), 16);
        if (shadow[0] != 0) return 91;
        if (shadow[1] != 0) return 92;

        shadow[0] = 6;
        __asan_poison_memory_region(reinterpret_cast<const volatile void*>(app + 2), 4);
        if (shadow[0] != 2) return 93;

        __asan_unpoison_memory_region(reinterpret_cast<const volatile void*>(app), 5);
        if (shadow[0] != 5) return 94;

        shadow[0] = -9;
        __asan_poison_memory_region(reinterpret_cast<const volatile void*>(app), 0);
        if (shadow[0] != -9) return 95;
        __asan_unpoison_memory_region(reinterpret_cast<const volatile void*>(app), 0);
        if (shadow[0] != -9) return 96;

        try {
            __asan_poison_memory_region(reinterpret_cast<const volatile void*>(REGION_BASE), 8);
            return 97;
        } catch (const std::exception&) {
        }

        try {
            __asan_poison_memory_region(reinterpret_cast<const volatile void*>((1ULL << 47) - 4), 8);
            return 98;
        } catch (const std::exception&) {
        }

        shadow[0] = 0;
        shadow[1] = 0;
        shadow[2] = 0;
        shadow[3] = 0;
        shadow[4] = 0;
        {
            constexpr std::uintptr_t top = (1ULL << 47) - 8;
            auto* topShadow = reinterpret_cast<volatile std::int8_t*>(REGION_BASE + (top >> 3));
            __asan_poison_memory_region(reinterpret_cast<const volatile void*>(top), 8);
            if (*topShadow != -9) return 99;
            __asan_unpoison_memory_region(reinterpret_cast<const volatile void*>(top), 8);
            if (*topShadow != 0) return 79;
        }
    }
    alignas(8) std::array<std::uint8_t, 24> unaligned{};
    auto* unalignedPtr = unaligned.data() + 1;
    auto unalignedAddress = reinterpret_cast<std::uintptr_t>(unalignedPtr);
    auto* unalignedShadow = reinterpret_cast<volatile std::int8_t*>(REGION_BASE + (unalignedAddress >> 3));
    const auto shadow0 = unalignedShadow[0];
    const auto shadow1 = unalignedShadow[1];
    unalignedShadow[0] = 0;
    unalignedShadow[1] = 0;
    __sanitizer_unaligned_store64(unalignedPtr, 0x1122334455667788ull);
    if (__sanitizer_unaligned_load64(unalignedPtr) != 0x1122334455667788ull) return 27;
    __sanitizer_unaligned_store32(unalignedPtr, 0xaabbccddu);
    if (__sanitizer_unaligned_load32(unalignedPtr) != 0xaabbccddu) return 28;
    __sanitizer_unaligned_store16(unalignedPtr, 0x1234u);
    if (__sanitizer_unaligned_load16(unalignedPtr) != 0x1234u) return 29;
    {
        alignas(16) std::array<std::uint8_t, 64> clean{};
        const auto base = reinterpret_cast<std::uintptr_t>(clean.data());
        auto* cleanShadow = reinterpret_cast<volatile std::int8_t*>(REGION_BASE + (base >> 3));
        for (std::size_t i = 0; i < 8; ++i) cleanShadow[i] = 0;
        __asan_load1(base);
        __asan_load2(base);
        __asan_load4(base);
        __asan_load8(base);
        __asan_load16(base);
        __asan_store1(base);
        __asan_store2(base);
        __asan_store4(base);
        __asan_store8(base);
        __asan_store16(base);
        __asan_load1_noabort(base);
        __asan_load2_noabort(base);
        __asan_load4_noabort(base);
        __asan_load8_noabort(base);
        __asan_load16_noabort(base);
        __asan_store1_noabort(base);
        __asan_store2_noabort(base);
        __asan_store4_noabort(base);
        __asan_store8_noabort(base);
        __asan_store16_noabort(base);
        __asan_loadN(base, 64);
        __asan_storeN(base, 64);
        __asan_loadN_noabort(base, 64);
        __asan_storeN_noabort(base, 64);
        __asan_loadN(base, 0);
        cleanShadow[0] = 4;
        __asan_load4(base);
        __asan_load2(base + 2);
        __asan_store1(base + 3);
        __asan_loadN(base, 4);
        __sanitizer_unaligned_store16(clean.data() + 2, 0x1234u);
        if (__sanitizer_unaligned_load16(clean.data() + 2) != 0x1234u) return 30;
        cleanShadow[0] = 0;
        try {
            __asan_load1(REGION_BASE);
            return 31;
        } catch (const std::exception&) {
        }
        try {
            __asan_loadN(REGION_BASE - 8, 16);
            return 32;
        } catch (const std::exception&) {
        }
    }
    unalignedShadow[0] = shadow0;
    unalignedShadow[1] = shadow1;
    return 0;
}
