#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

using LookupCallback = int (APS5_VABI*)(LibreSslX509Lookup*);
using LookupFree = void (APS5_VABI*)(LibreSslX509Lookup*);

struct MemoryBuffer {
    MemoryBuffer* next;
    std::size_t length;
    unsigned char* data;
};

struct MemoryData {
    MemoryBuffer* head;
};

int APS5_VABI NewMemoryLookup(LibreSslX509Lookup* lookup) {
    auto* data = static_cast<MemoryData*>(malloc_nid_postfix(sizeof(MemoryData)));
    if (!data) return 0;
    data->head = nullptr;
    lookup->methodData = reinterpret_cast<char*>(data);
    return 1;
}

void APS5_VABI FreeMemoryLookup(LibreSslX509Lookup* lookup) {
    auto* data = reinterpret_cast<MemoryData*>(lookup->methodData);
    if (!data) return;
    while (data->head) {
        MemoryBuffer* buffer = data->head;
        data->head = buffer->next;
        free_nid_postfix(buffer->data);
        free_nid_postfix(buffer);
    }
    free_nid_postfix(data);
    lookup->methodData = nullptr;
}

LibreSslX509LookupMethod memoryLookup = {
    "MEM",
    reinterpret_cast<void*>(NewMemoryLookup),
    reinterpret_cast<void*>(FreeMemoryLookup),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

}

extern "C" {

int APS5_VABI SSL_CTX_load_verify_mem_nid_postfix(LibreSslCtx* ctx, const void* buffer, int length) {
    if (!ctx) throw std::invalid_argument("SSL_CTX_load_verify_mem: null context");
    if (length < 0) return 0;
    if (length > 0 && !buffer) return 0;
    auto* store = static_cast<LibreSslX509Store*>(ctx->certStore);
    if (!store) return 0;
    LibreSslX509Lookup* lookup = X509_STORE_add_lookup_nid_postfix(store, &memoryLookup);
    if (!lookup) return 0;
    auto* data = reinterpret_cast<MemoryData*>(lookup->methodData);
    if (!data) return 0;
    auto* entry = static_cast<MemoryBuffer*>(malloc_nid_postfix(sizeof(MemoryBuffer)));
    if (!entry) return 0;
    entry->length = static_cast<std::size_t>(length);
    entry->data = static_cast<unsigned char*>(malloc_nid_postfix(entry->length ? entry->length : 1));
    if (!entry->data) {
        free_nid_postfix(entry);
        return 0;
    }
    if (entry->length != 0) {
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(entry->data, buffer, entry->length);
        errno = savedErrno;
        if (!copied) {
            free_nid_postfix(entry->data);
            free_nid_postfix(entry);
            NotImplemented_nid_no_patch("SSL_CTX_load_verify_mem: unreadable buffer");
        }
    }
    entry->next = data->head;
    data->head = entry;
    return 1;
}

}
