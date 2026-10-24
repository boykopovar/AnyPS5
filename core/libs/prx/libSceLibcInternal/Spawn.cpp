#include "prx/libSceLibcInternal/include/Spawn.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

extern "C" char* APS5_VABI getenv_nid_postfix(const char* name);

namespace {

constexpr int NoSuchFile = 2;
constexpr int BadDescriptor = 9;
constexpr int OutOfMemory = 12;
constexpr std::string_view DefaultSearchPath = "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin";

PosixSpawnAttributes& Attributes(PosixSpawnAttributes* const* attributes, const char* function) {
    if (attributes == nullptr || *attributes == nullptr) throw std::invalid_argument(std::string(function) + ": uninitialized attributes");
    return **attributes;
}

PosixSpawnFileActions& FileActions(PosixSpawnFileActions* const* actions, const char* function) {
    if (actions == nullptr || *actions == nullptr) throw std::invalid_argument(std::string(function) + ": uninitialized file actions");
    return **actions;
}

bool GuestPathExists(const std::string& path) {
    std::filesystem::path native;
    if (ResolvePathError_nid_no_patch(path.c_str(), native) != 0) return false;
    std::error_code error;
    return std::filesystem::exists(native, error);
}

bool ProgramFound(std::string_view name, bool searchesPath) {
    if (name.empty()) return false;
    if (!searchesPath || name.find('/') != std::string_view::npos) return GuestPathExists(std::string(name));
    const char* variable = getenv_nid_postfix("PATH");
    std::string_view directories = variable != nullptr ? std::string_view(variable) : DefaultSearchPath;
    while (true) {
        const auto separator = directories.find(':');
        const auto directory = directories.substr(0, separator);
        if (GuestPathExists((directory.empty() ? std::string(".") : std::string(directory)) + '/' + std::string(name))) return true;
        if (separator == std::string_view::npos) return false;
        directories.remove_prefix(separator + 1);
    }
}

int Spawn(const char* function, const char* file, bool searchesPath, PosixSpawnFileActions* const* actions, PosixSpawnAttributes* const* attributes) {
    if (file == nullptr) throw std::invalid_argument(std::string(function) + ": null path");
    if (actions != nullptr) FileActions(actions, function);
    if (attributes != nullptr) Attributes(attributes, function);
    if (!ProgramFound(file, searchesPath)) return NoSuchFile;
    NotImplemented_nid_no_patch((std::string(function) + ": process creation").c_str());
    return -1;
}

int Append(PosixSpawnFileActions& actions, PosixSpawnFileAction action) {
    try {
        actions.actions.push_back(std::move(action));
    } catch (const std::bad_alloc&) {
        return OutOfMemory;
    }
    return 0;
}

}

extern "C" {

int APS5_VABI posix_spawnattr_init_nid_postfix(PosixSpawnAttributes** attributes) {
    if (attributes == nullptr) throw std::invalid_argument("posix_spawnattr_init: null attributes");
    auto* created = new (std::nothrow) PosixSpawnAttributes{};
    if (created == nullptr) return OutOfMemory;
    *attributes = created;
    return 0;
}

int APS5_VABI posix_spawnattr_destroy_nid_postfix(PosixSpawnAttributes** attributes) {
    if (attributes == nullptr) throw std::invalid_argument("posix_spawnattr_destroy: null attributes");
    delete *attributes;
    return 0;
}

int APS5_VABI posix_spawnattr_setflags_nid_postfix(PosixSpawnAttributes** attributes, std::int16_t flags) {
    Attributes(attributes, "posix_spawnattr_setflags").flags = flags;
    return 0;
}

int APS5_VABI posix_spawnattr_setsigdefault_nid_postfix(PosixSpawnAttributes** attributes, const GuestSignalSet* signalDefault) {
    auto& target = Attributes(attributes, "posix_spawnattr_setsigdefault");
    if (signalDefault == nullptr) throw std::invalid_argument("posix_spawnattr_setsigdefault: null signal set");
    target.signalDefault = *signalDefault;
    return 0;
}

int APS5_VABI posix_spawnattr_getflags_nid_postfix(PosixSpawnAttributes* const* attributes, std::int16_t* flags) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getflags");
    if (flags == nullptr) throw std::invalid_argument("posix_spawnattr_getflags: null flags");
    *flags = source.flags;
    return 0;
}

int APS5_VABI posix_spawnattr_getpgroup_nid_postfix(PosixSpawnAttributes* const* attributes, std::int32_t* processGroup) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getpgroup");
    if (processGroup == nullptr) throw std::invalid_argument("posix_spawnattr_getpgroup: null process group");
    *processGroup = source.processGroup;
    return 0;
}

int APS5_VABI posix_spawnattr_getschedparam_nid_postfix(PosixSpawnAttributes* const* attributes, std::int32_t* schedulingParameter) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getschedparam");
    if (schedulingParameter == nullptr) throw std::invalid_argument("posix_spawnattr_getschedparam: null scheduling parameter");
    *schedulingParameter = source.schedulingPriority;
    return 0;
}

int APS5_VABI posix_spawnattr_getschedpolicy_nid_postfix(PosixSpawnAttributes* const* attributes, std::int32_t* schedulingPolicy) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getschedpolicy");
    if (schedulingPolicy == nullptr) throw std::invalid_argument("posix_spawnattr_getschedpolicy: null scheduling policy");
    *schedulingPolicy = source.schedulingPolicy;
    return 0;
}

int APS5_VABI posix_spawnattr_getsigdefault_nid_postfix(PosixSpawnAttributes* const* attributes, GuestSignalSet* signalDefault) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getsigdefault");
    if (signalDefault == nullptr) throw std::invalid_argument("posix_spawnattr_getsigdefault: null signal set");
    *signalDefault = source.signalDefault;
    return 0;
}

int APS5_VABI posix_spawnattr_getsigmask_nid_postfix(PosixSpawnAttributes* const* attributes, GuestSignalSet* signalMask) {
    const auto& source = Attributes(attributes, "posix_spawnattr_getsigmask");
    if (signalMask == nullptr) throw std::invalid_argument("posix_spawnattr_getsigmask: null signal set");
    *signalMask = source.signalMask;
    return 0;
}

int APS5_VABI posix_spawnattr_setpgroup_nid_postfix(PosixSpawnAttributes** attributes, std::int32_t processGroup) {
    Attributes(attributes, "posix_spawnattr_setpgroup").processGroup = processGroup;
    return 0;
}

int APS5_VABI posix_spawnattr_setschedparam_nid_postfix(PosixSpawnAttributes** attributes, const std::int32_t* schedulingParameter) {
    auto& target = Attributes(attributes, "posix_spawnattr_setschedparam");
    if (schedulingParameter == nullptr) throw std::invalid_argument("posix_spawnattr_setschedparam: null scheduling parameter");
    target.schedulingPriority = *schedulingParameter;
    return 0;
}

int APS5_VABI posix_spawnattr_setschedpolicy_nid_postfix(PosixSpawnAttributes** attributes, std::int32_t schedulingPolicy) {
    Attributes(attributes, "posix_spawnattr_setschedpolicy").schedulingPolicy = schedulingPolicy;
    return 0;
}

int APS5_VABI posix_spawnattr_setsigmask_nid_postfix(PosixSpawnAttributes** attributes, const GuestSignalSet* signalMask) {
    auto& target = Attributes(attributes, "posix_spawnattr_setsigmask");
    if (signalMask == nullptr) throw std::invalid_argument("posix_spawnattr_setsigmask: null signal set");
    target.signalMask = *signalMask;
    return 0;
}

int APS5_VABI posix_spawn_file_actions_init_nid_postfix(PosixSpawnFileActions** actions) {
    if (actions == nullptr) throw std::invalid_argument("posix_spawn_file_actions_init: null file actions");
    auto* created = new (std::nothrow) PosixSpawnFileActions{};
    if (created == nullptr) return -1;
    *actions = created;
    return 0;
}

int APS5_VABI posix_spawn_file_actions_destroy_nid_postfix(PosixSpawnFileActions** actions) {
    delete &FileActions(actions, "posix_spawn_file_actions_destroy");
    return 0;
}

int APS5_VABI posix_spawn_file_actions_addopen_nid_postfix(PosixSpawnFileActions** actions, int descriptor, const char* path, int openFlags, std::uint16_t mode) {
    if (descriptor < 0) return BadDescriptor;
    if (path == nullptr) throw std::invalid_argument("posix_spawn_file_actions_addopen: null path");
    auto& target = FileActions(actions, "posix_spawn_file_actions_addopen");
    try {
        return Append(target, {PosixSpawnFileActionKind::Open, descriptor, path, openFlags, mode, 0});
    } catch (const std::bad_alloc&) {
        return OutOfMemory;
    }
}

int APS5_VABI posix_spawn_file_actions_adddup2_nid_postfix(PosixSpawnFileActions** actions, int descriptor, int newDescriptor) {
    if (descriptor < 0 || newDescriptor < 0) return BadDescriptor;
    return Append(FileActions(actions, "posix_spawn_file_actions_adddup2"), {PosixSpawnFileActionKind::Duplicate, descriptor, {}, 0, 0, newDescriptor});
}

int APS5_VABI posix_spawn_file_actions_addclose_nid_postfix(PosixSpawnFileActions** actions, int descriptor) {
    if (descriptor < 0) return BadDescriptor;
    return Append(FileActions(actions, "posix_spawn_file_actions_addclose"), {PosixSpawnFileActionKind::Close, descriptor, {}, 0, 0, 0});
}

int APS5_VABI posix_spawn_nid_postfix(std::int32_t* processId, const char* path, PosixSpawnFileActions* const* actions, PosixSpawnAttributes* const* attributes, char* const* arguments, char* const* environment) {
    (void)processId;
    (void)arguments;
    (void)environment;
    return Spawn("posix_spawn", path, false, actions, attributes);
}

int APS5_VABI posix_spawnp_nid_postfix(std::int32_t* processId, const char* file, PosixSpawnFileActions* const* actions, PosixSpawnAttributes* const* attributes, char* const* arguments, char* const* environment) {
    (void)processId;
    (void)arguments;
    (void)environment;
    return Spawn("posix_spawnp", file, true, actions, attributes);
}

}
