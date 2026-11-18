#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidFormat = 3;
constexpr std::int32_t BufferOverflow = 15;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU constrained field position: %s\n", message);
    std::abort();
}

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void Defaults() {
    std::int32_t error = Success;
    auto* position = ucfpos_open_nid_postfix(&error);
    Require(position && error == Success, "open");
    Require(ucfpos_getCategory_nid_postfix(position, &error) == 0 && ucfpos_getField_nid_postfix(position, &error) == 0, "undefined category and field");
    std::int32_t start = -1, limit = -1;
    ucfpos_getIndexes_nid_postfix(position, &start, &limit, &error);
    Require(start == 0 && limit == 0 && error == Success, "empty indexes");
    ucfpos_close_nid_postfix(position);
    error = BufferOverflow;
    position = ucfpos_open_nid_postfix(&error);
    Require(position && error == BufferOverflow, "open ignores an incoming error");
    ucfpos_close_nid_postfix(position);
    position = ucfpos_open_nid_postfix(nullptr);
    Require(position != nullptr, "open accepts a null error pointer");
    ucfpos_close_nid_postfix(position);
}

void Constraints() {
    std::int32_t error = Success;
    auto* position = ucfpos_open_nid_postfix(&error);
    ucfpos_constrainCategory_nid_postfix(position, 5, &error);
    Require(ucfpos_getCategory_nid_postfix(position, &error) == 5 && ucfpos_getField_nid_postfix(position, &error) == 0, "category constraint");
    ucfpos_constrainField_nid_postfix(position, 7, 3, &error);
    Require(ucfpos_getCategory_nid_postfix(position, &error) == 7 && ucfpos_getField_nid_postfix(position, &error) == 3, "field constraint");
    ucfpos_constrainCategory_nid_postfix(position, -9, &error);
    Require(ucfpos_getCategory_nid_postfix(position, &error) == -9 && ucfpos_getField_nid_postfix(position, &error) == 3, "a category constraint keeps the field");
    Require(error == Success, "constraints succeed");
    error = IllegalArgument;
    ucfpos_constrainField_nid_postfix(position, 1, 1, &error);
    ucfpos_constrainCategory_nid_postfix(position, 1, &error);
    Require(ucfpos_getCategory_nid_postfix(position, &error) == 0 && ucfpos_getField_nid_postfix(position, &error) == 0, "a set error returns defaults");
    std::int32_t start = 11, limit = 12;
    ucfpos_getIndexes_nid_postfix(position, &start, &limit, &error);
    Require(start == 11 && limit == 12 && error == IllegalArgument, "a set error leaves indexes untouched");
    error = Success;
    Require(ucfpos_getCategory_nid_postfix(position, &error) == -9 && ucfpos_getField_nid_postfix(position, &error) == 3, "a set error changes nothing");
    ucfpos_close_nid_postfix(position);
}

void Handles() {
    std::int32_t error = Success;
    Require(ucfpos_getCategory_nid_postfix(nullptr, &error) == 0 && error == IllegalArgument, "null handle");
    error = Success;
    ucfpos_constrainCategory_nid_postfix(nullptr, 4, &error);
    Require(error == IllegalArgument, "null handle constraint");
    alignas(8) std::array<std::uint8_t, 64> foreign{};
    auto* forged = reinterpret_cast<UConstrainedFieldPosition*>(foreign.data());
    error = Success;
    Require(ucfpos_getField_nid_postfix(forged, &error) == 0 && error == InvalidFormat, "foreign memory is rejected");
    error = Success;
    std::int32_t start = 21, limit = 22;
    ucfpos_getIndexes_nid_postfix(forged, &start, &limit, &error);
    Require(start == 21 && limit == 22 && error == InvalidFormat, "foreign memory indexes");
    ucfpos_close_nid_postfix(nullptr);
    ucfpos_close_nid_postfix(forged);
    auto* position = ucfpos_open_nid_postfix(&error);
    Require(Rejects([&] { ucfpos_getCategory_nid_postfix(position, nullptr); }), "null error pointer");
    Require(Rejects([&] { ucfpos_constrainField_nid_postfix(position, 1, 2, nullptr); }), "null error pointer in a constraint");
    error = Success;
    Require(Rejects([&] { ucfpos_getIndexes_nid_postfix(position, nullptr, &start, &error); }), "null start output");
    Require(Rejects([&] { ucfpos_getIndexes_nid_postfix(position, &start, nullptr, &error); }), "null limit output");
    ucfpos_close_nid_postfix(position);
}

void GuestPointers() {
    auto rejectsRuntime = [](auto&& call) {
        errno = 71;
        try {
            call();
        } catch (const std::runtime_error&) {
            Require(errno == 71, "errno preserved across rejected guest pointer");
            return;
        }
        Require(false, "faulted guest pointer did not throw runtime_error");
    };
    const std::size_t pageSize =
#ifdef _WIN32
        4096;
#else
        static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
#endif
    void* roPage = nullptr;
#ifdef _WIN32
    roPage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(roPage != nullptr, "VirtualAlloc failed");
#else
    roPage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(roPage != MAP_FAILED, "mmap failed");
#endif
    const std::int32_t zero = 0;
    const std::int32_t existingErr = BufferOverflow;
    std::memcpy(roPage, &zero, sizeof(zero));
    std::memcpy(static_cast<char*>(roPage) + 16, &existingErr, sizeof(existingErr));
    std::memset(static_cast<char*>(roPage) + 64, 0, 64);
#ifdef _WIN32
    DWORD oldProtect = 0;
    Require(VirtualProtect(roPage, pageSize, PAGE_READONLY, &oldProtect) != 0, "VirtualProtect failed");
#else
    Require(mprotect(roPage, pageSize, PROT_READ) == 0, "mprotect failed");
#endif
    auto* roErrorZero = static_cast<std::int32_t*>(roPage);
    auto* roErrorFailing = reinterpret_cast<std::int32_t*>(static_cast<char*>(roPage) + 16);
    auto* roForeign = reinterpret_cast<UConstrainedFieldPosition*>(static_cast<char*>(roPage) + 64);
    auto* badHandle = reinterpret_cast<UConstrainedFieldPosition*>(std::uintptr_t{0x10});
    auto* badInt = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});

    std::int32_t error = Success;
    auto* position = ucfpos_open_nid_postfix(&error);
    Require(position != nullptr, "open valid");
    ucfpos_constrainField_nid_postfix(position, 4, 2, &error);

    rejectsRuntime([&] { ucfpos_open_nid_postfix(badInt); });
    rejectsRuntime([&] { ucfpos_open_nid_postfix(roErrorZero); });
    rejectsRuntime([&] { ucfpos_close_nid_postfix(badHandle); });
    rejectsRuntime([&] { ucfpos_getCategory_nid_postfix(position, badInt); });
    rejectsRuntime([&] { ucfpos_getCategory_nid_postfix(badHandle, &error); });
    rejectsRuntime([&] { ucfpos_getCategory_nid_postfix(nullptr, roErrorZero); });
    rejectsRuntime([&] { ucfpos_getCategory_nid_postfix(roForeign, roErrorZero); });
    Require(ucfpos_getCategory_nid_postfix(position, roErrorFailing) == 0, "failing ro error short-circuits");
    rejectsRuntime([&] { ucfpos_getField_nid_postfix(badHandle, &error); });
    rejectsRuntime([&] { ucfpos_constrainCategory_nid_postfix(badHandle, 1, &error); });
    rejectsRuntime([&] { ucfpos_constrainField_nid_postfix(badHandle, 1, 2, &error); });
    std::int32_t start = 11;
    std::int32_t limit = 22;
    rejectsRuntime([&] { ucfpos_getIndexes_nid_postfix(position, badInt, &limit, &error); });
    Require(limit == 22, "limit preserved on bad start");
    rejectsRuntime([&] { ucfpos_getIndexes_nid_postfix(position, &start, badInt, &error); });
    Require(start == 11, "start preserved on bad limit");
    rejectsRuntime([&] { ucfpos_getIndexes_nid_postfix(position, roErrorZero, &limit, &error); });
    Require(limit == 22, "limit preserved on ro start");
    rejectsRuntime([&] { ucfpos_getIndexes_nid_postfix(position, &start, roErrorZero, &error); });
    Require(start == 11, "start preserved on ro limit");

    ucfpos_close_nid_postfix(position);
#ifdef _WIN32
    VirtualFree(roPage, 0, MEM_RELEASE);
#else
    munmap(roPage, pageSize);
#endif
}

}

int main() {
    Defaults();
    Constraints();
    Handles();
    GuestPointers();
    return 0;
}
