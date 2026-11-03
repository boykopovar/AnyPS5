#ifndef CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_GUESTLIST_HPP
#define CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_GUESTLIST_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace FreeType {

struct GuestListNodeRec {
    GuestListNodeRec* prev;
    GuestListNodeRec* next;
    void* data;
};

struct GuestListRec {
    GuestListNodeRec* head;
    GuestListNodeRec* tail;
};

struct GuestMemoryRec {
    void* user;
    void* (APS5_VABI *alloc)(GuestMemoryRec*, std::int64_t);
    void (APS5_VABI *free)(GuestMemoryRec*, void*);
    void* (APS5_VABI *realloc)(GuestMemoryRec*, std::int64_t, std::int64_t, void*);
};

using GuestListIterator = int (APS5_VABI *)(GuestListNodeRec*, void*);
using GuestListDestructor = void (APS5_VABI *)(GuestMemoryRec*, void*, void*);

static_assert(sizeof(GuestListNodeRec) == 24 && offsetof(GuestListNodeRec, prev) == 0 && offsetof(GuestListNodeRec, next) == 8 && offsetof(GuestListNodeRec, data) == 16);
static_assert(sizeof(GuestListRec) == 16 && offsetof(GuestListRec, head) == 0 && offsetof(GuestListRec, tail) == 8);
static_assert(sizeof(GuestMemoryRec) == 32 && offsetof(GuestMemoryRec, alloc) == 8 && offsetof(GuestMemoryRec, free) == 16 && offsetof(GuestMemoryRec, realloc) == 24);

}

#endif
