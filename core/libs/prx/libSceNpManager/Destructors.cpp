#include "prx/libSceNpCommon/include/Api.hpp"
#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::size_t NpObjectHeaderSize = 16;

struct TicketObject {
    const void* const* vtable;
    NpAllocatorEx* allocator;
    void* data;
    std::size_t size;
};
static_assert(sizeof(TicketObject) == 0x20);

void* const NpTicketVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np8NpTicketD1Ev),
    reinterpret_cast<void*>(&_ZN3sce2np8NpTicketD0Ev),
};

void ReleaseTicket(const TicketObject& ticket) {
    if (ticket.data == nullptr || ticket.allocator == nullptr) return;
    NpAllocatorEx allocatorEx{};
    if (!GuestCopyIn_nid_no_patch(&allocatorEx, ticket.allocator, sizeof(allocatorEx))) return;
    if (allocatorEx.allocator.release == nullptr) return;
    allocatorEx.allocator.release(ticket.data, allocatorEx.allocator.context);
}

void ReleaseObject(void* object) {
    if (object == nullptr) return;
    const auto address = reinterpret_cast<std::uintptr_t>(object);
    if (address < NpObjectHeaderSize) return;
    auto* const header = reinterpret_cast<std::uint8_t*>(address - NpObjectHeaderSize);
    std::uint8_t headerBytes[NpObjectHeaderSize]{};
    if (!GuestCopyIn_nid_no_patch(headerBytes, header, sizeof(headerBytes))) return;
    NpAllocator* allocatorPtr = nullptr;
    std::memcpy(&allocatorPtr, headerBytes, sizeof(allocatorPtr));
    if (allocatorPtr == nullptr) return;
    NpAllocator allocator{};
    if (!GuestCopyIn_nid_no_patch(&allocator, allocatorPtr, sizeof(allocator))) return;
    if (allocator.release == nullptr) return;
    allocator.release(header, allocator.context);
}

bool DestroyTicket(void* self) {
    if (self == nullptr) return false;
    TicketObject ticket{};
    if (!GuestCopyIn_nid_no_patch(&ticket, self, sizeof(ticket))) return false;
    const void* const* const vtable = &NpTicketVtable[2];
    if (!GuestCopyOut_nid_no_patch(static_cast<std::uint8_t*>(self) + offsetof(TicketObject, vtable), &vtable, sizeof(vtable))) {
        return false;
    }
    ReleaseTicket(ticket);
    return true;
}

}

extern "C" {

void APS5_VABI _ZN3sce2np10NpOnlineIdD0Ev(void* self) {
    ReleaseObject(self);
}

void APS5_VABI _ZN3sce2np4NpIdD0Ev(void* self) {
    ReleaseObject(self);
}

void APS5_VABI _ZN3sce2np4UserD0Ev(void* self) {
    ReleaseObject(self);
}

void APS5_VABI _ZN3sce2np12NpTitleTokenD0Ev(void* self) {
    ReleaseObject(self);
}

void APS5_VABI _ZN3sce2np13NpAccessTokenD0Ev(void* self) {
    ReleaseObject(self);
}

void APS5_VABI _ZN3sce2np8NpTicketD1Ev(void* self) {
    DestroyTicket(self);
}

void APS5_VABI _ZN3sce2np8NpTicketD2Ev(void* self) {
    _ZN3sce2np8NpTicketD1Ev(self);
}

void APS5_VABI _ZN3sce2np8NpTicketD0Ev(void* self) {
    if (DestroyTicket(self)) ReleaseObject(self);
}

void APS5_VABI _ZN3sce2np8NpTicket5ClearEv(void* self) {
    if (self == nullptr) return;
    TicketObject ticket{};
    if (!GuestCopyIn_nid_no_patch(&ticket, self, sizeof(ticket))) return;
    ReleaseTicket(ticket);
    void* const clearedData = nullptr;
    const std::size_t clearedSize = 0;
    GuestCopyOut_nid_no_patch(static_cast<std::uint8_t*>(self) + offsetof(TicketObject, data), &clearedData, sizeof(clearedData));
    GuestCopyOut_nid_no_patch(static_cast<std::uint8_t*>(self) + offsetof(TicketObject, size), &clearedSize, sizeof(clearedSize));
}

}
