#include "prx/libc/include/exceptions/CompactUnwind.hpp"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "exception_compact_unwind: %s\n", message);
        std::abort();
    }
}

void Append32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<unsigned char>(value >> shift));
}

std::vector<unsigned char> SampleUnwind() {
    constexpr std::uint32_t common = 0x1c;
    constexpr std::uint32_t personality = 0x20;
    constexpr std::uint32_t lsda = 0x28;
    constexpr std::uint32_t index = 0x30;
    constexpr std::uint32_t regular = 0x54;
    constexpr std::uint32_t compressed = 0x84;
    std::vector<unsigned char> bytes;
    Append32(bytes, 1);
    Append32(bytes, common);
    Append32(bytes, 1);
    Append32(bytes, personality);
    Append32(bytes, 2);
    Append32(bytes, index);
    Append32(bytes, 3);
    Append32(bytes, 0x02010000u);
    Append32(bytes, 0x40);
    Append32(bytes, static_cast<std::uint32_t>(-8));
    Append32(bytes, 0x1000);
    Append32(bytes, 0x0abc);
    Append32(bytes, 0x1000);
    Append32(bytes, regular);
    Append32(bytes, lsda);
    Append32(bytes, 0x2000);
    Append32(bytes, compressed);
    Append32(bytes, lsda + 8);
    Append32(bytes, 0x3000);
    Append32(bytes, 0);
    Append32(bytes, lsda + 8);
    Append32(bytes, 2);
    Append32(bytes, 0x00050008u);
    Append32(bytes, 0x1000);
    Append32(bytes, 0x52020000u);
    Append32(bytes, 0x1100);
    Append32(bytes, 0);
    Append32(bytes, 0x1200);
    Append32(bytes, 0x04000004u);
    Append32(bytes, 0x1300);
    Append32(bytes, 0x04ffff00u);
    Append32(bytes, 0x1400);
    Append32(bytes, 0x04000000u);
    Append32(bytes, 3);
    Append32(bytes, 0x0002000cu);
    Append32(bytes, 0x00010014u);
    Append32(bytes, 0x00000010u);
    Append32(bytes, 0x01000080u);
    Append32(bytes, 0x23040000u);
    Require(bytes.size() == compressed + 24, "sample unwind blob size");
    Require(regular + 8 + 5 * 8 == compressed, "regular page ends where the compressed page starts");
    return bytes;
}

LibcUnwind::CompactParse Lookup(const std::vector<unsigned char>& info, std::span<const unsigned char> eh, std::uint32_t target, LibcUnwind::CompactDwarfDecode decode, bool* called, LibcUnwind::CompactEntry& entry) {
    return LibcUnwind::ParseCompactUnwind(info, eh, target, decode, called, entry);
}

bool AcceptDwarf(std::uint32_t, void* user) {
    if (user) *static_cast<bool*>(user) = true;
    return true;
}

bool RejectDwarf(std::uint32_t, void* user) {
    if (user) *static_cast<bool*>(user) = true;
    return false;
}

void CheckIndex(const std::vector<unsigned char>& info) {
    using LibcUnwind::CompactParse;
    LibcUnwind::CompactEntry entry {};
    bool called = false;
    const unsigned char ehBytes[] = {1, 2, 3, 4, 5, 6, 7, 8};
    const std::span<const unsigned char> eh{ehBytes, sizeof(ehBytes)};
    Require(Lookup(info, eh, 0x0fff, AcceptDwarf, &called, entry) == CompactParse::Miss, "an address before the first page is a miss");
    Require(!called, "a miss does not decode an FDE");
    Require(Lookup(info, eh, 0x1000, AcceptDwarf, &called, entry) == CompactParse::Found, "the first regular-page function is found");
    Require(entry.encoding == 0x52020000u, "the first function keeps its real encoding");
    Require(entry.encoding != 0x01000000u, "a real entry is not rewritten as an RBP frame");
    Require(entry.functionOffset == 0x1000 && entry.functionEndOffset == 0x1100, "the first function spans until the next entry");
    Require(entry.hasPersonality && entry.personalityDelta == 0x40, "personality index 1 is the first signed delta");
    Require(entry.hasLsda && entry.lsdaOffset == 0x0abc, "the LSDA index matches the function offset");
    Require(Lookup(info, eh, 0x10ff, nullptr, nullptr, entry) == CompactParse::Found && entry.functionOffset == 0x1000, "the last byte of the first function stays on that entry");
    Require(Lookup(info, eh, 0x1100, nullptr, nullptr, entry) == CompactParse::Failed, "encoding 0 fails");
    called = false;
    Require(Lookup(info, {}, 0x1200, AcceptDwarf, &called, entry) == CompactParse::Failed, "a DWARF-mode entry with no eh section fails");
    Require(!called, "a missing eh section is not decoded");
    called = false;
    Require(Lookup(info, eh, 0x1200, AcceptDwarf, &called, entry) == CompactParse::Found, "an in-range FDE that decodes is found");
    Require(called && entry.dwarf && entry.dwarfOffset == 4 && entry.encoding == 0x04000004u, "a decoded DWARF-mode entry keeps its eh offset");
    called = false;
    Require(Lookup(info, eh, 0x1300, AcceptDwarf, &called, entry) == CompactParse::Failed, "an eh offset past the section fails");
    Require(!called, "an out-of-range eh offset is not decoded");
    called = false;
    Require(Lookup(info, eh, 0x1400, RejectDwarf, &called, entry) == CompactParse::Failed, "a rejected FDE fails");
    Require(called, "an in-range FDE is offered to the decoder");
    Require(Lookup(info, eh, 0x2010, nullptr, nullptr, entry) == CompactParse::Found, "the first compressed-page function is found");
    Require(entry.encoding == 0x02010000u && entry.functionOffset == 0x2010 && entry.functionEndOffset == 0x2080, "a compressed entry uses the common encoding and the next slot as its end");
    Require(!entry.hasPersonality && !entry.hasLsda && !entry.dwarf, "the common compressed entry has no personality, LSDA, or dwarf offset");
    Require(Lookup(info, eh, 0x2080, nullptr, nullptr, entry) == CompactParse::Found, "the last page's last function is found");
    Require(entry.encoding == 0x23040000u && entry.functionOffset == 0x2080 && entry.functionEndOffset == 0x3000, "the last function runs to the sentinel");
    Require(entry.hasPersonality && entry.personalityDelta == -8, "personality index 2 is the second signed delta");
    Require(Lookup(info, eh, 0x2fff, nullptr, nullptr, entry) == CompactParse::Found && entry.functionOffset == 0x2080, "the last byte of the last page stays on that function");
    Require(Lookup(info, eh, 0x3000, nullptr, nullptr, entry) == CompactParse::Miss, "the sentinel is not a function");
}

void CheckRules() {
    LibcUnwind::Rules rules {};
    Require(!LibcUnwind::CompactUnwindRules(0, {}, rules), "encoding 0 is not a frame");
    Require(!LibcUnwind::CompactUnwindRules(0x10000000u, {}, rules), "mode 0 with a personality still fails");
    Require(LibcUnwind::CompactUnwindRules(0x02020000u, {}, rules), "a frameless encoding does not need function bytes");
    Require(rules.cfaRegister == 7 && rules.cfaOffset == 16, "mode 2 stack size is the encoded count times 8");
    const unsigned char immediate[] = {0, 0, 0, 0, 0x10, 0, 0, 0};
    Require(LibcUnwind::CompactUnwindRules(0x03040000u, {immediate, sizeof(immediate)}, rules), "mode 3 reads the immediate inside the function");
    Require(rules.cfaRegister == 7 && rules.cfaOffset == 0x10, "mode 3 stack size is the immediate");
    Require(!LibcUnwind::CompactUnwindRules(0x03040000u, {immediate, 7}, rules), "mode 3 fails when the immediate would pass the function end");
    Require(!LibcUnwind::CompactUnwindRules(0x03040000u, {immediate, 4}, rules), "mode 3 fails when the immediate starts at the function end");
    unsigned char far[8] = {};
    Require(!LibcUnwind::CompactUnwindRules(0x03ff0000u, {far, sizeof(far)}, rules), "mode 3 fails when the immediate offset is outside a short function");
    Require(LibcUnwind::CompactUnwindRules(0x01000000u, {}, rules), "a real RBP-mode encoding still has rules");
    Require(rules.cfaRegister == 6 && rules.cfaOffset == 16, "RBP mode is CFA rbp+16");
}

void CheckPersonality() {
    using LibcUnwind::CompactPersonality;
    Require(LibcUnwind::MatchCompactPersonality(nullptr) == CompactPersonality::Unknown, "a missing name is unknown");
    Require(LibcUnwind::MatchCompactPersonality("__gxx_personality_v0") == CompactPersonality::Gxx, "the C++ personality name is exact");
    Require(LibcUnwind::MatchCompactPersonality("___gxx_personality_v0") == CompactPersonality::Gxx, "the Mach-O C++ personality symbol is exact");
    Require(LibcUnwind::MatchCompactPersonality("__gcc_personality_v0") == CompactPersonality::Gcc, "the C personality name is exact");
    Require(LibcUnwind::MatchCompactPersonality("___gcc_personality_v0") == CompactPersonality::Gcc, "the Mach-O C personality symbol is exact");
    const char* unknown[] = {
        "gcc_personality_v0",
        "my_gcc_personality_v0",
        "__gxx_personality_v0_extra",
        "objc_personality",
        "__objc_personality_v0",
        "personality",
        "__gxx_personality_v0 ",
    };
    for (const char* name : unknown)
        Require(LibcUnwind::MatchCompactPersonality(name) == CompactPersonality::Unknown, name);
}

}

int main() {
    CheckIndex(SampleUnwind());
    CheckRules();
    CheckPersonality();
    return 0;
}
