#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <typeinfo>

namespace Dinkum::threads {
struct lock_error : std::runtime_error {};
struct thread_resource_error : std::runtime_error {};
}

extern "C" {
extern const void* const _ZTVSt13runtime_error_nid_postfix[];
}

namespace {

using Destructor = void (APS5_VABI *)(DinkumThreadError*);
using Thrower = void (APS5_VABI *)();
using What = const char* (APS5_VABI *)(const DinkumThreadError*);
using Doraise = void (APS5_VABI *)(const DinkumThreadError*);

std::array<void*, 8> released {};
std::size_t releaseCount = 0;
bool failAllocation = false;

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "Dinkum thread error check failed at line %d\n", line);
    std::abort();
}
#define REQUIRE(value) Require((value), __LINE__)

void* APS5_VABI Allocate(std::size_t size) { return failAllocation ? nullptr : std::malloc(size); }
void APS5_VABI Free(void* pointer) {
    REQUIRE(releaseCount < released.size());
    released[releaseCount++] = pointer;
    std::free(pointer);
}
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return Allocate(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return Reallocate(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = Allocate(size);
    return *pointer ? 0 : 12;
}

void ResetReleases() {
    released.fill(nullptr);
    releaseCount = 0;
}

void* Populate(DinkumThreadError& object, std::size_t capacity) {
    object = {};
    object.message.reserved = 0x8172635445362718;
    object.message.capacity = capacity;
    object.message.size = 4;
    char* data = object.message.buffer;
    void* allocation = nullptr;
    if (capacity > 15) {
        data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
        object.message.pointer = data;
        allocation = capacity + 1 >= 0x1000 ? reinterpret_cast<void**>(data)[-1] : data;
    }
    std::memcpy(data, "test", 5);
    return allocation;
}

void CheckDestructorCallbacks(const std::array<Destructor, 2>& complete, const std::array<Destructor, 2>& deleting) {
    for (std::size_t capacity : {15, 31, 4095, 8191}) {
        for (auto destroy : complete) {
            struct Guarded {
                std::uint64_t before;
                DinkumThreadError object;
                std::uint64_t after;
            } guarded {0x123456789abcdef0, {}, 0xfedcba9876543210};
            void* allocation = Populate(guarded.object, capacity);
            ResetReleases();
            destroy(&guarded.object);
            REQUIRE(releaseCount == (capacity > 15 ? 1 : 0));
            if (allocation) REQUIRE(released[0] == allocation);
            REQUIRE(guarded.before == 0x123456789abcdef0 && guarded.after == 0xfedcba9876543210);
            REQUIRE(guarded.object.vtable == _ZTVSt13runtime_error_nid_postfix + 2);
            REQUIRE(guarded.object.message.reserved == 0x8172635445362718);
            REQUIRE(guarded.object.message.capacity == 15 && guarded.object.message.size == 0);
            REQUIRE(guarded.object.message.buffer[0] == 0);
        }
        for (auto destroy : deleting) {
            auto* object = static_cast<DinkumThreadError*>(_Znwm_nid_postfix(sizeof(DinkumThreadError)));
            void* allocation = Populate(*object, capacity);
            ResetReleases();
            destroy(object);
            REQUIRE(releaseCount == (capacity > 15 ? 2 : 1));
            if (allocation) REQUIRE(released[0] == allocation);
            REQUIRE(released[releaseCount - 1] == object);
        }
    }
}

void CheckDestructors() {
    CheckDestructorCallbacks({
        _ZN6Dinkum7threads10lock_errorD1Ev_nid_postfix,
        _ZN6Dinkum7threads21thread_resource_errorD1Ev_nid_postfix
    }, {
        _ZN6Dinkum7threads10lock_errorD0Ev_nid_postfix,
        _ZN6Dinkum7threads21thread_resource_errorD0Ev_nid_postfix
    });
}

const void* const* CheckObject(const void* pointer, const char* name, const char* message) {
    const auto* object = static_cast<const DinkumThreadError*>(pointer);
    const auto* table = static_cast<const void* const*>(object->vtable);
    const auto* type = static_cast<const std::type_info*>(table[-1]);
    REQUIRE(table[-2] == nullptr);
    REQUIRE(std::strcmp(type->name(), name) == 0);
    REQUIRE(std::strcmp(std::bit_cast<What>(table[2])(object), message) == 0);
    REQUIRE(object->message.size == std::strlen(message));
    REQUIRE(object->message.capacity == (object->message.size > 15 ? 31 : 15));
    const DinkumThreadError before = *object;
    std::bit_cast<Doraise>(table[3])(object);
    REQUIRE(std::memcmp(object, &before, sizeof(before)) == 0);
    return table;
}

void CheckThrows() {
    std::array<const void* const*, 2> tables {};
    ResetReleases();
    bool caught = false;
    try { _ZN6Dinkum7threads17_Throw_lock_errorEv_nid_postfix(); }
    catch (const Dinkum::threads::thread_resource_error&) { REQUIRE(false); }
    catch (const Dinkum::threads::lock_error& error) {
        caught = true;
        tables[0] = CheckObject(&error, "N6Dinkum7threads10lock_errorE", "lock error");
    }
    REQUIRE(caught && releaseCount == 0);

    caught = false;
    try { _ZN6Dinkum7threads21_Throw_resource_errorEv_nid_postfix(); }
    catch (const Dinkum::threads::lock_error&) { REQUIRE(false); }
    catch (const Dinkum::threads::thread_resource_error& error) {
        caught = true;
        tables[1] = CheckObject(&error, "N6Dinkum7threads21thread_resource_errorE", "thread resource error");
    }
    REQUIRE(caught && releaseCount == 1);
    CheckDestructorCallbacks({
        std::bit_cast<Destructor>(tables[0][0]), std::bit_cast<Destructor>(tables[1][0])
    }, {
        std::bit_cast<Destructor>(tables[0][1]), std::bit_cast<Destructor>(tables[1][1])
    });

    const std::array<Thrower, 2> throwers {
        _ZN6Dinkum7threads17_Throw_lock_errorEv_nid_postfix,
        _ZN6Dinkum7threads21_Throw_resource_errorEv_nid_postfix
    };
    for (auto raise : throwers) {
        ResetReleases();
        caught = false;
        try { raise(); }
        catch (const std::logic_error&) { REQUIRE(false); }
        catch (const std::runtime_error&) { caught = true; }
        REQUIRE(caught);
        caught = false;
        try { raise(); }
        catch (const std::exception&) { caught = true; }
        REQUIRE(caught);
    }
}

void CheckAllocationFailure() {
    ResetReleases();
    failAllocation = true;
    bool caught = false;
    try { _ZN6Dinkum7threads21_Throw_resource_errorEv_nid_postfix(); }
    catch (const std::bad_alloc&) { caught = true; }
    failAllocation = false;
    REQUIRE(caught && releaseCount == 0);
    CheckThrows();
}

}

int main(int argc, char** argv) {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free),
        reinterpret_cast<void*>(Calloc), reinterpret_cast<void*>(Reallocate),
        reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign), nullptr, nullptr, nullptr
    };
    ApplicationHeapRegister_nid_no_patch(api.data());
    REQUIRE(argc == 2);
    if (std::strcmp(argv[1], "destructors") == 0) CheckDestructors();
    else if (std::strcmp(argv[1], "throws") == 0) CheckThrows();
    else if (std::strcmp(argv[1], "allocation_failure") == 0) CheckAllocationFailure();
    else REQUIRE(false);
    std::puts("Dinkum thread error checks passed");
}
