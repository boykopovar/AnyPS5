#include "SceTypes.hpp"
#include "prx/libSceAddressSanitizer/src/Ubsan.hpp"
#include "prx/libSceAddressSanitizer/src/Report.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace AsanRuntime::Ubsan {

namespace {

using SIntMax = __int128;
using UIntMax = unsigned __int128;

[[noreturn]] void Unexpected(const char* what) {
    throw std::runtime_error(what);
}

struct SourceLocation {
    const char* filename;
    std::uint32_t line;
    std::uint32_t column;

    bool Invalid() const { return filename == nullptr; }
    bool Disabled() const { return column == ~std::uint32_t(0); }
    SourceLocation Acquire() {
        const auto oldColumn = std::atomic_ref<std::uint32_t>(column).exchange(~std::uint32_t(0), std::memory_order_relaxed);
        return {filename, line, oldColumn};
    }
};

struct TypeDescriptor {
    std::uint16_t kind;
    std::uint16_t info;
    char name[1];

    bool Integer() const { return kind == 0; }
    bool Signed() const { return Integer() && (info & 1) != 0; }
    bool Unsigned() const { return Integer() && (info & 1) == 0; }
    unsigned IntegerBits() const {
        if (!Integer()) {
            Unexpected("ubsan: integer width of a non-integer type");
        }
        return 1u << (info >> 1);
    }
    bool Float() const { return kind == 1; }
    unsigned FloatBits() const {
        if (!Float()) {
            Unexpected("ubsan: float width of a non-float type");
        }
        return info;
    }
};

struct TypeMismatchData {
    SourceLocation loc;
    const TypeDescriptor* type;
    unsigned char logAlignment;
    unsigned char typeCheckKind;
};

struct AlignmentAssumptionData {
    SourceLocation loc;
    SourceLocation assumptionLoc;
    const TypeDescriptor* type;
};

struct OverflowData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct ShiftOutOfBoundsData {
    SourceLocation loc;
    const TypeDescriptor* lhsType;
    const TypeDescriptor* rhsType;
};

struct OutOfBoundsData {
    SourceLocation loc;
    const TypeDescriptor* arrayType;
    const TypeDescriptor* indexType;
};

struct UnreachableData {
    SourceLocation loc;
};

struct VlaBoundData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct FloatCastOverflowData {
    const TypeDescriptor* fromType;
    const TypeDescriptor* toType;
};

struct FloatCastOverflowDataV2 {
    SourceLocation loc;
    const TypeDescriptor* fromType;
    const TypeDescriptor* toType;
};

struct InvalidValueData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

struct ImplicitConversionData {
    SourceLocation loc;
    const TypeDescriptor* fromType;
    const TypeDescriptor* toType;
    unsigned char kind;
};

struct InvalidBuiltinData {
    SourceLocation loc;
    unsigned char kind;
};

struct NonNullReturnData {
    SourceLocation attrLoc;
};

struct NonNullArgData {
    SourceLocation loc;
    SourceLocation attrLoc;
    int argIndex;
};

struct PointerOverflowData {
    SourceLocation loc;
};

struct FunctionTypeMismatchData {
    SourceLocation loc;
    const TypeDescriptor* type;
};

class Value {
public:
    Value(const TypeDescriptor& type, ValueHandle handle) : type_(type), handle_(handle) {}

    const TypeDescriptor& Type() const { return type_; }

    SIntMax SignedValue() const {
        if (!type_.Signed()) {
            Unexpected("ubsan: signed value of a non-signed type");
        }
        if (InlineInteger()) {
            const unsigned extraBits = sizeof(SIntMax) * 8 - type_.IntegerBits();
            return static_cast<SIntMax>(static_cast<UIntMax>(handle_) << extraBits) >> extraBits;
        }
        if (type_.IntegerBits() == 128) {
            return *reinterpret_cast<const SIntMax*>(handle_);
        }
        Unexpected("ubsan: unexpected integer bit width");
    }

    UIntMax UnsignedValue() const {
        if (!type_.Unsigned()) {
            Unexpected("ubsan: unsigned value of a non-unsigned type");
        }
        if (InlineInteger()) {
            return handle_;
        }
        if (type_.IntegerBits() == 128) {
            return *reinterpret_cast<const UIntMax*>(handle_);
        }
        Unexpected("ubsan: unexpected integer bit width");
    }

    UIntMax PositiveValue() const {
        if (type_.Unsigned()) {
            return UnsignedValue();
        }
        const auto value = SignedValue();
        if (value < 0) {
            Unexpected("ubsan: negative value where a positive one is required");
        }
        return static_cast<UIntMax>(value);
    }

    bool MinusOne() const { return type_.Signed() && SignedValue() == -1; }
    bool Negative() const { return type_.Signed() && SignedValue() < 0; }

    long double FloatValue() const {
        if (!type_.Float()) {
            Unexpected("ubsan: float value of a non-float type");
        }
        const unsigned bits = type_.FloatBits();
        if (bits <= sizeof(ValueHandle) * 8) {
            if (bits == 32) {
                float value;
                std::memcpy(&value, &handle_, 4);
                return value;
            }
            if (bits == 64) {
                double value;
                std::memcpy(&value, &handle_, 8);
                return value;
            }
        } else {
            switch (bits) {
            case 64:
                return *reinterpret_cast<const double*>(handle_);
            case 80:
            case 96:
            case 128:
                return *reinterpret_cast<const long double*>(handle_);
            }
        }
        Unexpected("ubsan: unexpected floating point bit width");
    }

private:
    bool InlineInteger() const { return type_.IntegerBits() <= sizeof(ValueHandle) * 8; }

    const TypeDescriptor& type_;
    ValueHandle handle_;
};

enum class ErrorType {
    GenericUB,
    NullPointerUse,
    NullPointerUseWithNullability,
    NullptrWithOffset,
    NullptrWithNonZeroOffset,
    NullptrAfterNonZeroOffset,
    PointerOverflow,
    MisalignedPointerUse,
    AlignmentAssumption,
    InsufficientObjectSize,
    SignedIntegerOverflow,
    UnsignedIntegerOverflow,
    IntegerDivideByZero,
    FloatDivideByZero,
    InvalidBuiltin,
    ImplicitUnsignedIntegerTruncation,
    ImplicitSignedIntegerTruncation,
    ImplicitIntegerSignChange,
    ImplicitSignedIntegerTruncationOrSignChange,
    InvalidShiftBase,
    InvalidShiftExponent,
    OutOfBoundsIndex,
    UnreachableCall,
    MissingReturn,
    NonPositiveVLAIndex,
    FloatCastOverflow,
    InvalidBoolLoad,
    InvalidEnumLoad,
    FunctionTypeMismatch,
    InvalidNullReturn,
    InvalidNullReturnWithNullability,
    InvalidNullArgument,
    InvalidNullArgumentWithNullability,
};

const char* SummaryKind(ErrorType type) {
    switch (type) {
    case ErrorType::GenericUB: return "undefined-behavior";
    case ErrorType::NullPointerUse:
    case ErrorType::NullPointerUseWithNullability: return "null-pointer-use";
    case ErrorType::NullptrWithOffset: return "nullptr-with-offset";
    case ErrorType::NullptrWithNonZeroOffset: return "nullptr-with-nonzero-offset";
    case ErrorType::NullptrAfterNonZeroOffset: return "nullptr-after-nonzero-offset";
    case ErrorType::PointerOverflow: return "pointer-overflow";
    case ErrorType::MisalignedPointerUse: return "misaligned-pointer-use";
    case ErrorType::AlignmentAssumption: return "alignment-assumption";
    case ErrorType::InsufficientObjectSize: return "insufficient-object-size";
    case ErrorType::SignedIntegerOverflow: return "signed-integer-overflow";
    case ErrorType::UnsignedIntegerOverflow: return "unsigned-integer-overflow";
    case ErrorType::IntegerDivideByZero: return "integer-divide-by-zero";
    case ErrorType::FloatDivideByZero: return "float-divide-by-zero";
    case ErrorType::InvalidBuiltin: return "invalid-builtin-use";
    case ErrorType::ImplicitUnsignedIntegerTruncation: return "implicit-unsigned-integer-truncation";
    case ErrorType::ImplicitSignedIntegerTruncation: return "implicit-signed-integer-truncation";
    case ErrorType::ImplicitIntegerSignChange: return "implicit-integer-sign-change";
    case ErrorType::ImplicitSignedIntegerTruncationOrSignChange: return "implicit-signed-integer-truncation-or-sign-change";
    case ErrorType::InvalidShiftBase: return "invalid-shift-base";
    case ErrorType::InvalidShiftExponent: return "invalid-shift-exponent";
    case ErrorType::OutOfBoundsIndex: return "out-of-bounds-index";
    case ErrorType::UnreachableCall: return "unreachable-call";
    case ErrorType::MissingReturn: return "missing-return";
    case ErrorType::NonPositiveVLAIndex: return "non-positive-vla-index";
    case ErrorType::FloatCastOverflow: return "float-cast-overflow";
    case ErrorType::InvalidBoolLoad: return "invalid-bool-load";
    case ErrorType::InvalidEnumLoad: return "invalid-enum-load";
    case ErrorType::FunctionTypeMismatch: return "function-type-mismatch";
    case ErrorType::InvalidNullReturn:
    case ErrorType::InvalidNullReturnWithNullability: return "invalid-null-return";
    case ErrorType::InvalidNullArgument:
    case ErrorType::InvalidNullArgumentWithNullability: return "invalid-null-argument";
    }
    Unexpected("ubsan: unknown error type");
}

struct ModuleInfo {
    std::uintptr_t address = 0;
    std::string module;
    std::uintptr_t offset = 0;
};

ModuleInfo SymbolizeAddress(std::uintptr_t address) {
    ModuleInfo info;
    info.address = address;
#ifdef _WIN32
    HMODULE handle = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(address), &handle)) {
        char path[MAX_PATH];
        const DWORD length = GetModuleFileNameA(handle, path, sizeof(path));
        if (length != 0 && length < sizeof(path)) {
            info.module.assign(path, length);
            info.offset = address - reinterpret_cast<std::uintptr_t>(handle);
        }
    }
#else
    Dl_info found{};
    if (dladdr(reinterpret_cast<void*>(address), &found) != 0 && found.dli_fname != nullptr) {
        info.module = found.dli_fname;
        info.offset = address - reinterpret_cast<std::uintptr_t>(found.dli_fbase);
    }
#endif
    return info;
}

ModuleInfo CallerLocation(std::uintptr_t pc) {
    return SymbolizeAddress(pc - 1);
}

struct Location {
    enum class Kind { Null, Source, Memory, Symbolized };

    Kind kind = Kind::Null;
    SourceLocation source{};
    std::uintptr_t memory = 0;
    ModuleInfo symbolized;

    static Location FromSource(SourceLocation value) {
        Location location;
        location.kind = Kind::Source;
        location.source = value;
        return location;
    }

    static Location FromMemory(std::uintptr_t value) {
        Location location;
        location.kind = Kind::Memory;
        location.memory = value;
        return location;
    }

    static Location FromSymbolized(ModuleInfo value) {
        Location location;
        location.kind = Kind::Symbolized;
        location.symbolized = std::move(value);
        return location;
    }
};

void AppendSourceLocation(std::string& out, const char* filename, std::uint32_t line, std::uint32_t column) {
    out += filename;
    if (line > 0) {
        out += ":" + std::to_string(line);
        if (column > 0) {
            out += ":" + std::to_string(column);
        }
    }
}

void AppendModuleLocation(std::string& out, const ModuleInfo& info) {
    char offset[32];
    std::snprintf(offset, sizeof(offset), "+0x%zx)", static_cast<std::size_t>(info.offset));
    out += "(" + info.module + offset;
}

void AppendLocation(std::string& out, const Location& location) {
    switch (location.kind) {
    case Location::Kind::Source:
        if (location.source.Invalid()) {
            out += "<unknown>";
        } else {
            AppendSourceLocation(out, location.source.filename, location.source.line, location.source.column);
        }
        return;
    case Location::Kind::Memory:
        AppendPointer_nid_no_patch(out, location.memory);
        return;
    case Location::Kind::Symbolized:
        if (!location.symbolized.module.empty()) {
            AppendModuleLocation(out, location.symbolized);
        } else {
            AppendPointer_nid_no_patch(out, location.symbolized.address);
        }
        return;
    case Location::Kind::Null:
        out += "<unknown>";
        return;
    }
}

std::mutex reportMutex;

struct CurrentReport {
    bool present = false;
    const char* issueKind = nullptr;
    Location location;
    std::string message;
};

CurrentReport currentReport;

void Emit(const std::string& text) {
    std::fputs(text.c_str(), stderr);
    std::fflush(stderr);
}

void AppendWide(std::string& out, UIntMax value) {
    char text[64];
    std::snprintf(text, sizeof(text), "0x%08x%08x%08x%08x", static_cast<unsigned>(value >> 96), static_cast<unsigned>(value >> 64), static_cast<unsigned>(value >> 32), static_cast<unsigned>(value));
    out += text;
}

bool Readable(std::uintptr_t address, std::size_t size) {
#ifdef _WIN32
    auto cursor = address;
    const auto end = address + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region)) != sizeof(region)) {
            return false;
        }
        if (region.State != MEM_COMMIT || (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            return false;
        }
        cursor = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
    }
    return true;
#else
    static int pipeFds[2] = {-1, -1};
    static std::once_flag once;
    std::call_once(once, [] {
        if (pipe(pipeFds) != 0) {
            pipeFds[0] = pipeFds[1] = -1;
        }
    });
    if (pipeFds[1] < 0) {
        return false;
    }
    char sink[64];
    const auto written = write(pipeFds[1], reinterpret_cast<const void*>(address), size);
    if (written < 0) {
        return false;
    }
    std::size_t drained = 0;
    while (drained < static_cast<std::size_t>(written)) {
        const auto count = read(pipeFds[0], sink, sizeof(sink));
        if (count <= 0) {
            break;
        }
        drained += static_cast<std::size_t>(count);
    }
    return true;
#endif
}

void PrintMemorySnippet(std::uintptr_t location) {
    constexpr std::uintptr_t minBytesNearLocation = 4;
    constexpr std::uintptr_t bytesToShow = 32;
    const std::uintptr_t first = location < minBytesNearLocation ? 0 : location - minBytesNearLocation;
    const std::uintptr_t limit = first + bytesToShow;
    if (limit < first || !Readable(first, limit - first)) {
        Emit("<memory cannot be printed>\n");
        return;
    }
    std::string buffer;
    char byte[8];
    for (auto position = first; position != limit; ++position) {
        std::snprintf(byte, sizeof(byte), "%s%02x", position % 8 == 0 ? "  " : " ", *reinterpret_cast<const unsigned char*>(position));
        buffer += byte;
    }
    buffer += "\n";
    for (auto position = first; position != limit; ++position) {
        if (position > location) {
            break;
        }
        if (position % 8 == 0) {
            buffer += ' ';
        }
        buffer += ' ';
        buffer += position == location ? '^' : ' ';
        buffer += ' ';
    }
    buffer += "\n";
    Emit(buffer);
}

struct ReportOptions {
    bool unrecoverable;
    std::uintptr_t pc;
    std::uintptr_t bp;
};

class ScopedReport {
public:
    ScopedReport(Location summaryLocation, ErrorType type) : lock_(reportMutex), summaryLocation_(std::move(summaryLocation)) {
        static_cast<void>(type);
    }

    ~ScopedReport() {
        std::string summary = "SUMMARY: UndefinedBehaviorSanitizer: ";
        summary += SummaryKind(ErrorType::GenericUB);
        if (summaryLocation_.kind == Location::Kind::Source && !summaryLocation_.source.Invalid()) {
            summary += " ";
            AppendSourceLocation(summary, summaryLocation_.source.filename, summaryLocation_.source.line, summaryLocation_.source.column);
        } else if (summaryLocation_.kind == Location::Kind::Symbolized) {
            summary += " ";
            if (!summaryLocation_.symbolized.module.empty()) {
                AppendModuleLocation(summary, summaryLocation_.symbolized);
            } else {
                summary += "(<unknown module>)";
            }
        }
        summary += "\n";
        Emit(summary);
    }

    ScopedReport(const ScopedReport&) = delete;
    ScopedReport& operator=(const ScopedReport&) = delete;

private:
    std::unique_lock<std::mutex> lock_;
    Location summaryLocation_;
};

enum class DiagLevel { Error, Note };

class Diag {
public:
    Diag(Location location, DiagLevel level, ErrorType type, const char* message) : location_(std::move(location)), level_(level), type_(type), message_(message) {}

    Diag(const Diag&) = delete;
    Diag& operator=(const Diag&) = delete;

    ~Diag() {
        std::string text;
        RenderText(text);
        if (level_ == DiagLevel::Error) {
            currentReport.present = true;
            currentReport.issueKind = SummaryKind(type_);
            currentReport.location = location_;
            currentReport.message = text;
        }
        std::string out;
        AppendLocation(out, location_);
        out += level_ == DiagLevel::Error ? ": runtime error: " : ": note: ";
        out += text;
        out += "\n";
        Emit(out);
        if (location_.kind == Location::Kind::Memory) {
            PrintMemorySnippet(location_.memory);
        }
    }

    Diag& operator<<(const char* value) {
        args_[count_++] = Arg{ArgKind::String, value, 0, 0, 0, nullptr};
        return *this;
    }

    Diag& operator<<(unsigned long long value) {
        args_[count_++] = Arg{ArgKind::UInt, nullptr, value, 0, 0, nullptr};
        return *this;
    }

    Diag& operator<<(const void* value) {
        args_[count_++] = Arg{ArgKind::Pointer, nullptr, 0, 0, 0, value};
        return *this;
    }

    Diag& operator<<(const TypeDescriptor& value) {
        return *this << static_cast<const char*>(value.name);
    }

    Diag& operator<<(const Value& value) {
        if (value.Type().Signed()) {
            args_[count_++] = Arg{ArgKind::SInt, nullptr, 0, value.SignedValue(), 0, nullptr};
        } else if (value.Type().Unsigned()) {
            args_[count_++] = Arg{ArgKind::UInt, nullptr, value.UnsignedValue(), 0, 0, nullptr};
        } else if (value.Type().Float()) {
            args_[count_++] = Arg{ArgKind::Float, nullptr, 0, 0, value.FloatValue(), nullptr};
        } else {
            return *this << "<unknown>";
        }
        return *this;
    }

private:
    enum class ArgKind { String, UInt, SInt, Float, Pointer };

    struct Arg {
        ArgKind kind;
        const char* string;
        UIntMax unsignedValue;
        SIntMax signedValue;
        long double floatValue;
        const void* pointer;
    };

    void RenderText(std::string& out) const {
        for (const char* cursor = message_; *cursor != '\0'; ++cursor) {
            if (*cursor != '%') {
                out += *cursor;
                continue;
            }
            const Arg& arg = args_[*++cursor - '0'];
            switch (arg.kind) {
            case ArgKind::String:
                out += arg.string;
                break;
            case ArgKind::SInt:
                if (arg.signedValue >= INT64_MIN && arg.signedValue <= INT64_MAX) {
                    out += std::to_string(static_cast<long long>(arg.signedValue));
                } else {
                    AppendWide(out, static_cast<UIntMax>(arg.signedValue));
                }
                break;
            case ArgKind::UInt:
                if (arg.unsignedValue <= UINT64_MAX) {
                    out += std::to_string(static_cast<unsigned long long>(arg.unsignedValue));
                } else {
                    AppendWide(out, arg.unsignedValue);
                }
                break;
            case ArgKind::Float: {
                char text[32];
#ifdef _WIN32
                std::snprintf(text, sizeof(text), "%g", static_cast<double>(arg.floatValue));
#else
                std::snprintf(text, sizeof(text), "%Lg", arg.floatValue);
#endif
                out += text;
                break;
            }
            case ArgKind::Pointer:
                AppendPointer_nid_no_patch(out, reinterpret_cast<std::uintptr_t>(arg.pointer));
                break;
            }
        }
    }

    static constexpr unsigned kMaxArgs = 8;

    Location location_;
    DiagLevel level_;
    ErrorType type_;
    const char* message_;
    Arg args_[kMaxArgs];
    unsigned count_ = 0;
};

constexpr unsigned char kTypeCheckNonnullAssign = 10;

const char* const typeCheckKinds[] = {"load of", "store to", "reference binding to", "member access within", "member call on", "constructor call on", "downcast of", "downcast of", "upcast of", "cast to virtual base of", "_Nonnull binding to", "dynamic operation on"};

bool IgnoreReport(const SourceLocation& location, const ReportOptions& options) {
    if (options.unrecoverable) {
        return false;
    }
    return location.Disabled();
}

void TypeMismatch(TypeMismatchData* data, ValueHandle pointer, const ReportOptions& options) {
    Location location = Location::FromSource(data->loc.Acquire());
    const std::uintptr_t alignment = std::uintptr_t(1) << data->logAlignment;
    ErrorType type;
    if (pointer == 0) {
        type = data->typeCheckKind == kTypeCheckNonnullAssign ? ErrorType::NullPointerUseWithNullability : ErrorType::NullPointerUse;
    } else if ((pointer & (alignment - 1)) != 0) {
        type = ErrorType::MisalignedPointerUse;
    } else {
        type = ErrorType::InsufficientObjectSize;
    }
    if (IgnoreReport(location.source, options)) {
        return;
    }
    if (data->loc.Invalid()) {
        location = Location::FromSymbolized(CallerLocation(options.pc));
    }
    ScopedReport report(location, type);
    switch (type) {
    case ErrorType::NullPointerUse:
    case ErrorType::NullPointerUseWithNullability:
        Diag(location, DiagLevel::Error, type, "%0 null pointer of type %1") << typeCheckKinds[data->typeCheckKind] << *data->type;
        break;
    case ErrorType::MisalignedPointerUse:
        Diag(location, DiagLevel::Error, type, "%0 misaligned address %1 for type %3, which requires %2 byte alignment") << typeCheckKinds[data->typeCheckKind] << reinterpret_cast<const void*>(pointer) << static_cast<unsigned long long>(alignment) << *data->type;
        break;
    default:
        Diag(location, DiagLevel::Error, type, "%0 address %1 with insufficient space for an object of type %2") << typeCheckKinds[data->typeCheckKind] << reinterpret_cast<const void*>(pointer) << *data->type;
        break;
    }
    if (pointer != 0) {
        Diag(Location::FromMemory(pointer), DiagLevel::Note, type, "pointer points here");
    }
}

void AlignmentAssumption(AlignmentAssumptionData* data, ValueHandle pointer, ValueHandle alignment, ValueHandle offset, const ReportOptions& options) {
    Location location = Location::FromSource(data->loc.Acquire());
    SourceLocation assumptionLocation = data->assumptionLoc.Acquire();
    const ErrorType type = ErrorType::AlignmentAssumption;
    if (IgnoreReport(location.source, options)) {
        return;
    }
    ScopedReport report(location, type);
    const std::uintptr_t realPointer = pointer - offset;
    if (realPointer == 0) {
        Unexpected("ubsan: alignment assumption on a null pointer");
    }
    const std::uintptr_t leastSetBit = static_cast<std::uintptr_t>(__builtin_ctzll(realPointer));
    const std::uintptr_t actualAlignment = std::uintptr_t(1) << leastSetBit;
    const std::uintptr_t mask = alignment - 1;
    const std::uintptr_t misalignmentOffset = realPointer & mask;
    if (offset == 0) {
        Diag(location, DiagLevel::Error, type, "assumption of %0 byte alignment for pointer of type %1 failed") << static_cast<unsigned long long>(alignment) << *data->type;
    } else {
        Diag(location, DiagLevel::Error, type, "assumption of %0 byte alignment (with offset of %1 byte) for pointer of type %2 failed") << static_cast<unsigned long long>(alignment) << static_cast<unsigned long long>(offset) << *data->type;
    }
    if (!assumptionLocation.Invalid()) {
        Diag(Location::FromSource(assumptionLocation), DiagLevel::Note, type, "alignment assumption was specified here");
    }
    Diag(Location::FromMemory(realPointer), DiagLevel::Note, type, "%0address is %1 aligned, misalignment offset is %2 bytes") << (offset != 0 ? "offset " : "") << static_cast<unsigned long long>(actualAlignment) << static_cast<unsigned long long>(misalignmentOffset);
}

void IntegerOverflow(OverflowData* data, ValueHandle lhs, const char* operation, ValueHandle rhs, const ReportOptions& options) {
    SourceLocation location = data->loc.Acquire();
    const bool isSigned = data->type->Signed();
    const ErrorType type = isSigned ? ErrorType::SignedIntegerOverflow : ErrorType::UnsignedIntegerOverflow;
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    Diag(Location::FromSource(location), DiagLevel::Error, type, "%0 integer overflow: %1 %2 %3 cannot be represented in type %4") << (isSigned ? "signed" : "unsigned") << Value(*data->type, lhs) << operation << Value(*data->type, rhs) << *data->type;
}

void NegateOverflow(OverflowData* data, ValueHandle oldValue, const ReportOptions& options) {
    SourceLocation location = data->loc.Acquire();
    const bool isSigned = data->type->Signed();
    const ErrorType type = isSigned ? ErrorType::SignedIntegerOverflow : ErrorType::UnsignedIntegerOverflow;
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    if (isSigned) {
        Diag(Location::FromSource(location), DiagLevel::Error, type, "negation of %0 cannot be represented in type %1; cast to an unsigned type to negate this value to itself") << Value(*data->type, oldValue) << *data->type;
    } else {
        Diag(Location::FromSource(location), DiagLevel::Error, type, "negation of %0 cannot be represented in type %1") << Value(*data->type, oldValue) << *data->type;
    }
}

void DivremOverflow(OverflowData* data, ValueHandle lhs, ValueHandle rhs, const ReportOptions& options) {
    SourceLocation location = data->loc.Acquire();
    const Value lhsValue(*data->type, lhs);
    const Value rhsValue(*data->type, rhs);
    ErrorType type;
    if (rhsValue.MinusOne()) {
        type = ErrorType::SignedIntegerOverflow;
    } else if (data->type->Integer()) {
        type = ErrorType::IntegerDivideByZero;
    } else {
        type = ErrorType::FloatDivideByZero;
    }
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    if (type == ErrorType::SignedIntegerOverflow) {
        Diag(Location::FromSource(location), DiagLevel::Error, type, "division of %0 by -1 cannot be represented in type %1") << lhsValue << *data->type;
    } else {
        Diag(Location::FromSource(location), DiagLevel::Error, type, "division by zero");
    }
}

void ShiftOutOfBounds(ShiftOutOfBoundsData* data, ValueHandle lhs, ValueHandle rhs, const ReportOptions& options) {
    SourceLocation location = data->loc.Acquire();
    const Value lhsValue(*data->lhsType, lhs);
    const Value rhsValue(*data->rhsType, rhs);
    ErrorType type;
    if (rhsValue.Negative() || rhsValue.PositiveValue() >= data->lhsType->IntegerBits()) {
        type = ErrorType::InvalidShiftExponent;
    } else {
        type = ErrorType::InvalidShiftBase;
    }
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    if (type == ErrorType::InvalidShiftExponent) {
        if (rhsValue.Negative()) {
            Diag(Location::FromSource(location), DiagLevel::Error, type, "shift exponent %0 is negative") << rhsValue;
        } else {
            Diag(Location::FromSource(location), DiagLevel::Error, type, "shift exponent %0 is too large for %1-bit type %2") << rhsValue << static_cast<unsigned long long>(data->lhsType->IntegerBits()) << *data->lhsType;
        }
    } else if (lhsValue.Negative()) {
        Diag(Location::FromSource(location), DiagLevel::Error, type, "left shift of negative value %0") << lhsValue;
    } else {
        Diag(Location::FromSource(location), DiagLevel::Error, type, "left shift of %0 by %1 places cannot be represented in type %2") << lhsValue << rhsValue << *data->lhsType;
    }
}

void OutOfBounds(OutOfBoundsData* data, ValueHandle index, const ReportOptions& options) {
    SourceLocation location = data->loc.Acquire();
    const ErrorType type = ErrorType::OutOfBoundsIndex;
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    Diag(Location::FromSource(location), DiagLevel::Error, type, "index %0 out of bounds for type %1") << Value(*data->indexType, index) << *data->arrayType;
}

void BuiltinUnreachable(UnreachableData* data) {
    const ErrorType type = ErrorType::UnreachableCall;
    ScopedReport report(Location::FromSource(data->loc), type);
    Diag(Location::FromSource(data->loc), DiagLevel::Error, type, "execution reached an unreachable program point");
}

void MissingReturn(UnreachableData* data) {
    const ErrorType type = ErrorType::MissingReturn;
    ScopedReport report(Location::FromSource(data->loc), type);
    Diag(Location::FromSource(data->loc), DiagLevel::Error, type, "execution reached the end of a value-returning function without returning a value");
}

void VlaBoundNotPositive(VlaBoundData* data, ValueHandle bound, const ReportOptions& options) {
    SourceLocation location = data->loc.Acquire();
    const ErrorType type = ErrorType::NonPositiveVLAIndex;
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    Diag(Location::FromSource(location), DiagLevel::Error, type, "variable length array bound evaluates to non-positive value %0") << Value(*data->type, bound);
}

bool LooksLikeFloatCastOverflowDataV1(void* data) {
    const unsigned char* filenameOrTypeDescriptor;
    std::memcpy(&filenameOrTypeDescriptor, data, sizeof(filenameOrTypeDescriptor));
    const std::uint16_t maybeFromTypeKind = static_cast<std::uint16_t>(filenameOrTypeDescriptor[0] + filenameOrTypeDescriptor[1]);
    return maybeFromTypeKind < 2 || filenameOrTypeDescriptor[0] == 0xff || filenameOrTypeDescriptor[1] == 0xff;
}

void FloatCastOverflow(void* dataPointer, ValueHandle from, const ReportOptions& options) {
    Location location;
    const TypeDescriptor* fromType;
    const TypeDescriptor* toType;
    const ErrorType type = ErrorType::FloatCastOverflow;
    if (LooksLikeFloatCastOverflowDataV1(dataPointer)) {
        const auto* data = static_cast<FloatCastOverflowData*>(dataPointer);
        location = Location::FromSymbolized(CallerLocation(options.pc));
        fromType = data->fromType;
        toType = data->toType;
    } else {
        auto* data = static_cast<FloatCastOverflowDataV2*>(dataPointer);
        SourceLocation sourceLocation = data->loc.Acquire();
        if (IgnoreReport(sourceLocation, options)) {
            return;
        }
        location = Location::FromSource(sourceLocation);
        fromType = data->fromType;
        toType = data->toType;
    }
    ScopedReport report(location, type);
    Diag(location, DiagLevel::Error, type, "%0 is outside the range of representable values of type %2") << Value(*fromType, from) << *fromType << *toType;
}

void LoadInvalidValue(InvalidValueData* data, ValueHandle value, const ReportOptions& options) {
    SourceLocation location = data->loc.Acquire();
    const bool isBool = std::strcmp(data->type->name, "'bool'") == 0 || std::strncmp(data->type->name, "'BOOL'", 6) == 0;
    const ErrorType type = isBool ? ErrorType::InvalidBoolLoad : ErrorType::InvalidEnumLoad;
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    Diag(Location::FromSource(location), DiagLevel::Error, type, "load of value %0, which is not a valid value for type %1") << Value(*data->type, value) << *data->type;
}

void ImplicitConversion(ImplicitConversionData* data, ValueHandle source, ValueHandle destination, const ReportOptions& options) {
    SourceLocation location = data->loc.Acquire();
    ErrorType type = ErrorType::GenericUB;
    const TypeDescriptor& sourceType = *data->fromType;
    const TypeDescriptor& destinationType = *data->toType;
    const bool sourceSigned = sourceType.Signed();
    const bool destinationSigned = destinationType.Signed();
    switch (data->kind) {
    case 0:
        type = !sourceSigned && !destinationSigned ? ErrorType::ImplicitUnsignedIntegerTruncation : ErrorType::ImplicitSignedIntegerTruncation;
        break;
    case 1:
        type = ErrorType::ImplicitUnsignedIntegerTruncation;
        break;
    case 2:
        type = ErrorType::ImplicitSignedIntegerTruncation;
        break;
    case 3:
        type = ErrorType::ImplicitIntegerSignChange;
        break;
    case 4:
        type = ErrorType::ImplicitSignedIntegerTruncationOrSignChange;
        break;
    }
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    Diag(Location::FromSource(location), DiagLevel::Error, type, "implicit conversion from type %0 of value %1 (%2-bit, %3signed) to type %4 changed the value to %5 (%6-bit, %7signed)") << sourceType << Value(sourceType, source) << static_cast<unsigned long long>(sourceType.IntegerBits()) << (sourceSigned ? "" : "un") << destinationType << Value(destinationType, destination) << static_cast<unsigned long long>(destinationType.IntegerBits()) << (destinationSigned ? "" : "un");
}

void InvalidBuiltin(InvalidBuiltinData* data, const ReportOptions& options) {
    SourceLocation location = data->loc.Acquire();
    const ErrorType type = ErrorType::InvalidBuiltin;
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    Diag(Location::FromSource(location), DiagLevel::Error, type, "passing zero to %0, which is not a valid argument") << (data->kind == 0 ? "ctz()" : "clz()");
}

void NonNullReturn(NonNullReturnData* data, SourceLocation* locationPointer, const ReportOptions& options, bool isAttribute) {
    if (locationPointer == nullptr) {
        Unexpected("ubsan: source location pointer is null");
    }
    SourceLocation location = locationPointer->Acquire();
    const ErrorType type = isAttribute ? ErrorType::InvalidNullReturn : ErrorType::InvalidNullReturnWithNullability;
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    Diag(Location::FromSource(location), DiagLevel::Error, type, "null pointer returned from function declared to never return null");
    if (!data->attrLoc.Invalid()) {
        Diag(Location::FromSource(data->attrLoc), DiagLevel::Note, type, "%0 specified here") << (isAttribute ? "returns_nonnull attribute" : "_Nonnull return type annotation");
    }
}

void NonNullArg(NonNullArgData* data, const ReportOptions& options, bool isAttribute) {
    SourceLocation location = data->loc.Acquire();
    const ErrorType type = isAttribute ? ErrorType::InvalidNullArgument : ErrorType::InvalidNullArgumentWithNullability;
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    Diag(Location::FromSource(location), DiagLevel::Error, type, "null pointer passed as argument %0, which is declared to never be null") << static_cast<unsigned long long>(data->argIndex);
    if (!data->attrLoc.Invalid()) {
        Diag(Location::FromSource(data->attrLoc), DiagLevel::Note, type, "%0 specified here") << (isAttribute ? "nonnull attribute" : "_Nonnull type annotation");
    }
}

void PointerOverflow(PointerOverflowData* data, ValueHandle base, ValueHandle result, const ReportOptions& options) {
    SourceLocation location = data->loc.Acquire();
    ErrorType type;
    if (base == 0 && result == 0) {
        type = ErrorType::NullptrWithOffset;
    } else if (base == 0) {
        type = ErrorType::NullptrWithNonZeroOffset;
    } else if (result == 0) {
        type = ErrorType::NullptrAfterNonZeroOffset;
    } else {
        type = ErrorType::PointerOverflow;
    }
    if (IgnoreReport(location, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(location), type);
    const Location where = Location::FromSource(location);
    if (type == ErrorType::NullptrWithOffset) {
        Diag(where, DiagLevel::Error, type, "applying zero offset to null pointer");
    } else if (type == ErrorType::NullptrWithNonZeroOffset) {
        Diag(where, DiagLevel::Error, type, "applying non-zero offset %0 to null pointer") << static_cast<unsigned long long>(result);
    } else if (type == ErrorType::NullptrAfterNonZeroOffset) {
        Diag(where, DiagLevel::Error, type, "applying non-zero offset to non-null pointer %0 produced null pointer") << reinterpret_cast<const void*>(base);
    } else if ((static_cast<std::intptr_t>(base) >= 0) == (static_cast<std::intptr_t>(result) >= 0)) {
        if (base > result) {
            Diag(where, DiagLevel::Error, type, "addition of unsigned offset to %0 overflowed to %1") << reinterpret_cast<const void*>(base) << reinterpret_cast<const void*>(result);
        } else {
            Diag(where, DiagLevel::Error, type, "subtraction of unsigned offset from %0 overflowed to %1") << reinterpret_cast<const void*>(base) << reinterpret_cast<const void*>(result);
        }
    } else {
        Diag(where, DiagLevel::Error, type, "pointer index expression with base %0 overflowed to %1") << reinterpret_cast<const void*>(base) << reinterpret_cast<const void*>(result);
    }
}

void FunctionTypeMismatch(FunctionTypeMismatchData* data, ValueHandle function, const ReportOptions& options) {
    SourceLocation callLocation = data->loc.Acquire();
    const ErrorType type = ErrorType::FunctionTypeMismatch;
    if (IgnoreReport(callLocation, options)) {
        return;
    }
    ScopedReport report(Location::FromSource(callLocation), type);
    const char* functionName = "(unknown)";
    Diag(Location::FromSource(callLocation), DiagLevel::Error, type, "call to function %0 through pointer to incorrect function type %1") << functionName << *data->type;
    Diag(Location::FromSymbolized(SymbolizeAddress(function)), DiagLevel::Note, type, "%0 defined here") << functionName;
}

}  // namespace

void Handle_nid_no_patch(Check check, void* data, ValueHandle first, ValueHandle second, ValueHandle third, bool unrecoverable, std::uintptr_t pc, std::uintptr_t bp) {
    const ReportOptions options{unrecoverable, pc, bp};
    switch (check) {
    case Check::TypeMismatch:
        TypeMismatch(static_cast<TypeMismatchData*>(data), first, options);
        break;
    case Check::AlignmentAssumption:
        AlignmentAssumption(static_cast<AlignmentAssumptionData*>(data), first, second, third, options);
        break;
    case Check::AddOverflow:
        IntegerOverflow(static_cast<OverflowData*>(data), first, "+", second, options);
        break;
    case Check::SubOverflow:
        IntegerOverflow(static_cast<OverflowData*>(data), first, "-", second, options);
        break;
    case Check::MulOverflow:
        IntegerOverflow(static_cast<OverflowData*>(data), first, "*", second, options);
        break;
    case Check::NegateOverflow:
        NegateOverflow(static_cast<OverflowData*>(data), first, options);
        break;
    case Check::DivremOverflow:
        DivremOverflow(static_cast<OverflowData*>(data), first, second, options);
        break;
    case Check::ShiftOutOfBounds:
        ShiftOutOfBounds(static_cast<ShiftOutOfBoundsData*>(data), first, second, options);
        break;
    case Check::OutOfBounds:
        OutOfBounds(static_cast<OutOfBoundsData*>(data), first, options);
        break;
    case Check::BuiltinUnreachable:
        BuiltinUnreachable(static_cast<UnreachableData*>(data));
        break;
    case Check::MissingReturn:
        MissingReturn(static_cast<UnreachableData*>(data));
        break;
    case Check::VlaBound:
        VlaBoundNotPositive(static_cast<VlaBoundData*>(data), first, options);
        break;
    case Check::FloatCastOverflow:
        FloatCastOverflow(data, first, options);
        break;
    case Check::LoadInvalidValue:
        LoadInvalidValue(static_cast<InvalidValueData*>(data), first, options);
        break;
    case Check::ImplicitConversion:
        ImplicitConversion(static_cast<ImplicitConversionData*>(data), first, second, options);
        break;
    case Check::InvalidBuiltin:
        InvalidBuiltin(static_cast<InvalidBuiltinData*>(data), options);
        break;
    case Check::NonNullReturn:
        NonNullReturn(static_cast<NonNullReturnData*>(data), reinterpret_cast<SourceLocation*>(first), options, true);
        break;
    case Check::NullabilityReturn:
        NonNullReturn(static_cast<NonNullReturnData*>(data), reinterpret_cast<SourceLocation*>(first), options, false);
        break;
    case Check::NonNullArg:
        NonNullArg(static_cast<NonNullArgData*>(data), options, true);
        break;
    case Check::NullabilityArg:
        NonNullArg(static_cast<NonNullArgData*>(data), options, false);
        break;
    case Check::PointerOverflow:
        PointerOverflow(static_cast<PointerOverflowData*>(data), first, second, options);
        break;
    case Check::FunctionTypeMismatch:
        FunctionTypeMismatch(static_cast<FunctionTypeMismatchData*>(data), first, options);
        break;
    }
}

void GetCurrentReportData_nid_no_patch(const char** issueKind, const char** message, const char** filename, unsigned* line, unsigned* column, char** memoryAddress) {
    if (issueKind == nullptr || message == nullptr || filename == nullptr || line == nullptr || column == nullptr || memoryAddress == nullptr) {
        Unexpected("Invalid arguments passed to __ubsan_get_current_report_data");
    }
    std::lock_guard lock(reportMutex);
    if (!currentReport.present) {
        Unexpected("__ubsan_get_current_report_data called without a report");
    }
    if (!currentReport.message.empty() && currentReport.message[0] >= 'a' && currentReport.message[0] <= 'z') {
        currentReport.message[0] = static_cast<char>(currentReport.message[0] + 'A' - 'a');
    }
    *issueKind = currentReport.issueKind;
    *message = currentReport.message.c_str();
    if (currentReport.location.kind != Location::Kind::Source) {
        *filename = "<unknown>";
        *line = 0;
        *column = 0;
    } else {
        *filename = currentReport.location.source.filename;
        *line = currentReport.location.source.line;
        *column = currentReport.location.source.column;
    }
    *memoryAddress = currentReport.location.kind == Location::Kind::Memory ? reinterpret_cast<char*>(currentReport.location.memory) : nullptr;
}

}  // namespace AsanRuntime::Ubsan
