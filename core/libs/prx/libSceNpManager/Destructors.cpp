#include "prx/libSceNpCommon/include/Api.hpp"
#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
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

void ReleaseTicket(TicketObject* self) {
    if (self->data != nullptr) {
        self->allocator->allocator.release(self->data, self->allocator->allocator.context);
    }
}

void ReleaseObject(void* object) {
    if (object == nullptr) return;
    auto* header = static_cast<std::uint8_t*>(object) - NpObjectHeaderSize;
    NpAllocator* allocator = nullptr;
    std::memcpy(&allocator, header, sizeof(allocator));
    allocator->release(header, allocator->context);
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
    auto* ticket = static_cast<TicketObject*>(self);
    ticket->vtable = &NpTicketVtable[2];
    ReleaseTicket(ticket);
}

void APS5_VABI _ZN3sce2np8NpTicketD2Ev(void* self) {
    _ZN3sce2np8NpTicketD1Ev(self);
}

void APS5_VABI _ZN3sce2np8NpTicketD0Ev(void* self) {
    auto* ticket = static_cast<TicketObject*>(self);
    ticket->vtable = &NpTicketVtable[2];
    ReleaseTicket(ticket);
    ReleaseObject(self);
}

void APS5_VABI _ZN3sce2np8NpTicket5ClearEv(void* self) {
    auto* ticket = static_cast<TicketObject*>(self);
    ReleaseTicket(ticket);
    ticket->data = nullptr;
    ticket->size = 0;
}

}
