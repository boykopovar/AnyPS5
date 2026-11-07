#ifndef PS5LIFT_BRIDGE_HPP
#define PS5LIFT_BRIDGE_HPP

#include <ps5lift/State.h>

#include <array>
#include <atomic>
#include <cfenv>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <type_traits>
#include <utility>

namespace Ps5Lift::Bridge {

struct GuestUnwind {};

constexpr int IntegerRegisters[6] = {PS5LIFT_RDI, PS5LIFT_RSI, PS5LIFT_RDX, PS5LIFT_RCX, PS5LIFT_R8, PS5LIFT_R9};

template <typename T>
T FromBits(const std::uint64_t bits) {
    if constexpr (std::is_pointer_v<T>) {
        return reinterpret_cast<T>(bits);
    } else if constexpr (std::is_same_v<T, bool>) {
        return (bits & 0xff) != 0;
    } else if constexpr (std::is_floating_point_v<T>) {
        T value;
        std::memcpy(&value, &bits, sizeof(T));
        return value;
    } else {
        return static_cast<T>(bits);
    }
}

template <typename T>
T Integer(const Ps5LiftState* state, const int index) {
    return FromBits<T>(state->Gpr[IntegerRegisters[index]]);
}

template <typename T>
T Floating(const Ps5LiftState* state, const int index) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, state->Vector[index], sizeof(T));
    return FromBits<T>(bits);
}

template <typename T>
T Stack(const Ps5LiftState* state, const int slot) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, reinterpret_cast<const void*>(state->Gpr[PS5LIFT_RSP] + 8 + 8 * static_cast<std::uint64_t>(slot)), sizeof(bits));
    return FromBits<T>(bits);
}

template <typename T>
T Integer128(const Ps5LiftState* state, const int index) {
    const auto low = static_cast<unsigned __int128>(state->Gpr[IntegerRegisters[index]]);
    const auto high = static_cast<unsigned __int128>(state->Gpr[IntegerRegisters[index + 1]]);
    return static_cast<T>(low | high << 64);
}

template <typename T>
T Stack128(const Ps5LiftState* state, const int slot) {
    unsigned __int128 value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(state->Gpr[PS5LIFT_RSP] + 8 + 8 * static_cast<std::uint64_t>(slot)), sizeof(value));
    return static_cast<T>(value);
}

enum class Eightbyte { Integer, Sse };

template <typename T, typename... Eightbytes>
T Record(const Eightbytes... eightbytes) {
    static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= 8 * sizeof...(Eightbytes) && sizeof(T) > 8 * (sizeof...(Eightbytes) - 1));
    const std::uint64_t bits[] = {eightbytes...};
    T value;
    std::memcpy(&value, bits, sizeof(T));
    return value;
}

template <typename T>
T StackRecord(const Ps5LiftState* state, const int slot) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(state->Gpr[PS5LIFT_RSP] + 8 + 8 * static_cast<std::uint64_t>(slot)), sizeof(T));
    return value;
}

template <Eightbyte... Classes, typename T>
void ReturnRecord(Ps5LiftState* state, const T& value) {
    static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= 8 * sizeof...(Classes) && sizeof(T) > 8 * (sizeof...(Classes) - 1));
    constexpr Eightbyte classes[] = {Classes...};
    std::uint64_t bits[sizeof...(Classes)] = {};
    std::memcpy(bits, &value, sizeof(T));
    constexpr int IntegerResults[] = {PS5LIFT_RAX, PS5LIFT_RDX};
    int integers = 0;
    int vectors = 0;
    for (std::size_t index = 0; index < sizeof...(Classes); ++index) {
        if (classes[index] == Eightbyte::Integer) {
            state->Gpr[IntegerResults[integers++]] = bits[index];
        } else {
            std::memset(state->Vector[vectors], 0, 16);
            std::memcpy(state->Vector[vectors++], &bits[index], 8);
        }
    }
}

template <typename T>
void ReturnIndirect(Ps5LiftState* state, const std::uint64_t address, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
    state->Gpr[PS5LIFT_RAX] = address;
}

class GuestEntry {
public:
    GuestEntry() : _state(ps5lift_enter_guest()) {}

    ~GuestEntry() { ps5lift_leave_guest(); }

    GuestEntry(const GuestEntry&) = delete;
    GuestEntry& operator=(const GuestEntry&) = delete;

    Ps5LiftState* State() const { return _state; }

private:
    Ps5LiftState* _state;
};

class alignas(16) LongDouble {
public:
    LongDouble() = default;

    friend LongDouble operator/(const LongDouble& dividend, const LongDouble& divisor) {
        LongDouble quotient;
        ps5lift_extended_operation(ps5lift_current_state(), PS5LIFT_X87_DIVIDE, dividend._bytes, divisor._bytes, quotient._bytes);
        return quotient;
    }

private:
    unsigned char _bytes[16] = {};
};

inline const void* ExtendedSlot(const Ps5LiftState* state, const int slot) {
    return reinterpret_cast<const void*>(state->Gpr[PS5LIFT_RSP] + 8 + 8 * static_cast<std::uint64_t>(slot));
}

template <typename T = LongDouble>
T Extended(const Ps5LiftState* state, const int slot) {
    static_assert(sizeof(T) == 16 && std::is_trivially_copyable_v<T>, "a guest long double is 16 bytes holding x87's 80 bits");
    T value{};
    std::memcpy(&value, ExtendedSlot(state, slot), 10);
    return value;
}

template <typename T>
void ReturnExtended(Ps5LiftState* state, const T& value) {
    static_assert(sizeof(T) == 16 && std::is_trivially_copyable_v<T>, "a guest long double is 16 bytes holding x87's 80 bits");
    ps5lift_x87(state, PS5LIFT_X87_LOAD, PS5LIFT_X87_FLOAT80, reinterpret_cast<std::uint64_t>(&value), 0);
}

#ifdef PS5LIFT_HOST_LONG_DOUBLE_HOLDS_EXTENDED
class ExtendedScope {
public:
    explicit ExtendedScope(const Ps5LiftState* state) : _saved(std::fegetround()) {
        static constexpr int Rounding[4] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
        std::fesetround(Rounding[(state->X87Control >> 10) & 3]);
    }

    ~ExtendedScope() { std::fesetround(_saved); }

    ExtendedScope(const ExtendedScope&) = delete;
    ExtendedScope& operator=(const ExtendedScope&) = delete;

private:
    int _saved;
};

inline long double HostExtended(const Ps5LiftState* state, const int slot) {
    long double value;
    ps5lift_extended_to_host(ExtendedSlot(state, slot), &value);
    return value;
}

inline void ReturnExtended(Ps5LiftState* state, const long double value) {
    unsigned char extended[16] = {};
    ps5lift_extended_from_host(state, &value, extended);
    ps5lift_x87(state, PS5LIFT_X87_LOAD, PS5LIFT_X87_FLOAT80, reinterpret_cast<std::uint64_t>(extended), 0);
}
#endif

class VariadicArguments {
public:
    VariadicArguments(const Ps5LiftState* state, const int integers, const int floats, const int stack) {
        for (int index = 0; index < 6; ++index) std::memcpy(_saveArea + 8 * index, &state->Gpr[IntegerRegisters[index]], 8);
        for (int index = 0; index < 8; ++index) std::memcpy(_saveArea + 48 + 16 * index, state->Vector[index], 16);
        _list.GpOffset = static_cast<std::uint32_t>(8 * integers);
        _list.FpOffset = static_cast<std::uint32_t>(48 + 16 * floats);
        _list.OverflowArea = state->Gpr[PS5LIFT_RSP] + 8 + 8 * static_cast<std::uint64_t>(stack);
        _list.SaveArea = reinterpret_cast<std::uint64_t>(_saveArea);
    }

    VariadicArguments(const VariadicArguments&) = delete;
    VariadicArguments& operator=(const VariadicArguments&) = delete;

    void* List() { return &_list; }

private:
    struct {
        std::uint32_t GpOffset;
        std::uint32_t FpOffset;
        std::uint64_t OverflowArea;
        std::uint64_t SaveArea;
    } _list;
    alignas(16) unsigned char _saveArea[176];
};

template <typename T>
void Return(Ps5LiftState* state, const T value) {
    if constexpr (std::is_same_v<T, __int128> || std::is_same_v<T, unsigned __int128>) {
        const auto bits = static_cast<unsigned __int128>(value);
        state->Gpr[PS5LIFT_RAX] = static_cast<std::uint64_t>(bits);
        state->Gpr[PS5LIFT_RDX] = static_cast<std::uint64_t>(bits >> 64);
    } else if constexpr (std::is_floating_point_v<T>) {
        std::memset(state->Vector[0], 0, 16);
        std::memcpy(state->Vector[0], &value, sizeof(T));
    } else if constexpr (std::is_pointer_v<T>) {
        state->Gpr[PS5LIFT_RAX] = reinterpret_cast<std::uint64_t>(value);
    } else if constexpr (std::is_signed_v<T>) {
        state->Gpr[PS5LIFT_RAX] = static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
    } else {
        state->Gpr[PS5LIFT_RAX] = static_cast<std::uint64_t>(value);
    }
}

template <typename T>
constexpr bool IsFloating = std::is_floating_point_v<T>;

template <typename T>
std::uint64_t ToBits(const T value) {
    if constexpr (std::is_pointer_v<T>) {
        return reinterpret_cast<std::uint64_t>(value);
    } else if constexpr (std::is_floating_point_v<T>) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(T));
        return bits;
    } else if constexpr (std::is_signed_v<T>) {
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
    } else {
        return static_cast<std::uint64_t>(value);
    }
}

template <typename R>
R GuestResult(const Ps5LiftState* state) {
    if constexpr (std::is_void_v<R>) {
        return;
    } else if constexpr (std::is_floating_point_v<R>) {
        return Floating<R>(state, 0);
    } else {
        return FromBits<R>(state->Gpr[PS5LIFT_RAX]);
    }
}

inline void RunGuest(Ps5LiftState* state, const std::uint64_t target, const std::uint64_t* stackArguments, const int stackCount, const int vectors) {
    const auto savedRsp = state->Gpr[PS5LIFT_RSP];
    auto rsp = (savedRsp - 8 * static_cast<std::uint64_t>(stackCount)) & ~std::uint64_t{15};
    std::memcpy(reinterpret_cast<void*>(rsp), stackArguments, 8 * static_cast<std::size_t>(stackCount));
    rsp -= 8;
    constexpr std::uint64_t ReturnSentinel = 0;
    std::memcpy(reinterpret_cast<void*>(rsp), &ReturnSentinel, sizeof(ReturnSentinel));
    state->Gpr[PS5LIFT_RSP] = rsp;
    state->Gpr[PS5LIFT_RAX] = static_cast<std::uint64_t>(vectors);
    ps5lift_dispatch(state, target);
    if (state->Unwinding) ps5lift_fail("a longjmp or exception in guest code that native code called leaves through native code for 0x%" PRIx64, state->ResumeRip);
    state->Gpr[PS5LIFT_RSP] = savedRsp;
}

template <typename R, typename... Args>
R CallGuest(const std::uint64_t target, Args... arguments) {
    static_assert(((std::is_arithmetic_v<Args> || std::is_pointer_v<Args> || std::is_enum_v<Args>) && ...), "callbacks take scalar arguments");
    const GuestEntry entry;
    auto* state = entry.State();
    std::uint64_t stackArguments[sizeof...(Args) + 1]{};
    int stackCount = 0;
    int integers = 0;
    int floats = 0;
    const auto place = [&](const auto value) {
        using T = std::decay_t<decltype(value)>;
        const auto bits = ToBits(value);
        if constexpr (IsFloating<T>) {
            if (floats < 8) {
                std::memset(state->Vector[floats], 0, 16);
                std::memcpy(state->Vector[floats++], &bits, sizeof(T));
                return;
            }
        } else {
            if (integers < 6) {
                state->Gpr[IntegerRegisters[integers++]] = bits;
                return;
            }
        }
        stackArguments[stackCount++] = bits;
    };
    (place(arguments), ...);
    RunGuest(state, target, stackArguments, stackCount, floats);
    if constexpr (!std::is_void_v<R>) return GuestResult<R>(state);
}

inline bool SentinelFunction(const std::uint64_t value) {
    return value < 4096 || value == ~std::uint64_t{0};
}

template <typename Signature>
struct CallbackPool;

template <typename R, typename... Args>
struct CallbackPool<R(Args...)> {
    static constexpr std::size_t Capacity = 256;
    using Native = R (*)(Args...);

    static inline std::atomic<std::uint64_t> Targets[Capacity]{};
    static inline std::atomic<std::size_t> Count{0};
    static inline std::mutex Lock;

    template <std::size_t Index>
    static R Thunk(Args... arguments) {
        return CallGuest<R>(Targets[Index].load(std::memory_order_acquire), arguments...);
    }

    template <std::size_t... Indices>
    static constexpr std::array<Native, Capacity> MakeThunks(std::index_sequence<Indices...>) {
        return {&Thunk<Indices>...};
    }

    static inline constexpr std::array<Native, Capacity> Thunks = MakeThunks(std::make_index_sequence<Capacity>{});

    static Native For(const std::uint64_t target) {
        if (SentinelFunction(target)) return reinterpret_cast<Native>(target);
        const auto count = Count.load(std::memory_order_acquire);
        for (std::size_t i = 0; i < count; ++i) {
            if (Targets[i].load(std::memory_order_relaxed) == target) return Thunks[i];
        }
        std::lock_guard guard(Lock);
        const auto current = Count.load(std::memory_order_relaxed);
        for (std::size_t i = count; i < current; ++i) {
            if (Targets[i].load(std::memory_order_relaxed) == target) return Thunks[i];
        }
        if (current == Capacity) ps5lift_fail("guest function 0x%" PRIx64 " needs a callback thunk, but its signature's %zu are taken", target, Capacity);
        Targets[current].store(target, std::memory_order_release);
        Count.store(current + 1, std::memory_order_release);
        return Thunks[current];
    }

    static std::uint64_t Guest(const Native native) {
        const auto count = Count.load(std::memory_order_acquire);
        for (std::size_t i = 0; i < count; ++i) {
            if (Thunks[i] == native) return Targets[i].load(std::memory_order_relaxed);
        }
        return reinterpret_cast<std::uint64_t>(native);
    }
};

template <typename Signature>
auto Callback(const std::uint64_t target) {
    return CallbackPool<Signature>::For(target);
}

template <typename Signature>
std::uint64_t GuestFunction(Signature* const native) {
    return CallbackPool<Signature>::Guest(native);
}

}

#endif
