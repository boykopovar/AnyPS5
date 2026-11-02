#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
constexpr int ReadOnly = 0;
constexpr int ReadWrite = 2;
constexpr int Create = 0x200;
constexpr int Exclusive = 0x800;
constexpr int ProtRead = 1;
constexpr int ProtReadWrite = 3;
constexpr int MapShared = 1;
constexpr int MapPrivate = 2;
constexpr std::size_t Size = 0x8000;
void* const Failed = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "named shared-memory mapping: %s (guest errno %d)\n", message, *__error_nid_postfix());
    std::abort();
}

char* Map(std::size_t length, int protection, int flags, int descriptor, std::int64_t offset) {
    return static_cast<char*>(mmap_nid_postfix(nullptr, length, protection, flags, descriptor, offset));
}
}

int main() {
    const std::string name = "/aps5-named-map-" + std::to_string(::GetCurrentProcessId());
    shm_unlink_nid_postfix(name.c_str());
    const int first = shm_open_nid_postfix(name.c_str(), Create | Exclusive | ReadWrite, 0600);
    Require(first >= 0, "creating the named object failed");
    Require(ftruncate_nid_postfix(first, Size) == 0, "sizing the named object failed");

    auto* view = Map(Size, ProtReadWrite, MapShared, first, 0);
    Require(view != Failed, "a shared read-write mapping of the named object failed");
    view[0x10] = 'a';

    const int second = shm_open_nid_postfix(name.c_str(), ReadWrite, 0);
    Require(second >= 0 && second != first, "reopening the named object failed");
    auto* other = Map(Size, ProtReadWrite, MapShared, second, 0);
    Require(other != Failed && other != view, "a second shared mapping of the named object failed");
    Require(other[0x10] == 'a', "a store through one mapping is missing from a mapping of another descriptor");
    other[0x5000] = 'b';
    Require(view[0x5000] == 'b', "a store through the second mapping is missing from the first");

    char byte = 0;
    Require(pread_nid_postfix(second, &byte, 1, 0x5000) == 1 && byte == 'b', "pread does not see a store made through a mapping");
    const char written = 'c';
    Require(pwrite_nid_disambig1_nid_postfix(first, &written, 1, 0x6000) == 1, "pwrite into the named object failed");
    Require(view[0x6000] == 'c' && other[0x6000] == 'c', "a mapping does not see a pwrite into the named object");

    auto* offset = Map(0x4000, ProtRead, MapShared, second, 0x4000);
    Require(offset != Failed && offset[0x1000] == 'b' && offset[0x2000] == 'c', "a mapping at a nonzero offset shows the wrong bytes");
    Require(munmap_nid_postfix(offset, 0x4000) == 0, "unmapping the offset mapping failed");

    const int reader = shm_open_nid_postfix(name.c_str(), ReadOnly, 0);
    Require(reader >= 0, "opening the named object read-only failed");
    auto* readView = Map(Size, ProtRead, MapShared, reader, 0);
    Require(readView != Failed && readView[0x5000] == 'b', "a read-only mapping of a read-only descriptor failed");
    Require(munmap_nid_postfix(readView, Size) == 0, "unmapping the read-only mapping failed");
    *__error_nid_postfix() = 0;
    Require(Map(Size, ProtReadWrite, MapShared, reader, 0) == Failed && *__error_nid_postfix() == 13, "a writable shared mapping of a read-only descriptor was not refused with EACCES");
    Require(close_nid_postfix(reader) == 0, "closing the read-only descriptor failed");

    Require(shm_unlink_nid_postfix(name.c_str()) == 0, "unlinking the named object failed");
    *__error_nid_postfix() = 0;
    Require(shm_open_nid_postfix(name.c_str(), ReadWrite, 0) == -1 && *__error_nid_postfix() == 2, "the unlinked name still opens");
    view[0x7000] = 'u';
    Require(other[0x7000] == 'u', "mappings of an unlinked object stopped sharing stores");
    Require(pread_nid_postfix(second, &byte, 1, 0x7000) == 1 && byte == 'u', "a descriptor of an unlinked object stopped seeing mapped stores");

    auto* copy = Map(Size, ProtReadWrite, MapPrivate, second, 0);
    Require(copy != Failed && copy[0x10] == 'a', "a private mapping of the named object failed");
    copy[0x10] = 'p';
    Require(view[0x10] == 'a' && pread_nid_postfix(first, &byte, 1, 0x10) == 1 && byte == 'a', "a store through a private mapping reached the named object");
    Require(munmap_nid_postfix(copy, Size) == 0, "unmapping the private mapping failed");

    Require(munmap_nid_postfix(view, Size) == 0 && munmap_nid_postfix(other, Size) == 0, "unmapping the shared mappings failed");
    Require(close_nid_postfix(first) == 0 && close_nid_postfix(second) == 0, "closing the named object's descriptors failed");
    std::puts("named shared-memory mapping tests passed");
    return 0;
}
