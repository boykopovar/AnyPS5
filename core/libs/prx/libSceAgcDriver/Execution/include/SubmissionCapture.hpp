#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SUBMISSIONCAPTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SUBMISSIONCAPTURE_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace AgcDriver {

enum class SubmissionKind : std::uint8_t { Submit, Rewind, Suspend };

struct CapturedSubmission {
    std::uint64_t ordinal = 0;
    std::uint32_t queue = 0;
    std::uint64_t serial = 0;
    std::uint64_t timeUs = 0;
    SubmissionKind kind = SubmissionKind::Submit;
    std::size_t dwords = 0;
    std::vector<std::uint32_t> commands;
    std::vector<std::size_t> heldAtSubmit;
    std::vector<std::pair<std::size_t, std::size_t>> conditionalRanges;
    bool heldKnown = true;
    bool conditionalKnown = true;
};

class SubmissionCapture {
public:
    static constexpr std::string_view IndexName = "index.txt";
    static constexpr std::string_view Magic = "aps5-submissions 1";
    static constexpr std::string_view Columns = "ordinal queue type serial time_us dwords kind file held conditional";
    static constexpr std::string_view ColumnsWithoutHeld = "ordinal queue type serial time_us dwords kind file";

    explicit SubmissionCapture(const std::filesystem::path& directory);
    SubmissionCapture(const SubmissionCapture&) = delete;
    SubmissionCapture& operator=(const SubmissionCapture&) = delete;

    static SubmissionCapture* Active();
    static CapturedSubmission Prepare(std::uint32_t queue, SubmissionKind kind, std::span<const std::uint32_t> commands, const std::map<std::size_t, std::size_t>& conditionalEnds);
    void Take(CapturedSubmission& submission, std::uint64_t serial, std::uint64_t timeUs, const std::set<std::size_t>& heldAtSubmit) noexcept;
    void Write(const CapturedSubmission& submission);

    static std::string FileName(const CapturedSubmission& submission);
    static std::string_view KindName(SubmissionKind kind);
    static std::string_view QueueType(std::uint32_t queue);
    static std::vector<CapturedSubmission> ReadIndex(const std::filesystem::path& directory);
    static std::vector<std::uint32_t> ReadCommands(const std::filesystem::path& directory, const CapturedSubmission& submission);
    static std::vector<CapturedSubmission> Read(const std::filesystem::path& directory);

private:
    std::filesystem::path directory;
    std::mutex mutex;
    std::ofstream index;
    std::atomic<std::uint64_t> next{0};
};

}

#endif
