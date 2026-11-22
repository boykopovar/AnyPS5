#include "prx/libc/include/Api.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <typeinfo>

namespace {

using VirtualCall = void (APS5_VABI*)(void*);

struct ControlBlock {
    const VirtualCall* vtable;
    std::int64_t sharedOwners;
    std::int64_t weakOwners;
    std::atomic<unsigned> callbacks{};
};

static_assert(offsetof(ControlBlock, sharedOwners) == 8);
static_assert(offsetof(ControlBlock, weakOwners) == 16);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void APS5_VABI Finish(void* self) {
    static_cast<ControlBlock*>(self)->callbacks.fetch_add(1, std::memory_order_relaxed);
}

constexpr std::array<VirtualCall, 5> Vtable{nullptr, nullptr, nullptr, nullptr, Finish};

void CheckLock() {
    constexpr unsigned ThreadCount = 8;
    constexpr unsigned Attempts = 64;
    ControlBlock control{Vtable.data(), 0, 0};
    std::atomic<bool> success{true};
    {
        std::array<std::jthread, ThreadCount> workers;
        for (auto& worker : workers) {
            worker = std::jthread([&] {
                for (unsigned i = 0; i < Attempts; ++i) {
                    if (_ZNSt3__119__shared_weak_count4lockEv_nid_postfix(&control) != &control)
                        success.store(false, std::memory_order_relaxed);
                }
            });
        }
    }
    Require(success.load() && control.sharedOwners == ThreadCount * Attempts,
            "weak lock did not retain each live shared owner");
    control.sharedOwners = -1;
    Require(_ZNSt3__119__shared_weak_count4lockEv_nid_postfix(&control) == nullptr && control.sharedOwners == -1,
            "weak lock resurrected an expired shared owner");
    control.sharedOwners = -2;
    Require(_ZNSt3__119__shared_weak_count4lockEv_nid_postfix(&control) == nullptr && control.sharedOwners == -2,
            "weak lock promoted a negative shared owner count");
}

void CheckRelease() {
    ControlBlock control{Vtable.data(), -1, 1};
    _ZNSt3__119__shared_weak_count14__release_weakEv_nid_postfix(&control);
    Require(control.weakOwners == 0 && control.callbacks.load() == 0,
            "non-final weak release must retain the control block");
    _ZNSt3__119__shared_weak_count14__release_weakEv_nid_postfix(&control);
    Require(control.callbacks.load() == 1, "final weak release did not call the derived callback exactly once");
    bool rejected = false;
    try {
        _ZNSt3__119__shared_weak_count14__release_weakEv_nid_postfix(&control);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected && control.callbacks.load() == 1, "duplicate weak release finalized retained storage twice");

    constexpr unsigned ThreadCount = 8;
    ControlBlock concurrent{Vtable.data(), -1, ThreadCount - 1};
    {
        std::array<std::jthread, ThreadCount> workers;
        for (auto& worker : workers)
            worker = std::jthread([&] { _ZNSt3__119__shared_weak_count14__release_weakEv_nid_postfix(&concurrent); });
    }
    Require(concurrent.callbacks.load() == 1, "concurrent weak releases did not finalize exactly once");
}

void CheckTypeInfo() {
    const auto* type = reinterpret_cast<const __cxxabiv1::__vmi_class_type_info*>(&_ZTINSt3__119__shared_weak_countE_nid_postfix);
    const auto* addressPoint = *reinterpret_cast<const std::type_info* const* const*>(type);
    Require(std::strcmp(addressPoint[-1]->name(), "N10__cxxabiv121__vmi_class_type_infoE") == 0 &&
            std::strcmp(type->name(), "NSt3__119__shared_weak_countE") == 0,
            "weak control block has incorrect guest VMI RTTI");
    Require(type->__flags == 0 && type->__base_count == 1 &&
            type->__base_info[0].__offset_flags == 0 &&
            std::strcmp(type->__base_info[0].__base_type->name(), "NSt3__114__shared_countE") == 0,
            "weak control block RTTI must describe one private base at offset zero");
}

}

int main() {
    CheckLock();
    CheckRelease();
    CheckTypeInfo();

    ControlBlock block{Vtable.data(), 0, 0};
    Require(_ZNKSt3__119__shared_weak_count13__get_deleterERKSt9type_info_nid_postfix(&block, &typeid(int)) == nullptr,
            "base control block must report no custom deleter");
    _ZNSt3__119__shared_weak_countD2Ev_nid_postfix(&block);
    _ZNSt3__114__shared_countD2Ev_nid_postfix(&block);
    Require(block.callbacks.load() == 0, "base destructors must leave deletion to the derived control block");

    bool rejected = false;
    try {
        _ZNSt3__119__shared_weak_count4lockEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "weak lock accepted a null control block");
    std::puts("libc++ shared pointer control block checks passed");
    return 0;
}
