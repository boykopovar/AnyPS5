#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) {
    if (!value) std::abort();
}

namespace {

int callbackCount = 0;

void ReportCallback(const char*) {
    ++callbackCount;
}

bool Aligned(const void* pointer, std::size_t alignment) {
    return reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

}

int main() {
    std::array<void*, 10> heapApi{};
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    Require(__asan_report_present() == 0);
    __asan_init();
    __asan_init();
    __asan_init_v5();
    __asan_version_mismatch_check_v6();
    __asan_version_mismatch_check_v7();
    __asan_version_mismatch_check_v8();
    __asan_before_dynamic_init("guest_address_sanitizer_tests");
    __asan_after_dynamic_init();
    __asan_register_globals(nullptr, 3);
    __asan_unregister_globals(nullptr, 3);
    std::uintptr_t elfFlag = 0;
    __asan_register_elf_globals(&elfFlag, nullptr, nullptr);
    Require(elfFlag == 1);
    __asan_unregister_elf_globals(&elfFlag, nullptr, nullptr);
    Require(elfFlag == 0);
    __asan_register_image_globals(&elfFlag);
    Require(elfFlag == 1);
    __asan_unregister_image_globals(&elfFlag);
    Require(elfFlag == 0);
    __asan_handle_no_return();

    auto* block = static_cast<unsigned char*>(__asan_malloc(64));
    Require(block != nullptr);
    Require(__asan_malloc_usable_size(block) >= 64);
    Require(__sanitizer_get_allocated_size(block) >= 64);
    Require(__sanitizer_get_allocated_begin(block) == block);
    Require(__sanitizer_get_ownership(block) == 1);
    Require(__sanitizer_get_ownership(nullptr) == 0);
    Require(__sanitizer_get_estimated_allocated_size(64) >= 64);
    std::memset(block, 0x5a, 64);
    Require(block[0] == 0x5a && block[63] == 0x5a);

    auto* grown = static_cast<unsigned char*>(__asan_realloc(block, 256));
    Require(grown != nullptr);
    Require(__asan_malloc_usable_size(grown) >= 256);
    Require(grown[0] == 0x5a && grown[63] == 0x5a);
    __asan_free(grown);

    auto* cleared = static_cast<unsigned char*>(__asan_calloc(8, 16));
    Require(cleared != nullptr);
    for (std::size_t index = 0; index < 128; ++index) Require(cleared[index] == 0);
    __asan_free(cleared);

    void* aligned = nullptr;
    Require(__asan_posix_memalign(&aligned, 64, 96) == 0);
    Require(aligned != nullptr && Aligned(aligned, 64));
    __asan_free(aligned);

    void* memaligned = __asan_memalign(128, 64);
    Require(memaligned != nullptr && Aligned(memaligned, 128));
    __asan_free(memaligned);

    void* alloced = __asan_aligned_alloc(32, 96);
    Require(alloced != nullptr && Aligned(alloced, 32));
    __asan_free(alloced);

    auto* realigned = static_cast<unsigned char*>(__asan_reallocalign(nullptr, 64, 128));
    Require(realigned != nullptr && Aligned(realigned, 128));
    __asan_free(realigned);

    __asan_malloc_stats();
    __asan_print_accumulated_stats();

    std::size_t cookie = 1234;
    Require(__asan_poison_cxx_array_cookie(&cookie) == &cookie);
    Require(__asan_load_cxx_array_cookie(&cookie) == 1234);
    Require(__asan_load_cxx_array_cookie(nullptr) == 0);

    std::array<unsigned char, 16> source{};
    std::array<unsigned char, 12> destination{};
    source.fill(0x11);
    Require(__asan_memset(destination.data(), 0x22, destination.size()) == destination.data());
    Require(destination[0] == 0x22 && destination[11] == 0x22);
    Require(__asan_memcpy(destination.data(), source.data(), destination.size()) == destination.data());
    Require(destination[0] == 0x11 && destination[11] == 0x11);
    destination.fill(0x33);
    Require(__asan_memmove(destination.data(), destination.data() + 3, 9) == destination.data());
    Require(destination[0] == 0x33 && destination[8] == 0x33);

    std::uint32_t unaligned32 = 0;
    __sanitizer_unaligned_store32(&unaligned32, 0x11223344u);
    Require(__sanitizer_unaligned_load32(&unaligned32) == 0x11223344u);
    std::uint64_t unaligned64 = 0;
    __sanitizer_unaligned_store64(&unaligned64, 0x1122334455667788ull);
    Require(__sanitizer_unaligned_load64(&unaligned64) == 0x1122334455667788ull);
    std::uint16_t unaligned16 = 0;
    __sanitizer_unaligned_store16(&unaligned16, 0xabcd);
    Require(__sanitizer_unaligned_load16(&unaligned16) == 0xabcd);

    const std::uintptr_t lowAddress = reinterpret_cast<std::uintptr_t>(&cookie);
    Require(__sanitizer_ptr_cmp(&cookie, &cookie) == 0);
    Require(__sanitizer_ptr_cmp(reinterpret_cast<void*>(lowAddress - 8), &cookie) == -1);
    Require(__sanitizer_ptr_cmp(reinterpret_cast<void*>(lowAddress + 8), &cookie) == 1);
    Require(__sanitizer_ptr_sub(reinterpret_cast<void*>(lowAddress + 16), &cookie) == 16);

    const std::array<std::uint32_t, 4> untouched{0x0u, 0x0u, 0x0u, 0x0u};
    auto* mutableBlock = const_cast<std::uint32_t*>(untouched.data());
    __asan_load1(lowAddress);
    __asan_load1_noabort(lowAddress);
    __asan_load2(lowAddress);
    __asan_load2_noabort(lowAddress);
    __asan_load4(lowAddress);
    __asan_load4_noabort(lowAddress);
    __asan_load8(lowAddress);
    __asan_load8_noabort(lowAddress);
    __asan_load16(lowAddress);
    __asan_load16_noabort(lowAddress);
    __asan_loadN(lowAddress, 32);
    __asan_loadN_noabort(lowAddress, 32);
    __asan_store1(lowAddress);
    __asan_store1_noabort(lowAddress);
    __asan_store2(lowAddress);
    __asan_store2_noabort(lowAddress);
    __asan_store4(lowAddress);
    __asan_store4_noabort(lowAddress);
    __asan_store8(lowAddress);
    __asan_store8_noabort(lowAddress);
    __asan_store16(lowAddress);
    __asan_store16_noabort(lowAddress);
    __asan_storeN(lowAddress, 32);
    __asan_storeN_noabort(lowAddress, 32);
    __asan_exp_load1(lowAddress, 1);
    __asan_exp_load1_noabort(lowAddress, 1);
    __asan_exp_load2(lowAddress, 2);
    __asan_exp_load2_noabort(lowAddress, 2);
    __asan_exp_load4(lowAddress, 4);
    __asan_exp_load4_noabort(lowAddress, 4);
    __asan_exp_load8(lowAddress, 8);
    __asan_exp_load8_noabort(lowAddress, 8);
    __asan_exp_load16(lowAddress, 16);
    __asan_exp_load16_noabort(lowAddress, 16);
    __asan_exp_loadN(lowAddress, 32, 32);
    __asan_exp_loadN_noabort(lowAddress, 32, 32);
    __asan_exp_store1(lowAddress, 1);
    __asan_exp_store1_noabort(lowAddress, 1);
    __asan_exp_store2(lowAddress, 2);
    __asan_exp_store2_noabort(lowAddress, 2);
    __asan_exp_store4(lowAddress, 4);
    __asan_exp_store4_noabort(lowAddress, 4);
    __asan_exp_store8(lowAddress, 8);
    __asan_exp_store8_noabort(lowAddress, 8);
    __asan_exp_store16(lowAddress, 16);
    __asan_exp_store16_noabort(lowAddress, 16);
    __asan_exp_storeN(lowAddress, 32, 32);
    __asan_exp_storeN_noabort(lowAddress, 32, 32);
    Require(mutableBlock[0] == 0 && mutableBlock[3] == 0);

    Require(__asan_address_is_poisoned(&cookie) == 0);
    Require(__asan_region_is_poisoned(&cookie, sizeof(cookie)) == nullptr);
    Require(__asan_get_current_fake_stack() == nullptr);
    void* fakeBegin = reinterpret_cast<void*>(1);
    void* fakeEnd = reinterpret_cast<void*>(2);
    Require(__asan_addr_is_in_fake_stack(nullptr, &cookie, &fakeBegin, &fakeEnd) == nullptr);
    Require(fakeBegin == nullptr && fakeEnd == nullptr);
    __asan_poison_memory_region(&cookie, sizeof(cookie));
    __asan_unpoison_memory_region(&cookie, sizeof(cookie));
    __asan_poison_stack_memory(lowAddress, 16);
    __asan_unpoison_stack_memory(lowAddress, 16);
    __asan_alloca_poison(lowAddress, 16);
    __asan_allocas_unpoison(lowAddress, lowAddress);
    __asan_poison_intra_object_redzone(lowAddress, 16);
    __asan_unpoison_intra_object_redzone(lowAddress, 16);
    __asan_set_shadow_00(lowAddress, 16);
    __asan_set_shadow_01(lowAddress, 16);
    __asan_set_shadow_02(lowAddress, 16);
    __asan_set_shadow_03(lowAddress, 16);
    __asan_set_shadow_04(lowAddress, 16);
    __asan_set_shadow_05(lowAddress, 16);
    __asan_set_shadow_06(lowAddress, 16);
    __asan_set_shadow_07(lowAddress, 16);
    __asan_set_shadow_f1(lowAddress, 16);
    __asan_set_shadow_f2(lowAddress, 16);
    __asan_set_shadow_f3(lowAddress, 16);
    __asan_set_shadow_f5(lowAddress, 16);
    __asan_set_shadow_f8(lowAddress, 16);
    Require(__asan_stack_malloc_0(32) == nullptr);
    Require(__asan_stack_malloc_1(32) == nullptr);
    Require(__asan_stack_malloc_2(32) == nullptr);
    Require(__asan_stack_malloc_3(32) == nullptr);
    Require(__asan_stack_malloc_4(32) == nullptr);
    Require(__asan_stack_malloc_5(32) == nullptr);
    Require(__asan_stack_malloc_6(32) == nullptr);
    Require(__asan_stack_malloc_7(32) == nullptr);
    Require(__asan_stack_malloc_8(32) == nullptr);
    Require(__asan_stack_malloc_9(32) == nullptr);
    Require(__asan_stack_malloc_10(32) == nullptr);
    Require(__asan_stack_malloc_always_0(32) == nullptr);
    Require(__asan_stack_malloc_always_1(32) == nullptr);
    Require(__asan_stack_malloc_always_2(32) == nullptr);
    Require(__asan_stack_malloc_always_3(32) == nullptr);
    Require(__asan_stack_malloc_always_4(32) == nullptr);
    Require(__asan_stack_malloc_always_5(32) == nullptr);
    Require(__asan_stack_malloc_always_6(32) == nullptr);
    Require(__asan_stack_malloc_always_7(32) == nullptr);
    Require(__asan_stack_malloc_always_8(32) == nullptr);
    Require(__asan_stack_malloc_always_9(32) == nullptr);
    Require(__asan_stack_malloc_always_10(32) == nullptr);
    __asan_stack_free_0(nullptr, 32);
    __asan_stack_free_1(nullptr, 32);
    __asan_stack_free_2(nullptr, 32);
    __asan_stack_free_3(nullptr, 32);
    __asan_stack_free_4(nullptr, 32);
    __asan_stack_free_5(nullptr, 32);
    __asan_stack_free_6(nullptr, 32);
    __asan_stack_free_7(nullptr, 32);
    __asan_stack_free_8(nullptr, 32);
    __asan_stack_free_9(nullptr, 32);
    __asan_stack_free_10(nullptr, 32);

    __asan_set_error_report_callback(ReportCallback);
    __asan_report_error(reinterpret_cast<void*>(0x1000), reinterpret_cast<void*>(0x2000),
                        reinterpret_cast<void*>(0x3000), reinterpret_cast<void*>(0x4000), 1, 8);
    Require(__asan_report_present() == 1);
    Require(callbackCount == 1);
    Require(__asan_get_report_pc() == reinterpret_cast<void*>(0x1000));
    Require(__asan_get_report_bp() == reinterpret_cast<void*>(0x2000));
    Require(__asan_get_report_sp() == reinterpret_cast<void*>(0x3000));
    Require(__asan_get_report_address() == reinterpret_cast<void*>(0x4000));
    Require(__asan_get_report_access_type() == 1);
    Require(__asan_get_report_access_size() == 8);
    Require(std::strcmp(__asan_get_report_description(), "write of size 8") == 0);

    __asan_report_load4(lowAddress);
    Require(callbackCount == 2);
    Require(__asan_get_report_access_type() == 0);
    Require(__asan_get_report_access_size() == 4);
    Require(__asan_get_report_address() == reinterpret_cast<void*>(lowAddress));
    Require(std::strcmp(__asan_get_report_description(), "read of size 4") == 0);

    __asan_report_load1(lowAddress);
    __asan_report_load2(lowAddress);
    __asan_report_load8(lowAddress);
    __asan_report_load16(lowAddress);
    __asan_report_load_n(lowAddress, 12);
    __asan_report_load1_noabort(lowAddress);
    __asan_report_load2_noabort(lowAddress);
    __asan_report_load4_noabort(lowAddress);
    __asan_report_load8_noabort(lowAddress);
    __asan_report_load16_noabort(lowAddress);
    __asan_report_load_n_noabort(lowAddress, 12);
    __asan_report_store1(lowAddress);
    __asan_report_store2(lowAddress);
    __asan_report_store4(lowAddress);
    __asan_report_store8(lowAddress);
    __asan_report_store16(lowAddress);
    __asan_report_store_n(lowAddress, 12);
    __asan_report_store1_noabort(lowAddress);
    __asan_report_store2_noabort(lowAddress);
    __asan_report_store4_noabort(lowAddress);
    __asan_report_store8_noabort(lowAddress);
    __asan_report_store16_noabort(lowAddress);
    __asan_report_store_n_noabort(lowAddress, 12);
    __asan_report_exp_load1(lowAddress, 1);
    __asan_report_exp_load2(lowAddress, 2);
    __asan_report_exp_load4(lowAddress, 4);
    __asan_report_exp_load8(lowAddress, 8);
    __asan_report_exp_load16(lowAddress, 16);
    __asan_report_exp_load_n(lowAddress, 12, 12);
    __asan_report_exp_load1_noabort(lowAddress, 1);
    __asan_report_exp_load2_noabort(lowAddress, 2);
    __asan_report_exp_load4_noabort(lowAddress, 4);
    __asan_report_exp_load8_noabort(lowAddress, 8);
    __asan_report_exp_load16_noabort(lowAddress, 16);
    __asan_report_exp_load_n_noabort(lowAddress, 12, 12);
    __asan_report_exp_store1(lowAddress, 1);
    __asan_report_exp_store2(lowAddress, 2);
    __asan_report_exp_store4(lowAddress, 4);
    __asan_report_exp_store8(lowAddress, 8);
    __asan_report_exp_store16(lowAddress, 16);
    __asan_report_exp_store_n(lowAddress, 12, 12);
    __asan_report_exp_store1_noabort(lowAddress, 1);
    __asan_report_exp_store2_noabort(lowAddress, 2);
    __asan_report_exp_store4_noabort(lowAddress, 4);
    __asan_report_exp_store8_noabort(lowAddress, 8);
    __asan_report_exp_store16_noabort(lowAddress, 16);
    __asan_report_exp_store_n_noabort(lowAddress, 12, 12);
    Require(callbackCount == 49);

    std::array<char, 32> name{};
    void* regionAddress = reinterpret_cast<void*>(1);
    std::size_t regionSize = 1;
    Require(__asan_locate_address(&cookie, name.data(), name.size(), &regionAddress, &regionSize) == nullptr);
    Require(name[0] == '\0' && regionAddress == nullptr && regionSize == 0);
    __asan_describe_address(&cookie);

    std::array<std::uint32_t, 3> guards{0u, 0u, 0u};
    __sanitizer_cov_trace_pc_guard_init(guards.data(), guards.data() + guards.size());
    Require(guards[0] != 0 && guards[1] != 0 && guards[2] != 0);
    Require(guards[0] != guards[1] && guards[1] != guards[2]);
    const std::uint32_t firstGuard = guards[0];
    __sanitizer_cov_trace_pc_guard_init(guards.data(), guards.data() + guards.size());
    Require(guards[0] == firstGuard);
    __sanitizer_cov_trace_pc_guard(&guards[0]);
    __sanitizer_cov_trace_pc_guard(nullptr);
    __sanitizer_cov_dump();
    __sanitizer_dump_coverage(nullptr, 0);
    __sanitizer_dump_trace_pc_guard_coverage();
    Require(__sancov_default_options() != nullptr);

    void* fiberSave = reinterpret_cast<void*>(1);
    __sanitizer_start_switch_fiber(&fiberSave, nullptr, 0);
    Require(fiberSave == nullptr);
    const void* oldBottom = reinterpret_cast<const void*>(1);
    std::size_t oldSize = 1;
    __sanitizer_finish_switch_fiber(nullptr, &oldBottom, &oldSize);
    Require(oldBottom == nullptr && oldSize == 0);
    __sanitizer_sandbox_on_notify(nullptr);
    __sanitizer_malloc_hook(nullptr, 0);
    __sanitizer_free_hook(nullptr);
    __sanitizer_purge_allocator();
    __sanitizer_print_stack_trace();

    const char path[] = "/tmp/guest_address_sanitizer_tests";
    __sanitizer_set_report_path(path);
    Require(std::strcmp(__sanitizer_get_report_path(), path) == 0);
    __sanitizer_set_report_path(nullptr);
    Require(__sanitizer_get_report_path()[0] == '\0');
    __sanitizer_set_report_fd(nullptr);

    std::array<char, 16> symbol{};
    symbol.fill('x');
    __sanitizer_symbolize_pc(reinterpret_cast<void*>(0x1000), "%p", symbol.data(), symbol.size());
    Require(symbol[0] == '\0');
    symbol.fill('x');
    __sanitizer_symbolize_global(&cookie, "%p", symbol.data(), symbol.size());
    Require(symbol[0] == '\0');

    std::array<unsigned char, 24> container{};
    __sanitizer_annotate_contiguous_container(container.data(), container.data() + 24, container.data() + 8,
                                              container.data() + 16);
    Require(__sanitizer_verify_contiguous_container(container.data(), container.data() + 16, container.data() + 24) == 1);
    Require(__sanitizer_verify_contiguous_container(container.data() + 24, container.data(), container.data() + 16) == 0);
    Require(__sanitizer_contiguous_container_find_bad_address(container.data(), container.data() + 8,
                                                              container.data() + 24) == nullptr);

    __ubsan_handle_add_overflow(nullptr);
    __ubsan_handle_alignment_assumption(nullptr);
    __ubsan_handle_builtin_unreachable(nullptr);
    __ubsan_handle_divrem_overflow(nullptr);
    __ubsan_handle_dynamic_type_cache_miss(nullptr);
    __ubsan_handle_float_cast_overflow(nullptr);
    __ubsan_handle_function_type_mismatch(nullptr);
    __ubsan_handle_function_type_mismatch_v1(nullptr);
    __ubsan_handle_implicit_conversion(nullptr);
    __ubsan_handle_invalid_builtin(nullptr);
    __ubsan_handle_load_invalid_value(nullptr);
    __ubsan_handle_missing_return(nullptr);
    __ubsan_handle_mul_overflow(nullptr);
    __ubsan_handle_negate_overflow(nullptr);
    __ubsan_handle_nonnull_arg(nullptr);
    __ubsan_handle_nonnull_return(nullptr);
    __ubsan_handle_nonnull_return_v1(nullptr);
    __ubsan_handle_nullability_arg(nullptr);
    __ubsan_handle_nullability_return_v1(nullptr);
    __ubsan_handle_out_of_bounds(nullptr);
    __ubsan_handle_pointer_overflow(nullptr);
    __ubsan_handle_shift_out_of_bounds(nullptr);
    __ubsan_handle_sub_overflow(nullptr);
    __ubsan_handle_type_mismatch(nullptr);
    __ubsan_handle_type_mismatch_v1(nullptr);
    __ubsan_handle_vla_bound_not_positive(nullptr);
    Require(__ubsan_default_options() != nullptr);

    return 0;
}
