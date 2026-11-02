#include "prx/libc/include/GuestArena.hpp"
#include <crt_externs.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" [[noreturn]] void Aps5StartGuest_nid_no_patch(void (*entry)(void*, void*), void* block);
extern "C" void Aps5RunOnMainThread_nid_no_patch(void (*function)(void*), void* context);

namespace {

constexpr std::uintptr_t MappingAlignment = 0x10000;
constexpr std::size_t MappingBytes = 0x10000;
constexpr std::size_t NeighbourBytes = std::size_t {1} << 20;

std::uintptr_t originalStack = 0;
std::string originalArgument;
bool originalInArena = false;
mach_vm_address_t neighbour = 0;

bool RegionOf(mach_vm_address_t address, mach_vm_address_t& begin, mach_vm_address_t& end, std::uint32_t& tag) {
    mach_vm_address_t base = address;
    mach_vm_size_t size = 0;
    natural_t depth = 0;
    vm_region_submap_info_data_64_t info {};
    mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
    if (mach_vm_region_recurse(mach_task_self(), &base, &size, &depth, reinterpret_cast<vm_region_recurse_info_t>(&info), &count) != KERN_SUCCESS || base > address) return false;
    begin = base;
    end = base + size;
    tag = info.user_tag;
    return true;
}

mach_vm_address_t MapStackNeighbour(std::uintptr_t stack) {
    mach_vm_address_t low = 0, end = 0;
    std::uint32_t tag = 0;
    if (!RegionOf(stack, low, end, tag) || tag != VM_MEMORY_STACK) return 0;
    for (;;) {
        mach_vm_address_t begin = 0, previousEnd = 0;
        if (!RegionOf(low - 1, begin, previousEnd, tag) || tag != VM_MEMORY_STACK || previousEnd != low) break;
        low = begin;
    }
    mach_vm_address_t at = low - NeighbourBytes;
    if (mach_vm_deallocate(mach_task_self(), at, NeighbourBytes) != KERN_SUCCESS) return 0;
    if (mach_vm_allocate(mach_task_self(), &at, NeighbourBytes, VM_MAKE_TAG(VM_MEMORY_STACK)) != KERN_SUCCESS) return 0;
    return at;
}

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "check failed: %s\n", what);
    std::exit(1);
}

bool InArena(std::uintptr_t address) {
    std::uintptr_t base = 0;
    std::size_t bytes = 0;
    GuestArena::GuestArenaRange_nid_postfix(&base, &bytes);
    return address >= base && address < base + bytes;
}

void CheckMainThread(void*) {
    int local = 0;
    Require(pthread_main_np() != 0, "the check runs on the main thread");
    const auto address = reinterpret_cast<std::uintptr_t>(&local);
    if (originalInArena) {
        Require(!InArena(address), "the main thread stack is outside the guest arena");
        Require(address != originalStack, "the main thread stack moved");
        const auto old = reinterpret_cast<const void*>(originalStack & ~(MappingAlignment - 1));
        try {
            GuestArena::GuestArenaMarkUsed_nid_postfix(old, MappingBytes);
        } catch (const std::runtime_error&) {
            Require(false, "a fixed guest mapping fits where the main thread stack was");
        }
        GuestArena::GuestArenaRelease_nid_postfix(old, MappingBytes);
    }
    if (neighbour != 0) {
        mach_vm_address_t begin = 0, end = 0;
        std::uint32_t tag = 0;
        Require(RegionOf(neighbour, begin, end, tag) && tag == VM_MEMORY_STACK, "a stack region next to the main stack survived the move");
    }
    Require(originalArgument == (*_NSGetArgv())[0], "argv survived the move");
    Require(std::getenv("PATH") != nullptr, "the environment survived the move");
}

void Guest(void*, void*) {
    Aps5RunOnMainThread_nid_no_patch(CheckMainThread, nullptr);
    std::exit(0);
}

}

int main(int, char** argv) {
    int local = 0;
    originalStack = reinterpret_cast<std::uintptr_t>(&local);
    originalInArena = InArena(originalStack);
    originalArgument = argv[0];
    if (originalInArena) neighbour = MapStackNeighbour(originalStack);
    Aps5StartGuest_nid_no_patch(Guest, nullptr);
}
