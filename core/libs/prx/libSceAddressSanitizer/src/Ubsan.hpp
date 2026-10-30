#pragma once

#include <cstdint>

namespace AsanRuntime::Ubsan {

using ValueHandle = std::uintptr_t;

enum class Check {
    TypeMismatch,
    AlignmentAssumption,
    AddOverflow,
    SubOverflow,
    MulOverflow,
    NegateOverflow,
    DivremOverflow,
    ShiftOutOfBounds,
    OutOfBounds,
    BuiltinUnreachable,
    MissingReturn,
    VlaBound,
    FloatCastOverflow,
    LoadInvalidValue,
    ImplicitConversion,
    InvalidBuiltin,
    NonNullReturn,
    NullabilityReturn,
    NonNullArg,
    NullabilityArg,
    PointerOverflow,
    FunctionTypeMismatch,
};

void Handle_nid_no_patch(Check check, void* data, ValueHandle first, ValueHandle second, ValueHandle third, bool unrecoverable, std::uintptr_t pc, std::uintptr_t bp);

void GetCurrentReportData_nid_no_patch(const char** issueKind, const char** message, const char** filename, unsigned* line, unsigned* column, char** memoryAddress);

}  // namespace AsanRuntime::Ubsan
