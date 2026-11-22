#include "prx/libSceNpCommon/include/Api.hpp"
#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libSceNpManager/libSceNpManager.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int AsmClientExtNotInitialized = static_cast<int>(0x8055CE07u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

using Destructor = void (APS5_VABI*)(void*);

struct NpReleaseEvent {
    void* pointer;
    void* context;
};

unsigned npReleases = 0;
std::array<NpReleaseEvent, 4> npReleaseEvents{};

void APS5_VABI TestNpRelease(void* pointer, void* context) {
    if (npReleases < npReleaseEvents.size()) {
        npReleaseEvents[npReleases] = {pointer, context};
    }
    ++npReleases;
}

void ClobberReadErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void ClobberWriteErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

struct alignas(16) GuestNpObjectBuffer {
    NpAllocator* headerAllocator;
    std::uint64_t headerPadding;
    std::array<std::uint8_t, 32> body;
};

static_assert(offsetof(GuestNpObjectBuffer, body) == 16);

struct alignas(16) GuestNpTicketBuffer {
    NpAllocator* headerAllocator;
    std::uint64_t headerPadding;
    const void* const* vtable;
    NpAllocatorEx* allocator;
    void* data;
    std::size_t size;
};

static_assert(offsetof(GuestNpTicketBuffer, vtable) == 16);

void CheckNpManagerDestructors() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberReadErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberWriteErrno);

    int objectContext = 1;
    int ticketContext = 2;
    int ticketPayload = 0x12345678;
    const void* const initialVtableSentinel = reinterpret_cast<const void*>(std::uintptr_t{0xdeadbeef});
    const void* const* const initialVtable = &initialVtableSentinel;

    NpAllocator headerAllocator{};
    headerAllocator.release = &TestNpRelease;
    headerAllocator.context = &objectContext;

    NpAllocatorEx ticketAllocator{};
    ticketAllocator.allocator.release = &TestNpRelease;
    ticketAllocator.allocator.context = &ticketContext;

    NpAllocator nullReleaseAllocator{};
    nullReleaseAllocator.release = nullptr;
    nullReleaseAllocator.context = &objectContext;

    NpAllocatorEx nullReleaseTicketAllocator{};
    nullReleaseTicketAllocator.allocator.release = nullptr;
    nullReleaseTicketAllocator.allocator.context = &ticketContext;

    auto* const unmapped = reinterpret_cast<void*>(std::uintptr_t{0x100});
    auto* const belowHeader = reinterpret_cast<void*>(std::uintptr_t{0x08});
    auto* const unmappedAllocator = reinterpret_cast<NpAllocator*>(std::uintptr_t{0x200});
    auto* const unmappedAllocatorEx = reinterpret_cast<NpAllocatorEx*>(std::uintptr_t{0x300});

    for (const Destructor fn : {
             &_ZN3sce2np10NpOnlineIdD0Ev,
             &_ZN3sce2np4NpIdD0Ev,
             &_ZN3sce2np4UserD0Ev,
             &_ZN3sce2np12NpTitleTokenD0Ev,
             &_ZN3sce2np13NpAccessTokenD0Ev,
         }) {
        npReleases = 0;
        errno = 71;
        fn(nullptr);
        Require(npReleases == 0 && errno == 71, "null object deleting destructor is a no-op and preserves errno");

        errno = 71;
        fn(belowHeader);
        Require(npReleases == 0 && errno == 71, "underflowing object address is ignored and preserves errno");

        errno = 71;
        fn(unmapped);
        Require(npReleases == 0 && errno == 71, "unmapped object address is ignored and preserves errno");

        GuestNpObjectBuffer nullHeaderObj{};
        nullHeaderObj.headerAllocator = nullptr;
        errno = 71;
        fn(nullHeaderObj.body.data());
        Require(npReleases == 0 && errno == 71, "object header with null allocator does not release");

        GuestNpObjectBuffer unmappedHeaderObj{};
        unmappedHeaderObj.headerAllocator = unmappedAllocator;
        errno = 71;
        fn(unmappedHeaderObj.body.data());
        Require(npReleases == 0 && errno == 71, "object header with unmapped allocator does not release");

        GuestNpObjectBuffer nullReleaseObj{};
        nullReleaseObj.headerAllocator = &nullReleaseAllocator;
        errno = 71;
        fn(nullReleaseObj.body.data());
        Require(npReleases == 0 && errno == 71, "object header with null release callback does not release");

        GuestNpObjectBuffer validObj{};
        validObj.headerAllocator = &headerAllocator;
        errno = 71;
        fn(validObj.body.data());
        Require(npReleases == 1 && errno == 71, "valid value-class deleting destructor invokes header release once");
        Require(npReleaseEvents[0].pointer == &validObj && npReleaseEvents[0].context == &objectContext,
            "valid value-class deleting destructor passes 16-byte-prefixed header and context");
    }

    for (const Destructor ticketFn : {
             &_ZN3sce2np8NpTicketD1Ev,
             &_ZN3sce2np8NpTicketD2Ev,
             &_ZN3sce2np8NpTicketD0Ev,
             &_ZN3sce2np8NpTicket5ClearEv,
         }) {
        npReleases = 0;
        errno = 71;
        ticketFn(nullptr);
        Require(npReleases == 0 && errno == 71, "null NpTicket is ignored and preserves errno");

        errno = 71;
        ticketFn(belowHeader);
        Require(npReleases == 0 && errno == 71, "below-header NpTicket pointer is ignored and preserves errno");

        errno = 71;
        ticketFn(unmapped);
        Require(npReleases == 0 && errno == 71, "unmapped NpTicket pointer is ignored and preserves errno");

        GuestNpTicketBuffer nullTicketAlloc{nullptr, 0, initialVtable, nullptr, &ticketPayload, 0x42};
        errno = 71;
        ticketFn(&nullTicketAlloc.vtable);
        Require(npReleases == 0 && errno == 71, "NpTicket with null allocator does not invoke release callback");

        GuestNpTicketBuffer unmappedTicketAlloc{nullptr, 0, initialVtable, unmappedAllocatorEx, &ticketPayload, 0x42};
        errno = 71;
        ticketFn(&unmappedTicketAlloc.vtable);
        Require(npReleases == 0 && errno == 71, "NpTicket with unmapped allocator does not invoke release callback");

        GuestNpTicketBuffer nullReleaseTicket{nullptr, 0, initialVtable, &nullReleaseTicketAllocator, &ticketPayload, 0x42};
        errno = 71;
        ticketFn(&nullReleaseTicket.vtable);
        Require(npReleases == 0 && errno == 71, "NpTicket with null release callback does not invoke release callback");
    }

    GuestNpTicketBuffer d1Ticket{&headerAllocator, 0, initialVtable, &ticketAllocator, &ticketPayload, 0x42};
    npReleases = 0;
    errno = 71;
    _ZN3sce2np8NpTicketD1Ev(&d1Ticket.vtable);
    Require(npReleases == 1 && errno == 71, "NpTicket D1 releases ticket data once and preserves errno");
    Require(npReleaseEvents[0].pointer == &ticketPayload && npReleaseEvents[0].context == &ticketContext,
        "NpTicket D1 releases ticket data through NpAllocatorEx");
    Require(d1Ticket.vtable != initialVtable && d1Ticket.vtable != nullptr, "NpTicket D1 resets vtable");

    GuestNpTicketBuffer d2Ticket{&headerAllocator, 0, initialVtable, &ticketAllocator, &ticketPayload, 0x42};
    npReleases = 0;
    errno = 71;
    _ZN3sce2np8NpTicketD2Ev(&d2Ticket.vtable);
    Require(npReleases == 1 && errno == 71, "NpTicket D2 releases ticket data once and preserves errno");
    Require(npReleaseEvents[0].pointer == &ticketPayload && npReleaseEvents[0].context == &ticketContext,
        "NpTicket D2 releases ticket data through NpAllocatorEx");
    Require(d2Ticket.vtable != initialVtable && d2Ticket.vtable != nullptr, "NpTicket D2 resets vtable");

    GuestNpTicketBuffer d0Ticket{&headerAllocator, 0, initialVtable, &ticketAllocator, &ticketPayload, 0x42};
    npReleases = 0;
    errno = 71;
    _ZN3sce2np8NpTicketD0Ev(&d0Ticket.vtable);
    Require(npReleases == 2 && errno == 71, "NpTicket D0 releases data and object header and preserves errno");
    Require(npReleaseEvents[0].pointer == &ticketPayload && npReleaseEvents[0].context == &ticketContext,
        "NpTicket D0 releases ticket data first");
    Require(npReleaseEvents[1].pointer == &d0Ticket && npReleaseEvents[1].context == &objectContext,
        "NpTicket D0 releases 16-byte-prefixed header second");
    Require(d0Ticket.vtable != initialVtable && d0Ticket.vtable != nullptr, "NpTicket D0 resets vtable");

    GuestNpTicketBuffer clearTicket{&headerAllocator, 0, initialVtable, &ticketAllocator, &ticketPayload, 0xA5A5};
    npReleases = 0;
    errno = 71;
    _ZN3sce2np8NpTicket5ClearEv(&clearTicket.vtable);
    Require(npReleases == 1 && errno == 71, "NpTicket Clear releases ticket data once and preserves errno");
    Require(npReleaseEvents[0].pointer == &ticketPayload && npReleaseEvents[0].context == &ticketContext,
        "NpTicket Clear releases ticket data through NpAllocatorEx");
    Require(clearTicket.data == nullptr && clearTicket.size == 0, "NpTicket Clear zeroes data and size");
    Require(clearTicket.vtable == initialVtable && clearTicket.allocator == &ticketAllocator,
        "NpTicket Clear preserves vtable and allocator");

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
}

} // namespace

int main() {
    for (const auto call : std::array<decltype(&sceNpAsmClientExtTerminate), 5>{
             sceNpAsmClientExtAbortRequest, sceNpAsmClientExtCreateRequest, sceNpAsmClientExtDeleteRequest,
             sceNpAsmClientExtGetPlayingTitleNpTitleToken, sceNpAsmClientExtTerminate}) {
        Require(call() == AsmClientExtNotInitialized, "an AsmClientExt call must report that the client is not initialized");
        Require(call() == AsmClientExtNotInitialized, "a second call must give the same answer");
    }

    CheckNpManagerDestructors();
}
