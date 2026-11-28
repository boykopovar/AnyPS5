#include "prx/libc/include/HostThreadSlot.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#ifdef HOST_THREAD_SLOT_TEB
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "HostThreadSlot check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

static void CheckHostThreadSlot() {
    struct PrimaryTag {};
    using PrimarySlot = HostThreadSlot<std::uintptr_t, PrimaryTag>;
    constexpr std::uintptr_t mainValue = static_cast<std::uintptr_t>(0x1122334455667788ULL);
    Require(PrimarySlot::Get() == 0);
    PrimarySlot::Set(mainValue);
    Require(PrimarySlot::Get() == mainValue);

    struct SubWordValue {
        std::uint16_t head;
        std::uint8_t tail;
    };
    struct SubWordTag {};
    using SubWordSlot = HostThreadSlot<SubWordValue, SubWordTag>;
    const auto initialSubWord = SubWordSlot::Get();
    Require(initialSubWord.head == 0 && initialSubWord.tail == 0);
    SubWordSlot::Set(SubWordValue{0x4a7bu, 0xc9u});
    const auto roundTripSubWord = SubWordSlot::Get();
    Require(roundTripSubWord.head == 0x4a7bu && roundTripSubWord.tail == 0xc9u);

    struct ByteTag {};
    using ByteSlot = HostThreadSlot<std::uint8_t, ByteTag>;
    Require(ByteSlot::Get() == 0);
    ByteSlot::Set(0x5au);
    Require(ByteSlot::Get() == 0x5au);

    std::uintptr_t workerInitial = 1;
    std::uintptr_t workerObserved = 0;
    std::uint8_t workerByteInitial = 1;
    std::uint8_t workerByteObserved = 0;
    std::thread worker([&] {
        workerInitial = PrimarySlot::Get();
        PrimarySlot::Set(static_cast<std::uintptr_t>(0x99aabbccddeeff00ULL));
        workerObserved = PrimarySlot::Get();
        workerByteInitial = ByteSlot::Get();
        ByteSlot::Set(0x3cu);
        workerByteObserved = ByteSlot::Get();
    });
    worker.join();
    Require(workerInitial == 0);
    Require(workerObserved == static_cast<std::uintptr_t>(0x99aabbccddeeff00ULL));
    Require(workerByteInitial == 0);
    Require(workerByteObserved == 0x3cu);
    Require(PrimarySlot::Get() == mainValue);
    Require(ByteSlot::Get() == 0x5au);

#ifdef HOST_THREAD_SLOT_TEB
    class TlsIndexReserve {
    public:
        TlsIndexReserve() {
            indices.fill(TLS_OUT_OF_INDEXES);
            for (auto& index : indices) {
                index = TlsAlloc();
                Require(index != TLS_OUT_OF_INDEXES);
            }
        }

        ~TlsIndexReserve() {
            for (const DWORD index : indices) {
                if (index != TLS_OUT_OF_INDEXES) TlsFree(index);
            }
        }

        TlsIndexReserve(const TlsIndexReserve&) = delete;
        TlsIndexReserve& operator=(const TlsIndexReserve&) = delete;

    private:
        std::array<DWORD, 64> indices{};
    };

    struct ExpansionTag {};
    using ExpansionSlot = HostThreadSlot<std::uintptr_t, ExpansionTag>;
    TlsIndexReserve reserved;

    SetLastError(ERROR_INVALID_HANDLE);
    const auto initialExpansionValue = ExpansionSlot::Get();
    Require(GetLastError() == ERROR_INVALID_HANDLE);
    Require(initialExpansionValue == 0);

    SetLastError(ERROR_ACCESS_DENIED);
    ExpansionSlot::Set(0x123456789abcdef0ULL);
    Require(GetLastError() == ERROR_ACCESS_DENIED);
    Require(ExpansionSlot::Get() == 0x123456789abcdef0ULL);

    SetLastError(ERROR_INVALID_PARAMETER);
    ExpansionSlot::Set(0x0fedcba987654321ULL);
    Require(GetLastError() == ERROR_INVALID_PARAMETER);
    Require(ExpansionSlot::Get() == 0x0fedcba987654321ULL);

    DWORD threadErrorAfterGet = 0;
    DWORD threadErrorAfterFirstSet = 0;
    DWORD threadErrorAfterSecondSet = 0;
    std::uintptr_t threadInitialValue = 1;
    std::uintptr_t threadFirstValue = 0;
    std::uintptr_t threadSecondValue = 0;

    std::thread expansionThread([&] {
        SetLastError(ERROR_INVALID_HANDLE);
        threadInitialValue = ExpansionSlot::Get();
        threadErrorAfterGet = GetLastError();

        SetLastError(ERROR_ACCESS_DENIED);
        ExpansionSlot::Set(0x2468ace013579bdfULL);
        threadErrorAfterFirstSet = GetLastError();
        threadFirstValue = ExpansionSlot::Get();

        SetLastError(ERROR_INVALID_PARAMETER);
        ExpansionSlot::Set(0xfdb975310eca8642ULL);
        threadErrorAfterSecondSet = GetLastError();
        threadSecondValue = ExpansionSlot::Get();
    });
    expansionThread.join();

    Require(threadInitialValue == 0);
    Require(threadErrorAfterGet == ERROR_INVALID_HANDLE);
    Require(threadFirstValue == 0x2468ace013579bdfULL);
    Require(threadErrorAfterFirstSet == ERROR_ACCESS_DENIED);
    Require(threadSecondValue == 0xfdb975310eca8642ULL);
    Require(threadErrorAfterSecondSet == ERROR_INVALID_PARAMETER);
    Require(ExpansionSlot::Get() == 0x0fedcba987654321ULL);
#endif
}

int main() {
    CheckHostThreadSlot();
}
