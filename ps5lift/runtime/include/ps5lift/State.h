#ifndef PS5LIFT_STATE_H
#define PS5LIFT_STATE_H

#include <float.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum Ps5LiftGpr {
    PS5LIFT_RAX,
    PS5LIFT_RCX,
    PS5LIFT_RDX,
    PS5LIFT_RBX,
    PS5LIFT_RSP,
    PS5LIFT_RBP,
    PS5LIFT_RSI,
    PS5LIFT_RDI,
    PS5LIFT_R8,
    PS5LIFT_R9,
    PS5LIFT_R10,
    PS5LIFT_R11,
    PS5LIFT_R12,
    PS5LIFT_R13,
    PS5LIFT_R14,
    PS5LIFT_R15,
    PS5LIFT_GPR_COUNT
};

enum Ps5LiftFlag {
    PS5LIFT_CF,
    PS5LIFT_PF,
    PS5LIFT_AF,
    PS5LIFT_ZF,
    PS5LIFT_SF,
    PS5LIFT_OF,
    PS5LIFT_DF,
    PS5LIFT_FLAG_COUNT
};

typedef struct Ps5LiftState {
    uint64_t Gpr[PS5LIFT_GPR_COUNT];
    uint64_t Rip;
    uint64_t FsBase;
    uint64_t GsBase;
    uint8_t Flags[PS5LIFT_FLAG_COUNT];
    uint8_t Padding[1];
    uint32_t Mxcsr;
    uint16_t X87Control;
    uint16_t X87Status;
    uint8_t X87Valid;
    uint8_t X87Padding[7];
    uint8_t X87Registers[8][16];
    uint8_t Vector[16][32];
    uint64_t ResumeRip;
    uint64_t ResumeRsp;
    uint8_t Unwinding;
    uint8_t UnwindPassed;
    uint8_t UnwindPadding[6];
    uint64_t ReturnSlot;
} Ps5LiftState;

enum Ps5LiftX87Operation {
    PS5LIFT_X87_LOAD,
    PS5LIFT_X87_STORE,
    PS5LIFT_X87_ADD,
    PS5LIFT_X87_SUBTRACT,
    PS5LIFT_X87_SUBTRACT_REVERSE,
    PS5LIFT_X87_MULTIPLY,
    PS5LIFT_X87_DIVIDE,
    PS5LIFT_X87_DIVIDE_REVERSE,
    PS5LIFT_X87_COMPARE,
    PS5LIFT_X87_COMPARE_UNORDERED,
    PS5LIFT_X87_COMPARE_FLAGS,
    PS5LIFT_X87_COMPARE_FLAGS_UNORDERED,
    PS5LIFT_X87_CHANGE_SIGN,
    PS5LIFT_X87_ABSOLUTE,
    PS5LIFT_X87_SQUARE_ROOT,
    PS5LIFT_X87_ROUND,
    PS5LIFT_X87_SCALE,
    PS5LIFT_X87_PARTIAL_REMAINDER,
    PS5LIFT_X87_IEEE_REMAINDER,
    PS5LIFT_X87_EXAMINE,
    PS5LIFT_X87_TEST,
    PS5LIFT_X87_EXCHANGE,
    PS5LIFT_X87_FREE,
    PS5LIFT_X87_INCREMENT_TOP,
    PS5LIFT_X87_DECREMENT_TOP,
    PS5LIFT_X87_INITIALIZE,
    PS5LIFT_X87_CLEAR_EXCEPTIONS,
    PS5LIFT_X87_WAIT,
    PS5LIFT_X87_STORE_STATUS,
    PS5LIFT_X87_STORE_CONTROL,
    PS5LIFT_X87_LOAD_CONTROL,
    PS5LIFT_X87_STORE_ENVIRONMENT,
    PS5LIFT_X87_LOAD_ENVIRONMENT,
    PS5LIFT_X87_LOAD_CONSTANT,
    PS5LIFT_X87_MOVE_REGISTER,
    PS5LIFT_X87_SAVE_STATE,
    PS5LIFT_X87_RESTORE_STATE
};

enum Ps5LiftX87Format {
    PS5LIFT_X87_REGISTER = 0,
    PS5LIFT_X87_FLOAT32 = 1,
    PS5LIFT_X87_FLOAT64 = 2,
    PS5LIFT_X87_FLOAT80 = 3,
    PS5LIFT_X87_INTEGER16 = 4,
    PS5LIFT_X87_INTEGER32 = 5,
    PS5LIFT_X87_INTEGER64 = 6,
    PS5LIFT_X87_TRUNCATE = 8,
    PS5LIFT_X87_POP = 16,
    PS5LIFT_X87_POP_TWICE = 32,
    PS5LIFT_X87_TO_REGISTER = 64
};

enum Ps5LiftX87Constant {
    PS5LIFT_X87_CONSTANT_ONE = 0,
    PS5LIFT_X87_CONSTANT_LOG2_10 = 1,
    PS5LIFT_X87_CONSTANT_LOG2_E = 2,
    PS5LIFT_X87_CONSTANT_PI = 3,
    PS5LIFT_X87_CONSTANT_LOG10_2 = 4,
    PS5LIFT_X87_CONSTANT_LN_2 = 5,
    PS5LIFT_X87_CONSTANT_ZERO = 6
};

enum Ps5LiftX87Comparison {
    PS5LIFT_X87_GREATER = 0,
    PS5LIFT_X87_LESS = 1,
    PS5LIFT_X87_EQUAL = 2,
    PS5LIFT_X87_UNORDERED = 3
};

enum Ps5LiftStringOperation {
    PS5LIFT_STRING_MOVS = 0,
    PS5LIFT_STRING_STOS = 1
};

enum Ps5LiftStringCompare {
    PS5LIFT_STRING_IMPLICIT = 1 << 8,
    PS5LIFT_STRING_SHORT_SECOND = 1 << 16,
    PS5LIFT_STRING_SHORT_FIRST = 1 << 17
};

typedef void (*Ps5LiftFunction)(Ps5LiftState*);

typedef struct Ps5LiftFunctionEntry {
    uint64_t GuestAddress;
    Ps5LiftFunction Host;
} Ps5LiftFunctionEntry;

typedef struct Ps5LiftSegment {
    uint64_t GuestAddress;
    uint64_t MemorySize;
    uint64_t FileSize;
    const uint8_t* Data;
    uint32_t Protection;
} Ps5LiftSegment;

typedef struct Ps5LiftDataImport {
    uint64_t Slot;
    void* const* Address;
    int64_t Addend;
} Ps5LiftDataImport;

typedef struct Ps5LiftImport {
    uint64_t GuestAddress;
    const char* Name;
} Ps5LiftImport;

typedef struct Ps5LiftExport {
    const char* Name;
    uint64_t Address;
} Ps5LiftExport;

typedef struct Ps5LiftImage {
    const char* Name;
    uint64_t LoadBias;
    uint64_t ProgramHeaders;
    uint64_t ProgramHeaderCount;
    const Ps5LiftSegment* Segments;
    uint64_t SegmentCount;
    uint64_t TlsModule;
    uint64_t TlsAddress;
    uint64_t TlsFileSize;
    uint64_t TlsMemorySize;
    uint64_t TlsAlignment;
    uint64_t TlsOffset;
    const Ps5LiftDataImport* DataImports;
    uint64_t DataImportCount;
    const Ps5LiftImport* Imports;
    uint64_t ImportCount;
    uint64_t Init;
    uint64_t InitArray;
    uint64_t InitCount;
    const Ps5LiftExport* Exports;
    uint64_t ExportCount;
} Ps5LiftImage;

typedef struct Ps5LiftProgram {
    const Ps5LiftImage* Images;
    uint64_t ImageCount;
    const uint64_t* Initialization;
    uint64_t InitializationCount;
    const Ps5LiftFunctionEntry* Functions;
    uint64_t FunctionCount;
    uint64_t Entry;
} Ps5LiftProgram;

void ps5lift_use_program(const Ps5LiftProgram* program);

void ps5lift_dispatch(Ps5LiftState* state, uint64_t target);
Ps5LiftFunction ps5lift_resolve(uint64_t target);
Ps5LiftFunction ps5lift_resolve_jump(Ps5LiftState* state, uint64_t target);
[[noreturn]] void ps5lift_fail_unresumed(const Ps5LiftState* state, const char* frames);
#define PS5LIFT_MXCSR_MASK 0xffffu

void ps5lift_apply_mxcsr(Ps5LiftState* state);
uint32_t ps5lift_read_mxcsr(Ps5LiftState* state);
uint32_t ps5lift_x87(Ps5LiftState* state, uint32_t operation, uint32_t format, uint64_t address, uint32_t index);
void ps5lift_extended_operation(Ps5LiftState* state, uint32_t operation, const void* left, const void* right, void* result);
void ps5lift_extended_load(Ps5LiftState* state, uint32_t format, const void* source, void* result);
void ps5lift_extended_store(Ps5LiftState* state, uint32_t format, const void* value, void* destination);
#if LDBL_MANT_DIG == 64 || LDBL_MANT_DIG == 113
#define PS5LIFT_HOST_LONG_DOUBLE_HOLDS_EXTENDED
void ps5lift_extended_to_host(const void* extended, long double* host);
void ps5lift_extended_from_host(const Ps5LiftState* state, const long double* host, void* extended);
#endif
Ps5LiftState* ps5lift_current_state(void);
void ps5lift_use_guest_stack(uint64_t stack_top);
uint64_t ps5lift_interrupt_stack(void);
uint64_t ps5lift_host_stack_size(void);
Ps5LiftState* ps5lift_enter_guest(void);
void ps5lift_leave_guest(void);
void ps5lift_run_main(void);
uint64_t ps5lift_read_tsc(void);
uint64_t ps5lift_random(void);
uint32_t ps5lift_processor_id(void);
void ps5lift_flush_cache_line(uint64_t address);
void ps5lift_write_back_cache_line(uint64_t address);
void ps5lift_cpuid(Ps5LiftState* state, uint64_t address);
uint64_t ps5lift_xgetbv(uint64_t address, uint32_t index);
[[noreturn, gnu::format(printf, 1, 2)]] void ps5lift_fail(const char* format, ...);
void ps5lift_unsupported(Ps5LiftState* state, uint64_t address, const char* mnemonic);
void ps5lift_unknown_resume(Ps5LiftState* state, uint64_t function, uint64_t resume);
void ps5lift_trap(Ps5LiftState* state, uint64_t address);
void ps5lift_left_jump_table(Ps5LiftState* state, uint64_t address);
void ps5lift_interrupt(Ps5LiftState* state, uint64_t address, uint32_t vector);
void ps5lift_divide_error(Ps5LiftState* state, uint64_t address);
void ps5lift_string_op(Ps5LiftState* state, uint32_t operation, uint32_t width, uint32_t repeat, uint64_t source_base);
uint32_t ps5lift_string_compare(uint64_t first_low, uint64_t first_high, uint64_t second_low, uint64_t second_high, int64_t first_length, int64_t second_length, uint32_t control);

#ifdef __cplusplus
}
#endif

#endif
