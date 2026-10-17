#ifndef CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_NIDTHUNK_HPP
#define CORE_WASM_RUNTIME_INCLUDE_WASM_RUNTIME_NIDTHUNK_HPP

#include "SceTypes.hpp"
#include "wasm-runtime/Guest.hpp"
#include "wasm-runtime/GuestCpu.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace WasmRuntime {

void RegisterDataExport(const char* nid, const void* address);
const void* FindDataExport(const char* nid);

struct DataExport {
    DataExport(const char* nid, const void* address) { RegisterDataExport(nid, address); }
};

namespace Thunk {

template <typename T>
inline constexpr bool IsFloat = std::is_same_v<T, float> || std::is_same_v<T, double>;

template <typename T>
consteval bool ScalarType() {
    if constexpr (std::is_integral_v<T> || std::is_enum_v<T> || std::is_pointer_v<T> || std::is_null_pointer_v<T>) return sizeof(T) <= 8;
    return false;
}

template <typename T>
consteval bool IndirectClassType() {
    if constexpr (std::is_class_v<T>) return !std::is_trivially_copyable_v<T> || !std::is_trivially_destructible_v<T>;
    return false;
}

template <typename T>
consteval bool MemoryClassType() {
    if constexpr (std::is_class_v<T>) return !IndirectClassType<T>() && sizeof(T) > 16;
    return false;
}

template <typename T>
inline constexpr bool IsScalar = ScalarType<T>();

template <typename T>
inline constexpr bool IsIndirectClass = IndirectClassType<T>();

template <typename T>
inline constexpr bool IsMemoryClass = MemoryClassType<T>();

template <typename T>
inline constexpr bool SupportedParameter = std::is_reference_v<T> || IsFloat<std::remove_cv_t<T>> || IsScalar<std::remove_cv_t<T>> || IsIndirectClass<std::remove_cv_t<T>> || IsMemoryClass<std::remove_cv_t<T>>;

template <typename T>
inline constexpr bool ReturnsInMemory = IsIndirectClass<std::remove_cv_t<T>> || IsMemoryClass<std::remove_cv_t<T>>;

template <typename T>
inline constexpr bool SupportedResult = std::is_void_v<T> || std::is_reference_v<T> || IsFloat<std::remove_cv_t<T>> || IsScalar<std::remove_cv_t<T>> || ReturnsInMemory<T>;

template <typename TFunction>
struct Signature {
    static constexpr bool Known = false;
};

template <typename TResult, bool TVariadic, typename... TParameters>
struct SignatureOf {
    static constexpr bool Known = true;
    static constexpr bool Variadic = TVariadic;
    using Result = TResult;
    using Values = std::tuple<TParameters...>;
    static constexpr bool Supported = SupportedResult<TResult> && (SupportedParameter<TParameters> && ...);
};

template <typename TResult, typename... TParameters>
struct Signature<TResult (*)(TParameters...)> : SignatureOf<TResult, false, TParameters...> {};
template <typename TResult, typename... TParameters>
struct Signature<TResult (*)(TParameters...) noexcept> : SignatureOf<TResult, false, TParameters...> {};
template <typename TResult, typename... TParameters>
struct Signature<TResult (*)(TParameters..., ...)> : SignatureOf<TResult, true, TParameters...> {};
template <typename TResult, typename... TParameters>
struct Signature<TResult (*)(TParameters..., ...) noexcept> : SignatureOf<TResult, true, TParameters...> {};

class Arguments {
public:
    Arguments(GuestCpu& cpu, bool hiddenResult) : _cpu(cpu), _integers(hiddenResult ? 1 : 0), _stack(cpu.gpr[Rsp] + 8) {}

    std::uint64_t Integer() {
        if (_integers < 6) return _cpu.gpr[IntegerArgumentRegisters[_integers++]];
        std::uint64_t value;
        std::memcpy(&value, reinterpret_cast<const void*>(_stack), sizeof(value));
        _stack += 8;
        return value;
    }

    template <typename T>
    T Float() {
        T value;
        if (_floats < FloatArgumentRegisters) {
            std::memcpy(&value, _cpu.ymm[_floats++], sizeof(value));
            return value;
        }
        std::memcpy(&value, reinterpret_cast<const void*>(_stack), sizeof(value));
        _stack += 8;
        return value;
    }

    template <typename T>
    T Memory() {
        const std::uint64_t alignment = std::max<std::uint64_t>(alignof(T), 8);
        _stack = (_stack + alignment - 1) & ~(alignment - 1);
        T value;
        std::memcpy(&value, reinterpret_cast<const void*>(_stack), sizeof(value));
        _stack += (sizeof(T) + 7) & ~std::uint64_t{7};
        return value;
    }

    VaList Remaining(std::uint8_t* saveArea) const {
        for (unsigned index = 0; index < 6; ++index) std::memcpy(saveArea + 8 * index, &_cpu.gpr[IntegerArgumentRegisters[index]], 8);
        for (unsigned index = 0; index < FloatArgumentRegisters; ++index) std::memcpy(saveArea + 48 + 16 * index, _cpu.ymm[index], 16);
        VaList list{};
        list.gp_offset = 8 * std::min(_integers, 6u);
        list.fp_offset = 48 + 16 * std::min(_floats, FloatArgumentRegisters);
        list.overflow_arg_area = reinterpret_cast<void*>(_stack);
        list.reg_save_area = saveArea;
        return list;
    }

private:
    GuestCpu& _cpu;
    unsigned _integers = 0;
    unsigned _floats = 0;
    std::uint64_t _stack;
};

template <typename T>
T Read(Arguments& arguments) {
    using TValue = std::remove_cv_t<T>;
    if constexpr (std::is_reference_v<T>) {
        return static_cast<T>(*reinterpret_cast<std::remove_reference_t<T>*>(static_cast<std::uintptr_t>(arguments.Integer())));
    } else if constexpr (IsFloat<TValue>) {
        return arguments.Float<TValue>();
    } else if constexpr (std::is_null_pointer_v<TValue>) {
        arguments.Integer();
        return nullptr;
    } else if constexpr (std::is_pointer_v<TValue>) {
        return reinterpret_cast<TValue>(static_cast<std::uintptr_t>(arguments.Integer()));
    } else if constexpr (std::is_same_v<TValue, bool>) {
        return (arguments.Integer() & 0xFF) != 0;
    } else if constexpr (std::is_integral_v<TValue> || std::is_enum_v<TValue>) {
        return static_cast<TValue>(arguments.Integer());
    } else if constexpr (IsIndirectClass<TValue>) {
        return TValue(*reinterpret_cast<const TValue*>(static_cast<std::uintptr_t>(arguments.Integer())));
    } else {
        return arguments.Memory<TValue>();
    }
}

template <typename T>
std::uint64_t ScalarBits(T value) {
    if constexpr (std::is_pointer_v<T>) {
        return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value));
    } else if constexpr (std::is_null_pointer_v<T>) {
        return 0;
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? 1 : 0;
    } else if constexpr (std::is_enum_v<T>) {
        return ScalarBits(static_cast<std::underlying_type_t<T>>(value));
    } else if constexpr (std::is_signed_v<T>) {
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
    } else {
        return static_cast<std::uint64_t>(value);
    }
}

template <typename TResult, typename TCall>
void Complete(GuestCpu& cpu, std::uint64_t hiddenResult, TCall&& call) {
    using TValue = std::remove_cv_t<std::remove_reference_t<TResult>>;
    if constexpr (std::is_void_v<TResult>) {
        call();
    } else if constexpr (std::is_reference_v<TResult>) {
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&call()));
        if (!GuestTransferPending()) cpu.gpr[Rax] = address;
    } else if constexpr (IsFloat<TValue>) {
        const TValue value = call();
        if (!GuestTransferPending()) std::memcpy(cpu.ymm[0], &value, sizeof(value));
    } else if constexpr (ReturnsInMemory<TResult>) {
        new (reinterpret_cast<void*>(static_cast<std::uintptr_t>(hiddenResult))) TValue(call());
        if (!GuestTransferPending()) cpu.gpr[Rax] = hiddenResult;
    } else {
        const auto bits = ScalarBits<TValue>(call());
        if (!GuestTransferPending()) cpu.gpr[Rax] = bits;
    }
}

[[noreturn]] void UnsupportedSignature(const char* name);
void ReportEscape(const char* name, const std::exception& error);
void ReturnToGuest(GuestCpu& cpu);

bool TraceCalls();
void TraceCall(const char* name, const GuestCpu& cpu);
void TraceReturn(const char* name, const GuestCpu& cpu);

template <auto Function>
void Invoke(GuestCpu& cpu, const char* name) {
    using TSignature = Signature<decltype(Function)>;
    const bool trace = TraceCalls();
    if (trace) TraceCall(name, cpu);
    if constexpr (!TSignature::Known || !TSignature::Supported) {
        UnsupportedSignature(name);
    } else {
        using TResult = typename TSignature::Result;
        constexpr bool hidden = ReturnsInMemory<TResult>;
        const std::uint64_t hiddenResult = hidden ? cpu.gpr[Rdi] : 0;
        Arguments arguments(cpu, hidden);
        auto values = [&]<typename... TParameters>(std::tuple<TParameters...>*) {
            return std::tuple<TParameters...>{Read<TParameters>(arguments)...};
        }(static_cast<typename TSignature::Values*>(nullptr));
        if constexpr (TSignature::Variadic) {
            alignas(16) std::uint8_t saveArea[176];
            VaList list = arguments.Remaining(saveArea);
            SetGuestVaList(&list);
            Complete<TResult>(cpu, hiddenResult, [&]() -> decltype(auto) { return std::apply(Function, std::move(values)); });
            SetGuestVaList(nullptr);
        } else {
            Complete<TResult>(cpu, hiddenResult, [&]() -> decltype(auto) { return std::apply(Function, std::move(values)); });
        }
    }
    if (trace) TraceReturn(name, cpu);
    ReturnToGuest(cpu);
}

}  // namespace Thunk
}  // namespace WasmRuntime

#define APS5_WASM_NID(symbol, nid, function) \
    extern "C" [[gnu::weak, clang::export_name("nid." nid)]] void symbol(::WasmRuntime::GuestCpu* cpu) { \
        try { \
            ::WasmRuntime::Thunk::Invoke<&function>(*cpu, #function); \
        } catch (const std::exception& error) { \
            ::WasmRuntime::Thunk::ReportEscape(#function, error); \
            throw; \
        } \
    }

#define APS5_WASM_DATA(symbol, nid, variable) \
    static const ::WasmRuntime::DataExport symbol{nid, &variable};

#endif
