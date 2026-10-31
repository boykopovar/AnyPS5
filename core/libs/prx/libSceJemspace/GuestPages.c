#define JEMALLOC_PAGES_C_
#include "jemalloc/internal/jemalloc_preamble.h"

#include "jemalloc/internal/jemalloc_internal_includes.h"
#include "jemalloc/internal/pages.h"

extern void* JemspaceMapPages_nid_no_patch(void*, size_t, size_t, bool);
extern void JemspaceUnmapPages_nid_no_patch(void*, size_t);
extern bool JemspaceSetPages_nid_no_patch(void*, size_t, bool);
extern bool JemspacePurgePages_nid_no_patch(void*, size_t);

const char* thp_mode_names[] = {"default", "always", "never", "not supported"};
thp_mode_t opt_thp = thp_mode_default;
thp_mode_t init_system_thp_mode = thp_mode_not_supported;

void* pages_map(void* address, size_t bytes, size_t alignment, bool* commit) {
    return JemspaceMapPages_nid_no_patch(address, bytes, alignment, *commit);
}

void pages_unmap(void* address, size_t bytes) {
    JemspaceUnmapPages_nid_no_patch(address, bytes);
}

bool pages_commit(void* address, size_t bytes) {
    return JemspaceSetPages_nid_no_patch(address, bytes, true);
}

bool pages_decommit(void* address, size_t bytes) {
    return JemspaceSetPages_nid_no_patch(address, bytes, false);
}

bool pages_purge_lazy(void* address, size_t bytes) {
    return JemspacePurgePages_nid_no_patch(address, bytes);
}

bool pages_purge_forced(void* address, size_t bytes) {
    return JemspaceSetPages_nid_no_patch(address, bytes, true);
}

bool pages_huge(void* address, size_t bytes) {
    (void)address;
    (void)bytes;
    return true;
}

bool pages_nohuge(void* address, size_t bytes) {
    (void)address;
    (void)bytes;
    return true;
}

bool pages_dontdump(void* address, size_t bytes) {
    (void)address;
    (void)bytes;
    return true;
}

bool pages_dodump(void* address, size_t bytes) {
    (void)address;
    (void)bytes;
    return true;
}

bool pages_boot(void) {
    opt_thp = thp_mode_not_supported;
    init_system_thp_mode = thp_mode_not_supported;
    return false;
}

void pages_set_thp_state(void* address, size_t bytes) {
    (void)address;
    (void)bytes;
}
