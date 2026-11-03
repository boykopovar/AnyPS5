#include "prx/libSceBeisobmf/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char *message) {
    if (condition) return;
    std::fprintf(stderr, "BEISOBMF BOXES: %s\n", message);
    std::abort();
}

void Write(void *destination, const void *source, std::size_t bytes) {
    const GuestArena::HostWrite access(destination, bytes);
    Require(access.Open(), "guest fixture memory was not writable");
    std::memcpy(destination, source, bytes);
}

template <typename TValue>
void WriteField(void *object, std::size_t offset, TValue value) {
    Write(static_cast<std::byte *>(object) + offset, &value, sizeof(value));
}

void *Allocate(std::size_t bytes) {
    void *memory = GuestHeap::GuestHeapAllocate_nid_postfix(bytes);
    Require(memory != nullptr, "guest fixture allocation failed");
    const std::array<std::byte, 0x100> zeros{};
    auto *destination = static_cast<std::byte *>(memory);
    for (std::size_t offset = 0; offset < bytes; offset += zeros.size()) {
        const auto count = (bytes - offset < zeros.size()) ? bytes - offset : zeros.size();
        Write(destination + offset, zeros.data(), count);
    }
    return memory;
}

std::uintptr_t ReadCursor(const void *box) {
    std::uintptr_t cursor{};
    std::memcpy(&cursor, static_cast<const std::byte *>(box) + 0x50, sizeof(cursor));
    return cursor;
}

} // namespace

int main() {
    auto *box = static_cast<std::byte *>(Allocate(0xa0));
    auto *inlineMember = static_cast<std::byte *>(Allocate(0x70));
    auto *pointerMember = static_cast<std::byte *>(Allocate(0x70));
    auto *payload = static_cast<std::byte *>(Allocate(8));
    auto *child = Allocate(0x10);
    auto *next = Allocate(0x10);
    auto *decoder = static_cast<std::byte *>(Allocate(0xd0));

    WriteField(box, 0x10, child);
    WriteField(box, 0x20, next);
    WriteField(box, 0x40, inlineMember);
    WriteField(box, 0x80, std::uint64_t{0x1122334455667788});
    WriteField(box, 0x8c, std::uint32_t{7});
    WriteField(box, 0x90, std::uint32_t{0x1234});
    WriteField(inlineMember, 0x10, pointerMember);
    constexpr char flagsName[] = "flags";
    constexpr char pointerName[] = "external";
    Write(inlineMember + 0x18, flagsName, sizeof(flagsName));
    Write(inlineMember + 0x58, "\x34\x12\x01", 3);
    WriteField(inlineMember, 0x68, std::uint32_t{3});
    WriteField(inlineMember, 0x6c, std::uint32_t{0});
    Write(pointerMember + 0x18, pointerName, sizeof(pointerName));
    WriteField(pointerMember, 0x68, std::uint32_t{8});
    WriteField(pointerMember, 0x6c, std::uint32_t{11});
    WriteField(pointerMember, 0x60, payload);
    WriteField(decoder, 0xc0, std::uint64_t{0xfedcba9876543210});

    Require(isobmf_box_getchildbox_nid_postfix(box) == child, "child box offset is wrong");
    Require(isobmf_box_getnextbox_nid_postfix(box) == next, "next box offset is wrong");
    Require(isobmf_box_getdiskoffset_nid_postfix(box) == 0x1122334455667788,
            "box disk offset is wrong");
    Require(isobmf_box_getstate_nid_postfix(box) == 7, "box state offset is wrong");
    Require(isobmf_box_gettype_nid_postfix(box) == 0x1234, "box type offset is wrong");
    Require(isobmf_box_getflags32b_nid_postfix(box) == 0x011234, "flags value is wrong");
    Require(isobmf_box_getmember_length_nid_postfix(box, flagsName) == 3,
            "inline member length is wrong");
    Require(isobmf_box_getmember_type_nid_postfix(box, flagsName) == 0,
            "inline member type is wrong");
    Require(isobmf_box_getmember_ptr_nid_postfix(box, flagsName) == inlineMember + 0x58,
            "inline member pointer is wrong");
    Require(isobmf_box_getmember_length_nid_postfix(box, pointerName) == 8,
            "external member length is wrong");
    Require(isobmf_box_getmember_type_nid_postfix(box, pointerName) == 11,
            "external member type is wrong");
    Require(isobmf_box_getmember_ptr_nid_postfix(box, pointerName) == payload,
            "external member pointer is wrong");
    Require(ReadCursor(box) == reinterpret_cast<std::uintptr_t>(pointerMember),
            "member lookup did not leave the cursor on the matched member");

    Require(isobmf_box_getmember_ptr_nid_postfix(box, "missing") == nullptr,
            "missing member pointer is not null");
    Require(isobmf_box_getmember_length_nid_postfix(box, "missing") == 0,
            "missing member length is not zero");
    Require(isobmf_box_getmember_type_nid_postfix(box, "missing") == 0,
            "missing member type is not zero");
    Require(ReadCursor(box) == 0, "missing member lookup did not clear the cursor");
    Require(isobmf_decoder_next_disk_offset_nid_postfix(decoder) == 0xfedcba9876543210,
            "decoder disk offset is wrong");

    Require(isobmf_box_getflags32b_nid_postfix(nullptr) == 0,
            "null flags box result is wrong");
    Require(isobmf_decoder_next_disk_offset_nid_postfix(nullptr) == 0xffffffff,
            "null decoder sentinel is wrong");
    Require(isobmf_box_getchildbox_nid_postfix(nullptr) == nullptr, "null child box result is wrong");
    Require(isobmf_box_getdiskoffset_nid_postfix(nullptr) == 0, "null disk-offset result is wrong");
    Require(isobmf_box_getnextbox_nid_postfix(nullptr) == nullptr, "null next box result is wrong");
    Require(isobmf_box_getstate_nid_postfix(nullptr) == 0, "null state result is wrong");
    Require(isobmf_box_gettype_nid_postfix(nullptr) == 0, "null type result is wrong");
    Require(isobmf_box_getmember_length_nid_postfix(nullptr, flagsName) == 0,
            "null member-length result is wrong");
    Require(isobmf_box_getmember_ptr_nid_postfix(nullptr, flagsName) == nullptr,
            "null member-pointer result is wrong");
    Require(isobmf_box_getmember_type_nid_postfix(nullptr, flagsName) == 0,
            "null member-type result is wrong");
    Require(isobmf_box_getmember_ptr_nid_postfix(box, nullptr) == nullptr,
            "null member name result is wrong");

    auto *badPointer = reinterpret_cast<void *>(std::uintptr_t{1});
    Require(isobmf_box_gettype_nid_postfix(badPointer) == 0, "unmapped box result is wrong");
    Require(isobmf_decoder_next_disk_offset_nid_postfix(badPointer) == 0xffffffff,
            "unmapped decoder did not return its safe sentinel");
    auto *badName = reinterpret_cast<const char *>(std::uintptr_t{1});
    Require(isobmf_box_getmember_ptr_nid_postfix(box, badName) == nullptr,
            "unmapped member name result is wrong");
    auto *badListBox = static_cast<std::byte *>(Allocate(0xa0));
    WriteField(badListBox, 0x40, std::uintptr_t{1});
    Require(isobmf_box_getmember_ptr_nid_postfix(badListBox, "missing") == nullptr,
            "unmapped member list result is wrong");

    auto *cycleBox = static_cast<std::byte *>(Allocate(0xa0));
    auto *cycleMember = static_cast<std::byte *>(Allocate(0x70));
    WriteField(cycleBox, 0x40, cycleMember);
    WriteField(cycleMember, 0x10, cycleMember);
    Require(isobmf_box_getmember_ptr_nid_postfix(cycleBox, "missing") == nullptr,
            "cyclic member list did not fail safely");

    std::puts("BEISOBMF BOXES tests passed");
    return 0;
}
