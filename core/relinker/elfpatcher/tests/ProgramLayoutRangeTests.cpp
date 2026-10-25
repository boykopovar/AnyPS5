#include <elfpatcher/general/ElfConstants.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <io/ByteWriter.hpp>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

class KeepAll : public Elfpatcher::ISegmentFilter {
public:
    bool ShouldSkip(const Domain::ProgramHeader&) const override { return false; }
};

static void Require(bool ok, const char* what) { if (!ok) throw std::runtime_error(std::string("program layout: ") + what); }

static Domain::ProgramHeader Load(std::uint64_t vaddr, std::uint64_t memorySize, std::uint64_t fileSize, std::uint32_t flags) {
    Domain::ProgramHeader load{};
    load.Type = 1; load.Flags = flags; load.Offset = vaddr; load.MappedAddress = vaddr;
    load.PhysicalAddress = vaddr; load.FileSize = fileSize; load.MemorySize = memorySize; load.Alignment = 0x1000;
    return load;
}

static std::vector<Domain::ProgramHeader> MappedLoads(const std::vector<std::uint8_t>& buf, std::uint64_t tableOffset, std::uint16_t count) {
    std::vector<Domain::ProgramHeader> loads;
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto* entry = buf.data() + tableOffset + index * 56;
        if (*reinterpret_cast<const std::uint32_t*>(entry) != 1)
            continue;
        Domain::ProgramHeader ph{};
        ph.Flags = *reinterpret_cast<const std::uint32_t*>(entry + 4);
        ph.Offset = *reinterpret_cast<const std::uint64_t*>(entry + 8);
        ph.MappedAddress = *reinterpret_cast<const std::uint64_t*>(entry + 16);
        ph.FileSize = *reinterpret_cast<const std::uint64_t*>(entry + 32);
        ph.MemorySize = *reinterpret_cast<const std::uint64_t*>(entry + 40);
        ph.Alignment = *reinterpret_cast<const std::uint64_t*>(entry + 48);
        loads.push_back(ph);
    }
    return loads;
}

static void RequireExtraBlockStartsOnItsOwnPage(std::vector<Domain::ProgramHeader> loads, std::uint64_t extraFileOffset) {
    std::sort(loads.begin(), loads.end(), [](const Domain::ProgramHeader& left, const Domain::ProgramHeader& right) {
        return left.MappedAddress < right.MappedAddress;
    });
    std::size_t extraIndex = loads.size();
    for (std::size_t index = 0; index < loads.size(); ++index) {
        Require(loads[index].FileSize <= loads[index].MemorySize, "load file size above its memory size");
        Require(loads[index].MappedAddress % loads[index].Alignment == loads[index].Offset % loads[index].Alignment, "load vaddr and offset differ modulo alignment");
        if (loads[index].Offset == extraFileOffset)
            extraIndex = index;
    }
    Require(extraIndex < loads.size(), "no load at the extra block offset");
    const auto& extraBlock = loads[extraIndex];
    Require(extraBlock.MemorySize >= extraBlock.FileSize, "extra block memory size below its file size");
    for (std::size_t index = 0; index < loads.size(); ++index) {
        if (index == extraIndex)
            continue;
        const auto& other = loads[index];
        if (other.MappedAddress < extraBlock.MappedAddress) {
            Require((other.MappedAddress + other.FileSize - 1) / 0x1000 < extraBlock.MappedAddress / 0x1000, "load below the extra block ends on its page");
        } else {
            Require(other.MappedAddress >= extraBlock.MappedAddress + extraBlock.FileSize, "load above the extra block overlaps it");
        }
    }
}

int main() {
    try {
        auto byteWriter = std::make_shared<Io::ByteWriter>();
        Elfpatcher::ProgramHeaderLayoutBuilder builder(std::make_shared<KeepAll>(), byteWriter);
        constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
        Domain::ProgramHeader load{};
        load.Type = 1; load.MappedAddress = maximum - 3; load.MemorySize = 8;
        bool rejected = false;
        try { builder.ComputeExtraBlockVaddr({load}, 0x1000); } catch (const std::exception&) { rejected = true; }
        Require(rejected, "overflowing load end accepted");
        load.MappedAddress = 0x1000; load.MemorySize = 0x1000;
        Require(builder.ComputeExtraBlockVaddr({load}, 0x1003) == 0x2003, "vaddr for aligned end 0x2000, offset 0x1003");
        load.MemorySize = 0x1800;
        Require(builder.ComputeExtraBlockVaddr({load}, 0x8A00) == 0x3A00, "vaddr for end 0x2800, offset 0x8A00");
        Require(builder.ComputeExtraBlockVaddr({load}, 0x8800) == 0x3800, "vaddr for end 0x2800, offset 0x8800");
        load.MappedAddress = 0x1000; load.MemorySize = 0x1000;
        Require(builder.ComputeExtraBlockVaddr({load}, 0x1000) == 0x2000, "vaddr for aligned end 0x2000, offset 0x1000");
        Elfpatcher::ProgramHeaderLayoutRequest valid{64, 56, 7, {load}, 0x1000, 0x2000, 0x100, 0x1000, 16, 0x1010, 8};
        std::vector<std::uint8_t> output(0x1100, 0xA5);
        Require(builder.WriteLayout(output, valid) == 7, "header count for a valid request");
        for (int kind = 0; kind < 11; ++kind) {
            auto request = valid;
            switch (kind) {
                case 0: request.PhEntSize = 55; break;
                case 1: request.PhOff = maximum - 10; break;
                case 2: request.PhOff = 0x10F0; break;
                case 3: request.ExtraBlockVaddr = maximum - 10; break;
                case 4: request.DynamicSegmentSize = maximum; break;
                case 5: request.InterpSize = maximum; break;
                case 6: request.DynamicSegmentOffset = maximum; break;
                case 7: request.InterpOffset = maximum; break;
                case 8: request.ExtraBlockOffset = maximum; break;
                case 9: request.ExtraBlockSize = 0x1000; break;
                case 10: request.PhOff = 0xF00; break;
            }
            std::vector<std::uint8_t> bytes(0x1100, 0xA5);
            const auto before = bytes;
            rejected = false;
            try { builder.WriteLayout(bytes, request); } catch (const std::exception&) { rejected = true; }
            Require(rejected && bytes == before, "invalid request accepted or output changed");
        }
        const std::uint16_t tableCount = 3 + 5;
        const auto writeLayout = [&builder, tableCount](const std::vector<Domain::ProgramHeader>& headers, std::uint64_t extraFileOffset, std::uint64_t extraFileSize) {
            const std::uint64_t tableSize = tableCount * 56;
            std::vector<std::uint8_t> bytes(std::max<std::uint64_t>(0xB000, extraFileOffset + tableSize + extraFileSize), 0);
            Elfpatcher::ProgramHeaderLayoutRequest request{extraFileOffset, 56, tableCount, headers, extraFileOffset,
                builder.ComputeExtraBlockVaddr(headers, extraFileOffset), tableSize + extraFileSize,
                extraFileOffset, 0x20, extraFileOffset + 0x20, 8};
            const std::uint16_t written = builder.WriteLayout(bytes, request);
            Require(written == tableCount, "written header count");
            RequireExtraBlockStartsOnItsOwnPage(MappedLoads(bytes, extraFileOffset, written), extraFileOffset);
        };
        writeLayout({Load(0x1000, 0x1800, 0x1800, 6), Load(0x3000, 0x1000, 0x1000, 4)}, 0x8A00, 0x400);
        writeLayout({Load(0x1000, 0x8000, 0x800, 6), Load(0xA000, 0x1000, 0x1000, 4)}, 0x8A00, 0x400);
        writeLayout({Load(0x1000, 0x8000, 0x800, 6), Load(0xA000, 0x1000, 0x1000, 4)}, 0x8000, 0x400);
        writeLayout({Load(0x1000, 0x2000, 0x800, 6), Load(0x3000, 0x3000, 0x1000, 4)}, 0x8A00, 0x400);
        writeLayout({Load(0x1000, 0x1200, 0x1200, 5), Load(0x2400, 0x1000, 0x1000, 6)}, 0x8A00, 0x400);
        writeLayout({Load(0x1000, 0x3000, 0x1000, 5), Load(0x3000, 0x4000, 0x1000, 6)}, 0x8A00, 0x400);
        writeLayout({Load(0x1000, 0x1000, 0x1000, 5), Load(0x2000, 0x1000, 0x0400, 6)}, 0x8800, 0x400);
        const std::vector<Domain::ProgramHeader> singleLoad{Load(0x1000, 0x1800, 0x1800, 6)};
        Require(builder.RequiredProgramHeaderCount(singleLoad) == 7, "required count must reserve a slot for the synthetic stack");
        const auto writeSingle = [&builder, &singleLoad](std::uint16_t slots) {
            std::vector<std::uint8_t> table(0xB000, 0);
            Elfpatcher::ProgramHeaderLayoutRequest request{0x8A00, 56, slots, singleLoad, 0x8A00,
                builder.ComputeExtraBlockVaddr(singleLoad, 0x8A00), static_cast<std::uint64_t>(slots) * 56 + 0x400, 0x8A00, 0x20, 0x8A20, 8};
            const std::uint16_t written = builder.WriteLayout(table, request);
            return std::pair(written, std::move(table));
        };
        const auto [written, table] = writeSingle(builder.RequiredProgramHeaderCount(singleLoad));
        Require(written == 7, "layout must write exactly the required header count");
        std::size_t stacks = 0;
        for (std::uint16_t index = 0; index < written; ++index) {
            const auto entry = 0x8A00 + index * 56;
            if (*reinterpret_cast<const std::uint32_t*>(table.data() + entry) != Elfpatcher::PT_GNU_STACK) continue;
            ++stacks;
            Require(*reinterpret_cast<const std::uint32_t*>(table.data() + entry + 4) == (Elfpatcher::PF_R | Elfpatcher::PF_W)
                && *reinterpret_cast<const std::uint64_t*>(table.data() + entry + 16) == 0
                && *reinterpret_cast<const std::uint64_t*>(table.data() + entry + 48) == 16,
                "synthetic stack must be RW with alignment 16 and no mapping");
        }
        Require(stacks == 1, "layout must synthesize exactly one PT_GNU_STACK when the input has none");
        bool stackSlotRejected = false;
        try { (void)writeSingle(6); } catch (const std::exception&) { stackSlotRejected = true; }
        Require(stackSlotRejected, "layout must refuse to drop the synthetic stack when its slot is missing");
        constexpr std::uint16_t extendedCount = 0xffff;
        std::vector<Domain::ProgramHeader> manyHeaders(extendedCount - Elfpatcher::kSyntheticProgramHeaderCount - 2);
        manyHeaders.front() = singleLoad.front();
        Require(builder.RequiredProgramHeaderCount(manyHeaders) == extendedCount - 1, "largest ordinary count changed");
        const auto writeMany = [&builder](const std::vector<Domain::ProgramHeader>& headers, std::vector<std::uint8_t>& bytes, std::uint16_t count) {
            const std::uint64_t extraOffset = 0x1000;
            const std::uint64_t extraSize = static_cast<std::uint64_t>(count) * 56;
            Elfpatcher::ProgramHeaderLayoutRequest request{extraOffset, 56, count, headers, extraOffset,
                builder.ComputeExtraBlockVaddr(headers, extraOffset), extraSize, extraOffset, 16, extraOffset + 16, 8};
            return builder.WriteLayout(bytes, request);
        };
        std::vector<std::uint8_t> largeOutput(0x1000 + static_cast<std::uint64_t>(extendedCount) * 56, 0xA5);
        Require(writeMany(manyHeaders, largeOutput, extendedCount - 1) == extendedCount - 1, "largest ordinary layout rejected");
        manyHeaders.push_back({});
        rejected = false;
        try { builder.RequiredProgramHeaderCount(manyHeaders); } catch (const Domain::RelinkerException&) { rejected = true; }
        Require(rejected, "extended program header count accepted");
        const auto beforeLarge = largeOutput;
        rejected = false;
        try { writeMany(manyHeaders, largeOutput, extendedCount); } catch (const Domain::RelinkerException&) { rejected = true; }
        Require(rejected && largeOutput == beforeLarge, "extended count written without section-zero encoding");
        std::cout << "25 program layout cases passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
