#include "prx/libSceAddressSanitizer/src/Describe.hpp"
#include "prx/libSceAddressSanitizer/src/Allocator.hpp"
#include "prx/libSceAddressSanitizer/src/Globals.hpp"
#include "prx/libSceAddressSanitizer/src/Report.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowAddressSpace.hpp"
#include "prx/libSceAddressSanitizer/src/Container.hpp"

namespace AsanRuntime {

namespace {

enum class ShadowKind {
    None,
    Low,
    Gap,
    High,
};

ShadowKind ClassifyShadow(std::uintptr_t address) {
    if (address < ShadowBase || address >= ShadowBase + ShadowSize) {
        return ShadowKind::None;
    }
    const std::uintptr_t gapBegin = ShadowBase + (ShadowBase >> 3);
    const std::uintptr_t gapEnd = ShadowBase + ((ShadowBase + ShadowSize - 1) >> 3);
    if (address >= gapBegin && address <= gapEnd) {
        return ShadowKind::Gap;
    }
    return address < gapBegin ? ShadowKind::Low : ShadowKind::High;
}

bool LooksLikeStack(std::uintptr_t address) {
    const std::uintptr_t shadowAddress = ShadowBase + (address >> 3);
    if (!ShadowRangeAvailable_nid_no_patch(shadowAddress, shadowAddress + 1)) {
        return false;
    }
    switch (*ShadowByte_nid_no_patch(address)) {
    case 0xf1:
    case 0xf2:
    case 0xf3:
    case 0xf5:
    case 0xf8:
    case 0xca:
    case 0xcb:
        return true;
    default:
        return false;
    }
}

const char* ShadowName(ShadowKind kind) {
    return kind == ShadowKind::Low ? "low shadow" : kind == ShadowKind::Gap ? "shadow gap" : "high shadow";
}

}  // namespace

RegionInfo LocateRegion_nid_no_patch(std::uintptr_t address) {
    if (const ShadowKind shadow = ClassifyShadow(address); shadow != ShadowKind::None) {
        return {ShadowName(shadow), 0, 0, ""};
    }
    Heap::ChunkView chunk{};
    if (Heap::FindChunk_nid_no_patch(address, chunk)) {
        return {"heap", chunk.begin, chunk.size, ""};
    }
    if (LooksLikeStack(address)) {
        return {"stack", 0, 0, ""};
    }
    Globals::Global globals[Globals::kMaxDescribed];
    if (Globals::ForAddress_nid_no_patch(address, globals) != 0) {
        return {"global", globals[0].beg, globals[0].size, globals[0].name ? globals[0].name : ""};
    }
    return {"heap-invalid", 0, 0, ""};
}

std::string DescribeAddress_nid_no_patch(std::uintptr_t address, std::size_t accessSize) {
    std::string text;
    if (const ShadowKind shadow = ClassifyShadow(address); shadow != ShadowKind::None) {
        text = "Address ";
        AppendPointer_nid_no_patch(text, address);
        text += std::string(" is located in the ") + ShadowName(shadow) + " area.\n";
        return text;
    }
    Heap::ChunkView chunk{};
    if (Heap::FindChunk_nid_no_patch(address, chunk)) {
        return Heap::DescribeChunk_nid_no_patch(address, accessSize);
    }
    if (LooksLikeStack(address)) {
        text = "Address ";
        AppendPointer_nid_no_patch(text, address);
        text += " is located in stack of thread T" + std::to_string(Heap::CurrentTid_nid_no_patch()) + "\n";
        return text;
    }
    text = Globals::Describe_nid_no_patch(address, accessSize);
    if (!text.empty()) {
        return text;
    }
    text = "Address ";
    AppendPointer_nid_no_patch(text, address);
    text += " is a wild pointer inside of access range of size ";
    AppendPointer_nid_no_patch(text, accessSize);
    text += ".\n";
    return text;
}

}  // namespace AsanRuntime
