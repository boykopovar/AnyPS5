#include <elfpatcher/general/GuestModuleWriter.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <limits>
#include <span>

namespace Elfpatcher {

std::vector<std::uint8_t> GuestModuleWriter::WriteLinux(const Relinker::GuestImage& image, const std::vector<std::string>& dependencies, const std::string& runPath) const {
    auto bytes = image.Bytes;
    std::vector<Domain::ProgramHeader> headers;
    std::uint64_t end = 0;
    for (auto header : image.Headers) {
        if (header.Type != 1 && header.Type != 7 && header.Type != 0x6474e550 && header.Type != PT_GNU_STACK) continue;
        if (header.Type == 1) {
            end = std::max(end, header.MappedAddress + header.MemorySize);
            if (header.Flags == 0) continue;
            header.Flags |= 4;
        }
        headers.push_back(header);
    }
    if (std::none_of(headers.begin(), headers.end(), [&](const auto& header) { return header.Type == PT_GNU_STACK; }))
        headers.push_back({PT_GNU_STACK, PF_R | PF_W, 0, 0, 0, 0, 0, 16});
    if (end > std::numeric_limits<std::uint64_t>::max() - 0x4000) throw Domain::RelinkerException("Guest virtual address overflow");
    Io::AlignBuffer(bytes, 0x4000);
    const auto extraOffset = bytes.size();
    const auto extraAddress = Io::AlignUp64(end, 0x4000);
    const auto address = [&] { return extraAddress + bytes.size() - extraOffset; };
    const bool passesModuleArgs = image.Init != 0;
    std::uint64_t argsSlot = 0;
    std::uint64_t resultSlot = 0;
    if (passesModuleArgs) {
        Io::AlignBuffer(bytes, 8);
        argsSlot = address();
        bytes.insert(bytes.end(), 8, 0);
        resultSlot = address();
        bytes.insert(bytes.end(), 8, 0);
    }
    auto strings = image.Dynamic.DynStrData;
    auto symbols = image.Dynamic.DynSymData;
    constexpr std::uint64_t kGlobDat = 6;
    const auto addString = [&](const std::string& value) {
        if (strings.size() > std::numeric_limits<std::uint32_t>::max()) throw Domain::RelinkerException("Guest string table too large");
        const auto offset = static_cast<std::uint32_t>(strings.size());
        Io::AppendString(strings, value);
        return offset;
    };
    bool needsTlsResolver = false;
    for (std::size_t index = 1; index < image.Symbols.size(); ++index) {
        const auto& symbol = image.Symbols[index];
        if (image.UsePlatformTlsResolver && symbol.Section == 0 && symbol.Name == "vNe1w4diLCs") {
            Io::WriteU32(symbols, index * 24, addString("__tls_get_addr"));
            needsTlsResolver = true;
        }
        if (symbol.Section == 0 && (symbol.Info >> 4) == 2) symbols[index * 24 + 4] = static_cast<std::uint8_t>(0x10 | (symbol.Info & 15));
    }
    std::vector<std::uint64_t> needed;
    for (const auto& dependency : dependencies) needed.push_back(addString(dependency));
    if (needsTlsResolver) needed.push_back(addString("ld-linux-x86-64.so.2"));
    const auto addUndefinedImport = [&](const std::string& name) {
        const auto nameOffset = addString(name);
        const auto entry = symbols.size();
        symbols.resize(entry + 24, 0);
        Io::WriteU32(symbols, entry, nameOffset);
        symbols[entry + 4] = 0x12;
        Io::WriteU16(symbols, entry + 6, 0);
        return entry / 24;
    };
    auto relaData = image.Dynamic.RelaData;
    std::uint64_t argsImport = 0;
    std::uint64_t resultImport = 0;
    const auto addGlobDat = [&](std::uint64_t slot, std::uint64_t symbol) {
        const auto entry = relaData.size();
        relaData.resize(entry + 24, 0);
        Io::WriteU64(relaData, entry, slot);
        Io::WriteU64(relaData, entry + 8, (symbol << 32) | kGlobDat);
    };
    if (passesModuleArgs) {
        argsImport = addUndefinedImport("__aps5_get_pending_module_args_nid_no_patch");
        resultImport = addUndefinedImport("__aps5_set_module_init_result_nid_no_patch");
        addGlobDat(argsSlot, argsImport);
        addGlobDat(resultSlot, resultImport);
    }
    const auto soname = addString(image.OutputName);
    const auto search = addString(runPath);
    const auto strAddress = address();
    bytes.insert(bytes.end(), strings.begin(), strings.end());
    Io::AlignBuffer(bytes, 8);
    const auto symAddress = address();
    bytes.insert(bytes.end(), symbols.begin(), symbols.end());
    const auto hashAddress = address();
    const auto count = static_cast<std::uint32_t>(symbols.size() / 24);
    const auto bucketCount = std::max<std::uint32_t>(count, 1);
    std::vector<std::uint32_t> buckets(bucketCount), chains(count);
    for (std::uint32_t index = count; index-- > 1;) {
        std::uint32_t hash = 0;
        for (auto offset = Io::ReadU32(symbols, index * 24); offset < strings.size() && strings[offset] != 0; ++offset) {
            hash = (hash << 4u) + strings[offset];
            hash = (hash ^ ((hash & 0xf0000000u) >> 24u)) & 0x0fffffffu;
        }
        chains[index] = buckets[hash % bucketCount];
        buckets[hash % bucketCount] = index;
    }
    Io::AppendU32(bytes, bucketCount);
    Io::AppendU32(bytes, count);
    for (const auto bucket : buckets) Io::AppendU32(bytes, bucket);
    for (const auto chain : chains) Io::AppendU32(bytes, chain);
    Io::AlignBuffer(bytes, 8);
    const auto relaAddress = address();
    bytes.insert(bytes.end(), relaData.begin(), relaData.end());
    const auto pltAddress = address();
    bytes.insert(bytes.end(), image.Dynamic.RelaPltData.begin(), image.Dynamic.RelaPltData.end());
    constexpr std::size_t kArgsDisplacement = 3, kTargetDisplacement = 0x11, kResultDisplacement = 0x19;
    constexpr std::size_t kStubSize = 0x1f;
    const auto stubDisplacement = [](std::uint64_t target, std::uint64_t next) {
        const auto distance = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(next);
        if (distance < std::numeric_limits<std::int32_t>::min() || distance > std::numeric_limits<std::int32_t>::max())
            throw Domain::RelinkerException("Guest initializer exceeds relative branch range");
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(distance));
    };
    const auto lifecycle = [&](std::uint64_t target) {
        if (target == 0) return std::uint64_t{};
        const auto start = address();
        std::vector<std::uint8_t> stub(kStubSize, 0xc3);
        stub[0x00] = 0x50;
        stub[0x01] = 0xff; stub[0x02] = 0x15;
        Io::WriteU32(stub, kArgsDisplacement, stubDisplacement(argsSlot, start + 7));
        stub[0x07] = 0x48; stub[0x08] = 0x8b; stub[0x09] = 0x38;
        stub[0x0a] = 0x48; stub[0x0b] = 0x8b; stub[0x0c] = 0x70; stub[0x0d] = 0x08;
        stub[0x0e] = 0x31; stub[0x0f] = 0xd2;
        stub[0x10] = 0xe8;
        Io::WriteU32(stub, kTargetDisplacement, stubDisplacement(target, start + 0x15));
        stub[0x15] = 0x89; stub[0x16] = 0xc7;
        stub[0x17] = 0xff; stub[0x18] = 0x15;
        Io::WriteU32(stub, kResultDisplacement, stubDisplacement(resultSlot, start + 0x1d));
        stub[0x1d] = 0x58;
        stub[0x1e] = 0xc3;
        bytes.insert(bytes.end(), stub.begin(), stub.end());
        return start;
    };
    const auto callThrough = [&](std::uint64_t target) {
        if (target == 0) return std::uint64_t{};
        const auto start = address();
        const auto distance = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(start + 5);
        if (distance < std::numeric_limits<std::int32_t>::min() || distance > std::numeric_limits<std::int32_t>::max())
            throw Domain::RelinkerException("Guest finaliser exceeds relative branch range");
        bytes.push_back(0xe9);
        Io::AppendU32(bytes, static_cast<std::uint32_t>(distance));
        return start;
    };
    const auto init = lifecycle(image.Init);
    const auto fini = callThrough(image.Fini);
    Io::AlignBuffer(bytes, 8);
    const auto dynamicOffset = bytes.size();
    const auto dynamicAddress = address();
    const auto tag = [&](std::uint64_t key, std::uint64_t value) { Io::AppendU64(bytes, key); Io::AppendU64(bytes, value); };
    for (const auto offset : needed) tag(1, offset);
    tag(14, soname);
    tag(29, search);
    tag(4, hashAddress);
    tag(5, strAddress);
    tag(10, strings.size());
    tag(6, symAddress);
    tag(11, 24);
    if (!relaData.empty()) {
        tag(7, relaAddress);
        tag(8, relaData.size());
        tag(9, 24);
    }
    if (!image.Dynamic.RelaPltData.empty()) {
        tag(23, pltAddress);
        tag(2, image.Dynamic.RelaPltData.size());
        tag(20, 7);
        tag(3, image.Got);
    }
    if (!image.InitArray.empty()) { tag(25, image.InitArray.front()); tag(27, image.InitArray.size() * 8); }
    if (!image.FiniArray.empty()) { tag(26, image.FiniArray.front()); tag(28, image.FiniArray.size() * 8); }
    if (init != 0) tag(12, init);
    if (fini != 0) tag(13, fini);
    tag(30, 8);
    tag(0, 0);
    const auto dynamicSize = bytes.size() - dynamicOffset;
    using namespace Codegen::Amd64OnlySubstitutionTable;
    const auto checkedAdd = [](const std::uint64_t left, const std::uint64_t right, const std::uint64_t offset) {
        if (right > std::numeric_limits<std::uint64_t>::max() - left)
            throw Domain::RelinkerException("AMD-only guest stub address overflow", offset);
        return left + right;
    };
    const auto displacement = [](const std::uint64_t target, const std::uint64_t next, const std::uint64_t offset) {
        if (target >= next) {
            const auto distance = target - next;
            if (distance > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
                throw Domain::RelinkerException("AMD-only guest stub exceeds rel32 range", offset);
            return static_cast<std::int32_t>(distance);
        }
        const auto distance = next - target;
        if (distance > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1)
            throw Domain::RelinkerException("AMD-only guest stub exceeds rel32 range", offset);
        return static_cast<std::int32_t>(-static_cast<std::int64_t>(distance));
    };
    for (const auto& site : image.Trampolines) {
        if (site.Length < kJmpRel32.Size || site.OriginalBytes.size() != site.Length ||
            site.Body.size() < kJmpRel32.Size ||
            site.ReturnBranchOffset > site.Body.size() - kJmpRel32.Size ||
            site.Body[site.ReturnBranchOffset] != kJmpRel32.Bytes[0])
            throw Domain::RelinkerException("Invalid AMD-only guest trampoline site", site.Offset);
        if (site.Offset > image.Bytes.size() || site.Length > image.Bytes.size() - site.Offset)
            throw Domain::RelinkerException("AMD-only guest instruction is outside the original image", site.Offset);
        const auto mapped = std::any_of(image.Headers.begin(), image.Headers.end(), [&](const auto& header) {
            if (header.Type != 1 || (header.Flags & 1) == 0 ||
                site.Address < header.MappedAddress || site.Offset < header.Offset)
                return false;
            const auto addressDelta = site.Address - header.MappedAddress;
            const auto offsetDelta = site.Offset - header.Offset;
            return addressDelta == offsetDelta && addressDelta <= header.FileSize &&
                   site.Length <= header.FileSize - addressDelta;
        });
        if (!mapped)
            throw Domain::RelinkerException("AMD-only guest instruction is outside an executable segment", site.Offset);
        if (!std::equal(site.OriginalBytes.begin(), site.OriginalBytes.end(),
                        bytes.begin() + static_cast<std::ptrdiff_t>(site.Offset)))
            throw Domain::RelinkerException("AMD-only guest site bytes changed before patching", site.Offset);
        bytes.resize(Io::AlignUp(bytes.size(), kStubAlignment), kTrapFill);
        const auto stubOffset = bytes.size();
        const auto stubAddress = checkedAdd(extraAddress, stubOffset - extraOffset, site.Offset);
        checkedAdd(stubAddress, site.Body.size(), site.Offset);
        const auto returnAddress = checkedAdd(site.Address, site.Length, site.Offset);
        const auto stubReturn = checkedAdd(stubAddress, site.ReturnBranchOffset + kJmpRel32.Size, site.Offset);
        const auto siteNext = checkedAdd(site.Address, kJmpRel32.Size, site.Offset);
        const auto returnDisplacement = displacement(returnAddress, stubReturn, site.Offset);
        const auto siteDisplacement = displacement(stubAddress, siteNext, site.Offset);
        bytes.insert(bytes.end(), site.Body.begin(), site.Body.end());
        Codegen::ApplyStubRelocations(std::span<std::uint8_t>(bytes.data() + stubOffset, site.ReturnBranchOffset), site.Relocations, site.Address, stubAddress, site.Offset);
        Io::WriteU32(bytes, stubOffset + site.ReturnBranchOffset + 1,
                     static_cast<std::uint32_t>(returnDisplacement));
        std::fill_n(bytes.begin() + static_cast<std::ptrdiff_t>(site.Offset), site.Length, kNop1.Bytes[0]);
        bytes[static_cast<std::size_t>(site.Offset)] = kJmpRel32.Bytes[0];
        Io::WriteU32(bytes, static_cast<std::size_t>(site.Offset + 1),
                     static_cast<std::uint32_t>(siteDisplacement));
    }
    const auto phOffset = bytes.size();
    const auto phCount = headers.size() + 2;
    if (phCount > std::numeric_limits<std::uint16_t>::max()) throw Domain::RelinkerException("Too many guest program headers");
    const auto extraSize = bytes.size() + phCount * 56 - extraOffset;
    headers.push_back({1, 7, extraOffset, extraAddress, extraAddress, extraSize, extraSize, 0x4000});
    headers.push_back({2, 6, dynamicOffset, dynamicAddress, dynamicAddress, dynamicSize, dynamicSize, 8});
    for (const auto& header : headers) {
        Io::AppendU32(bytes, header.Type);
        Io::AppendU32(bytes, header.Flags);
        Io::AppendU64(bytes, header.Offset);
        Io::AppendU64(bytes, header.MappedAddress);
        Io::AppendU64(bytes, header.PhysicalAddress);
        Io::AppendU64(bytes, header.FileSize);
        Io::AppendU64(bytes, header.MemorySize);
        Io::AppendU64(bytes, header.Alignment);
    }
    bytes[7] = 0;
    bytes[8] = 0;
    Io::WriteU16(bytes, 16, 3);
    Io::WriteU64(bytes, 24, 0);
    Io::WriteU64(bytes, 32, phOffset);
    Io::WriteU64(bytes, 40, 0);
    Io::WriteU16(bytes, 56, static_cast<std::uint16_t>(phCount));
    Io::WriteU16(bytes, 58, 0);
    Io::WriteU16(bytes, 60, 0);
    Io::WriteU16(bytes, 62, 0);
    return bytes;
}

}
