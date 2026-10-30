#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

alignas(16) static std::array<std::uint8_t, 128> buffer{};

static std::uintptr_t Address(std::size_t offset) {
    return reinterpret_cast<std::uintptr_t>(buffer.data()) + offset;
}

static const void* Pointer(std::size_t offset) {
    return buffer.data() + offset;
}

static void Reset() {
    __asan_unpoison_memory_region(buffer.data(), buffer.size());
    buffer.fill(0);
}

static int Poisoned(std::size_t offset) {
    return __asan_address_is_poisoned(buffer.data() + offset);
}

static int CleanAccess() {
    Reset();
    __asan_exp_load1(Address(0), 1);
    __asan_exp_load2(Address(0), 1);
    __asan_exp_load4(Address(0), 1);
    __asan_exp_load8(Address(0), 1);
    __asan_exp_load16(Address(0), 1);
    __asan_exp_store1(Address(0), 1);
    __asan_exp_store2(Address(0), 1);
    __asan_exp_store4(Address(0), 1);
    __asan_exp_store8(Address(0), 1);
    __asan_exp_store16(Address(0), 1);
    __asan_exp_load1_noabort(Address(0), 1);
    __asan_exp_load2_noabort(Address(0), 1);
    __asan_exp_load4_noabort(Address(0), 1);
    __asan_exp_load8_noabort(Address(0), 1);
    __asan_exp_load16_noabort(Address(0), 1);
    __asan_exp_store1_noabort(Address(0), 1);
    __asan_exp_store2_noabort(Address(0), 1);
    __asan_exp_store4_noabort(Address(0), 1);
    __asan_exp_store8_noabort(Address(0), 1);
    __asan_exp_store16_noabort(Address(0), 1);
    __asan_exp_loadN(Address(0), 100, 1);
    __asan_exp_storeN(Address(0), 100, 1);
    __asan_exp_loadN_noabort(Address(0), 100, 1);
    __asan_exp_storeN_noabort(Address(0), 100, 1);
    __sanitizer_ptr_cmp(buffer.data(), buffer.data() + 64);
    __sanitizer_ptr_sub(buffer.data() + 64, buffer.data());
    return 0;
}

static int Contiguous() {
    Reset();
    __sanitizer_annotate_contiguous_container(Pointer(0), Pointer(64), Pointer(64), Pointer(20));
    if (Poisoned(19) || !Poisoned(20) || !Poisoned(23) || !Poisoned(24) || !Poisoned(63) || Poisoned(64)) return 1;
    if (!__sanitizer_verify_contiguous_container(Pointer(0), Pointer(20), Pointer(64))) return 2;
    if (__sanitizer_verify_contiguous_container(Pointer(0), Pointer(30), Pointer(64))) return 3;
    if (__sanitizer_contiguous_container_find_bad_address(Pointer(0), Pointer(30), Pointer(64)) != Pointer(20)) return 4;
    if (__sanitizer_contiguous_container_find_bad_address(Pointer(0), Pointer(10), Pointer(64)) != Pointer(10)) return 5;
    __sanitizer_annotate_contiguous_container(Pointer(0), Pointer(64), Pointer(20), Pointer(50));
    if (Poisoned(49) || !Poisoned(50) || !Poisoned(63)) return 6;
    if (!__sanitizer_verify_contiguous_container(Pointer(0), Pointer(50), Pointer(64))) return 7;
    __sanitizer_annotate_contiguous_container(Pointer(0), Pointer(64), Pointer(50), Pointer(0));
    if (!Poisoned(0) || !Poisoned(63)) return 8;
    __sanitizer_annotate_contiguous_container(Pointer(0), Pointer(64), Pointer(0), Pointer(64));
    if (Poisoned(0) || Poisoned(63)) return 9;
    __sanitizer_annotate_contiguous_container(Pointer(3), Pointer(61), Pointer(61), Pointer(35));
    if (Poisoned(34) || !Poisoned(35) || !Poisoned(55) || Poisoned(56)) return 10;
    if (!__sanitizer_verify_contiguous_container(Pointer(3), Pointer(35), Pointer(61))) return 11;
    return 0;
}

static int DoubleEnded() {
    Reset();
    __sanitizer_annotate_double_ended_contiguous_container(Pointer(0), Pointer(64), Pointer(0), Pointer(64), Pointer(16), Pointer(40));
    if (!Poisoned(0) || !Poisoned(15) || Poisoned(16) || Poisoned(39) || !Poisoned(40) || !Poisoned(63)) return 1;
    if (!__sanitizer_verify_double_ended_contiguous_container(Pointer(0), Pointer(16), Pointer(40), Pointer(64))) return 2;
    if (__sanitizer_verify_double_ended_contiguous_container(Pointer(0), Pointer(8), Pointer(40), Pointer(64))) return 3;
    if (__sanitizer_double_ended_contiguous_container_find_bad_address(Pointer(0), Pointer(8), Pointer(40), Pointer(64)) != Pointer(8)) return 4;
    __sanitizer_annotate_double_ended_contiguous_container(Pointer(0), Pointer(64), Pointer(16), Pointer(40), Pointer(8), Pointer(56));
    if (!Poisoned(7) || Poisoned(8) || Poisoned(55) || !Poisoned(56)) return 5;
    __sanitizer_annotate_double_ended_contiguous_container(Pointer(0), Pointer(64), Pointer(8), Pointer(56), Pointer(24), Pointer(30));
    if (!Poisoned(23) || Poisoned(24) || Poisoned(29) || !Poisoned(30)) return 6;
    __sanitizer_annotate_double_ended_contiguous_container(Pointer(0), Pointer(64), Pointer(24), Pointer(30), Pointer(24), Pointer(30));
    if (!__sanitizer_verify_double_ended_contiguous_container(Pointer(0), Pointer(24), Pointer(30), Pointer(64))) return 7;
    return 0;
}

static int Memory() {
    Reset();
    for (std::size_t i = 0; i < 32; ++i) buffer[i] = static_cast<std::uint8_t>(i + 1);
    if (__asan_memcpy(buffer.data() + 64, buffer.data(), 32) != buffer.data() + 64) return 1;
    if (std::memcmp(buffer.data() + 64, buffer.data(), 32) != 0) return 2;
    if (__asan_memmove(buffer.data() + 4, buffer.data(), 28) != buffer.data() + 4) return 3;
    if (buffer[4] != 1 || buffer[31] != 28) return 4;
    if (__asan_memset(buffer.data() + 96, 0x5a, 16) != buffer.data() + 96) return 5;
    if (buffer[96] != 0x5a || buffer[111] != 0x5a || buffer[112] != 0) return 6;
    __asan_memcpy(buffer.data(), buffer.data(), 16);
    __asan_memset(buffer.data(), 0, 0);
    __asan_memcpy(buffer.data() + 1, buffer.data(), 0);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 100;
    const std::string_view mode(argv[1]);
    if (mode == "clean") return CleanAccess();
    if (mode == "contiguous") return Contiguous();
    if (mode == "double_ended") return DoubleEnded();
    if (mode == "memory") return Memory();
    Reset();
    __asan_poison_memory_region(buffer.data() + 16, 16);
    if (mode == "exp_load4") {
        __asan_exp_load4(Address(16), 7);
    } else if (mode == "exp_store16_noabort") {
        __asan_exp_store16_noabort(Address(8), 7);
    } else if (mode == "exp_loadN") {
        __asan_exp_loadN(Address(8), 16, 7);
    } else if (mode == "report_exp_store2") {
        __asan_report_exp_store2(Address(20), 7);
    } else if (mode == "report_exp_load_n") {
        __asan_report_exp_load_n(Address(20), 6, 7);
    } else if (mode == "memcpy_source") {
        __asan_memcpy(buffer.data() + 64, buffer.data() + 8, 16);
    } else if (mode == "memset_poison") {
        __asan_memset(buffer.data() + 8, 0, 16);
    } else if (mode == "memmove_poison") {
        __asan_memmove(buffer.data() + 20, buffer.data(), 8);
    } else if (mode == "memcpy_overlap") {
        __asan_memcpy(buffer.data() + 4, buffer.data(), 16);
    } else if (mode == "memset_negative") {
        __asan_memset(buffer.data(), 0, static_cast<std::size_t>(-1));
    } else if (mode == "container_params") {
        __sanitizer_annotate_contiguous_container(Pointer(3), Pointer(64), Pointer(40), Pointer(2));
    } else if (mode == "double_ended_params") {
        __sanitizer_annotate_double_ended_contiguous_container(Pointer(3), Pointer(64), Pointer(10), Pointer(5), Pointer(8), Pointer(12));
    }
    return 2;
}
