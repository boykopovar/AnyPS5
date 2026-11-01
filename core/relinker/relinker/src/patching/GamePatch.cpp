#include <relinker/patching/GamePatch.hpp>
#include <domain/Types.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <io/NativePath.hpp>
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace Relinker::GamePatch {

namespace {

using Integer = PatchInteger;

struct RawValue {
    bool IsString = false;
    std::string Text;
    Integer Number;
    std::size_t Line = 0;
};

struct IntegerType {
    std::string_view Name;
    unsigned Bits;
    bool Signed;
};

constexpr std::array<IntegerType, 8> kIntegerTypes{{
    {"u8", 8, false}, {"u16", 16, false}, {"u32", 32, false}, {"u64", 64, false},
    {"i8", 8, true}, {"i16", 16, true}, {"i32", 32, true}, {"i64", 64, true},
}};

constexpr std::array<std::string_view, 8> kIgnoredTopLevelKeys{"relinker_version", "relinker_commit", "config", "config_sha256", "main_target", "linux_base", "settings", "settings_sha256"};
constexpr std::array<std::string_view, 9> kReservedNames{"format", "relinker_version", "relinker_commit", "config", "config_sha256", "main_target", "linux_base", "settings", "settings_sha256"};
constexpr std::array<std::string_view, 10> kEntryKeys{"name", "target", "va", "type", "expected", "value", "min", "max", "align", "description"};

std::string HexValue(const std::uint64_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << value;
    return stream.str();
}

std::string IntegerText(const Integer& number) {
    return number.Negative ? "-" + std::to_string(number.Magnitude) : HexValue(number.Magnitude);
}

[[noreturn]] void FailLine(const std::size_t line, const std::string& message, const std::string_view text) {
    throw Domain::RelinkerException("patch file line " + std::to_string(line) + ": " + message + ": `" + std::string(text) + "`");
}

std::string_view Trim(std::string_view text) {
    constexpr std::string_view whitespace = " \t\r";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

bool IsControlCharacter(const char character) {
    return (static_cast<unsigned char>(character) < 0x20 && character != '\t') || character == 0x7f;
}

std::string_view StripComment(const std::string_view line, const std::size_t lineNumber) {
    bool inString = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        if (line[index] == '"') inString = !inString;
        else if (line[index] == '#' && !inString) {
            for (const char character : line.substr(index + 1))
                if (IsControlCharacter(character)) FailLine(lineNumber, "control characters are not supported in comments", line);
            return line.substr(0, index);
        }
    }
    return line;
}

bool Contains(const auto& names, const std::string_view name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

bool IsKeyCharacter(const char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '_';
}

int HexDigit(const char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

Integer ParseInteger(const std::string_view text, const std::size_t line, const std::string_view whole) {
    std::string_view digits = text;
    Integer number;
    if (digits.starts_with('-')) {
        number.Negative = true;
        digits.remove_prefix(1);
    }
    if (digits.empty()) FailLine(line, "unsupported value", whole);
    const bool hexadecimal = digits.starts_with("0x");
    if (hexadecimal) {
        if (number.Negative) FailLine(line, "hexadecimal integers cannot be negative", whole);
        digits.remove_prefix(2);
        if (digits.empty()) FailLine(line, "unsupported value", whole);
    }
    if (!hexadecimal && digits.size() > 1 && digits.front() == '0') FailLine(line, "decimal integers cannot have leading zeros", whole);
    for (const char character : digits) {
        const int digit = HexDigit(character);
        if (digit < 0 || (!hexadecimal && digit > 9)) {
            if (!hexadecimal && (character == '.' || character == 'e' || character == 'E')) FailLine(line, "floats are not supported", whole);
            FailLine(line, "unsupported value", whole);
        }
        const std::uint64_t base = hexadecimal ? 16 : 10;
        if (number.Magnitude > (UINT64_MAX - static_cast<std::uint64_t>(digit)) / base) FailLine(line, "integer is out of range", whole);
        number.Magnitude = number.Magnitude * base + static_cast<std::uint64_t>(digit);
    }
    if (number.Magnitude == 0) number.Negative = false;
    return number;
}

RawValue ParseValue(const std::string_view text, const std::size_t line, const std::string_view whole) {
    RawValue value;
    value.Line = line;
    if (text.empty()) FailLine(line, "missing value", whole);
    if (text.front() == '"') {
        if (text.starts_with("\"\"\"")) FailLine(line, "multiline strings are not supported", whole);
        const auto end = text.find('"', 1);
        if (end == std::string_view::npos) FailLine(line, "unterminated string", whole);
        if (end + 1 != text.size()) FailLine(line, "unexpected text after string", whole);
        const auto content = text.substr(1, end - 1);
        if (content.find('\\') != std::string_view::npos) FailLine(line, "escapes are not supported in strings", whole);
        for (const char character : content)
            if (IsControlCharacter(character)) FailLine(line, "control characters are not supported in strings", whole);
        value.IsString = true;
        value.Text = std::string(content);
        return value;
    }
    if (text.front() == '\'') FailLine(line, "single-quoted strings are not supported", whole);
    if (text.front() == '[') FailLine(line, "arrays are not supported", whole);
    if (text.front() == '{') FailLine(line, "inline tables are not supported", whole);
    if (text == "true" || text == "false") FailLine(line, "booleans are not supported", whole);
    if (text == "inf" || text == "nan" || text.front() == '+') FailLine(line, "unsupported value", whole);
    value.Number = ParseInteger(text, line, whole);
    return value;
}

std::vector<std::uint8_t> ParseHexBytes(const RawValue& value, const std::string_view key, const std::string_view whole) {
    if (!value.IsString) FailLine(value.Line, std::string(key) + " must be a hex byte string without a type", whole);
    if (value.Text.empty() || value.Text.size() % 2 != 0) FailLine(value.Line, std::string(key) + " must be a nonempty even-length hex byte string", value.Text);
    std::vector<std::uint8_t> bytes;
    for (std::size_t index = 0; index < value.Text.size(); index += 2) {
        const int high = HexDigit(value.Text[index]);
        const int low = HexDigit(value.Text[index + 1]);
        if (high < 0 || low < 0) FailLine(value.Line, std::string(key) + " contains a non-hex character", value.Text);
        bytes.push_back(static_cast<std::uint8_t>(high * 16 + low));
    }
    return bytes;
}

bool IntegerLess(const Integer& left, const Integer& right) {
    if (left.Negative != right.Negative) return left.Negative;
    return left.Negative ? left.Magnitude > right.Magnitude : left.Magnitude < right.Magnitude;
}

bool FitsType(const Integer& number, const IntegerType& type) {
    if (!type.Signed) return !number.Negative && (type.Bits == 64 || number.Magnitude < (std::uint64_t{1} << type.Bits));
    const std::uint64_t limit = std::uint64_t{1} << (type.Bits - 1);
    return number.Negative ? number.Magnitude <= limit : number.Magnitude <= limit - 1;
}

std::vector<std::uint8_t> EncodeInteger(const Integer& number, const IntegerType& type) {
    const std::uint64_t raw = number.Negative ? std::uint64_t{0} - number.Magnitude : number.Magnitude;
    std::vector<std::uint8_t> bytes;
    for (unsigned index = 0; index < type.Bits / 8; ++index) bytes.push_back(static_cast<std::uint8_t>(raw >> (8 * index)));
    return bytes;
}

using Table = std::map<std::string, RawValue>;

const RawValue* Find(const Table& table, const std::string& key) {
    const auto found = table.find(key);
    return found == table.end() ? nullptr : &found->second;
}

PatchEntry BuildEntry(const Table& table, const std::size_t headerLine, std::set<std::string>& names) {
    PatchEntry entry;
    entry.Line = headerLine;
    const auto require = [&](const char* key) -> const RawValue& {
        const auto* value = Find(table, key);
        if (!value) FailLine(headerLine, std::string("missing key `") + key + "`", "[[patch]]");
        return *value;
    };
    if (const auto* name = Find(table, "name")) {
        if (!name->IsString || name->Text.empty() || !std::all_of(name->Text.begin(), name->Text.end(), [](const char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }))
            FailLine(name->Line, "name must be a nonempty string of [a-z0-9_]", name->Text);
        if (Contains(kReservedNames, name->Text)) FailLine(name->Line, "name is reserved", name->Text);
        if (!names.insert(name->Text).second) FailLine(name->Line, "duplicate name", name->Text);
        entry.Name = name->Text;
    }
    const auto& target = require("target");
    if (!target.IsString || target.Text.empty()) FailLine(target.Line, "target must be a nonempty string", target.Text);
    entry.Target = target.Text;
    const auto& address = require("va");
    if (address.IsString || address.Number.Negative) FailLine(address.Line, "va must be a nonnegative integer", address.Text);
    entry.VirtualAddress = address.Number.Magnitude;
    const auto& expected = require("expected");
    const auto& replacement = require("value");
    if (const auto* description = Find(table, "description")) {
        if (!description->IsString) FailLine(description->Line, "description must be a string", description->Text);
        entry.Description = description->Text;
    }
    const auto* type = Find(table, "type");
    if (!type) {
        for (const char* key : {"min", "max", "align"})
            if (const auto* value = Find(table, key)) FailLine(value->Line, std::string(key) + " requires a type", key);
        entry.Expected = ParseHexBytes(expected, "expected", "expected");
        entry.Value = ParseHexBytes(replacement, "value", "value");
        if (entry.Expected.size() != entry.Value.size()) FailLine(replacement.Line, "expected and value must have the same width", "value");
        return entry;
    }
    if (!type->IsString) FailLine(type->Line, "type must be a string", type->Text);
    const auto found = std::find_if(kIntegerTypes.begin(), kIntegerTypes.end(), [&](const IntegerType& candidate) { return candidate.Name == type->Text; });
    if (found == kIntegerTypes.end()) FailLine(type->Line, "unknown type", type->Text);
    const auto integer = [&](const RawValue& value, const char* key) -> const Integer& {
        if (value.IsString) FailLine(value.Line, std::string(key) + " must be an integer for a typed entry", value.Text);
        return value.Number;
    };
    const auto& expectedNumber = integer(expected, "expected");
    const auto& valueNumber = integer(replacement, "value");
    if (!FitsType(expectedNumber, *found)) FailLine(expected.Line, "expected does not fit " + std::string(found->Name), "expected");
    if (!FitsType(valueNumber, *found)) FailLine(replacement.Line, "value does not fit " + std::string(found->Name), "value");
    if (const auto* minimum = Find(table, "min"); minimum && IntegerLess(valueNumber, integer(*minimum, "min"))) FailLine(minimum->Line, "value is below min", "min");
    if (const auto* maximum = Find(table, "max"); maximum && IntegerLess(integer(*maximum, "max"), valueNumber)) FailLine(maximum->Line, "value is above max", "max");
    if (const auto* align = Find(table, "align")) {
        const auto& alignment = integer(*align, "align");
        if (alignment.Negative || alignment.Magnitude == 0) FailLine(align->Line, "align must be positive", "align");
        if (valueNumber.Magnitude % alignment.Magnitude != 0) FailLine(align->Line, "value is not a multiple of align", "align");
    }
    entry.Type = type->Text;
    entry.ExpectedNumber = expectedNumber;
    entry.ValueNumber = valueNumber;
    if (const auto* minimum = Find(table, "min")) entry.Min = minimum->Number;
    if (const auto* maximum = Find(table, "max")) entry.Max = maximum->Number;
    if (const auto* align = Find(table, "align")) entry.Align = align->Number;
    entry.Expected = EncodeInteger(expectedNumber, *found);
    entry.Value = EncodeInteger(valueNumber, *found);
    return entry;
}

std::string Describe(const PatchEntry& entry) {
    return "patch " + entry.Target + " va=" + HexValue(entry.VirtualAddress) + " (line " + std::to_string(entry.Line) + ")";
}

void ValidateWidth(const PatchEntry& entry) {
    if (entry.Expected.empty() || entry.Expected.size() != entry.Value.size())
        throw Domain::RelinkerException(Describe(entry) + ": expected and value must have the same nonzero width");
}

struct EntryRange {
    std::optional<PatchedRange> Range;
    std::string Reason;
};

bool TouchesHeaderArea(const ElfHeader& header, const std::uint64_t offset, const std::uint64_t length) {
    const auto overlaps = [offset, length](const std::uint64_t start, const std::uint64_t size) {
        return offset < start + size && start < offset + length;
    };
    if (overlaps(0, Elfpatcher::kElfHeaderSize)) return true;
    if (header.ProgramHeaderCount == 0 || header.ProgramHeaderEntrySize == 0) return false;
    const std::uint64_t tableSize = static_cast<std::uint64_t>(header.ProgramHeaderCount) * header.ProgramHeaderEntrySize;
    if (tableSize > std::numeric_limits<std::uint64_t>::max() - header.ProgramHeaderOffset) return true;
    return overlaps(header.ProgramHeaderOffset, tableSize);
}

EntryRange ResolveEntry(const std::vector<ProgramHeader>& segments, const std::uint64_t fileSize, const ElfHeader& header, const PatchEntry& entry) {
    const std::uint64_t length = entry.Expected.size();
    for (const auto& segment : segments) {
        if (segment.Type != Elfpatcher::PT_LOAD) continue;
        if (entry.VirtualAddress < segment.MappedAddress) continue;
        const std::uint64_t delta = entry.VirtualAddress - segment.MappedAddress;
        if (delta >= segment.FileSize || length > segment.FileSize - delta) continue;
        if (segment.Offset > fileSize || segment.FileSize > fileSize - segment.Offset) return {std::nullopt, "PT_LOAD file mapping is out of bounds"};
        const PatchedRange range{segment.Offset + delta, length, entry.VirtualAddress};
        if (TouchesHeaderArea(header, range.Offset, length)) return {std::nullopt, "range overlaps the ELF header or program header table"};
        return {range, {}};
    }
    return {std::nullopt, "range is not inside the file bytes of one PT_LOAD"};
}

std::vector<EntryRange> ResolveEach(const ElfReader& reader, const std::vector<PatchEntry>& entries) {
    const auto segments = reader.ReadProgramHeaders();
    const auto header = reader.ReadHeader();
    std::vector<EntryRange> resolved;
    for (const auto& entry : entries) {
        ValidateWidth(entry);
        resolved.push_back(ResolveEntry(segments, reader.GetFileSize(), header, entry));
    }
    return resolved;
}

bool Overlaps(const std::uint64_t firstOffset, const std::uint64_t firstLength, const std::uint64_t secondOffset, const std::uint64_t secondLength) {
    return firstOffset < secondOffset + secondLength && secondOffset < firstOffset + firstLength;
}

void RejectEntryOverlaps(const std::vector<PatchEntry>& entries, const std::vector<EntryRange>& resolved) {
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (!resolved[index].Range) continue;
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (!resolved[previous].Range) continue;
            const auto& first = *resolved[previous].Range;
            const auto& second = *resolved[index].Range;
            if (Overlaps(first.Offset, first.Length, second.Offset, second.Length))
                throw Domain::RelinkerException(Describe(entries[index]) + " overlaps " + Describe(entries[previous]));
        }
    }
}

bool IsElfOfType(const std::vector<std::uint8_t>& bytes, const std::uint16_t type) {
    return bytes.size() >= 18 && bytes[0] == 0x7f && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F' && bytes[16] == (type & 0xff) && bytes[17] == (type >> 8);
}

VerifyResult VerifyOne(const PatchEntry& entry, const EntryRange& resolved, const std::vector<std::uint8_t>& bytes, const bool outputMode) {
    VerifyResult result;
    result.Name = entry.Name;
    result.Target = entry.Target;
    result.VirtualAddress = entry.VirtualAddress;
    if (!resolved.Range) {
        result.Detail = resolved.Reason;
        return result;
    }
    result.Offset = resolved.Range->Offset;
    const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(resolved.Range->Offset);
    const std::vector<std::uint8_t> found(first, first + static_cast<std::ptrdiff_t>(resolved.Range->Length));
    const auto& wanted = outputMode ? entry.Value : entry.Expected;
    const auto& other = outputMode ? entry.Expected : entry.Value;
    if (found == wanted) {
        result.Status = outputMode ? VerifyStatus::Applied : VerifyStatus::Applicable;
        result.Detail = Hex(found);
    } else if (found == other) {
        result.Detail = outputMode ? "patch is not applied: found " + Hex(found) : "input already holds the patched value " + Hex(found);
    } else {
        result.Detail = "expected " + Hex(wanted) + " found " + Hex(found);
    }
    return result;
}

const VerifyTarget* FindTarget(const std::vector<VerifyTarget>& targets, const std::string& name) {
    const auto found = std::find_if(targets.begin(), targets.end(), [&](const VerifyTarget& target) { return target.Name == name; });
    return found == targets.end() ? nullptr : &*found;
}

}

std::string Hex(const std::span<const std::uint8_t> bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (const auto byte : bytes) {
        text += digits[byte >> 4];
        text += digits[byte & 15];
    }
    return text;
}

std::string FormatApplied(const AppliedPatch& patch) {
    std::string text = "Patch: " + patch.Target + (patch.Name.empty() ? std::string() : " [" + patch.Name + "]") + " va=" + HexValue(patch.VirtualAddress) + " off=" + HexValue(patch.Offset) + " " + Hex(patch.Expected) + " -> " + Hex(patch.Value);
    if (!patch.Description.empty()) text += " (" + patch.Description + ")";
    return text;
}

std::string ModuleTargetName(const std::filesystem::path& modulePath, const std::filesystem::path& moduleRoot) {
    auto name = Io::Utf8Path(modulePath.lexically_normal().lexically_relative(moduleRoot.lexically_normal()));
    std::replace(name.begin(), name.end(), '\\', '/');
    return name;
}

PatchFile ParsePatchFile(const std::string_view text) {
    PatchFile file;
    Table top;
    Table current;
    std::size_t entryLine = 0;
    std::set<std::string> names;
    bool inEntry = false;
    const auto finishEntry = [&] {
        if (inEntry) file.Entries.push_back(BuildEntry(current, entryLine, names));
        current.clear();
    };
    std::size_t lineNumber = 0;
    std::size_t position = 0;
    while (position <= text.size()) {
        auto end = text.find('\n', position);
        if (end == std::string_view::npos) end = text.size();
        auto whole = text.substr(position, end - position);
        if (end < text.size() && whole.ends_with('\r')) whole.remove_suffix(1);
        position = end + 1;
        ++lineNumber;
        const auto line = Trim(StripComment(whole, lineNumber));
        if (line.empty()) continue;
        if (line.front() == '[') {
            if (line != "[[patch]]") FailLine(lineNumber, "unsupported table header", Trim(whole));
            finishEntry();
            inEntry = true;
            entryLine = lineNumber;
            continue;
        }
        std::size_t keyEnd = 0;
        while (keyEnd < line.size() && IsKeyCharacter(line[keyEnd])) ++keyEnd;
        const std::string key(line.substr(0, keyEnd));
        const auto rest = Trim(line.substr(keyEnd));
        if (key.empty() || rest.empty() || rest.front() != '=') {
            if (!rest.empty() && rest.front() == '.') FailLine(lineNumber, "dotted keys are not supported", line);
            if (key.empty() && (line.front() == '"' || line.front() == '\'')) FailLine(lineNumber, "quoted keys are not supported", line);
            FailLine(lineNumber, "expected `key = value`", line);
        }
        auto value = ParseValue(Trim(rest.substr(1)), lineNumber, line);
        auto& table = inEntry ? current : top;
        if (inEntry ? !Contains(kEntryKeys, key) : !(key == "format" || Contains(kIgnoredTopLevelKeys, key)))
            FailLine(lineNumber, inEntry ? "unknown key" : "unknown top-level key", line);
        if (table.contains(key)) FailLine(lineNumber, "duplicate key", line);
        table.emplace(key, std::move(value));
    }
    finishEntry();
    const auto* format = Find(top, "format");
    if (!format) throw Domain::RelinkerException("patch file: `format = 1` is required");
    if (format->IsString || format->Number.Negative || format->Number.Magnitude != 1) FailLine(format->Line, "unsupported format", "format");
    if (const auto* mainTarget = Find(top, "main_target")) {
        if (!mainTarget->IsString || mainTarget->Text.empty()) FailLine(mainTarget->Line, "main_target must be a nonempty string", "main_target");
        file.MainTarget = mainTarget->Text;
    }
    if (const auto* linuxBase = Find(top, "linux_base")) {
        if (linuxBase->IsString || linuxBase->Number.Negative) FailLine(linuxBase->Line, "linux_base must be a non-negative integer", "linux_base");
        file.LinuxBase = linuxBase->Number.Magnitude;
    }
    return file;
}

std::vector<PatchedRange> ResolveRanges(const ElfReader& reader, const std::vector<PatchEntry>& entries) {
    const auto resolved = ResolveEach(reader, entries);
    std::vector<PatchedRange> ranges;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (!resolved[index].Range) throw Domain::RelinkerException(Describe(entries[index]) + ": " + resolved[index].Reason);
        ranges.push_back(*resolved[index].Range);
    }
    RejectEntryOverlaps(entries, resolved);
    return ranges;
}

std::vector<AppliedPatch> ApplyPatches(std::vector<std::uint8_t>& bytes, const std::string& target, const std::vector<PatchEntry>& entries, const std::vector<PatchedRange>& ranges) {
    if (entries.size() != ranges.size()) throw Domain::RelinkerException("Patch ranges do not match patch entries");
    for (std::size_t index = 0; index < entries.size(); ++index) {
        ValidateWidth(entries[index]);
        const auto& range = ranges[index];
        if (range.Length != entries[index].Expected.size())
            throw Domain::RelinkerException(Describe(entries[index]) + ": range length does not match the patch width", range.Offset);
        if (range.Offset > bytes.size() || range.Length > bytes.size() - range.Offset) throw Domain::RelinkerException(Describe(entries[index]) + ": range exceeds the image", range.Offset);
        const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(range.Offset);
        if (!std::equal(entries[index].Expected.begin(), entries[index].Expected.end(), first))
            throw Domain::RelinkerException(Describe(entries[index]) + " off=" + HexValue(range.Offset) + ": expected " + Hex(entries[index].Expected) + " found " + Hex(std::span<const std::uint8_t>(&*first, range.Length)), range.Offset);
    }
    std::vector<AppliedPatch> applied;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        std::copy(entries[index].Value.begin(), entries[index].Value.end(), bytes.begin() + static_cast<std::ptrdiff_t>(ranges[index].Offset));
        applied.push_back({target, entries[index].VirtualAddress, ranges[index].Offset, entries[index].Expected, entries[index].Value, entries[index].Description, entries[index].Name, entries[index].Type, entries[index].ExpectedNumber, entries[index].ValueNumber, entries[index].Min, entries[index].Max, entries[index].Align});
    }
    return applied;
}

PatchedTarget PatchTarget(std::vector<std::uint8_t>& bytes, const std::string& target, const std::vector<PatchEntry>& entries) {
    std::vector<PatchEntry> own;
    for (const auto& entry : entries) if (entry.Target == target) own.push_back(entry);
    if (own.empty()) return {};
    ElfReader reader(std::move(bytes));
    PatchedTarget patched;
    try {
        patched.Ranges = ResolveRanges(reader, own);
    } catch (...) {
        bytes = reader.TakeBytes();
        throw;
    }
    bytes = reader.TakeBytes();
    patched.Applied = ApplyPatches(bytes, target, own, patched.Ranges);
    return patched;
}

void CheckOverlaps(const std::string& target, const std::vector<PatchedRange>& patched, const WriterSites& writers) {
    if (patched.empty()) return;
    std::vector<WriterRange> sites;
    for (const auto& report : writers.Reports)
        if (report.Lowering != Codegen::Amd64OnlyLowering::Kept) sites.push_back({report.Offset, report.OriginalLength, "Intel in-place report"});
    for (const auto& site : writers.Trampolines) sites.push_back({site.Offset, site.Length, "Intel trampoline site"});
    for (const auto& patch : writers.RelinkPatches) sites.push_back({patch.Offset, patch.Bytes.size(), "relinker patch"});
    sites.insert(sites.end(), writers.Traps.begin(), writers.Traps.end());
    for (const auto& site : writers.RedZoneSites) sites.push_back({site.Offset, site.Length, "red-zone frame site"});
    for (const auto& moved : writers.RedZoneDisplacements) sites.push_back({moved.Offset, moved.Bytes.size(), "red-zone moved displacement"});
    const auto relaViolation = [&](const std::span<const std::uint8_t> table, const char* name, const PatchedRange& range) {
        for (std::size_t position = 0; position + 24 <= table.size(); position += 24) {
            std::uint64_t relocation = 0;
            for (unsigned byte = 0; byte < 8; ++byte) relocation |= static_cast<std::uint64_t>(table[position + byte]) << (8 * byte);
            if (Overlaps(range.VirtualAddress, range.Length, relocation, 8))
                throw Domain::RelinkerException("patch " + target + " va=" + HexValue(range.VirtualAddress) + " off=" + HexValue(range.Offset) + " overlaps " + name + " relocation target at va=" + HexValue(relocation), range.Offset);
        }
    };
    for (const auto& range : patched) {
        relaViolation(writers.RelaData, "RelaData", range);
        relaViolation(writers.RelaPltData, "RelaPltData", range);
    }
    for (const auto& range : patched)
        for (const auto& site : sites)
            if (Overlaps(range.Offset, range.Length, site.Offset, site.Length))
                throw Domain::RelinkerException("patch " + target + " va=" + HexValue(range.VirtualAddress) + " off=" + HexValue(range.Offset) + " overlaps " + site.Writer + " at off=" + HexValue(site.Offset) + " length " + std::to_string(site.Length), range.Offset);
}

void MatchTargets(const std::vector<PatchEntry>& entries, const std::vector<std::string>& targetNames) {
    for (const auto& entry : entries) {
        const auto count = std::count(targetNames.begin(), targetNames.end(), entry.Target);
        if (count == 0) throw Domain::RelinkerException("unmatched patch target: " + entry.Target + " (patch file line " + std::to_string(entry.Line) + ")");
        if (count > 1) throw Domain::RelinkerException("ambiguous patch target: " + entry.Target + " (patch file line " + std::to_string(entry.Line) + ")");
    }
}

std::vector<AppliedPatch> AggregateManifest(const std::vector<std::vector<AppliedPatch>>& records) {
    std::vector<AppliedPatch> merged;
    for (const auto& group : records) {
        for (const auto& patch : group) {
            const bool known = std::any_of(merged.begin(), merged.end(), [&](const AppliedPatch& existing) {
                return existing.Target == patch.Target && existing.VirtualAddress == patch.VirtualAddress && existing.Offset == patch.Offset;
            });
            if (!known) merged.push_back(patch);
        }
    }
    return merged;
}

std::string WriteManifest(const std::vector<AppliedPatch>& patches, const std::string& mainTarget, const std::string& configPath, const std::optional<std::uint64_t> linuxBase) {
    const auto holdable = [](const std::string& text) {
        return std::none_of(text.begin(), text.end(), [](const char character) {
            return character == '"' || character == '\\' || IsControlCharacter(character);
        });
    };
    const auto quoted = [&](const std::string& text, const char* field) {
        if (!holdable(text)) throw Domain::RelinkerException(std::string("Patch manifest cannot hold the ") + field + ": " + text);
        return "\"" + text + "\"";
    };
    std::string config = std::filesystem::path(Io::NativePath(configPath)).generic_string();
    if (!config.empty() && !holdable(config)) config = std::filesystem::path(Io::NativePath(configPath)).filename().generic_string();
    std::ostringstream manifest;
    manifest << "format = 1\nmain_target = " << quoted(mainTarget, "main_target") << '\n';
    if (linuxBase) manifest << "linux_base = " << HexValue(*linuxBase) << '\n';
    if (!config.empty()) manifest << "config = " << quoted(config, "config") << '\n';
    for (const auto& patch : patches) {
        manifest << "\n[[patch]]\n";
        if (!patch.Name.empty()) manifest << "name = " << quoted(patch.Name, "name") << '\n';
        manifest << "target = " << quoted(patch.Target, "target") << "\nva = " << HexValue(patch.VirtualAddress) << '\n';
        if (patch.Type.empty()) {
            manifest << "expected = \"" << Hex(patch.Expected) << "\"\nvalue = \"" << Hex(patch.Value) << "\"\n";
        } else {
            manifest << "type = " << quoted(patch.Type, "type") << "\nexpected = " << IntegerText(patch.ExpectedNumber) << "\nvalue = " << IntegerText(patch.ValueNumber) << '\n';
            if (patch.Min) manifest << "min = " << IntegerText(*patch.Min) << '\n';
            if (patch.Max) manifest << "max = " << IntegerText(*patch.Max) << '\n';
            if (patch.Align) manifest << "align = " << IntegerText(*patch.Align) << '\n';
        }
        if (!patch.Description.empty()) manifest << "description = " << quoted(patch.Description, "description") << '\n';
    }
    return manifest.str();
}

std::string_view StatusName(const VerifyStatus status) {
    switch (status) {
    case VerifyStatus::Applicable: return "applicable";
    case VerifyStatus::Applied: return "applied";
    case VerifyStatus::NotChecked: return "not-checked";
    case VerifyStatus::Mismatch: break;
    }
    return "mismatch";
}

std::vector<VerifyResult> VerifyPatches(const PatchFile& file, const std::vector<VerifyTarget>& executables, const std::vector<VerifyTarget>& modules, const bool outputMode) {
    if (outputMode) {
        if (!file.MainTarget) throw Domain::RelinkerException("The patch file has no main_target, so its entries cannot be matched to an output: verify the input executable, or verify the manifest written by the conversion");
        for (std::size_t index = 0; index < executables.size(); ++index)
            for (std::size_t other = index + 1; other < executables.size(); ++other)
                if (executables[index].Name == executables[other].Name) throw Domain::RelinkerException("ambiguous patch target: " + executables[index].Name);
        for (const auto& executable : executables) {
            const bool fixed = IsElfOfType(executable.Bytes, Elfpatcher::ET_EXEC);
            if (!fixed && !IsElfOfType(executable.Bytes, Elfpatcher::ET_DYN)) throw Domain::RelinkerException(executable.Name + " is not a Linux ELF output (PE and Mach-O outputs are not supported): verify the input instead");
            if (fixed && !file.LinuxBase) throw Domain::RelinkerException(executable.Name + " is an ET_EXEC Linux ELF output, but the patch file has no linux_base: verify the manifest written by its conversion");
            if (!fixed && file.LinuxBase) throw Domain::RelinkerException(executable.Name + " is an ET_DYN Linux ELF output, but the patch file has linux_base = " + HexValue(*file.LinuxBase) + ": verify the output of the conversion that wrote it");
        }
    } else {
        std::vector<std::string> names;
        for (const auto& executable : executables) names.push_back(executable.Name);
        for (const auto& module : modules) names.push_back(module.Name);
        try {
            MatchTargets(file.Entries, names);
        } catch (const Domain::RelinkerException& error) {
            if (std::string_view(error.what()).starts_with("unmatched")) throw Domain::RelinkerException(std::string(error.what()) + "; to check a converted output, use --verify-output");
            throw;
        }
    }
    std::vector<VerifyResult> results(file.Entries.size());
    std::vector<bool> done(file.Entries.size());
    for (std::size_t index = 0; index < file.Entries.size(); ++index) {
        if (done[index]) continue;
        const auto& name = file.Entries[index].Target;
        const auto* target = FindTarget(executables, name);
        if (!target && !outputMode) target = FindTarget(modules, name);
        std::vector<std::size_t> indices;
        for (std::size_t other = index; other < file.Entries.size(); ++other) if (file.Entries[other].Target == name) indices.push_back(other);
        if (!target) {
            for (const auto other : indices) {
                const auto& entry = file.Entries[other];
                results[other] = {entry.Name, entry.Target, entry.VirtualAddress, 0, VerifyStatus::NotChecked, "module, or executable not named with --executable"};
                done[other] = true;
            }
            continue;
        }
        const std::uint64_t base = outputMode && IsElfOfType(target->Bytes, Elfpatcher::ET_EXEC) ? *file.LinuxBase : 0;
        std::vector<PatchEntry> own;
        for (const auto other : indices) {
            own.push_back(file.Entries[other]);
            if (own.back().VirtualAddress > std::numeric_limits<std::uint64_t>::max() - base) throw Domain::RelinkerException(Describe(own.back()) + " lies outside the address space at linux_base = " + HexValue(base));
            own.back().VirtualAddress += base;
        }
        const ElfReader reader(target->Bytes);
        const auto resolved = ResolveEach(reader, own);
        RejectEntryOverlaps(own, resolved);
        for (std::size_t position = 0; position < indices.size(); ++position) {
            results[indices[position]] = VerifyOne(own[position], resolved[position], target->Bytes, outputMode);
            results[indices[position]].VirtualAddress = file.Entries[indices[position]].VirtualAddress;
            done[indices[position]] = true;
        }
    }
    return results;
}

std::filesystem::path OutputExecutablePath(const std::filesystem::path& outputPath, const std::string& executableArgument) {
    const auto relative = Io::NativePath(executableArgument).lexically_normal();
    if (relative.empty() || relative.has_root_path() || *relative.begin() == ".." || !relative.has_filename())
        throw Domain::RelinkerException("--executable must be a file inside the guest module parent directory: " + executableArgument);
    std::string name;
    for (const auto& part : relative) name += (name.empty() ? "" : ".") + Io::Utf8Path(part);
    return outputPath.parent_path() / Io::NativePath(name + Io::Utf8Path(outputPath.extension()));
}

}
