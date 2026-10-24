#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/General.hpp"

#include <zarchive/zarchivereader.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>

namespace {

struct Archive {
    std::mutex mutex;
    bool opened = false;
    std::unique_ptr<ZArchiveReader> reader;
    std::filesystem::path path;
    std::filesystem::path app0;
};

Archive& State() {
    static auto* archive = new Archive;
    return *archive;
}

bool SameComponent(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    const auto fold = [](unsigned char value) { return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value; };
    for (std::size_t i = 0; i < left.size(); ++i) if (fold(left[i]) != fold(right[i])) return false;
    return true;
}

Archive& Opened() {
    auto& archive = State();
    std::lock_guard lock(archive.mutex);
    if (archive.opened) return archive;
    const auto app0 = ResolveDiskPath_nid_no_patch("/app0").lexically_normal();
    auto candidate = app0;
    candidate += ".zar";
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) {
        archive.reader.reset(ZArchiveReader::OpenFromFile(candidate));
        if (!archive.reader) throw std::runtime_error("app0.zar is not a readable ZArchive: " + candidate.string());
        archive.path = candidate;
    }
    archive.app0 = app0;
    archive.opened = true;
    return archive;
}

bool ArchiveRelative(const Archive& archive, const std::filesystem::path& hostPath, std::string& relative) {
    const auto normal = hostPath.lexically_normal();
    auto part = normal.begin();
    for (const auto& component : archive.app0) {
        if (part == normal.end() || !SameComponent(part->generic_string(), component.generic_string())) return false;
        ++part;
    }
    relative.clear();
    for (; part != normal.end(); ++part) {
        const auto name = part->generic_string();
        if (name.empty() || name == ".") continue;
        if (name == "..") return false;
        if (!relative.empty()) relative += '/';
        relative += name;
    }
    return true;
}

struct OpenFile {
    AppArchiveNode node;
    std::uint64_t position = 0;
    std::mutex mutex;
};

std::mutex g_descriptorMutex;
std::map<int, std::shared_ptr<OpenFile>> g_descriptors;
std::map<int, AppArchiveNode> g_materializedDescriptors;

std::shared_ptr<OpenFile> Attached(int descriptor) {
    std::lock_guard lock(g_descriptorMutex);
    const auto found = g_descriptors.find(descriptor);
    return found == g_descriptors.end() ? nullptr : found->second;
}

ZArchiveReader& Reader(std::uint32_t handle) {
    auto& archive = Opened();
    if (!archive.reader) throw std::logic_error("app0.zar handle " + std::to_string(handle) + " used without an archive");
    return *archive.reader;
}

}

extern "C" {

bool AppArchiveLookup_nid_no_patch(const std::filesystem::path& hostPath, AppArchiveNode* node) {
    if (node == nullptr) throw std::invalid_argument("AppArchiveLookup: null node");
    auto& archive = Opened();
    if (!archive.reader) return false;
    std::string relative;
    if (!ArchiveRelative(archive, hostPath, relative)) return false;
    const auto handle = archive.reader->LookUp(relative);
    if (handle == ZARCHIVE_INVALID_NODE) return false;
    node->Handle = handle;
    node->Directory = archive.reader->IsDirectory(handle);
    node->Size = node->Directory ? 0 : archive.reader->GetFileSize(handle);
    return true;
}

std::uint64_t AppArchiveRead_nid_no_patch(std::uint32_t handle, std::uint64_t offset, void* buffer, std::uint64_t length) {
    auto& reader = Reader(handle);
    if (!reader.IsFile(handle)) throw std::invalid_argument("AppArchiveRead: handle " + std::to_string(handle) + " is not a file");
    const auto size = reader.GetFileSize(handle);
    if (length == 0 || offset >= size) return 0;
    if (buffer == nullptr) throw std::invalid_argument("AppArchiveRead: null buffer");
    const auto expected = std::min(length, size - offset);
    const auto read = reader.ReadFromFile(handle, offset, length, buffer);
    if (read != expected) throw std::runtime_error("app0.zar is damaged: a compressed block of handle " + std::to_string(handle) + " cannot be read");
    return read;
}

std::vector<AppArchiveDirectoryEntry> AppArchiveList_nid_no_patch(std::uint32_t handle) {
    auto& reader = Reader(handle);
    if (!reader.IsDirectory(handle)) throw std::invalid_argument("AppArchiveList: handle " + std::to_string(handle) + " is not a directory");
    std::vector<AppArchiveDirectoryEntry> entries;
    const auto count = reader.GetDirEntryCount(handle);
    entries.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        ZArchiveReader::DirEntry entry{};
        if (!reader.GetDirEntry(handle, index, entry)) throw std::runtime_error("app0.zar is damaged: directory entry " + std::to_string(index) + " cannot be read");
        entries.push_back({std::string(entry.name), entry.isDirectory});
    }
    return entries;
}

void AppArchiveReady_nid_no_patch() {
    (void)Opened();
}

void AppArchiveRefuseOnly_nid_no_patch(const std::filesystem::path& hostPath, const char* guestPath, const char* operation) {
    if (AppArchiveOnly_nid_no_patch(hostPath)) NotImplemented_nid_no_patch((std::string(operation) + " " + guestPath + " in app0.zar").c_str());
}

void AppArchiveRefuseMergedListing_nid_no_patch(const std::filesystem::path& hostPath, const char* guestPath) {
    if (AppArchiveHasDirectory_nid_no_patch(hostPath)) NotImplemented_nid_no_patch((std::string("listing ") + guestPath + " merged with app0.zar").c_str());
}

void AppArchiveRefuseDescriptor_nid_no_patch(int descriptor, const char* operation) {
    if (AppArchiveDescriptor_nid_no_patch(descriptor, nullptr, nullptr)) NotImplemented_nid_no_patch((std::string(operation) + " of a file read from app0.zar").c_str());
}

std::filesystem::path AppArchivePath_nid_no_patch() {
    return Opened().path;
}

bool AppArchiveOnly_nid_no_patch(const std::filesystem::path& hostPath) {
    auto& archive = Opened();
    if (!archive.reader) return false;
    std::error_code error;
    const auto status = std::filesystem::symlink_status(hostPath, error);
    if (std::filesystem::exists(status) || (error && error != std::errc::no_such_file_or_directory)) return false;
    AppArchiveNode node{};
    return AppArchiveLookup_nid_no_patch(hostPath, &node);
}

bool AppArchiveHasDirectory_nid_no_patch(const std::filesystem::path& hostPath) {
    AppArchiveNode node{};
    return AppArchiveLookup_nid_no_patch(hostPath, &node) && node.Directory;
}

void AppArchiveAttach_nid_no_patch(int descriptor, const AppArchiveNode& node) {
    if (node.Directory) throw std::invalid_argument("AppArchiveAttach: descriptor " + std::to_string(descriptor) + " cannot hold an app0.zar directory");
    auto file = std::make_shared<OpenFile>();
    file->node = node;
    std::lock_guard lock(g_descriptorMutex);
    g_descriptors[descriptor] = std::move(file);
    g_materializedDescriptors.erase(descriptor);
}

void AppArchiveAttachMaterialized_nid_no_patch(int descriptor, const AppArchiveNode& node) {
    if (node.Directory) throw std::invalid_argument("AppArchiveAttachMaterialized: descriptor " + std::to_string(descriptor) + " cannot hold an app0.zar directory");
    std::lock_guard lock(g_descriptorMutex);
    g_descriptors.erase(descriptor);
    g_materializedDescriptors[descriptor] = node;
}

bool AppArchiveMaterializedDescriptor_nid_no_patch(int descriptor, AppArchiveNode* node) {
    std::lock_guard lock(g_descriptorMutex);
    const auto found = g_materializedDescriptors.find(descriptor);
    if (found == g_materializedDescriptors.end()) return false;
    if (node != nullptr) *node = found->second;
    return true;
}

void AppArchiveShare_nid_no_patch(int from, int to) {
    std::lock_guard lock(g_descriptorMutex);
    const auto found = g_descriptors.find(from);
    if (found != g_descriptors.end()) {
        g_materializedDescriptors.erase(to);
        g_descriptors[to] = found->second;
        return;
    }
    g_descriptors.erase(to);
    const auto materialized = g_materializedDescriptors.find(from);
    if (materialized == g_materializedDescriptors.end()) g_materializedDescriptors.erase(to);
    else g_materializedDescriptors[to] = materialized->second;
}

void AppArchiveDetach_nid_no_patch(int descriptor) {
    std::lock_guard lock(g_descriptorMutex);
    g_descriptors.erase(descriptor);
    g_materializedDescriptors.erase(descriptor);
}

bool AppArchiveDescriptor_nid_no_patch(int descriptor, AppArchiveNode* node, std::uint64_t* position) {
    const auto file = Attached(descriptor);
    if (!file) return false;
    std::lock_guard lock(file->mutex);
    if (node != nullptr) *node = file->node;
    if (position != nullptr) *position = file->position;
    return true;
}

bool AppArchiveDescriptorRead_nid_no_patch(int descriptor, const AppArchiveBuffer* buffers, int count, const std::int64_t* offset, std::uint64_t* read) {
    const auto file = Attached(descriptor);
    if (!file) return false;
    if (read == nullptr || count < 0 || (buffers == nullptr && count != 0)) throw std::invalid_argument("AppArchiveDescriptorRead: invalid arguments for descriptor " + std::to_string(descriptor));
    if (offset != nullptr && *offset < 0) throw std::invalid_argument("AppArchiveDescriptorRead: negative offset for descriptor " + std::to_string(descriptor));
    std::lock_guard lock(file->mutex);
    auto cursor = offset != nullptr ? static_cast<std::uint64_t>(*offset) : file->position;
    std::uint64_t total = 0;
    for (int i = 0; i < count; ++i) {
        const auto done = AppArchiveRead_nid_no_patch(file->node.Handle, cursor, buffers[i].Base, buffers[i].Length);
        cursor += done;
        total += done;
        if (done < buffers[i].Length) break;
    }
    if (offset == nullptr) file->position = cursor;
    *read = total;
    return true;
}

bool AppArchiveDescriptorSeek_nid_no_patch(int descriptor, std::int64_t offset, int whence, std::int64_t* position) {
    const auto file = Attached(descriptor);
    if (!file) return false;
    if (position == nullptr) throw std::invalid_argument("AppArchiveDescriptorSeek: null position");
    std::lock_guard lock(file->mutex);
    constexpr auto limit = std::numeric_limits<std::int64_t>::max();
    std::int64_t base = 0;
    if (whence == 1) base = static_cast<std::int64_t>(file->position);
    else if (whence == 2) base = static_cast<std::int64_t>(file->node.Size);
    else if (whence != 0) throw std::invalid_argument("AppArchiveDescriptorSeek: invalid whence " + std::to_string(whence));
    if ((offset > 0 && base > limit - offset) || base + offset < 0) {
        *position = -1;
        return true;
    }
    file->position = static_cast<std::uint64_t>(base + offset);
    *position = base + offset;
    return true;
}

}
