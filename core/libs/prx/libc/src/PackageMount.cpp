#include "prx/libc/include/PackageMount.hpp"
#include "prx/libc/include/General.hpp"

#include <pkg/Package.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr int GuestEnoent = 2;
constexpr int GuestEbadf = 9;
constexpr int GuestEisdir = 21;
constexpr int GuestEinval = 22;
constexpr int GuestEmfile = 24;
constexpr int GuestErofs = 30;
constexpr int GuestEnotdir = 20;
constexpr int GuestAccessMode = 0x3;
constexpr int GuestWriteFlags = 0x0008 | 0x0200 | 0x0400 | 0x0800;
constexpr int GuestDirectoryFlag = 0x20000;
constexpr char ApplicationRoot[] = "/app0";
constexpr char SidecarName[] = "anyps5-package.ini";
constexpr std::uint8_t GuestDirectoryType = 4;
constexpr std::uint8_t GuestRegularType = 8;

int sceError(const int error) {
    return static_cast<int>(0x80020000u | static_cast<unsigned>(error));
}

struct Descriptor {
    const Pkg::PackageNode* Node = nullptr;
    std::uint64_t Position = 0;
};

struct MountState {
    std::once_flag once;
    std::unique_ptr<Pkg::Package> package;
    std::mutex mutex;
    std::unordered_map<int, Descriptor> descriptors;
    int next = PackageMount::FirstDescriptor;
};

MountState& State() {
    static MountState state;
    return state;
}

std::filesystem::path ExecutableDirectory() {
#ifdef _WIN32
    std::wstring module(MAX_PATH, L'\0');
    for (;;) {
        const auto length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        if (length == 0) throw std::runtime_error("PackageMount: cannot locate the executable");
        if (length < module.size()) {
            module.resize(length);
            return std::filesystem::path(module).parent_path();
        }
        module.resize(module.size() * 2);
    }
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}

std::filesystem::path Utf8Path(const std::string& text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::optional<std::filesystem::path> EnvironmentPath(const char* name) {
#ifdef _WIN32
    const std::wstring wide(name, name + std::strlen(name));
    const wchar_t* value = _wgetenv(wide.c_str());
#else
    const char* value = std::getenv(name);
#endif
    if (value == nullptr || value[0] == 0) return std::nullopt;
    return std::filesystem::path(value);
}

std::map<std::string, std::string> ReadSidecar(const std::filesystem::path& path) {
    std::map<std::string, std::string> values;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return values;
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto separator = line.find('=');
        if (line.empty()) continue;
        if (separator == std::string::npos) throw std::runtime_error("PackageMount: malformed line in " + path.string() + ": " + line);
        values[line.substr(0, separator)] = line.substr(separator + 1);
    }
    return values;
}

void Mount(MountState& state) {
    const auto sidecar = ReadSidecar(ExecutableDirectory() / SidecarName);
    const auto value = [&](const char* key) {
        const auto found = sidecar.find(key);
        return found == sidecar.end() ? std::string{} : found->second;
    };
    const auto packageOverride = EnvironmentPath("ANYPS5_PACKAGE");
    const auto oodleOverride = EnvironmentPath("ANYPS5_OODLE");
    const auto package = packageOverride ? *packageOverride : value("package").empty() ? std::filesystem::path{} : Utf8Path(value("package"));
    if (package.empty()) return;
    const auto oodle = oodleOverride ? *oodleOverride : value("oodle").empty() ? std::filesystem::path{} : Utf8Path(value("oodle"));
    state.package = std::make_unique<Pkg::Package>(package, oodle);
    if (packageOverride) return;
    const auto size = value("size");
    const auto contentId = value("content_id");
    if ((!size.empty() && size != std::to_string(state.package->FileSize())) || (!contentId.empty() && contentId != state.package->ContentId()))
        throw std::runtime_error("PackageMount: " + value("package") + " does not match " + SidecarName + "; relink the package");
}

Pkg::Package* Mounted() {
    auto& state = State();
    std::call_once(state.once, [&] { Mount(state); });
    return state.package.get();
}

const Pkg::PackageNode* Lookup(const char* path) {
    if (path == nullptr) return nullptr;
    const int savedError = errno;
    auto* package = Mounted();
    std::string guest;
    const Pkg::PackageNode* node = nullptr;
    constexpr std::size_t rootLength = sizeof(ApplicationRoot) - 1;
    if (package != nullptr && GuestPath_nid_no_patch(path, &guest) && guest.compare(0, rootLength, ApplicationRoot) == 0 &&
        (guest.size() == rootLength || guest[rootLength] == '/'))
        node = package->Find(std::string_view(guest).substr(rootLength));
    errno = savedError;
    return node;
}

std::uint32_t InodeOf(const Pkg::Package& package, const Pkg::PackageNode& node) {
    return static_cast<std::uint32_t>(&node - &package.Root()) + 1;
}

void FillStat(const Pkg::Package& package, const Pkg::PackageNode& node, FileStat* status) {
    *status = FileStat{};
    status->st_ino = InodeOf(package, node);
    status->st_mode = static_cast<std::uint16_t>(node.Directory ? 0x416d : 0x816d);
    status->st_nlink = static_cast<std::uint16_t>(node.Directory ? 2 : 1);
    status->st_size = static_cast<std::int64_t>(node.Directory ? 0x10000 : node.Size);
    status->st_blksize = 0x10000;
    status->st_blocks = (status->st_size + 511) / 512;
}

Descriptor* Find(MountState& state, const int descriptor) {
    const auto found = state.descriptors.find(descriptor);
    return found == state.descriptors.end() ? nullptr : &found->second;
}

}

extern "C" bool PackageLookup_nid_no_patch(const char* guestPath, PackageMount::EntryInfo* info) {
    const auto* node = Lookup(guestPath);
    if (node == nullptr) return false;
    if (info != nullptr) *info = PackageMount::EntryInfo{node->Directory, node->Size};
    return true;
}

extern "C" std::uint64_t PackageReadPath_nid_no_patch(const char* guestPath, const std::uint64_t offset, void* destination, const std::uint64_t size) {
    const auto* node = Lookup(guestPath);
    if (node == nullptr || node->Directory) throw std::runtime_error(std::string("PackageMount: not a package file: ") + (guestPath ? guestPath : "(null)"));
    return Mounted()->Read(*node, offset, destination, static_cast<std::size_t>(size));
}

extern "C" bool PackageReadAll_nid_no_patch(const char* guestPath, std::vector<std::uint8_t>* bytes) {
    const auto* node = Lookup(guestPath);
    if (node == nullptr || node->Directory) return false;
    *bytes = Mounted()->ReadAll(*node);
    return true;
}

extern "C" bool PackageOpen_nid_no_patch(const char* guestPath, const int flags, int* result) {
    const auto* node = Lookup(guestPath);
    if (node == nullptr) return false;
    if ((flags & GuestAccessMode) != 0 || (flags & GuestWriteFlags) != 0) {
        *result = sceError(GuestErofs);
        return true;
    }
    if ((flags & GuestDirectoryFlag) != 0 && !node->Directory) {
        *result = sceError(GuestEnotdir);
        return true;
    }
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.descriptors.size() > static_cast<std::size_t>(PackageMount::LastDescriptor - PackageMount::FirstDescriptor)) {
        *result = sceError(GuestEmfile);
        return true;
    }
    while (state.descriptors.contains(state.next)) state.next = state.next == PackageMount::LastDescriptor ? PackageMount::FirstDescriptor : state.next + 1;
    *result = state.next;
    state.descriptors.emplace(state.next, Descriptor{node, 0});
    state.next = state.next == PackageMount::LastDescriptor ? PackageMount::FirstDescriptor : state.next + 1;
    return true;
}

extern "C" int PackageClose_nid_no_patch(const int descriptor) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    return state.descriptors.erase(descriptor) != 0 ? 0 : sceError(GuestEbadf);
}

extern "C" std::int64_t PackagePread_nid_no_patch(const int descriptor, void* buffer, const std::size_t size, const std::int64_t offset) {
    auto& state = State();
    const Pkg::PackageNode* node = nullptr;
    {
        std::lock_guard lock(state.mutex);
        const auto* entry = Find(state, descriptor);
        if (entry == nullptr) return sceError(GuestEbadf);
        node = entry->Node;
    }
    if (node->Directory) return sceError(GuestEisdir);
    if (offset < 0) return sceError(GuestEinval);
    return static_cast<std::int64_t>(Mounted()->Read(*node, static_cast<std::uint64_t>(offset), buffer, size));
}

extern "C" std::int64_t PackageRead_nid_no_patch(const int descriptor, void* buffer, const std::size_t size) {
    auto& state = State();
    Descriptor snapshot;
    {
        std::lock_guard lock(state.mutex);
        const auto* entry = Find(state, descriptor);
        if (entry == nullptr) return sceError(GuestEbadf);
        snapshot = *entry;
    }
    if (snapshot.Node->Directory) return sceError(GuestEisdir);
    const auto read = Mounted()->Read(*snapshot.Node, snapshot.Position, buffer, size);
    std::lock_guard lock(state.mutex);
    if (auto* entry = Find(state, descriptor)) entry->Position = snapshot.Position + read;
    return static_cast<std::int64_t>(read);
}

extern "C" std::int64_t PackageSeek_nid_no_patch(const int descriptor, const std::int64_t offset, const int whence) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto* entry = Find(state, descriptor);
    if (entry == nullptr) return sceError(GuestEbadf);
    const auto size = static_cast<std::int64_t>(entry->Node->Directory ? 0 : entry->Node->Size);
    const std::int64_t base = whence == 0 ? 0 : whence == 1 ? static_cast<std::int64_t>(entry->Position) : whence == 2 ? size : -1;
    if (base < 0 || (offset < 0 && -offset > base) || (offset > 0 && offset > std::numeric_limits<std::int64_t>::max() - base)) return sceError(GuestEinval);
    entry->Position = static_cast<std::uint64_t>(base + offset);
    return base + offset;
}

extern "C" int PackageFstat_nid_no_patch(const int descriptor, FileStat* status) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    const auto* entry = Find(state, descriptor);
    if (entry == nullptr) return sceError(GuestEbadf);
    FillStat(*Mounted(), *entry->Node, status);
    return 0;
}

extern "C" bool PackageStat_nid_no_patch(const char* guestPath, FileStat* status) {
    const auto* node = Lookup(guestPath);
    if (node == nullptr) return false;
    FillStat(*Mounted(), *node, status);
    return true;
}

extern "C" bool PackageListDirectory_nid_no_patch(const char* guestPath, std::vector<PackageMount::DirectoryEntry>* entries) {
    const auto* node = Lookup(guestPath);
    if (node == nullptr || !node->Directory) return false;
    const auto& package = *Mounted();
    entries->clear();
    entries->push_back(PackageMount::DirectoryEntry{".", true, InodeOf(package, *node)});
    entries->push_back(PackageMount::DirectoryEntry{"..", true, InodeOf(package, *node)});
    for (const auto index : node->Children) {
        const auto& child = package.Node(index);
        entries->push_back(PackageMount::DirectoryEntry{child.Name, child.Directory, InodeOf(package, child)});
    }
    return true;
}

extern "C" int PackageGetdents_nid_no_patch(const int descriptor, char* buffer, const int size, std::int64_t* base) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto* entry = Find(state, descriptor);
    if (entry == nullptr) return sceError(GuestEbadf);
    if (!entry->Node->Directory) return sceError(GuestEnotdir);
    if (size <= 0) return sceError(GuestEinval);
    const auto& package = *Mounted();
    if (base != nullptr) *base = static_cast<std::int64_t>(entry->Position);
    const auto& children = entry->Node->Children;
    std::size_t used = 0;
    while (entry->Position < children.size() + 2) {
        const auto index = entry->Position;
        const Pkg::PackageNode* child = index < 2 ? nullptr : &package.Node(children[index - 2]);
        const std::string name = index == 0 ? "." : index == 1 ? ".." : child->Name;
        const std::size_t record = (8 + name.size() + 1 + 3) & ~std::size_t{3};
        if (used + record > static_cast<std::size_t>(size)) {
            if (used == 0) return sceError(GuestEinval);
            break;
        }
        char* out = buffer + used;
        std::memset(out, 0, record);
        const std::uint32_t fileNumber = child != nullptr ? InodeOf(package, *child) : InodeOf(package, *entry->Node);
        const auto recordLength = static_cast<std::uint16_t>(record);
        std::memcpy(out, &fileNumber, sizeof(fileNumber));
        std::memcpy(out + 4, &recordLength, sizeof(recordLength));
        out[6] = static_cast<char>(child == nullptr || child->Directory ? GuestDirectoryType : GuestRegularType);
        out[7] = static_cast<char>(name.size());
        std::memcpy(out + 8, name.c_str(), name.size() + 1);
        used += record;
        ++entry->Position;
    }
    return static_cast<int>(used);
}
