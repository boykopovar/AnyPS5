#include <elfpatcher/general/GuestModuleWriter.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <limits>

namespace Elfpatcher {

std::vector<std::uint8_t> GuestModuleWriter::WriteLinux(const Relinker::GuestImage& image, const std::vector<std::string>& dependencies, const std::string& runPath) const {
    auto bytes = image.Bytes;
    std::vector<Domain::ProgramHeader> headers;
    std::uint64_t end = 0;
    for (auto header : image.Headers) {
        if (header.Type != 1 && header.Type != 7 && header.Type != 0x6474e550 && header.Type != 0x6474e551) continue;
        if (header.Type == 1) {
            header.Flags |= 4;
            end = std::max(end, header.MappedAddress + header.MemorySize);
        }
        headers.push_back(header);
    }
    if (end > std::numeric_limits<std::uint64_t>::max() - 0x4000) throw Domain::RelinkerException("Guest virtual address overflow");
    Io::AlignBuffer(bytes, 0x4000);
    const auto extraOffset = bytes.size();
    const auto extraAddress = Io::AlignUp64(end, 0x4000);
    const auto address = [&] { return extraAddress + bytes.size() - extraOffset; };
    auto strings = image.Dynamic.DynStrData;
    auto symbols = image.Dynamic.DynSymData;
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
    const auto soname = addString(image.OutputName);
    const auto search = addString(runPath);
    const auto strAddress = address();
    bytes.insert(bytes.end(), strings.begin(), strings.end());
    Io::AlignBuffer(bytes, 8);
    const auto symAddress = address();
    bytes.insert(bytes.end(), symbols.begin(), symbols.end());
    const auto hashAddress = address();
    const auto count = static_cast<std::uint32_t>(image.Symbols.size());
    Io::AppendU32(bytes, 1);
    Io::AppendU32(bytes, count);
    Io::AppendU32(bytes, count > 1 ? 1 : 0);
    for (std::uint32_t index = 0; index < count; ++index) Io::AppendU32(bytes, index != 0 && index + 1 < count ? index + 1 : 0);
    Io::AlignBuffer(bytes, 8);
    const auto relaAddress = address();
    bytes.insert(bytes.end(), image.Dynamic.RelaData.begin(), image.Dynamic.RelaData.end());
    const auto pltAddress = address();
    bytes.insert(bytes.end(), image.Dynamic.RelaPltData.begin(), image.Dynamic.RelaPltData.end());
    const auto lifecycle = [&](std::uint64_t target) {
        if (target == 0) return std::uint64_t{};
        const auto start = address();
        bytes.insert(bytes.end(), {0x31, 0xff, 0x31, 0xf6, 0x31, 0xd2, 0xe9});
        const auto displacement = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(address() + 4);
        if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max()) throw Domain::RelinkerException("Guest initializer exceeds relative branch range");
        Io::AppendU32(bytes, static_cast<std::uint32_t>(displacement));
        return start;
    };
    const auto init = lifecycle(image.Init);
    const auto fini = lifecycle(image.Fini);
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
    if (!image.Dynamic.RelaData.empty()) {
        tag(7, relaAddress);
        tag(8, image.Dynamic.RelaData.size());
        tag(9, 24);
    }
    if (!image.Dynamic.RelaPltData.empty()) {
        tag(23, pltAddress);
        tag(2, image.Dynamic.RelaPltData.size());
        tag(20, 7);
        tag(3, image.Got);
    }
    if (init != 0) tag(12, init);
    if (fini != 0) tag(13, fini);
    tag(30, 8);
    tag(0, 0);
    const auto dynamicSize = bytes.size() - dynamicOffset;
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
