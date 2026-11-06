#include "prx/libSceAddressSanitizer/include/Api.hpp"

#include <cstddef>
#include <cstdint>

extern "C" {

void APS5_VABI __asan_load1(std::uintptr_t) {
}

void APS5_VABI __asan_load1_noabort(std::uintptr_t) {
}

void APS5_VABI __asan_load2(std::uintptr_t) {
}

void APS5_VABI __asan_load2_noabort(std::uintptr_t) {
}

void APS5_VABI __asan_load4(std::uintptr_t) {
}

void APS5_VABI __asan_load4_noabort(std::uintptr_t) {
}

void APS5_VABI __asan_load8(std::uintptr_t) {
}

void APS5_VABI __asan_load8_noabort(std::uintptr_t) {
}

void APS5_VABI __asan_load16(std::uintptr_t) {
}

void APS5_VABI __asan_load16_noabort(std::uintptr_t) {
}

void APS5_VABI __asan_loadN(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_loadN_noabort(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_store1(std::uintptr_t) {
}

void APS5_VABI __asan_store1_noabort(std::uintptr_t) {
}

void APS5_VABI __asan_store2(std::uintptr_t) {
}

void APS5_VABI __asan_store2_noabort(std::uintptr_t) {
}

void APS5_VABI __asan_store4(std::uintptr_t) {
}

void APS5_VABI __asan_store4_noabort(std::uintptr_t) {
}

void APS5_VABI __asan_store8(std::uintptr_t) {
}

void APS5_VABI __asan_store8_noabort(std::uintptr_t) {
}

void APS5_VABI __asan_store16(std::uintptr_t) {
}

void APS5_VABI __asan_store16_noabort(std::uintptr_t) {
}

void APS5_VABI __asan_storeN(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_storeN_noabort(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_exp_load1(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_load1_noabort(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_load2(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_load2_noabort(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_load4(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_load4_noabort(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_load8(std::uintptr_t, std::uint64_t) {
}

void APS5_VABI __asan_exp_load8_noabort(std::uintptr_t, std::uint64_t) {
}

void APS5_VABI __asan_exp_load16(std::uintptr_t, std::uint64_t) {
}

void APS5_VABI __asan_exp_load16_noabort(std::uintptr_t, std::uint64_t) {
}

void APS5_VABI __asan_exp_loadN(std::uintptr_t, std::size_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_loadN_noabort(std::uintptr_t, std::size_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_store1(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_store1_noabort(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_store2(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_store2_noabort(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_store4(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_store4_noabort(std::uintptr_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_store8(std::uintptr_t, std::uint64_t) {
}

void APS5_VABI __asan_exp_store8_noabort(std::uintptr_t, std::uint64_t) {
}

void APS5_VABI __asan_exp_store16(std::uintptr_t, std::uint64_t) {
}

void APS5_VABI __asan_exp_store16_noabort(std::uintptr_t, std::uint64_t) {
}

void APS5_VABI __asan_exp_storeN(std::uintptr_t, std::size_t, std::uint32_t) {
}

void APS5_VABI __asan_exp_storeN_noabort(std::uintptr_t, std::size_t, std::uint32_t) {
}

int APS5_VABI __asan_address_is_poisoned(const volatile void*) {
    return 0;
}

void* APS5_VABI __asan_region_is_poisoned(void*, std::size_t) {
    return nullptr;
}

void APS5_VABI __asan_poison_memory_region(const volatile void*, std::size_t) {
}

void APS5_VABI __asan_unpoison_memory_region(const volatile void*, std::size_t) {
}

void APS5_VABI __asan_poison_stack_memory(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_unpoison_stack_memory(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_alloca_poison(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_allocas_unpoison(std::uintptr_t, std::uintptr_t) {
}

void APS5_VABI __asan_poison_intra_object_redzone(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_unpoison_intra_object_redzone(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_00(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_01(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_02(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_03(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_04(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_05(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_06(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_07(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_f1(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_f2(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_f3(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_f5(std::uintptr_t, std::size_t) {
}

void APS5_VABI __asan_set_shadow_f8(std::uintptr_t, std::size_t) {
}

void* APS5_VABI __asan_stack_malloc_0(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_1(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_2(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_3(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_4(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_5(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_6(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_7(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_8(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_9(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_10(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_0(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_1(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_2(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_3(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_4(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_5(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_6(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_7(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_8(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_9(std::size_t) {
    return nullptr;
}

void* APS5_VABI __asan_stack_malloc_always_10(std::size_t) {
    return nullptr;
}

void APS5_VABI __asan_stack_free_0(void*, std::size_t) {
}

void APS5_VABI __asan_stack_free_1(void*, std::size_t) {
}

void APS5_VABI __asan_stack_free_2(void*, std::size_t) {
}

void APS5_VABI __asan_stack_free_3(void*, std::size_t) {
}

void APS5_VABI __asan_stack_free_4(void*, std::size_t) {
}

void APS5_VABI __asan_stack_free_5(void*, std::size_t) {
}

void APS5_VABI __asan_stack_free_6(void*, std::size_t) {
}

void APS5_VABI __asan_stack_free_7(void*, std::size_t) {
}

void APS5_VABI __asan_stack_free_8(void*, std::size_t) {
}

void APS5_VABI __asan_stack_free_9(void*, std::size_t) {
}

void APS5_VABI __asan_stack_free_10(void*, std::size_t) {
}

void* APS5_VABI __asan_get_current_fake_stack() {
    return nullptr;
}

void* APS5_VABI __asan_addr_is_in_fake_stack(void*, void*, void** begin, void** end) {
    if (begin != nullptr) *begin = nullptr;
    if (end != nullptr) *end = nullptr;
    return nullptr;
}

}
