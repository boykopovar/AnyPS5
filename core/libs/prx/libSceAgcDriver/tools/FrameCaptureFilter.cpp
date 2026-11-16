#include "FrameCaptureFilter.hpp"
#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

void requireFilter(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("frame capture filter: " + message);
}

std::uint64_t parseIndex(std::string_view value, const std::filesystem::path& path) {
    std::uint64_t index = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), index);
    requireFilter(!value.empty() && error == std::errc{} && end == value.data() + value.size(), path.string() + ": invalid index");
    return index;
}

std::filesystem::path normalized(const std::filesystem::path& path) {
    std::error_code error;
    auto result = std::filesystem::weakly_canonical(path, error);
    requireFilter(!error, "cannot resolve path " + path.string() + ": " + error.message());
    return result;
}

bool isWithin(const std::filesystem::path& child, const std::filesystem::path& parent) {
    auto relative = child.lexically_relative(parent);
    return !relative.empty() && *relative.begin() != ".." && !relative.is_absolute();
}

std::string memoryKind(AgcDriver::FrameMemoryKind kind) {
    switch (kind) {
    case AgcDriver::FrameMemoryKind::Range: return "range";
    case AgcDriver::FrameMemoryKind::Snapshot: return "snapshot";
    case AgcDriver::FrameMemoryKind::Delta: return "delta";
    case AgcDriver::FrameMemoryKind::Reference: return "reference";
    }
    throw std::runtime_error("frame capture filter: unknown memory kind");
}

}

namespace AgcDriverTools {

FrameCaptureFilterPlan ReadFrameCaptureFilterPlan(const std::filesystem::path& path) {
    std::ifstream input(path);
    requireFilter(input.is_open(), "cannot open plan " + path.string());
    std::string line;
    requireFilter(static_cast<bool>(std::getline(input, line)) && line == "aps5-frame-filter 1", path.string() + " is not a supported plan");
    FrameCaptureFilterPlan plan;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        const auto separator = line.find(' ');
        requireFilter(separator != std::string::npos && separator + 1 < line.size(), path.string() + ": invalid plan entry");
        const auto kind = std::string_view(line).substr(0, separator);
        const auto index = parseIndex(std::string_view(line).substr(separator + 1), path);
        if (kind == "call") requireFilter(plan.calls.insert(index).second, path.string() + ": duplicate call index");
        else if (kind == "chunk") requireFilter(plan.chunks.insert(index).second, path.string() + ": duplicate chunk index");
        else throw std::runtime_error("frame capture filter: " + path.string() + ": unknown plan entry " + std::string(kind));
    }
    requireFilter(input.eof(), "cannot read plan " + path.string());
    return plan;
}

std::string DescribeFrameCapture(const std::filesystem::path& path) {
    const AgcDriver::FrameCaptureReader reader(path);
    requireFilter(!reader.Truncated(), "cannot minimize a truncated capture");
    requireFilter(reader.Version() >= 2, "calls format 1 cannot be replayed; record the capture again");
    std::string result = "{\"calls\":[";
    for (std::size_t index = 0; index < reader.Calls().size(); ++index) {
        if (index != 0) result += ',';
        const auto& call = reader.Calls()[index];
        result += "{\"index\":" + std::to_string(index) + ",\"kind\":\"" + std::string(AgcDriver::FrameCallName(call.kind)) + "\",\"ordinal\":" + std::to_string(call.ordinal) + ",\"memory_group\":";
        result += call.kind == AgcDriver::FrameCallKind::Memory && !call.values.empty() ? std::to_string(call.values[0]) : "null";
        result += "}";
    }
    result += "],\"chunks\":[";
    for (std::size_t index = 0; index < reader.Chunks().size(); ++index) {
        if (index != 0) result += ',';
        const auto& chunk = reader.Chunks()[index];
        result += "{\"index\":" + std::to_string(index) + ",\"kind\":\"" + memoryKind(chunk.kind) + "\",\"group\":" + std::to_string(chunk.group) + "}";
    }
    result += "]}\n";
    return result;
}

void FilterFrameCapture(const std::filesystem::path& source, const std::filesystem::path& destination, const FrameCaptureFilterPlan& plan) {
    const AgcDriver::FrameCaptureReader reader(source);
    requireFilter(!reader.Truncated(), "cannot minimize a truncated capture");
    requireFilter(reader.Version() >= 2, "calls format 1 cannot be replayed; record the capture again");
    std::error_code destinationStatusError;
    const auto destinationStatus = std::filesystem::symlink_status(destination, destinationStatusError);
    requireFilter(!destinationStatusError || destinationStatusError == std::errc::no_such_file_or_directory,
        "cannot inspect destination " + destination.string() + ": " + destinationStatusError.message());
    requireFilter(destinationStatus.type() == std::filesystem::file_type::not_found,
        "destination already exists: " + destination.string());
    const auto sourcePath = normalized(source);
    const auto destinationPath = normalized(destination);
    requireFilter(sourcePath != destinationPath, "source and destination must be different paths");
    requireFilter(!isWithin(destinationPath, sourcePath) && !isWithin(sourcePath, destinationPath), "source and destination directories must not contain one another");
    for (const auto index : plan.calls) requireFilter(index < reader.Calls().size(), "call index is outside the source capture");
    for (const auto index : plan.chunks) requireFilter(index < reader.Chunks().size(), "chunk index is outside the source capture");

    std::set<std::uint64_t> removedGroups;
    for (const auto index : plan.calls) {
        const auto& call = reader.Calls()[static_cast<std::size_t>(index)];
        if (call.kind == AgcDriver::FrameCallKind::Memory && !call.values.empty()) removedGroups.insert(call.values[0]);
    }
    for (std::size_t index = 0; index < reader.Calls().size(); ++index) {
        const auto& call = reader.Calls()[index];
        if (!plan.calls.contains(index) && call.kind == AgcDriver::FrameCallKind::Memory && !call.values.empty()) removedGroups.erase(call.values[0]);
    }

    auto parent = destination.parent_path();
    if (parent.empty()) parent = ".";
    std::error_code createError;
    std::filesystem::create_directories(parent, createError);
    requireFilter(!createError, "cannot create destination parent " + parent.string() + ": " + createError.message());
    const bool createdDestination = std::filesystem::create_directory(destination, createError);
    requireFilter(!createError && createdDestination, "destination already exists or cannot be created: " + destination.string());

    try {
        AgcDriver::FrameCaptureWriter writer(destination);
        for (std::size_t index = 0; index < reader.Calls().size(); ++index) {
            if (!plan.calls.contains(index)) writer.WriteCall(reader.Calls()[index]);
        }
        for (std::size_t index = 0; index < reader.Chunks().size(); ++index) {
            const auto& chunk = reader.Chunks()[index];
            if (plan.chunks.contains(index) || (chunk.kind == AgcDriver::FrameMemoryKind::Delta && removedGroups.contains(chunk.group))) continue;
            if (chunk.kind == AgcDriver::FrameMemoryKind::Range) {
                writer.WriteRange(chunk.address, chunk.bytes, chunk.rangeKind, chunk.protection, chunk.access);
                continue;
            }
            const auto bytes = reader.Read(chunk);
            if (chunk.kind == AgcDriver::FrameMemoryKind::Snapshot) writer.WriteSnapshot(chunk.address, bytes);
            else if (chunk.kind == AgcDriver::FrameMemoryKind::Delta) writer.WriteDelta(chunk.group, chunk.completed, chunk.address, bytes);
            else if (chunk.kind == AgcDriver::FrameMemoryKind::Reference) writer.WriteReference(chunk.group, chunk.address, bytes);
            else throw std::runtime_error("frame capture filter: unknown memory kind");
        }
        writer.Finish(reader.Header());
    } catch (...) {
        if (createdDestination) {
            std::error_code error;
            std::filesystem::remove_all(destination, error);
        }
        throw;
    }
}

}
