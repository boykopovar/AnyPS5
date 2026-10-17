#include "TestProgress.hpp"
#include <relinker/pipeline/Conversion.hpp>
#include <relinker/patching/GamePatch.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace Relinker;
using namespace Relinker::GamePatch;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void rejects(TAction action, const std::string& fragment) {
    try {
        action();
    } catch (const Domain::RelinkerException& error) {
        require(std::string(error.what()).find(fragment) != std::string::npos, "wrong error, expected `" + fragment + "` in: " + error.what());
        return;
    }
    throw std::runtime_error("accepted: expected `" + fragment + "`");
}

std::string header(const std::string& body) {
    return "format = 1\n" + body;
}

void rejectsText(const std::string& text, const std::size_t line, const std::string& fragment = {}) {
    rejects([&] { ParsePatchFile(text); }, "line " + std::to_string(line) + ":");
    if (!fragment.empty()) rejects([&] { ParsePatchFile(text); }, fragment);
}

PatchEntry entry(const std::string& target, const std::uint64_t address, std::vector<std::uint8_t> expected, std::vector<std::uint8_t> value, const std::string& name = {}) {
    PatchEntry result;
    result.Target = target;
    result.VirtualAddress = address;
    result.Expected = std::move(expected);
    result.Value = std::move(value);
    result.Name = name;
    result.Line = 1;
    return result;
}

std::vector<std::uint8_t> twoSegmentFixture() {
    std::vector<std::uint8_t> bytes(0x3000);
    const std::array<std::uint8_t, 7> magic{0x7f, 'E', 'L', 'F', 2, 1, 1};
    std::copy(magic.begin(), magic.end(), bytes.begin());
    Io::WriteU16(bytes, 16, 3);
    Io::WriteU16(bytes, 18, 62);
    Io::WriteU32(bytes, 20, 1);
    Io::WriteU64(bytes, 32, 64);
    Io::WriteU16(bytes, 52, 64);
    Io::WriteU16(bytes, 54, 56);
    Io::WriteU16(bytes, 56, 2);
    const auto segment = [&](const std::size_t index, const std::uint64_t offset, const std::uint64_t address, const std::uint64_t fileSize, const std::uint64_t memorySize) {
        const auto at = 64 + index * 56;
        Io::WriteU32(bytes, at, 1);
        Io::WriteU32(bytes, at + 4, 6);
        Io::WriteU64(bytes, at + 8, offset);
        Io::WriteU64(bytes, at + 16, address);
        Io::WriteU64(bytes, at + 24, address);
        Io::WriteU64(bytes, at + 32, fileSize);
        Io::WriteU64(bytes, at + 40, memorySize);
        Io::WriteU64(bytes, at + 48, 8);
    };
    segment(0, 0, 0, 0x1100, 0x1100);
    segment(1, 0x2000, 0x1100, 0x100, 0x300);
    for (std::size_t index = 0; index < 0x100; ++index) {
        bytes[0x1000 + index] = static_cast<std::uint8_t>(index);
        bytes[0x2000 + index] = static_cast<std::uint8_t>(0x80 + index);
    }
    return bytes;
}

std::vector<std::uint8_t> conversionFixture() {
    std::vector<std::uint8_t> bytes(0x3000);
    const std::array<std::uint8_t, 7> magic{0x7f, 'E', 'L', 'F', 2, 1, 1};
    std::copy(magic.begin(), magic.end(), bytes.begin());
    Io::WriteU16(bytes, 16, 3);
    Io::WriteU16(bytes, 18, 62);
    Io::WriteU32(bytes, 20, 1);
    Io::WriteU64(bytes, 24, 0x1000);
    Io::WriteU64(bytes, 32, 64);
    Io::WriteU16(bytes, 52, 64);
    Io::WriteU16(bytes, 54, 56);
    Io::WriteU16(bytes, 56, 8);
    Io::WriteU16(bytes, 58, 64);
    const auto programHeader = [&](std::size_t index, unsigned type, unsigned flags, unsigned offset, unsigned size) {
        const auto at = 64 + index * 56;
        Io::WriteU32(bytes, at, type);
        Io::WriteU32(bytes, at + 4, flags);
        Io::WriteU64(bytes, at + 8, offset);
        Io::WriteU64(bytes, at + 16, offset);
        Io::WriteU64(bytes, at + 24, offset);
        Io::WriteU64(bytes, at + 32, size);
        Io::WriteU64(bytes, at + 40, size);
        Io::WriteU64(bytes, at + 48, 8);
    };
    programHeader(0, 1, 5, 0x1000, 8);
    programHeader(1, 1, 6, 0x2000, 0x1000);
    for (std::size_t index = 3; index < 8; ++index) programHeader(index, 0x6fffff01, 0, 0, 0);
    bytes[0x1000] = 0xc3;
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 9> tags{{{5, 0x2600}, {10, 1}, {6, 0x2620}, {11, 24}, {7, 0x2700}, {8, 0}, {9, 24}, {4, 0x2680}, {0, 0}}};
    programHeader(2, 2, 6, 0x2000, tags.size() * 16);
    for (std::size_t index = 0; index < tags.size(); ++index) {
        Io::WriteU64(bytes, 0x2000 + index * 16, tags[index].first);
        Io::WriteU64(bytes, 0x2008 + index * 16, tags[index].second);
    }
    Io::WriteU32(bytes, 0x2680, 1);
    Io::WriteU32(bytes, 0x2684, 1);
    return bytes;
}

ConversionInput conversionInput(const std::vector<std::uint8_t>& executable) {
    ConversionInput result;
    result.Executable = executable;
    result.InputPath = "virtual/title/custom-name.bin";
    result.OutputPath = "virtual/output/title";
    result.ModuleRoot = "virtual/title";
    result.Modules.push_back({"virtual/title/sce_module/plugin.prx", conversionFixture()});
    return result;
}

void grammarAccepted() {
    const auto file = ParsePatchFile(
        "# leading comment\n"
        "format = 1 # trailing comment\n"
        "relinker_version = \"x\"\n"
        "relinker_commit = \"y\"\n"
        "config = \"z\"\n"
        "config_sha256 = \"w\"\n"
        "main_target = \"eboot.elf\"\n"
        "\n"
        "[[patch]]\n"
        "name = \"pool_size\"\n"
        "target = \"eboot.elf\"\n"
        "va = 0x1234\n"
        "type = \"i32\"\n"
        "expected = 0x10000000\n"
        "value = 0x18000000\n"
        "max = 0x7f000000\n"
        "align = 0x1000\n"
        "description = \"has # inside\"\n"
        "\n"
        "[[patch]]\n"
        "target = \"sce_module/libc.prx\"\n"
        "va = 4096\n"
        "expected = \"00ff\"\n"
        "value = \"AB12\"\n");
    require(file.MainTarget == std::string("eboot.elf"), "main_target was not read");
    require(file.Entries.size() == 2, "wrong entry count");
    const auto& typed = file.Entries[0];
    require(typed.Name == "pool_size" && typed.Target == "eboot.elf" && typed.VirtualAddress == 0x1234, "typed entry fields");
    require(typed.Expected == std::vector<std::uint8_t>({0, 0, 0, 0x10}) && typed.Value == std::vector<std::uint8_t>({0, 0, 0, 0x18}), "typed encoding is not little-endian");
    require(typed.Description == "has # inside" && typed.Line == 9, "description or line");
    const auto& raw = file.Entries[1];
    require(raw.Name.empty() && raw.VirtualAddress == 4096, "raw entry fields");
    require(raw.Expected == std::vector<std::uint8_t>({0x00, 0xff}) && raw.Value == std::vector<std::uint8_t>({0xab, 0x12}), "raw hex bytes");
    require(!ParsePatchFile("format = 1\n").MainTarget && ParsePatchFile("format = 1\n").Entries.empty(), "empty file");
}

void grammarRejected() {
    rejects([] { ParsePatchFile(""); }, "format = 1");
    rejectsText("format = 2\n", 1, "unsupported format");
    rejectsText(header("flavour = 1\n"), 2, "unknown top-level key");
    rejectsText(header("[[patch]]\ntarget = \"a\"\nva = 1\nexpected = \"00\"\nvalue = \"01\"\ncolour = 1\n"), 7, "unknown key");
    rejectsText(header("[[patch]]\ntarget = \"a\"\ntarget = \"b\"\n"), 4, "duplicate key");
    rejectsText(header("[patch]\n"), 2, "unsupported table header");
    rejectsText(header("[other]\n"), 2, "unsupported table header");
    rejectsText(header("[[patch]]\ndescription = \"a\\nb\"\n"), 3, "escapes");
    rejectsText(header("[[patch]]\ndescription = [1, 2]\n"), 3, "arrays");
    rejectsText(header("[[patch]]\ndescription = 'x'\n"), 3, "single-quoted");
    rejectsText(header("[[patch]]\ndescription = { a = 1 }\n"), 3, "inline tables");
    rejectsText(header("[[patch]]\ndescription = \"\"\"x\"\"\"\n"), 3, "multiline");
    rejectsText(header("[[patch]]\nva = 1.5\n"), 3, "floats");
    rejectsText(header("[[patch]]\nva = true\n"), 3, "booleans");
    rejectsText(header("[[patch]]\nsub.key = 1\n"), 3, "dotted keys");
    rejectsText(header("[[patch]]\n\"quoted\" = 1\n"), 3, "quoted keys");
    rejectsText(header("[[patch]]\nva\n"), 3, "key = value");
    rejectsText(header("[[patch]]\nva = 1_000\n"), 3);
    rejectsText(header("[[patch]]\nva = \"unterminated\n"), 3, "unterminated");
}

std::string typedEntry(const std::string& type, const std::string& expected, const std::string& value, const std::string& extra = {}) {
    return header("[[patch]]\ntarget = \"a\"\nva = 1\ntype = \"" + type + "\"\nexpected = " + expected + "\nvalue = " + value + "\n" + extra);
}

void integerRanges() {
    struct Case { const char* Type; const char* Fits; const char* Overflows; std::size_t Width; };
    for (const auto& c : {Case{"u8", "255", "256", 1}, Case{"u16", "65535", "65536", 2}, Case{"u32", "0xffffffff", "0x100000000", 4}, Case{"u64", "0xffffffffffffffff", "0x10000000000000000", 8},
                          Case{"i8", "127", "128", 1}, Case{"i16", "32767", "32768", 2}, Case{"i32", "0x7fffffff", "0x80000000", 4}, Case{"i64", "0x7fffffffffffffff", "0x8000000000000000", 8}}) {
        const auto accepted = ParsePatchFile(typedEntry(c.Type, "0", c.Fits));
        require(accepted.Entries.front().Value.size() == c.Width, std::string("width of ") + c.Type);
        rejects([&] { ParsePatchFile(typedEntry(c.Type, "0", c.Overflows)); }, "line 7");
    }
    require(ParsePatchFile(typedEntry("i32", "-1", "-2147483648")).Entries.front().Value == std::vector<std::uint8_t>({0, 0, 0, 0x80}), "i32 minimum");
    require(ParsePatchFile(typedEntry("i8", "-128", "-1")).Entries.front().Value == std::vector<std::uint8_t>({0xff}), "i8 -1");
    require(ParsePatchFile(typedEntry("i64", "-1", "-1")).Entries.front().Expected == std::vector<std::uint8_t>(8, 0xff), "i64 -1");
    rejects([] { ParsePatchFile(typedEntry("i8", "0", "-129")); }, "line 7");
    rejects([] { ParsePatchFile(typedEntry("u8", "0", "-1")); }, "line 7");
    rejects([] { ParsePatchFile(typedEntry("i16", "40000", "0")); }, "line 6");
    rejects([] { ParsePatchFile(typedEntry("f32", "0", "0")); }, "unknown type");
    rejects([] { ParsePatchFile(typedEntry("u32", "\"00\"", "1")); }, "integer");
}

void boundsAndWidth() {
    ParsePatchFile(typedEntry("u32", "0", "8", "min = 8\nmax = 8\nalign = 8\n"));
    rejects([] { ParsePatchFile(typedEntry("u32", "0", "7", "min = 8\n")); }, "below min");
    rejects([] { ParsePatchFile(typedEntry("u32", "0", "9", "max = 8\n")); }, "above max");
    rejects([] { ParsePatchFile(typedEntry("u32", "0", "12", "align = 8\n")); }, "multiple of align");
    rejects([] { ParsePatchFile(typedEntry("u32", "0", "8", "align = 0\n")); }, "align must be positive");
    ParsePatchFile(typedEntry("i32", "0", "-2097152", "align = 2097152\n"));
    rejects([] { ParsePatchFile(typedEntry("i32", "0", "-1048576", "align = 2097152\n")); }, "multiple of align");
    rejects([] { ParsePatchFile(typedEntry("u32", "0", "768MiB")); }, "line 7");
    rejects([] { ParsePatchFile(typedEntry("u32", "0", "007")); }, "leading zeros");
    ParsePatchFile(typedEntry("u32", "0", "0"));
    ParsePatchFile(typedEntry("i32", "-0", "10"));
    rejects([] { ParsePatchFile(header("[[patch]]\ntarget = \"a\"\nva = 1\nexpected = \"00\"\nvalue = \"0000\"\nmin = 1\n")); }, "requires a type");
    rejects([] { ParsePatchFile(header("[[patch]]\ntarget = \"a\"\nva = 1\nexpected = \"00\"\nvalue = \"0000\"\n")); }, "same width");
    rejects([] { ParsePatchFile(header("[[patch]]\ntarget = \"a\"\nva = 1\nexpected = \"0\"\nvalue = \"1\"\n")); }, "even-length");
    rejects([] { ParsePatchFile(header("[[patch]]\ntarget = \"a\"\nva = 1\nexpected = \"zz\"\nvalue = \"00\"\n")); }, "non-hex");
    rejects([] { ParsePatchFile(header("[[patch]]\nva = 1\nexpected = \"00\"\nvalue = \"01\"\n")); }, "missing key `target`");
    rejects([] { ParsePatchFile(header("[[patch]]\ntarget = \"a\"\nva = -1\nexpected = \"00\"\nvalue = \"01\"\n")); }, "va must be");
}

std::string namedEntry(const std::string& name, const std::string& extra = {}) {
    return header("[[patch]]\nname = \"" + name + "\"\ntarget = \"a\"\nva = 1\nexpected = \"00\"\nvalue = \"01\"\n" + extra);
}

void names() {
    require(ParsePatchFile(namedEntry("pool_size_2")).Entries.front().Name == "pool_size_2", "valid name");
    for (const char* bad : {"Upper", "has-dash", "has space", "", "dot.ted", "\xc3\xa9"}) rejects([&] { ParsePatchFile(namedEntry(bad)); }, "line 3: name must be");
    rejects([] { ParsePatchFile(header("[[patch]]\nname = 5\ntarget = \"a\"\nva = 1\nexpected = \"00\"\nvalue = \"01\"\n")); }, "line 3: name must be");
    rejects([] { ParsePatchFile(namedEntry("same") + "\n[[patch]]\nname = \"same\"\ntarget = \"a\"\nva = 9\nexpected = \"00\"\nvalue = \"01\"\n"); }, "line 10: duplicate name");
    ParsePatchFile(namedEntry("one") + "\n[[patch]]\nname = \"two\"\ntarget = \"a\"\nva = 9\nexpected = \"00\"\nvalue = \"01\"\n");
}

void ranges() {
    const ElfReader reader(twoSegmentFixture());
    const auto resolved = ResolveRanges(reader, {entry("t", 0x1000, {0, 1}, {9, 9}), entry("t", 0x1100, {0x80}, {1}), entry("t", 0x11fc, {0xfc, 0xfd, 0xfe, 0xff}, {0, 0, 0, 0})});
    require(resolved[0].Offset == 0x1000 && resolved[0].Length == 2, "first segment translation");
    require(resolved[1].Offset == 0x2000, "second segment translation");
    require(resolved[2].Offset == 0x20fc, "second segment end");
    rejects([&] { ResolveRanges(reader, {entry("t", 0x10fe, {0, 0, 0, 0}, {1, 1, 1, 1})}); }, "one PT_LOAD");
    rejects([&] { ResolveRanges(reader, {entry("t", 0x1200, {0}, {1})}); }, "one PT_LOAD");
    rejects([&] { ResolveRanges(reader, {entry("t", 0x11fe, {0, 0, 0}, {1, 1, 1})}); }, "one PT_LOAD");
    rejects([&] { ResolveRanges(reader, {entry("t", 0x5000, {0}, {1})}); }, "one PT_LOAD");
    rejects([&] { ResolveRanges(reader, {entry("t", 0x1000, {0, 1, 2}, {1, 1, 1}), entry("t", 0x1002, {2, 3}, {1, 1})}); }, "overlaps");
    ResolveRanges(reader, {entry("t", 0x1000, {0, 1}, {1, 1}), entry("t", 0x1002, {2, 3}, {1, 1})});
}

void headerRange() {
    const ElfReader reader(twoSegmentFixture());
    rejects([&] { ResolveRanges(reader, {entry("t", 0x0, {0x7f}, {0})}); }, "program header table");
    rejects([&] { ResolveRanges(reader, {entry("t", 0x40, {0}, {1})}); }, "program header table");
    rejects([&] { ResolveRanges(reader, {entry("t", 0xaf, {0}, {1})}); }, "program header table");
    const auto ok = ResolveRanges(reader, {entry("t", 0xb0, {0}, {1})});
    require(ok.front().Offset == 0xb0, "range after the header table was rejected");
}

void applyAndMismatch() {
    auto bytes = twoSegmentFixture();
    const auto original = bytes;
    const std::vector<PatchEntry> entries{entry("t", 0x1000, {0, 1}, {9, 8}, "first"), entry("t", 0x1100, {0x80}, {1})};
    const auto patched = PatchTarget(bytes, "t", entries);
    require(patched.Applied.size() == 2 && patched.Applied[0].Offset == 0x1000 && patched.Applied[0].Name == "first", "applied records");
    require(bytes[0x1000] == 9 && bytes[0x1001] == 8 && bytes[0x2000] == 1, "values were not written");
    for (std::size_t index = 0; index < bytes.size(); ++index)
        if (index != 0x1000 && index != 0x1001 && index != 0x2000) require(bytes[index] == original[index], "byte outside the patch changed");

    bytes = original;
    rejects([&] { PatchTarget(bytes, "t", {entry("t", 0x1000, {0, 1}, {9, 8}), entry("t", 0x1100, {0x81}, {1})}); }, "expected 81 found 80");
    require(bytes == original, "a mismatch wrote bytes");
    require(PatchTarget(bytes, "other", entries).Applied.empty() && bytes == original, "entry hit another target");
}

void overlapWithWriters() {
    const std::vector<PatchedRange> patched{{0x1010, 2, 0x401010}};
    const auto reject = [&](const WriterSites& sites, const std::string& writer) { rejects([&] { CheckOverlaps("t", patched, sites); }, writer); };
    const std::vector<Codegen::Amd64OnlySubstitutionReport> inPlace{{"VPSHUFB", 0x100f, 4, 4, Codegen::Amd64OnlyLowering::InPlace}};
    reject({.Reports = inPlace}, "Intel in-place report");
    const std::vector<Codegen::Amd64OnlySubstitutionReport> kept{{"VPSHUFB", 0x100f, 4, 4, Codegen::Amd64OnlyLowering::Kept}};
    CheckOverlaps("t", patched, {.Reports = kept});
    const std::vector<Codegen::Amd64OnlySubstitutionReport> distant{{"VPSHUFB", 0x1000, 4, 4, Codegen::Amd64OnlyLowering::InPlace}};
    CheckOverlaps("t", patched, {.Reports = distant});
    const std::vector<Codegen::TrampolineSite> absorbed{{0x1008, 0x401008, 0x10, {}, {}, 0}};
    reject({.Trampolines = absorbed}, "Intel trampoline site");
    const std::vector<Codegen::TrampolineSite> shortSite{{0x1008, 0x401008, 4, {}, {}, 0}};
    CheckOverlaps("t", patched, {.Trampolines = shortSite});
    const std::vector<RelinkPatch> relink{{0x1011, {1, 2, 3}}};
    reject({.RelinkPatches = relink}, "relinker patch");
    const std::vector<RelinkPatch> relinkAfter{{0x1012, {1, 2, 3}}};
    CheckOverlaps("t", patched, {.RelinkPatches = relinkAfter});
    const std::vector<WriterRange> trap{{0x1011, 2, "syscall trap"}};
    reject({.Traps = trap}, "syscall trap");
    const std::vector<WriterRange> trapBefore{{0x100e, 2, "syscall trap"}};
    CheckOverlaps("t", patched, {.Traps = trapBefore});
    const std::vector<WriterRange> window{{0x100f, 4, "window"}};
    reject({.Traps = window}, "window");
}

void conversion() {
    TestProgress progress;
    const Conversion converter(progress);
    const auto executable = conversionFixture();
    ConversionOptions plain;
    plain.TargetName = "custom-name.bin";
    const auto baseline = converter.Convert(conversionInput(executable), plain);
    require(baseline.AppliedPatches.empty(), "patches applied without entries");

    ConversionOptions patched = plain;
    patched.Patches = {entry("custom-name.bin", 0x1001, {0}, {0x90}, "nop"), entry("sce_module/plugin.prx", 0x1002, {0}, {0x41})};
    const auto output = converter.Convert(conversionInput(executable), patched);
    require(output.AppliedPatches.size() == 2, "executable and module patches were not both applied");
    require(output.AppliedPatches[0].Target == "custom-name.bin" && output.AppliedPatches[1].Target == "sce_module/plugin.prx", "applied targets");
    require(output.Executable != baseline.Executable && output.Modules.front().Bytes != baseline.Modules.front().Bytes, "patches changed nothing");
    require(progress.Messages.find("Patch: custom-name.bin [nop] va=0x1001 off=0x1001 00 -> 90\n") != std::string::npos, "progress line: " + progress.Messages);
    require(progress.Messages.find("Patch: sce_module/plugin.prx va=0x1002 off=0x1002 00 -> 41\n") != std::string::npos, "module progress line");

    ConversionOptions other = patched;
    other.TargetName = "other.bin";
    auto different = executable;
    different[0x1005] = 0x90;
    const auto second = converter.Convert(conversionInput(different), other);
    require(second.Modules.size() == output.Modules.size() && second.Modules.front().Bytes == output.Modules.front().Bytes, "shared modules differ between executables");
    require(second.AppliedPatches.size() == 1, "an executable entry hit another target");

    ConversionOptions failing = plain;
    failing.Patches = {entry("custom-name.bin", 0x1001, {7}, {1})};
    rejects([&] { converter.Convert(conversionInput(executable), failing); }, "expected 07 found 00");
    failing.Patches = {entry("sce_module/plugin.prx", 0x1002, {7}, {1})};
    rejects([&] { converter.Convert(conversionInput(executable), failing); }, "expected 07 found 00");
    failing.Patches = {entry("custom-name.bin", 0x1000, {0xc3, 0}, {0, 0}), entry("custom-name.bin", 0x1001, {0}, {1})};
    rejects([&] { converter.Convert(conversionInput(executable), failing); }, "overlaps");

    const auto repeated = converter.Convert(conversionInput(executable), plain);
    require(repeated.Executable == baseline.Executable && repeated.Modules.front().Bytes == baseline.Modules.front().Bytes, "no entries changed the output");
}

void targets() {
    const std::vector<PatchEntry> entries{entry("a.elf", 1, {0}, {1}), entry("sce_module/libc.prx", 1, {0}, {1})};
    MatchTargets(entries, {"a.elf", "b.elf", "sce_module/libc.prx"});
    rejects([&] { MatchTargets(entries, {"a.elf", "b.elf"}); }, "unmatched patch target: sce_module/libc.prx");
    rejects([&] { MatchTargets(entries, {"a.elf", "a.elf", "sce_module/libc.prx"}); }, "ambiguous patch target: a.elf");
    MatchTargets({}, {});
}

void verify() {
    auto bytes = twoSegmentFixture();
    const std::vector<VerifyTarget> executables{{"main.elf", bytes}};
    auto file = ParsePatchFile(header("main_target = \"main.elf\"\n[[patch]]\nname = \"one\"\ntarget = \"main.elf\"\nva = 0x1000\nexpected = \"0001\"\nvalue = \"0908\"\n"
                                      "[[patch]]\ntarget = \"main.elf\"\nva = 0x1100\nexpected = \"ff\"\nvalue = \"01\"\n"
                                      "[[patch]]\ntarget = \"sce_module/libc.prx\"\nva = 0x1000\nexpected = \"00\"\nvalue = \"01\"\n"));
    const auto before = bytes;
    const std::vector<VerifyTarget> modules{{"sce_module/libc.prx", twoSegmentFixture()}};
    auto input = VerifyPatches(file, executables, modules, false);
    require(input.size() == 3 && input[0].Status == VerifyStatus::Applicable && input[0].Name == "one" && input[0].Offset == 0x1000, "input applicable");
    require(input[1].Status == VerifyStatus::Mismatch && input[1].Detail.find("expected ff found 80") != std::string::npos, "input mismatch");
    require(input[2].Status == VerifyStatus::Applicable, "module entry in input mode");
    require(bytes == before && executables.front().Bytes == before, "verify wrote bytes");

    auto patchedBytes = twoSegmentFixture();
    patchedBytes[0x1000] = 9;
    patchedBytes[0x1001] = 8;
    patchedBytes[0x2000] = 1;
    const std::vector<VerifyTarget> outputs{{"main.elf", patchedBytes}};
    auto output = VerifyPatches(file, outputs, {}, true);
    require(output[0].Status == VerifyStatus::Applied && output[1].Status == VerifyStatus::Applied, "output applied");
    require(output[2].Status == VerifyStatus::NotChecked && output[2].Detail == "module, or executable not named with --executable", "output module not-checked");
    rejects([&] { VerifyPatches(file, {{"main.elf", patchedBytes}, {"main.elf", patchedBytes}}, {}, true); }, "ambiguous patch target: main.elf");
    rejects([&] { VerifyPatches(file, {{"main.elf", patchedBytes}, {"bin/a.elf", patchedBytes}, {"bin/a.elf", patchedBytes}}, {}, true); }, "ambiguous patch target: bin/a.elf");
    output = VerifyPatches(file, {{"main.elf", twoSegmentFixture()}}, {}, true);
    require(output[0].Status == VerifyStatus::Mismatch && output[0].Detail.find("not applied") != std::string::npos, "unpatched output");

    rejects([&] { VerifyPatches(ParsePatchFile(header("[[patch]]\ntarget = \"typo.prx\"\nva = 0x1000\nexpected = \"00\"\nvalue = \"01\"\n")), executables, modules, false); }, "unmatched patch target: typo.prx");
    rejects([&] { VerifyPatches(ParsePatchFile(header("[[patch]]\ntarget = \"typo.prx\"\nva = 0x1000\nexpected = \"00\"\nvalue = \"01\"\n")), executables, modules, false); }, "--verify-output");
    rejects([&] { VerifyPatches(ParsePatchFile(header("[[patch]]\ntarget = \"main.elf\"\nva = 0x1000\nexpected = \"00\"\nvalue = \"01\"\n")), outputs, {}, true); }, "no main_target");
    auto exec = twoSegmentFixture();
    Io::WriteU16(exec, 16, 2);
    rejects([&] { VerifyPatches(file, {{"main.elf", exec}}, {}, true); }, "verify the input instead");
    rejects([&] { VerifyPatches(file, {{"main.elf", {'M', 'Z', 0x90, 0}}}, {}, true); }, "verify the input instead");
    rejects([&] { VerifyPatches(file, {{"main.elf", {0xcf, 0xfa, 0xed, 0xfe, 7, 0, 0, 1}}}, {}, true); }, "verify the input instead");
    rejects([&] { VerifyPatches(file, {{"main.elf", {}}}, {}, true); }, "verify the input instead");

    auto sce = twoSegmentFixture();
    Io::WriteU16(sce, 16, 0xfe10);
    require(VerifyPatches(ParsePatchFile(header("[[patch]]\ntarget = \"main.elf\"\nva = 0x1000\nexpected = \"00\"\nvalue = \"01\"\n")), {{"main.elf", sce}}, {}, false).front().Status == VerifyStatus::Applicable,
            "input mode must accept any e_type");

    require(OutputExecutablePath("out/app.elf", "bin/tool.elf") == std::filesystem::path("out/bin.tool.elf.elf"), "output executable name");
    require(OutputExecutablePath("out/app.exe", "./a/b/c.bin") == std::filesystem::path("out/a.b.c.bin.exe"), "output executable extension");
    for (const char* bad : {"../x", "/abs/x", "dir/", ""}) rejects([&] { OutputExecutablePath("out/app.elf", bad); }, "--executable must be a file");
}

void rangeFailureInVerify() {
    const auto file = ParsePatchFile(header("[[patch]]\ntarget = \"main.elf\"\nva = 0x1200\nexpected = \"00\"\nvalue = \"01\"\n"));
    const auto results = VerifyPatches(file, {{"main.elf", twoSegmentFixture()}}, {}, false);
    require(results.front().Status == VerifyStatus::Mismatch && results.front().Detail.find("PT_LOAD") != std::string::npos, "range failure should be a mismatch");
}

void manifest() {
    const AppliedPatch first{"main.elf", 0x1000, 0x1000, {0, 1}, {9, 8}, "set \"x\" later", "alpha"};
    const AppliedPatch second{"sce_module/libc.prx", 0x2000, 0x2000, {5}, {6}, "", ""};
    AppliedPatch duplicate = first;
    const auto merged = AggregateManifest({{first, second}, {duplicate, second}});
    require(merged.size() == 2, "records were not deduplicated");
    rejects([&] { WriteManifest(merged, "main.elf", ""); }, "Patch manifest cannot hold the description: set \"x\" later");

    const AppliedPatch plain{"main.elf", 0x1000, 0x1000, {0, 1}, {9, 8}, "plain description", "alpha"};
    const auto text = WriteManifest(AggregateManifest({{plain}, {second}}), "main.elf", "");
    const auto parsed = ParsePatchFile(text);
    require(parsed.MainTarget == std::string("main.elf"), "main_target did not round-trip");
    require(parsed.Entries.size() == 2, "entry count did not round-trip");
    require(parsed.Entries[0].Name == "alpha" && parsed.Entries[0].Target == "main.elf" && parsed.Entries[0].VirtualAddress == 0x1000, "first entry");
    require(parsed.Entries[0].Expected == plain.Expected && parsed.Entries[0].Value == plain.Value && parsed.Entries[0].Description == "plain description", "first bytes");
    require(parsed.Entries[1].Name.empty() && parsed.Entries[1].Target == "sce_module/libc.prx" && parsed.Entries[1].Description.empty(), "second entry");
    require(text.find("name = \"alpha\"") != std::string::npos && text.find("main_target = \"main.elf\"") != std::string::npos, "manifest text");
    require(WriteManifest(AggregateManifest({{plain}, {second}}), "main.elf", "") == text, "manifest is not deterministic");
}

void reservedNames() {
    for (const char* reserved : {"format", "relinker_version", "relinker_commit", "config", "config_sha256", "main_target", "settings", "settings_sha256"})
        rejects([&] { ParsePatchFile(namedEntry(reserved)); }, "line 3: name is reserved");
    ParsePatchFile(header("settings = \"a\"\nsettings_sha256 = \"b\"\n"));
}

void relocationOverlap() {
    std::vector<std::uint8_t> table(48);
    Io::WriteU64(table, 0, 0x2010);
    Io::WriteU64(table, 24, 0x2100);
    const std::vector<PatchedRange> hit{{0x1010, 2, 0x2017}};
    const std::vector<PatchedRange> adjacent{{0x1010, 2, 0x2018}};
    const std::vector<PatchedRange> before{{0x1010, 2, 0x200f}};
    rejects([&] { CheckOverlaps("t", hit, {.RelaData = table}); }, "RelaData relocation target");
    rejects([&] { CheckOverlaps("t", hit, {.RelaPltData = table}); }, "RelaPltData relocation target");
    rejects([&] { CheckOverlaps("t", {{0x1010, 1, 0x2100}}, {.RelaData = table}); }, "relocation target at va=0x2100");
    rejects([&] { CheckOverlaps("t", before, {.RelaPltData = table}); }, "RelaPltData");
    CheckOverlaps("t", adjacent, {.RelaData = table, .RelaPltData = table});
    CheckOverlaps("t", {{0x1010, 2, 0x200c}}, {.RelaData = table});
}

void manifestConfig() {
    const AppliedPatch patch{"main.elf", 0x1000, 0x1000, {0}, {1}, "", ""};
    const auto config = [&](const std::string& path) { return WriteManifest({patch}, "main.elf", path); };
    require(config("").find("config =") == std::string::npos, "empty config was recorded");
    require(config("dir/p.toml").find("config = \"dir/p.toml\"\n") != std::string::npos, "forward-slash path");
    rejects([&] { config("dir/p\x7f.toml"); }, "Patch manifest cannot hold the config");
    rejects([&] { config("dir/p\x01.toml"); }, "Patch manifest cannot hold the config: p\x01.toml");
    const auto quoted = config("we\"ird/p.toml");
    require(quoted.find("config = \"p.toml\"\n") != std::string::npos, "quote path was not reduced to the file name");
    require(ParsePatchFile(quoted).Entries.size() == 1, "manifest with a reduced config did not parse");
    rejects([&] { config("dir/a\"b.toml"); }, "Patch manifest cannot hold the config");
    const auto hold = [&](AppliedPatch bad, const std::string& field) { rejects([&] { WriteManifest({bad}, "main.elf", ""); }, "Patch manifest cannot hold the " + field + ": "); };
    auto bad = patch;
    bad.Name = "a\"b";
    hold(bad, "name");
    bad = patch;
    bad.Name = "a\\b";
    hold(bad, "name");
    bad = patch;
    bad.Target = "a\"b";
    hold(bad, "target");
    bad = patch;
    bad.Description = "a\"b";
    hold(bad, "description");
}

void typedManifest() {
    const auto file = ParsePatchFile(header("main_target = \"main.elf\"\n[[patch]]\nname = \"neg\"\ntarget = \"main.elf\"\nva = 0x1000\ntype = \"i32\"\nexpected = -1\nvalue = 0x18000000\nmin = -5\nmax = 0x7f000000\nalign = 0x1000\n"
                                      "[[patch]]\ntarget = \"main.elf\"\nva = 0x1100\ntype = \"u8\"\nexpected = 128\nvalue = 1\n"
                                      "[[patch]]\ntarget = \"main.elf\"\nva = 0x1104\nexpected = \"84\"\nvalue = \"02\"\n"));
    auto bytes = twoSegmentFixture();
    Io::WriteU32(bytes, 0x1000, 0xffffffff);
    const auto before = bytes;
    const auto patched = PatchTarget(bytes, "main.elf", file.Entries);
    require(patched.Applied.size() == 3 && patched.Applied[0].Type == "i32" && patched.Applied[0].Min && patched.Applied[0].Min->Negative, "typed fields are not carried");
    const auto text = WriteManifest(AggregateManifest({patched.Applied}), "main.elf", "");
    require(text.find("type = \"i32\"") != std::string::npos && text.find("expected = -1\n") != std::string::npos && text.find("min = -5\n") != std::string::npos
            && text.find("max = 0x7f000000\n") != std::string::npos && text.find("align = 0x1000\n") != std::string::npos && text.find("value = 0x18000000\n") != std::string::npos, "typed manifest text: " + text);
    require(text.find("expected = \"84\"") != std::string::npos, "raw entry stayed raw");
    const auto reparsed = ParsePatchFile(text);
    require(reparsed.Entries.size() == 3 && reparsed.Entries[0].Name == "neg" && reparsed.Entries[0].Type == "i32", "typed round-trip");
    for (std::size_t index = 0; index < 3; ++index)
        require(reparsed.Entries[index].Expected == file.Entries[index].Expected && reparsed.Entries[index].Value == file.Entries[index].Value, "round-trip bytes");
    auto again = before;
    PatchTarget(again, "main.elf", reparsed.Entries);
    require(again == bytes, "re-applying the manifest gives different bytes");
}

}

int main() {
    try {
        grammarAccepted();
        grammarRejected();
        integerRanges();
        boundsAndWidth();
        names();
        reservedNames();
        relocationOverlap();
        typedManifest();
        manifestConfig();
        ranges();
        headerRange();
        applyAndMismatch();
        overlapWithWriters();
        conversion();
        targets();
        verify();
        rangeFailureInVerify();
        manifest();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
