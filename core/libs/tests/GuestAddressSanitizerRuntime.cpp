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

struct Type {
    std::uint16_t kind;
    std::uint16_t info;
    char name[16];
};

static const Type signedInt = {0, 11, "'int'"};
static const Type unsignedInt = {0, 10, "'unsigned int'"};
static const Type boolType = {0, 6, "'bool'"};
static const Type unsignedChar = {0, 6, "'unsigned char'"};
static const Type doubleType = {1, 64, "'double'"};
static const Type intArray = {0xffff, 0, "'int [4]'"};
static const Type charPointer = {0xffff, 0, "'char *'"};
static const Type functionPointer = {0xffff, 0, "'void (*)(int)'"};

struct Location {
    const char* file;
    std::uint32_t line;
    std::uint32_t column;
};

struct OverflowData {
    Location loc;
    const Type* type;
};

struct ShiftData {
    Location loc;
    const Type* lhs;
    const Type* rhs;
};

struct TypeMismatchData {
    Location loc;
    const Type* type;
    unsigned char logAlignment;
    unsigned char kind;
};

struct NonNullArgData {
    Location loc;
    Location attrLoc;
    int index;
};

struct ConversionData {
    Location loc;
    const Type* from;
    const Type* to;
    unsigned char kind;
};

struct FloatCastData {
    Location loc;
    const Type* from;
    const Type* to;
};

struct BuiltinData {
    Location loc;
    unsigned char kind;
};

struct AlignmentData {
    Location loc;
    Location assumptionLoc;
    const Type* type;
};

struct NonNullReturnData {
    Location attrLoc;
};

struct FunctionData {
    Location loc;
    const Type* type;
};

static void APS5_VABI PrintDeathCallback() {
    std::fputs("death callback\n", stderr);
}

static std::uintptr_t Word(std::int64_t value) {
    return static_cast<std::uintptr_t>(value);
}

static void Sample() {
}

static int UbsanArithmetic() {
    static OverflowData add = {{"test.cpp", 10, 5}, &signedInt};
    static OverflowData sub = {{"test.cpp", 11, 6}, &unsignedInt};
    static OverflowData mul = {{"test.cpp", 12, 7}, &signedInt};
    static OverflowData negate = {{"test.cpp", 13, 8}, &signedInt};
    static OverflowData divZero = {{"test.cpp", 14, 9}, &signedInt};
    static OverflowData divMinus = {{"test.cpp", 15, 10}, &signedInt};
    static ShiftData shiftLarge = {{"test.cpp", 16, 11}, &signedInt, &signedInt};
    static ShiftData shiftNegative = {{"test.cpp", 17, 12}, &signedInt, &signedInt};
    static ShiftData leftNegative = {{"test.cpp", 18, 13}, &signedInt, &signedInt};
    static ShiftData leftBase = {{"test.cpp", 19, 14}, &signedInt, &signedInt};
    static struct {
        Location loc;
        const Type* array;
        const Type* index;
    } bounds = {{"test.cpp", 20, 15}, &intArray, &signedInt};
    static OverflowData vla = {{"test.cpp", 21, 16}, &signedInt};
    __ubsan_handle_add_overflow(&add, 2147483647, 1);
    __ubsan_handle_sub_overflow(&sub, 0, 1);
    __ubsan_handle_mul_overflow(&mul, 65536, 65536);
    __ubsan_handle_negate_overflow(&negate, 0x80000000u);
    __ubsan_handle_divrem_overflow(&divZero, 5, 0);
    __ubsan_handle_divrem_overflow(&divMinus, 0x80000000u, 0xffffffffu);
    __ubsan_handle_shift_out_of_bounds(&shiftLarge, 1, 40);
    __ubsan_handle_shift_out_of_bounds(&shiftNegative, 1, 0xffffffffu);
    __ubsan_handle_shift_out_of_bounds(&leftNegative, 0xffffffffu, 3);
    __ubsan_handle_shift_out_of_bounds(&leftBase, 1073741824, 2);
    __ubsan_handle_out_of_bounds(&bounds, 4);
    __ubsan_handle_vla_bound_not_positive(&vla, 0xfffffffdu);
    return 0;
}

static int UbsanMisc() {
    static TypeMismatchData nullLoad = {{"test.cpp", 30, 3}, &signedInt, 2, 0};
    static TypeMismatchData misaligned = {{"test.cpp", 31, 4}, &signedInt, 2, 1};
    static TypeMismatchData insufficient = {{"test.cpp", 32, 5}, &signedInt, 2, 0};
    static NonNullArgData nonNull = {{"test.cpp", 33, 6}, {"decl.h", 5, 1}, 2};
    static NonNullArgData nullability = {{"test.cpp", 34, 7}, {"decl.h", 6, 2}, 1};
    static OverflowData pointerA = {{"test.cpp", 35, 8}, nullptr};
    static OverflowData pointerB = {{"test.cpp", 36, 9}, nullptr};
    static OverflowData pointerC = {{"test.cpp", 37, 10}, nullptr};
    static OverflowData pointerD = {{"test.cpp", 38, 11}, nullptr};
    static OverflowData invalid = {{"test.cpp", 39, 12}, &boolType};
    static ConversionData conversion = {{"test.cpp", 40, 13}, &signedInt, &unsignedChar, 1};
    static FloatCastData floatCast = {{"test.cpp", 41, 14}, &doubleType, &signedInt};
    static BuiltinData builtin = {{"test.cpp", 42, 15}, 0};
    static AlignmentData alignment = {{"test.cpp", 43, 16}, {"test.cpp", 44, 17}, &charPointer};
    static NonNullReturnData returnData = {{"decl.h", 9, 2}};
    static Location returnLocation = {"test.cpp", 45, 18};
    static FunctionData function = {{"test.cpp", 46, 19}, &functionPointer};
    const auto base = Address(16);
    __ubsan_handle_type_mismatch_v1(&nullLoad, 0);
    __ubsan_handle_type_mismatch_v1(&misaligned, base + 1);
    __ubsan_handle_type_mismatch_v1(&insufficient, base);
    __ubsan_handle_nonnull_arg(&nonNull);
    __ubsan_handle_nullability_arg(&nullability);
    __ubsan_handle_pointer_overflow(&pointerA, 0, 0);
    __ubsan_handle_pointer_overflow(&pointerB, 0, 16);
    __ubsan_handle_pointer_overflow(&pointerC, 0x1000, 0);
    __ubsan_handle_pointer_overflow(&pointerD, 0x1000, 0x800);
    __ubsan_handle_load_invalid_value(&invalid, 5);
    __ubsan_handle_implicit_conversion(&conversion, 300, 44);
    double huge = 1e10;
    std::uint64_t bits;
    std::memcpy(&bits, &huge, sizeof(bits));
    __ubsan_handle_float_cast_overflow(&floatCast, bits);
    __ubsan_handle_invalid_builtin(&builtin);
    __ubsan_handle_alignment_assumption(&alignment, base + 3, 8, 0);
    __ubsan_handle_nullability_return_v1(&returnData, &returnLocation);
    __ubsan_handle_function_type_mismatch(&function, reinterpret_cast<std::uintptr_t>(&Sample));
    return 0;
}

static int UbsanData() {
    static OverflowData add = {{"test.cpp", 50, 4}, &signedInt};
    __ubsan_handle_add_overflow(&add, Word(2147483647), 1);
    const char* kind = nullptr;
    const char* message = nullptr;
    const char* file = nullptr;
    unsigned line = 0;
    unsigned column = 0;
    char* memory = reinterpret_cast<char*>(1);
    __ubsan_get_current_report_data(&kind, &message, &file, &line, &column, &memory);
    std::printf("data kind=%s file=%s line=%u column=%u memory=%p message=%s\n", kind, file, line, column, static_cast<void*>(memory), message);
    return 0;
}

static int UbsanDedup() {
    static OverflowData add = {{"test.cpp", 60, 4}, &signedInt};
    const char* capture = "ubsan-dedup.log";
    std::freopen(capture, "w", stderr);
    __ubsan_handle_add_overflow(&add, 2147483647, 1);
    __ubsan_handle_add_overflow(&add, 2147483647, 1);
    __ubsan_handle_add_overflow(&add, 2147483647, 1);
    std::fclose(stderr);
    std::FILE* file = std::fopen(capture, "r");
    if (file == nullptr) return 1;
    int reports = 0;
    char line[512];
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        if (std::strstr(line, "runtime error") != nullptr) ++reports;
    }
    std::fclose(file);
    std::remove(capture);
    std::printf("dedup count=%d\n", reports);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 100;
    const std::string_view mode(argv[1]);
    if (mode == "ubsan_arithmetic") return UbsanArithmetic();
    if (mode == "ubsan_misc") return UbsanMisc();
    if (mode == "ubsan_data") return UbsanData();
    if (mode == "ubsan_dedup") return UbsanDedup();
    if (mode == "ubsan_abort") {
        static OverflowData sub = {{"test.cpp", 70, 2}, &unsignedInt};
        __sanitizer_set_death_callback(PrintDeathCallback);
        __ubsan_handle_sub_overflow_abort(&sub, 0, 1);
        return 2;
    }
    if (mode == "ubsan_unreachable") {
        static Location unreachable = {"test.cpp", 71, 3};
        __sanitizer_set_death_callback(PrintDeathCallback);
        __ubsan_handle_builtin_unreachable(&unreachable);
        return 2;
    }
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
