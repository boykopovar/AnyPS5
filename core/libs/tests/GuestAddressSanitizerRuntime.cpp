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

static int Heap() {
    auto* p = static_cast<unsigned char*>(__asan_malloc(13));
    if (!p) return 1;
    if (reinterpret_cast<std::uintptr_t>(p) % 8) return 2;
    for (int i = 0; i < 13; ++i) {
        if (p[i] != 0xbe) return 3;
    }
    if (__asan_address_is_poisoned(p + 12) || !__asan_address_is_poisoned(p + 13) || !__asan_address_is_poisoned(p - 1)) return 4;
    if (__sanitizer_get_allocated_size(p) != 13 || __asan_malloc_usable_size(p) != 13) return 5;
    if (!__sanitizer_get_ownership(p) || __sanitizer_get_ownership(p + 1)) return 6;
    if (__sanitizer_get_allocated_begin(p + 5) != p) return 7;
    if (__sanitizer_get_allocated_size_fast(p) != 13) return 8;
    const std::size_t before = __sanitizer_get_current_allocated_bytes();
    auto* q = static_cast<unsigned char*>(__asan_calloc(5, 7));
    for (int i = 0; i < 35; ++i) {
        if (q[i] != 0) return 9;
    }
    if (__sanitizer_get_current_allocated_bytes() != before + 35) return 10;
    if (__sanitizer_get_heap_size() == 0 || __sanitizer_get_free_bytes() > __sanitizer_get_heap_size()) return 11;
    std::memset(q, 0x42, 35);
    auto* r = static_cast<unsigned char*>(__asan_realloc(q, 100));
    if (r == q) return 12;
    for (int i = 0; i < 35; ++i) {
        if (r[i] != 0x42) return 13;
    }
    if (!__asan_address_is_poisoned(q) || __sanitizer_get_ownership(q)) return 14;
    if (__asan_address_is_poisoned(r + 99) || !__asan_address_is_poisoned(r + 100)) return 15;
    if (__asan_realloc(r, 0) != nullptr) return 16;
    if (!__asan_address_is_poisoned(r)) return 17;
    auto* grown = static_cast<unsigned char*>(__asan_realloc(nullptr, 8));
    if (!grown || __sanitizer_get_allocated_size(grown) != 8) return 18;
    __asan_free(grown);
    __asan_free(nullptr);
    auto* zero = static_cast<unsigned char*>(__asan_malloc(0));
    if (!zero || __sanitizer_get_allocated_size(zero) != 1) return 19;
    __asan_free(zero);
    if (__asan_malloc(13) == p) return 20;
    if (!__asan_update_allocation_context(p) || __asan_update_allocation_context(p + 1)) return 21;
    std::uintptr_t trace[4] = {};
    std::uint32_t tid = 99;
    if (__asan_get_alloc_stack(reinterpret_cast<std::uintptr_t>(p), trace, 4, &tid) != 1 || trace[0] == 0 || tid == 99) return 22;
    if (__asan_get_free_stack(reinterpret_cast<std::uintptr_t>(p), trace, 4, &tid) != 0) return 23;
    __asan_free(p);
    if (!__asan_address_is_poisoned(p) || __asan_get_free_stack(reinterpret_cast<std::uintptr_t>(p), trace, 4, &tid) != 1) return 24;
    if (__sanitizer_get_ownership(p)) return 25;
    __sanitizer_purge_allocator();
    if (__asan_address_is_poisoned(zero)) return 26;
    return 0;
}

static int HeapAligned() {
    void* a = __asan_memalign(64, 10);
    if (!a || reinterpret_cast<std::uintptr_t>(a) % 64) return 1;
    if (__sanitizer_get_allocated_size(a) != 10 || !__asan_address_is_poisoned(static_cast<char*>(a) + 10)) return 2;
    void* b = __asan_aligned_alloc(32, 64);
    if (!b || reinterpret_cast<std::uintptr_t>(b) % 32) return 3;
    void* c = nullptr;
    if (__asan_posix_memalign(&c, 128, 5) != 0 || !c || reinterpret_cast<std::uintptr_t>(c) % 128) return 4;
    void* d = __asan_memalign(4096, 100000);
    if (!d || reinterpret_cast<std::uintptr_t>(d) % 4096 || __asan_address_is_poisoned(static_cast<char*>(d) + 99999) || !__asan_address_is_poisoned(static_cast<char*>(d) + 100000)) return 5;
    void* e = __asan_memalign(0, 8);
    if (!e || reinterpret_cast<std::uintptr_t>(e) % 8) return 6;
    __asan_free(a);
    __asan_free(b);
    __asan_free(c);
    __asan_free(d);
    __asan_free(e);
    return 0;
}

static int HeapStats() {
    void* p = __asan_malloc(100);
    __asan_free(p);
    __asan_print_accumulated_stats();
    return 0;
}

#define FAKE_CLASS(n)                                                                  \
    {                                                                                  \
        const std::uintptr_t size = std::uintptr_t{64} << n;                           \
        const std::uintptr_t first = __asan_stack_malloc_always_##n(size);             \
        if (!first || __asan_region_is_poisoned(first, size)) return 10 + n;           \
        __asan_stack_free_##n(first, size);                                            \
        if (__asan_region_is_poisoned(first, size) != first) return 30 + n;            \
        const std::uintptr_t second = __asan_stack_malloc_always_##n(size);            \
        if (!second || second == first) return 50 + n;                                 \
        __asan_stack_free_##n(second, size);                                           \
    }

static int FakeStack() {
    if (__asan_stack_malloc_0(64) != 0 || __asan_stack_malloc_10(65536) != 0) return 1;
    if (__asan_get_current_fake_stack() != nullptr) return 2;
    if (__asan_addr_is_in_fake_stack(nullptr, buffer.data(), nullptr, nullptr) != nullptr) return 3;
    FAKE_CLASS(0)
    FAKE_CLASS(1)
    FAKE_CLASS(2)
    FAKE_CLASS(3)
    FAKE_CLASS(4)
    FAKE_CLASS(5)
    FAKE_CLASS(6)
    FAKE_CLASS(7)
    FAKE_CLASS(8)
    FAKE_CLASS(9)
    FAKE_CLASS(10)
    return 0;
}

static int NoReturn() {
    volatile char local[256];
    __asan_poison_memory_region(const_cast<char*>(local), sizeof(local));
    if (!__asan_address_is_poisoned(const_cast<char*>(local) + 10)) return 1;
    __asan_handle_no_return();
    if (__asan_region_is_poisoned(reinterpret_cast<std::uintptr_t>(local), sizeof(local))) return 2;
    return 0;
}

struct TestLocation {
    const char* file;
    int line;
    int column;
};

struct TestGlobal {
    std::uintptr_t beg;
    std::uintptr_t size;
    std::uintptr_t sizeWithRedzone;
    const char* name;
    const char* module;
    std::uintptr_t hasDynamicInit;
    const TestLocation* location;
    std::uintptr_t odrIndicator;
};

alignas(64) static unsigned char globalArea[256];
alignas(64) static unsigned char secondArea[64];
static TestLocation globalLocation = {"test.cpp", 5, 6};
static unsigned char odrIndicator;
alignas(8) static TestGlobal globalTable[2];
alignas(8) static TestGlobal secondTable[1];

static std::uintptr_t Area(const unsigned char* base, std::size_t offset = 0) {
    return reinterpret_cast<std::uintptr_t>(base) + offset;
}

static void FillGlobals(std::uintptr_t indicator) {
    globalTable[0] = {Area(globalArea), 13, 64, "g1", "test.module", 0, &globalLocation, indicator};
    globalTable[1] = {Area(globalArea, 128), 16, 32, "g2", "test.module", 0, &globalLocation, indicator ? indicator + 1 : 0};
}

static int Globals() {
    std::memset(globalArea, 0, sizeof(globalArea));
    FillGlobals(0);
    __asan_register_globals(globalTable, 2);
    const unsigned char* a = globalArea;
    if (__asan_address_is_poisoned(a + 12) || !__asan_address_is_poisoned(a + 13) || !__asan_address_is_poisoned(a + 15) || !__asan_address_is_poisoned(a + 16) || !__asan_address_is_poisoned(a + 63)) return 1;
    if (__asan_address_is_poisoned(a + 64) || __asan_address_is_poisoned(a + 128) || __asan_address_is_poisoned(a + 143) || !__asan_address_is_poisoned(a + 144) || !__asan_address_is_poisoned(a + 159) || __asan_address_is_poisoned(a + 160)) return 2;
    if (!__asan_address_is_poisoned(globalTable)) return 3;
    char name[8] = {};
    std::uintptr_t begin = 0;
    std::uintptr_t size = 0;
    const char* kind = __asan_locate_address(Area(a, 5), name, sizeof(name), &begin, &size);
    if (std::strcmp(kind, "global") || std::strcmp(name, "g1") || begin != Area(a) || size != 13) return 4;
    kind = __asan_locate_address(Area(a, 134), name, 2, &begin, &size);
    if (std::strcmp(kind, "global") || std::strcmp(name, "g") || begin != Area(a, 128) || size != 16) return 5;
    __asan_unregister_globals(globalTable, 2);
    if (__asan_region_is_poisoned(Area(a), 160) || __asan_address_is_poisoned(globalTable)) return 6;
    kind = __asan_locate_address(Area(a, 5), name, sizeof(name), &begin, &size);
    if (std::strcmp(kind, "heap-invalid") || name[0] != 0 || begin != 0 || size != 0) return 7;
    return 0;
}

static int GlobalsElf() {
    std::memset(globalArea, 0, sizeof(globalArea));
    FillGlobals(0);
    std::uintptr_t flag = 0;
    __asan_register_elf_globals(&flag, globalTable, globalTable + 2);
    if (flag != 1 || !__asan_address_is_poisoned(globalArea + 20)) return 1;
    __asan_register_elf_globals(&flag, globalTable, globalTable + 2);
    __asan_unregister_elf_globals(&flag, globalTable, globalTable + 2);
    if (flag != 0 || __asan_region_is_poisoned(Area(globalArea), 160)) return 2;
    __asan_unregister_elf_globals(&flag, globalTable, globalTable + 2);
    __asan_register_elf_globals(&flag, nullptr, nullptr);
    if (flag != 0) return 3;
    return 0;
}

static int GlobalsOdrIndicator() {
    std::memset(globalArea, 0, sizeof(globalArea));
    std::memset(secondArea, 0, sizeof(secondArea));
    odrIndicator = 0;
    globalTable[0] = {Area(globalArea), 13, 64, "older", "test.module", 0, &globalLocation, reinterpret_cast<std::uintptr_t>(&odrIndicator)};
    __asan_register_globals(globalTable, 1);
    if (odrIndicator != 1) return 1;
    secondTable[0] = {Area(secondArea), 24, 64, "newer", "other.module", 0, &globalLocation, reinterpret_cast<std::uintptr_t>(&odrIndicator)};
    __asan_register_globals(secondTable, 1);
    return 2;
}

static int GlobalsOdrPoison() {
    std::memset(globalArea, 0, sizeof(globalArea));
    globalTable[0] = {Area(globalArea), 13, 64, "older", "test.module", 0, &globalLocation, 0};
    __asan_register_globals(globalTable, 1);
    secondTable[0] = {Area(globalArea), 24, 64, "newer", "other.module", 0, &globalLocation, 0};
    __asan_register_globals(secondTable, 1);
    return 2;
}

static int GlobalsDescribe() {
    std::memset(globalArea, 0, sizeof(globalArea));
    std::memcpy(globalArea + 128, "text\0", 5);
    FillGlobals(0);
    globalTable[1].size = 5;
    __asan_register_globals(globalTable, 2);
    __asan_describe_address(Area(globalArea, 130));
    __asan_describe_address(Area(globalArea, 134));
    __asan_describe_address(Area(globalArea, 250));
    return 0;
}

static int Fibers() {
    static unsigned char fiberStack[4096];
    void* save = nullptr;
    __sanitizer_start_switch_fiber(&save, fiberStack, sizeof(fiberStack));
    const void* oldBottom = nullptr;
    std::size_t oldSize = 0;
    __sanitizer_finish_switch_fiber(save, &oldBottom, &oldSize);
    if (!oldBottom || oldSize == 0) return 1;
    __sanitizer_start_switch_fiber(nullptr, oldBottom, oldSize);
    const void* fiberBottom = nullptr;
    std::size_t fiberSize = 0;
    __sanitizer_finish_switch_fiber(nullptr, &fiberBottom, &fiberSize);
    if (fiberBottom != fiberStack || fiberSize != sizeof(fiberStack)) return 2;
    return 0;
}

static int Locate() {
    char name[16] = {};
    std::uintptr_t begin = 1;
    std::uintptr_t size = 1;
    char* p = static_cast<char*>(__asan_malloc(10));
    const char* kind = __asan_locate_address(reinterpret_cast<std::uintptr_t>(p + 2), name, sizeof(name), &begin, &size);
    if (std::strcmp(kind, "heap") || begin != reinterpret_cast<std::uintptr_t>(p) || size != 10 || name[0] != 0) return 1;
    kind = __asan_locate_address(reinterpret_cast<std::uintptr_t>(p + 12), nullptr, 0, nullptr, nullptr);
    if (std::strcmp(kind, "heap")) return 2;
    kind = __asan_locate_address(std::uintptr_t{1} << 40, name, sizeof(name), &begin, &size);
    if (std::strcmp(kind, "low shadow")) return 3;
    kind = __asan_locate_address((std::uintptr_t{1} << 40) + (std::uintptr_t{1} << 37), name, sizeof(name), &begin, &size);
    if (std::strcmp(kind, "shadow gap")) return 4;
    kind = __asan_locate_address((std::uintptr_t{1} << 40) + (std::uintptr_t{1} << 44) - 8, name, sizeof(name), &begin, &size);
    if (std::strcmp(kind, "high shadow")) return 5;
    __asan_free(p);
    return 0;
}

static int DescribeHeap() {
    char* p = static_cast<char*>(__asan_malloc(10));
    __asan_describe_address(reinterpret_cast<std::uintptr_t>(p + 3));
    __asan_describe_address(reinterpret_cast<std::uintptr_t>(p + 12));
    __asan_describe_address(reinterpret_cast<std::uintptr_t>(p - 4));
    __asan_free(p);
    __asan_describe_address(reinterpret_cast<std::uintptr_t>(p));
    __asan_describe_address(std::uintptr_t{0x1000});
    __asan_describe_address(std::uintptr_t{1} << 40);
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
    if (mode == "heap") return Heap();
    if (mode == "heap_aligned") return HeapAligned();
    if (mode == "heap_stats") return HeapStats();
    if (mode == "heap_double_free") {
        void* p = __asan_malloc(10);
        __asan_free(p);
        __asan_free(p);
        return 2;
    }
    if (mode == "heap_bad_free") {
        __asan_free(buffer.data());
        return 2;
    }
    if (mode == "heap_interior_free") {
        char* p = static_cast<char*>(__asan_malloc(32));
        __asan_free(p + 1);
        return 2;
    }
    if (mode == "heap_usable_size_unowned") {
        __asan_malloc_usable_size(buffer.data());
        return 2;
    }
    if (mode == "heap_allocated_size_unowned") {
        __sanitizer_get_allocated_size(buffer.data());
        return 2;
    }
    if (mode == "heap_calloc_overflow") {
        __asan_calloc(1ull << 40, 1ull << 40);
        return 2;
    }
    if (mode == "heap_aligned_alloc_bad") {
        __asan_aligned_alloc(24, 48);
        return 2;
    }
    if (mode == "heap_posix_memalign_bad") {
        void* p = nullptr;
        __asan_posix_memalign(&p, 12, 8);
        return 2;
    }
    if (mode == "heap_memalign_bad") {
        __asan_memalign(24, 8);
        return 2;
    }
    if (mode == "heap_too_big") {
        __asan_malloc(1ull << 41);
        return 2;
    }
    if (mode == "heap_realloc_freed") {
        void* p = __asan_malloc(10);
        __asan_free(p);
        __asan_realloc(p, 20);
        return 2;
    }
    if (mode == "fake_stack") return FakeStack();
    if (mode == "no_return") return NoReturn();
    if (mode == "globals") return Globals();
    if (mode == "globals_elf") return GlobalsElf();
    if (mode == "globals_odr_indicator") return GlobalsOdrIndicator();
    if (mode == "globals_odr_poison") return GlobalsOdrPoison();
    if (mode == "globals_describe") return GlobalsDescribe();
    if (mode == "fibers") return Fibers();
    if (mode == "locate") return Locate();
    if (mode == "describe_heap") return DescribeHeap();
    if (mode == "init") {
        __asan_init();
        __asan_poison_memory_region(buffer.data(), 16);
        return __asan_address_is_poisoned(buffer.data()) ? 0 : 1;
    }
    if (mode == "fiber_unstarted") {
        __sanitizer_finish_switch_fiber(nullptr, nullptr, nullptr);
        return 2;
    }
    if (mode == "fiber_double_start") {
        __sanitizer_start_switch_fiber(nullptr, buffer.data(), buffer.size());
        __sanitizer_start_switch_fiber(nullptr, buffer.data(), buffer.size());
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
