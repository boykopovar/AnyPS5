#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

static int allocationBudget = -1;
static const char* activeCase = "no allocation";
static int activeBudget = 0;

void* operator new(std::size_t bytes) {
    if (allocationBudget == 0) throw std::bad_alloc();
    if (allocationBudget > 0) --allocationBudget;
    if (void* pointer = std::malloc(bytes ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

namespace {
constexpr auto existingName = "ANYPS5_ALLOCATION_EXISTING_LONG_NAME_82BA";
constexpr auto neighborName = "ANYPS5_ALLOCATION_NEIGHBOR_LONG_NAME_82BA";
constexpr auto missingName = "ANYPS5_ALLOCATION_MISSING_LONG_NAME_82BA";
constexpr auto originalValue = "original-long-value-before-allocation-failure";
constexpr auto updatedValue = "updated-long-value-after-successful-allocation";

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest environment allocation: %s budget=%d: %s\n", activeCase, activeBudget, message);
    std::abort();
}

void MissingRemoval(char**& storage) {
    auto* const previous = storage;
    errno = 73;
    allocationBudget = 0;
    const int result = unsetenv_nid_postfix(missingName);
    const int error = errno;
    allocationBudget = -1;
    Require(result == 0, "missing unsetenv failed without allocation budget");
    Require(error == 73, "missing unsetenv changed errno");
    Require(storage == previous, "missing unsetenv replaced the table");
}

void NoAllocation(char**& storage) {
    MissingRemoval(storage);
    Require(setenv_nid_postfix(existingName, originalValue, 1) == 0, "setup failed");
    char* const original = getenv_nid_postfix(existingName);
    auto* const previous = storage;
    MissingRemoval(storage);
    errno = 73;
    allocationBudget = 0;
    const int result = setenv_nid_postfix(existingName, updatedValue, 0);
    char* const found = getenv_nid_postfix(existingName);
    char* const absent = getenv_nid_postfix(missingName);
    const int error = errno;
    allocationBudget = -1;
    Require(result == 0 && found == original && absent == nullptr, "nonallocating lookup or overwrite failed");
    Require(error == 73, "nonallocating operation changed errno");
    Require(storage == previous && storage[1] == nullptr, "nonallocating operation changed the table");
    Require(std::strcmp(original, originalValue) == 0, "nonallocating operation changed the value");
}

void Transaction(char**& storage, int operation) {
    const char* cases[] = {"setenv replace", "unsetenv", "putenv replace", "setenv append", "putenv append"};
    activeCase = cases[operation];
    bool failed = false;
    bool succeeded = false;
    for (activeBudget = 0; activeBudget < 64; ++activeBudget) {
        storage = nullptr;
        Require(setenv_nid_postfix(existingName, originalValue, 1) == 0, "existing setup failed");
        Require(setenv_nid_postfix(neighborName, "neighbor", 1) == 0, "neighbor setup failed");
        auto* const previous = storage;
        char* const firstEntry = storage[0];
        char* const secondEntry = storage[1];
        char* const original = getenv_nid_postfix(existingName);
        char* const neighbor = getenv_nid_postfix(neighborName);
        char borrowedExisting[] = "ANYPS5_ALLOCATION_EXISTING_LONG_NAME_82BA=borrowed";
        char borrowedMissing[] = "ANYPS5_ALLOCATION_MISSING_LONG_NAME_82BA=borrowed";
        errno = 0;
        allocationBudget = activeBudget;
        const int result = operation == 0 ? setenv_nid_postfix(existingName, updatedValue, 1)
            : operation == 1 ? unsetenv_nid_postfix(existingName)
            : operation == 2 ? putenv_nid_postfix(borrowedExisting)
            : operation == 3 ? setenv_nid_postfix(missingName, updatedValue, 1)
            : putenv_nid_postfix(borrowedMissing);
        const int error = errno;
        allocationBudget = -1;
        if (result != 0) {
            failed = true;
            Require(result == -1 && error == 12, "allocation failure did not return ENOMEM");
            Require(storage == previous && storage[0] == firstEntry && storage[1] == secondEntry && storage[2] == nullptr,
                "allocation failure changed the table");
            Require(getenv_nid_postfix(existingName) == original && std::strcmp(original, originalValue) == 0,
                "allocation failure changed the original value");
            Require(getenv_nid_postfix(missingName) == nullptr, "allocation failure published an added entry");
        } else {
            succeeded = true;
            const char* name = operation >= 3 ? missingName : existingName;
            char* const value = getenv_nid_postfix(name);
            if (operation == 1) {
                Require(value == nullptr && storage[0] == secondEntry && storage[1] == nullptr, "successful removal was not published");
            } else {
                const char* expected = operation == 2 || operation == 4 ? "borrowed" : updatedValue;
                Require(value != nullptr && std::strcmp(value, expected) == 0, "successful mutation has wrong value");
                const int index = operation >= 3 ? 2 : 0;
                Require(std::strchr(storage[index], '=') + 1 == value && storage[operation >= 3 ? 3 : 2] == nullptr,
                    "successful mutation was not published");
                if (operation == 2 || operation == 4)
                    Require(storage[index] == (operation == 2 ? borrowedExisting : borrowedMissing), "putenv copied borrowed storage");
                if (operation >= 3)
                    Require(getenv_nid_postfix(existingName) == original && std::strcmp(original, originalValue) == 0,
                        "append changed the original value");
            }
        }
        Require(getenv_nid_postfix(neighborName) == neighbor && std::strcmp(neighbor, "neighbor") == 0,
            "mutation invalidated an unrelated value");
        storage = nullptr;
        if (succeeded) break;
    }
    Require(failed && succeeded, "did not exercise both allocation failure and success");
}
}

int main() {
    char** storage = nullptr;
    BindGuestEnvironment_nid_no_patch(&storage);
    NoAllocation(storage);
    for (int operation = 0; operation < 5; ++operation) Transaction(storage, operation);
    BindGuestEnvironment_nid_no_patch(nullptr);
    std::puts("guest environment allocation checks passed");
}
