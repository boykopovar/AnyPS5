#include "../tools/OracleCodeObject.hpp"
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>

namespace {

constexpr std::array<std::uint32_t, 34> Text{
    0xf4080100, 0xfa000000, 0x34020084, 0x34040086, 0xbf8cc07f, 0xdc388000, 0x04040001, 0xbf8c3f70,
    0x7e140280, 0x7e160280, 0x7e180280, 0x7e1a0280, 0x7e1c0280, 0x7e1e0280, 0x7e200280, 0x7e220280,
    0x7e240280, 0x7e260280, 0x7e280280, 0x7e2a0280, 0x7e2c0280, 0x7e2e0280, 0x7e300280, 0x7e320280,
    0x64140b04, 0xdc788000, 0x00060a02, 0xdc788010, 0x00060e02, 0xdc788020, 0x00061202, 0xdc788030,
    0x00061602, 0xbf810000,
};

struct Spec {
    std::uint16_t machine = 224;
    bool probe = true;
    bool descriptor = true;
    std::uint64_t descriptorSize = 64;
    std::uint64_t probeSize = Text.size() * 4;
    std::uint64_t textSectionSize = Text.size() * 4;
    std::uint32_t group = 4096;
    std::uint32_t privateBytes = 0;
    std::uint32_t kernarg = 16;
    std::int64_t entry = 4288;
    std::uint32_t rsrc1 = 0x642ed00f;
    std::uint32_t rsrc2 = 0x84;
    std::uint16_t properties = 0x0408;
};

constexpr std::uint64_t RodataAddress = 0x1000;
constexpr std::uint64_t TextAddress = RodataAddress + 4288;

template <typename T>
void Put(std::vector<std::byte>& bytes, std::size_t offset, T value) {
    if (bytes.size() < offset + sizeof(T)) bytes.resize(offset + sizeof(T));
    std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

std::size_t AddString(std::vector<char>& table, const std::string& text) {
    const auto offset = table.size();
    table.insert(table.end(), text.begin(), text.end());
    table.push_back('\0');
    return offset;
}

std::vector<std::byte> Build(const Spec& spec) {
    std::vector<std::byte> bytes(64);
    const auto append = [&](const void* data, std::size_t size) {
        const auto offset = bytes.size();
        bytes.resize(offset + size);
        if (size != 0) std::memcpy(bytes.data() + offset, data, size);
        return offset;
    };
    const auto textOffset = append(Text.data(), Text.size() * 4);
    std::vector<std::byte> descriptor(64);
    Put<std::uint32_t>(descriptor, 0, spec.group);
    Put<std::uint32_t>(descriptor, 4, spec.privateBytes);
    Put<std::uint32_t>(descriptor, 8, spec.kernarg);
    Put<std::int64_t>(descriptor, 16, spec.entry);
    Put<std::uint32_t>(descriptor, 48, spec.rsrc1);
    Put<std::uint32_t>(descriptor, 52, spec.rsrc2);
    Put<std::uint16_t>(descriptor, 56, spec.properties);
    const auto rodataOffset = append(descriptor.data(), descriptor.size());

    std::vector<char> strings{'\0'};
    std::vector<std::byte> symbols(24);
    const auto addSymbol = [&](const std::string& name, std::uint8_t type, std::uint16_t section, std::uint64_t value, std::uint64_t size) {
        const auto at = symbols.size();
        symbols.resize(at + 24);
        Put<std::uint32_t>(symbols, at, static_cast<std::uint32_t>(AddString(strings, name)));
        Put<std::uint8_t>(symbols, at + 4, static_cast<std::uint8_t>(0x10u | type));
        Put<std::uint16_t>(symbols, at + 6, section);
        Put<std::uint64_t>(symbols, at + 8, value);
        Put<std::uint64_t>(symbols, at + 16, size);
    };
    if (spec.probe) addSymbol("probe", 2, 1, TextAddress, spec.probeSize);
    if (spec.descriptor) addSymbol("probe.kd", 1, 2, RodataAddress, spec.descriptorSize);
    const auto symtabOffset = append(symbols.data(), symbols.size());
    const auto strtabOffset = append(strings.data(), strings.size());

    std::vector<char> names{'\0'};
    const auto textName = AddString(names, ".text");
    const auto rodataName = AddString(names, ".rodata");
    const auto symtabName = AddString(names, ".symtab");
    const auto strtabName = AddString(names, ".strtab");
    const auto shstrtabName = AddString(names, ".shstrtab");
    const auto namesOffset = append(names.data(), names.size());
    while (bytes.size() % 8 != 0) bytes.push_back(std::byte{0});
    const auto headersOffset = bytes.size();
    bytes.resize(headersOffset + 6 * 64);
    const auto header = [&](std::size_t index, std::size_t name, std::uint32_t type, std::uint64_t address, std::uint64_t offset, std::uint64_t size, std::uint32_t link) {
        const auto at = headersOffset + index * 64;
        Put<std::uint32_t>(bytes, at, static_cast<std::uint32_t>(name));
        Put<std::uint32_t>(bytes, at + 4, type);
        Put<std::uint64_t>(bytes, at + 16, address);
        Put<std::uint64_t>(bytes, at + 24, offset);
        Put<std::uint64_t>(bytes, at + 32, size);
        Put<std::uint32_t>(bytes, at + 40, link);
    };
    header(1, textName, 1, TextAddress, textOffset, spec.textSectionSize, 0);
    header(2, rodataName, 1, RodataAddress, rodataOffset, 64, 0);
    header(3, symtabName, 2, 0, symtabOffset, symbols.size(), 4);
    header(4, strtabName, 3, 0, strtabOffset, strings.size(), 0);
    header(5, shstrtabName, 3, 0, namesOffset, names.size(), 0);

    std::memcpy(bytes.data(), "\x7f" "ELF", 4);
    Put<std::uint8_t>(bytes, 4, 2);
    Put<std::uint8_t>(bytes, 5, 1);
    Put<std::uint8_t>(bytes, 6, 1);
    Put<std::uint16_t>(bytes, 16, 3);
    Put<std::uint16_t>(bytes, 18, spec.machine);
    Put<std::uint32_t>(bytes, 20, 1);
    Put<std::uint64_t>(bytes, 40, headersOffset);
    Put<std::uint16_t>(bytes, 52, 64);
    Put<std::uint16_t>(bytes, 58, 64);
    Put<std::uint16_t>(bytes, 60, 6);
    Put<std::uint16_t>(bytes, 62, 5);
    return bytes;
}

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void Rejects(const std::string& label, const std::vector<std::byte>& bytes, const std::string& expected) {
    try {
        ParseOracleCodeObject(bytes);
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, label + ": message \"" + error.what() + "\" does not name \"" + expected + "\"");
        return;
    }
    throw std::runtime_error(label + ": accepted");
}

void Rejects(const std::string& label, const std::function<void(Spec&)>& change, const std::string& expected) {
    Spec spec;
    change(spec);
    Rejects(label, Build(spec), expected);
}

}

int main() {
    try {
        const auto kernel = ParseOracleCodeObject(Build(Spec{}));
        Require(kernel.text.size() == Text.size(), "text word count differs");
        for (std::size_t index = 0; index < Text.size(); ++index) Require(kernel.text[index] == Text[index], "text word " + std::to_string(index) + " differs");
        Require(kernel.ldsBytes == 4096, "ldsBytes is not 4096");
        Require(kernel.waveSize == 32, "waveSize is not 32");
        Require(kernel.floatMode.floatMode == 237, "floatMode is not 237");
        Require(kernel.floatMode.dx10Clamp && !kernel.floatMode.ieeeMode && kernel.floatMode.fp16Overflow, "dx10Clamp, ieeeMode or fp16Overflow differs");
        Require(kernel.groupIdEnable == (std::array<bool, 3>{true, false, false}), "groupIdEnable differs");
        Require(!kernel.tgSizeEnable, "tgSizeEnable is set");
        Require(kernel.threadIdComponentCount == 1, "threadIdComponentCount is not 1");

        Spec wide;
        wide.properties = 0x0008;
        wide.rsrc2 = 0x84u | (1u << 8) | (1u << 9) | (1u << 10) | (2u << 11);
        const auto wave64 = ParseOracleCodeObject(Build(wide));
        Require(wave64.waveSize == 64, "waveSize is not 64");
        Require(wave64.groupIdEnable == (std::array<bool, 3>{true, true, true}), "groupIdEnable differs for XYZ");
        Require(wave64.tgSizeEnable && wave64.threadIdComponentCount == 3, "tgSizeEnable or threadIdComponentCount differs");

        Rejects("machine", [](Spec& spec) { spec.machine = 62; }, "e_machine");
        Rejects("probe", [](Spec& spec) { spec.probe = false; }, "probe");
        Rejects("probe.kd", [](Spec& spec) { spec.descriptor = false; }, "probe.kd");
        Rejects("descriptor size", [](Spec& spec) { spec.descriptorSize = 63; }, "probe.kd size");
        Rejects("kernarg_size", [](Spec& spec) { spec.kernarg = 24; }, "kernarg_size");
        Rejects("private segment", [](Spec& spec) { spec.privateBytes = 16; }, "private_segment_fixed_size");
        Rejects("group segment", [](Spec& spec) { spec.group = 65540; }, "group_segment_fixed_size");
        Rejects("private segment enable", [](Spec& spec) { spec.rsrc2 |= 1u; }, "ENABLE_PRIVATE_SEGMENT");
        Rejects("user sgpr count", [](Spec& spec) { spec.rsrc2 = 0x86; }, "USER_SGPR_COUNT");
        Rejects("kernarg pointer", [](Spec& spec) { spec.properties = 0x0400; }, "ENABLE_SGPR_KERNARG_SEGMENT_PTR");
        Rejects("dispatch pointer", [](Spec& spec) { spec.properties = 0x040c; }, "user SGPR");
        Rejects("entry offset", [](Spec& spec) { spec.entry = 4292; }, "kernel_code_entry_byte_offset");
        Rejects("text size", [](Spec& spec) { spec.probeSize = 135; }, "multiple of 4");
        Rejects("zero text size", [](Spec& spec) { spec.probeSize = 0; }, "size 0");
        Rejects("symbol outside section", [](Spec& spec) { spec.textSectionSize = 64; }, "outside section");
        const auto whole = Build(Spec{});
        Rejects("truncated", std::vector<std::byte>(whole.begin(), whole.end() - 100), "truncated");
        Rejects("truncated header", std::vector<std::byte>(whole.begin(), whole.begin() + 30), "truncated");

        for (const auto& [text, expected] : std::initializer_list<std::pair<const char*, std::uint32_t>>{{"0", 0u}, {"1", 1u}, {"16", 16u}, {"1024", 1024u}, {"007", 7u}, {"4294967295", 0xffffffffu}}) {
            Require(ParseOracleCount(text, "threads") == expected, std::string("count \"") + text + "\" was not accepted as " + std::to_string(expected));
        }
        for (const auto& [text, field] : std::initializer_list<std::pair<const char*, const char*>>{{"-18446744073709551615", "threads"}, {"-18446744073709551600", "out_dwords"}, {"-1", "threads"}, {"-0", "out_dwords"}, {"+1", "threads"}, {"", "out_dwords"}, {" 1", "threads"}, {"1 ", "out_dwords"}, {"1a", "threads"}, {"0x10", "out_dwords"}, {"4294967296", "threads"}, {"18446744073709551616", "out_dwords"}, {"99999999999999999999999", "threads"}}) {
            try {
                ParseOracleCount(text, field);
            } catch (const std::runtime_error& error) {
                Require(std::string(error.what()).find(field) != std::string::npos, std::string("count \"") + text + "\": message \"" + error.what() + "\" does not name " + field);
                continue;
            }
            throw std::runtime_error(std::string("count \"") + text + "\" was accepted for " + field);
        }
        std::printf("oracle code object tests passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
