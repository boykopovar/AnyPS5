#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t NotTerminated = -124;
constexpr std::int32_t Overflow = 15;
constexpr std::int32_t Minimum = std::numeric_limits<std::int32_t>::min();
constexpr std::int32_t Maximum = std::numeric_limits<std::int32_t>::max();

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU termination check failed at line %u\n", location.line());
    std::abort();
}

enum class Outcome { Write, Warning, Failure, Unchanged };

struct Case {
    std::int32_t capacity;
    std::int32_t length;
    Outcome outcome;
};

constexpr Case cases[] {
    {4, 0, Outcome::Write}, {4, 1, Outcome::Write}, {4, 3, Outcome::Write},
    {1, 0, Outcome::Write}, {Maximum, 3, Outcome::Write},
    {0, 0, Outcome::Warning}, {3, 3, Outcome::Warning}, {Maximum, Maximum, Outcome::Warning},
    {0, 1, Outcome::Failure}, {2, 3, Outcome::Failure}, {-1, 0, Outcome::Failure},
    {Minimum, 0, Outcome::Failure}, {0, Maximum, Outcome::Failure},
    {-1, -1, Outcome::Unchanged}, {0, -1, Outcome::Unchanged},
    {Maximum, Minimum, Outcome::Unchanged}
};

constexpr std::int32_t statuses[] {0, NotTerminated, -128, -1, Minimum, 1, Overflow, Maximum};

std::int32_t Expected(std::int32_t status, Outcome outcome) {
    if (status > 0) return status;
    switch (outcome) {
    case Outcome::Write: return status == NotTerminated ? 0 : status;
    case Outcome::Warning: return NotTerminated;
    case Outcome::Failure: return Overflow;
    case Outcome::Unchanged: return status;
    }
    std::abort();
}

template<typename T, auto TTerminate>
void Check() {
    for (const auto& test : cases) {
        for (const auto initial : statuses) {
            std::array<T, 12> buffer;
            for (std::size_t index = 0; index < buffer.size(); ++index)
                buffer[index] = static_cast<T>(0x41 + index);
            auto expected = buffer;
            if (initial <= 0 && test.outcome == Outcome::Write)
                expected[4 + static_cast<std::size_t>(test.length)] = 0;
            struct { std::int32_t before, value, after; } status {0x12345678, initial, 0x23456789};
            Require(TTerminate(buffer.data() + 4, test.capacity, test.length, &status.value) == test.length);
            Require(buffer == expected);
            Require(status.value == Expected(initial, test.outcome));
            Require(status.before == 0x12345678 && status.after == 0x23456789);

            const auto before = buffer;
            Require(TTerminate(buffer.data() + 4, test.capacity, test.length, nullptr) == test.length);
            Require(buffer == before);

            status.value = initial;
            if (initial <= 0 && test.outcome == Outcome::Write) {
                try {
                    TTerminate(nullptr, test.capacity, test.length, &status.value);
                    Require(false);
                } catch (const std::invalid_argument&) {
                    Require(status.value == initial);
                }
            } else {
                Require(TTerminate(nullptr, test.capacity, test.length, &status.value) == test.length);
                Require(status.value == Expected(initial, test.outcome));
            }
            Require(status.before == 0x12345678 && status.after == 0x23456789);
        }
        Require(TTerminate(nullptr, test.capacity, test.length, nullptr) == test.length);
    }
}

enum class Access { ReadWrite, ReadOnly, None };

struct Pages {
    char* begin = nullptr;
    std::size_t size = 0;

    Pages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        begin = static_cast<char*>(VirtualAlloc(nullptr, size * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(begin != nullptr);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        begin = static_cast<char*>(mapped);
#endif
    }

    ~Pages() {
#ifdef _WIN32
        VirtualFree(begin, 0, MEM_RELEASE);
#else
        munmap(begin, size * 2);
#endif
    }

    Pages(const Pages&) = delete;
    Pages& operator=(const Pages&) = delete;

    char* Second() const { return begin + size; }

    void Protect(Access access) {
#ifdef _WIN32
        const DWORD protection = access == Access::ReadWrite ? PAGE_READWRITE
                               : access == Access::ReadOnly ? PAGE_READONLY : PAGE_NOACCESS;
        DWORD previous = 0;
        Require(VirtualProtect(Second(), size, protection, &previous) != 0);
#else
        const int protection = access == Access::ReadWrite ? PROT_READ | PROT_WRITE
                             : access == Access::ReadOnly ? PROT_READ : PROT_NONE;
        Require(mprotect(Second(), size, protection) == 0);
#endif
    }
};

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

std::int32_t Read(const std::int32_t* status) {
    std::int32_t value = 0;
    std::memcpy(&value, status, sizeof(value));
    return value;
}

template<typename T, auto TTerminate>
void GuestMemory() {
    Pages pages;
    auto* status = reinterpret_cast<std::int32_t*>(pages.Second());
    auto* unitsBeforeSecond = reinterpret_cast<T*>(pages.Second()) - 2;
    std::array<T, 4> buffer{};
    std::int32_t value = 0;

    auto* unreadable = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    errno = 71;
    Require(Throws([&] { TTerminate(buffer.data(), 4, 1, unreadable); }));
    Require(errno == 71);

    for (const std::int32_t initial : {0, -1}) {
        *status = initial;
        pages.Protect(Access::ReadOnly);
        buffer.fill(static_cast<T>(0x41));
        Require(TTerminate(buffer.data(), 4, 2, status) == 2);
        Require(buffer[2] == 0 && buffer[1] == static_cast<T>(0x41) && Read(status) == initial);
        Require(TTerminate(buffer.data(), 4, -1, status) == -1);
        Require(Throws([&] { TTerminate(buffer.data(), 3, 3, status); }));
        Require(Throws([&] { TTerminate(buffer.data(), 2, 3, status); }));
        Require(Read(status) == initial);
        pages.Protect(Access::ReadWrite);
    }
    *status = NotTerminated;
    pages.Protect(Access::ReadOnly);
    Require(Throws([&] { TTerminate(buffer.data(), 4, 1, status); }));
    pages.Protect(Access::ReadWrite);
    *status = Overflow;
    pages.Protect(Access::ReadOnly);
    Require(TTerminate(buffer.data(), 4, 1, status) == 1);
    Require(TTerminate(buffer.data(), 1, 3, status) == 3);
    Require(Read(status) == Overflow);
    pages.Protect(Access::None);
    Require(Throws([&] { TTerminate(buffer.data(), 4, 1, status); }));
    pages.Protect(Access::ReadWrite);

    for (const auto access : {Access::ReadOnly, Access::None}) {
        pages.Protect(access);
        value = NotTerminated;
        Require(Throws([&] { TTerminate(reinterpret_cast<T*>(pages.Second()), 4, 0, &value); }));
        Require(value == NotTerminated);
        Require(TTerminate(reinterpret_cast<T*>(pages.Second()), 4, 4, &value) == 4);
        Require(value == NotTerminated);
        value = 0;
        Require(TTerminate(reinterpret_cast<T*>(pages.Second()), 4, 5, &value) == 5);
        Require(value == Overflow);
        value = 0;
        Require(TTerminate(unitsBeforeSecond, 4, 1, &value) == 1);
        Require(unitsBeforeSecond[1] == 0 && value == 0);
        Require(Throws([&] { TTerminate(unitsBeforeSecond, 4, 2, &value); }));
        Require(value == 0);
        pages.Protect(Access::ReadWrite);
    }
}

}

int main() {
    Check<char, u_terminateChars_nid_postfix>();
    Check<char16_t, u_terminateUChars_nid_postfix>();
    Check<std::int32_t, u_terminateUChar32s_nid_postfix>();
    Check<std::uint16_t, u_terminateWChars_nid_postfix>();
    GuestMemory<char, u_terminateChars_nid_postfix>();
    GuestMemory<char16_t, u_terminateUChars_nid_postfix>();
    GuestMemory<std::int32_t, u_terminateUChar32s_nid_postfix>();
    GuestMemory<std::uint16_t, u_terminateWChars_nid_postfix>();
    return 0;
}
