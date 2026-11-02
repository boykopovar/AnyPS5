#include "prx/libSceAgcDriver/Execution/include/SubmissionCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionRing.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace AgcDriver {
namespace {

static_assert(std::endian::native == std::endian::little, "captured dwords are stored in the guest's little-endian order");

void requireCapture(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error("submission capture: " + reason);
}

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> result;
    while (true) {
        const auto end = text.find(separator);
        result.push_back(text.substr(0, end));
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
    }
    return result;
}

template <typename TValue>
bool parse(std::string_view text, TValue& value, int base = 10) {
    if (text.empty()) return false;
    const auto [last, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    return error == std::errc{} && last == text.data() + text.size();
}

SubmissionKind kindFromName(std::string_view name, const std::string& where) {
    for (const auto kind : {SubmissionKind::Submit, SubmissionKind::Rewind, SubmissionKind::Suspend}) {
        if (SubmissionCapture::KindName(kind) == name) return kind;
    }
    throw std::runtime_error("submission capture: " + where + ": unknown kind '" + std::string(name) + "'");
}

bool waitPacket(std::uint32_t header) {
    const auto opcode = (header >> 8u) & 0xffu;
    return (header >> 30u) == 3u && (opcode == 0x3c || opcode == 0x93);
}

std::string heldField(const CapturedSubmission& submission) {
    if (submission.heldAtSubmit.empty()) return "-";
    std::string text;
    for (const auto offset : submission.heldAtSubmit) text += (text.empty() ? "" : ",") + std::to_string(offset);
    return text;
}

std::string conditionalField(const CapturedSubmission& submission) {
    if (submission.conditionalRanges.empty()) return "-";
    std::string text;
    for (const auto& [start, end] : submission.conditionalRanges) text += (text.empty() ? "" : ",") + std::to_string(start) + ":" + std::to_string(end);
    return text;
}

std::vector<std::size_t> readHeld(std::string_view field, std::size_t dwords, const std::string& where) {
    std::vector<std::size_t> result;
    if (field == "-") return result;
    for (const auto item : split(field, ',')) {
        std::size_t offset = 0;
        requireCapture(parse(item, offset) && offset < dwords && (result.empty() || offset > result.back()), where + ": malformed held offsets '" + std::string(field) + "'");
        result.push_back(offset);
    }
    return result;
}

std::vector<std::pair<std::size_t, std::size_t>> readConditional(std::string_view field, std::size_t dwords, const std::string& where) {
    std::vector<std::pair<std::size_t, std::size_t>> result;
    if (field == "-") return result;
    for (const auto item : split(field, ',')) {
        const auto bounds = split(item, ':');
        std::size_t start = 0, end = 0;
        requireCapture(bounds.size() == 2 && parse(bounds[0], start) && parse(bounds[1], end) && start < end && end <= dwords && (result.empty() || start > result.back().first),
                       where + ": malformed conditional ranges '" + std::string(field) + "'");
        result.emplace_back(start, end);
    }
    return result;
}

}

SubmissionCapture::SubmissionCapture(const std::filesystem::path& directory) : directory(directory) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    requireCapture(!error && std::filesystem::is_directory(directory), "cannot create the directory " + directory.string());
    index.open(directory / IndexName, std::ios::binary | std::ios::trunc);
    requireCapture(index.is_open(), "cannot create " + (directory / IndexName).string());
    index << Magic << '\n' << Columns << '\n';
    index.flush();
    requireCapture(index.good(), "cannot write " + (directory / IndexName).string());
}

SubmissionCapture* SubmissionCapture::Active() {
    static SubmissionCapture* const capture = [] {
#ifdef _WIN32
        const wchar_t* directory = _wgetenv(L"APS5_DUMP_SUBMITS");
#else
        const char* directory = std::getenv("APS5_DUMP_SUBMITS");
#endif
        return directory != nullptr && *directory != 0 ? new SubmissionCapture(directory) : nullptr;
    }();
    return capture;
}

CapturedSubmission SubmissionCapture::Prepare(std::uint32_t queue, SubmissionKind kind, std::span<const std::uint32_t> commands, const std::map<std::size_t, std::size_t>& conditionalEnds) {
    QueueType(queue);
    requireCapture(kind != SubmissionKind::Suspend || commands.empty(), "a suspend point has no commands");
    CapturedSubmission submission;
    submission.queue = queue;
    submission.kind = kind;
    submission.dwords = commands.size();
    submission.commands.assign(commands.begin(), commands.end());
    for (std::size_t cursor = 0; cursor < commands.size(); cursor += Pm4::PacketWords(commands[cursor])) {
        if (waitPacket(commands[cursor])) submission.heldAtSubmit.push_back(cursor);
    }
    for (const auto& [start, end] : conditionalEnds) {
        requireCapture(start < end && end <= commands.size(), "a conditional range lies outside the commands");
        submission.conditionalRanges.emplace_back(start, end);
    }
    return submission;
}

void SubmissionCapture::Take(CapturedSubmission& submission, std::uint64_t serial, std::uint64_t timeUs, const std::set<std::size_t>& heldAtSubmit) noexcept {
    submission.ordinal = next.fetch_add(1, std::memory_order_relaxed);
    submission.serial = serial;
    submission.timeUs = timeUs;
    std::erase_if(submission.heldAtSubmit, [&](std::size_t offset) { return !heldAtSubmit.contains(offset); });
}

void SubmissionCapture::Write(const CapturedSubmission& submission) {
    requireCapture(submission.dwords == submission.commands.size(), "dword count does not match the commands");
    const auto name = FileName(submission);
    if (!submission.commands.empty()) {
        std::ofstream file(directory / name, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(submission.commands.data()), static_cast<std::streamsize>(submission.commands.size() * sizeof(std::uint32_t)));
        file.close();
        requireCapture(!file.fail(), "cannot write " + (directory / name).string());
    }
    char head[160];
    const int length = std::snprintf(head, sizeof(head), "%llu 0x%x %s %llu %llu %zu %s ", static_cast<unsigned long long>(submission.ordinal), submission.queue, QueueType(submission.queue).data(),
                                     static_cast<unsigned long long>(submission.serial), static_cast<unsigned long long>(submission.timeUs), submission.dwords, KindName(submission.kind).data());
    requireCapture(length > 0 && static_cast<std::size_t>(length) < sizeof(head), "index line does not fit");
    const auto line = std::string(head, static_cast<std::size_t>(length)) + name + " " + heldField(submission) + " " + conditionalField(submission) + "\n";
    std::lock_guard lock(mutex);
    index.write(line.data(), static_cast<std::streamsize>(line.size()));
    index.flush();
    requireCapture(index.good(), "cannot append to " + (directory / IndexName).string());
}

std::string SubmissionCapture::FileName(const CapturedSubmission& submission) {
    if (submission.dwords == 0) return "-";
    char name[48];
    std::snprintf(name, sizeof(name), "submission_%06llu.bin", static_cast<unsigned long long>(submission.ordinal));
    return name;
}

std::string_view SubmissionCapture::KindName(SubmissionKind kind) {
    switch (kind) {
        case SubmissionKind::Submit: return "submit";
        case SubmissionKind::Rewind: return "rewind";
        case SubmissionKind::Suspend: return "suspend";
    }
    throw std::runtime_error("submission capture: unknown submission kind");
}

std::string_view SubmissionCapture::QueueType(std::uint32_t queue) {
    if (queue == 0) return "graphics";
    if (queue >= 0x20 && queue < DriverDetail::SubmissionQueueEnd) return "compute";
    char what[64];
    std::snprintf(what, sizeof(what), "queue 0x%x is neither the graphics queue nor a compute queue", queue);
    throw std::runtime_error(std::string("submission capture: ") + what);
}

std::vector<CapturedSubmission> SubmissionCapture::ReadIndex(const std::filesystem::path& directory) {
    const auto path = directory / IndexName;
    std::ifstream input(path, std::ios::binary);
    requireCapture(input.is_open(), "cannot open " + path.string());
    std::string line;
    requireCapture(std::getline(input, line) && line == Magic, path.string() + " does not start with '" + std::string(Magic) + "'");
    requireCapture(std::getline(input, line) && (line == Columns || line == ColumnsWithoutHeld), path.string() + " has no column line '" + std::string(Columns) + "' or '" + std::string(ColumnsWithoutHeld) + "'");
    const bool complete = line == Columns;
    const std::size_t columns = complete ? 10 : 8;
    std::vector<CapturedSubmission> result;
    for (std::size_t number = 3; std::getline(input, line); ++number) {
        const auto where = path.string() + " line " + std::to_string(number);
        requireCapture(!input.eof(), where + " is incomplete");
        const auto values = split(line, ' ');
        requireCapture(values.size() == columns, where + " does not have " + std::to_string(columns) + " fields");
        CapturedSubmission submission;
        requireCapture(parse(values[0], submission.ordinal) && values[1].starts_with("0x") && parse(values[1].substr(2), submission.queue, 16) && parse(values[3], submission.serial)
                       && parse(values[4], submission.timeUs) && parse(values[5], submission.dwords), where + " has a malformed number");
        requireCapture(QueueType(submission.queue) == values[2], where + ": queue type does not match the queue");
        submission.kind = kindFromName(values[6], where);
        requireCapture(submission.kind != SubmissionKind::Suspend || submission.dwords == 0, where + ": a suspend point has commands");
        requireCapture(submission.dwords <= std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t), where + ": dword count overflows");
        requireCapture(values[7] == FileName(submission), where + ": file name does not match the ordinal and size");
        submission.heldKnown = complete;
        submission.conditionalKnown = complete;
        if (complete) {
            submission.heldAtSubmit = readHeld(values[8], submission.dwords, where);
            submission.conditionalRanges = readConditional(values[9], submission.dwords, where);
        }
        result.push_back(std::move(submission));
    }
    requireCapture(input.eof(), "cannot read " + path.string());
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) { return left.ordinal < right.ordinal; });
    const auto repeated = std::adjacent_find(result.begin(), result.end(), [](const auto& left, const auto& right) { return left.ordinal == right.ordinal; });
    requireCapture(repeated == result.end(), path.string() + " lists ordinal " + (repeated == result.end() ? std::string() : std::to_string(repeated->ordinal)) + " twice");
    return result;
}

std::vector<std::uint32_t> SubmissionCapture::ReadCommands(const std::filesystem::path& directory, const CapturedSubmission& submission) {
    if (submission.dwords == 0) return {};
    const auto path = directory / FileName(submission);
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    requireCapture(!error, "cannot read the size of " + path.string());
    requireCapture(size == submission.dwords * sizeof(std::uint32_t), path.string() + " does not hold " + std::to_string(submission.dwords) + " dwords");
    std::vector<std::uint32_t> commands(submission.dwords);
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char*>(commands.data()), static_cast<std::streamsize>(size));
    requireCapture(input.good() && input.peek() == std::char_traits<char>::eof(), "cannot read " + path.string());
    return commands;
}

std::vector<CapturedSubmission> SubmissionCapture::Read(const std::filesystem::path& directory) {
    auto submissions = ReadIndex(directory);
    for (auto& submission : submissions) submission.commands = ReadCommands(directory, submission);
    return submissions;
}

}
