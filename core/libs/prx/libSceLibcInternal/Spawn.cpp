#include "prx/libSceLibcInternal/include/Spawn.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr int BadDescriptor = 9;
constexpr int OutOfMemory = 12;

PosixSpawnAttributes& Attributes(PosixSpawnAttributes* const* attributes, const char* function) {
    if (attributes == nullptr || *attributes == nullptr) throw std::invalid_argument(std::string(function) + ": uninitialized attributes");
    return **attributes;
}

PosixSpawnFileActions& FileActions(PosixSpawnFileActions* const* actions, const char* function) {
    if (actions == nullptr || *actions == nullptr) throw std::invalid_argument(std::string(function) + ": uninitialized file actions");
    return **actions;
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
    (void)path;
    (void)actions;
    (void)attributes;
    (void)arguments;
    (void)environment;
    return 78;
}

int APS5_VABI posix_spawnp_nid_postfix(std::int32_t* processId, const char* file, PosixSpawnFileActions* const* actions, PosixSpawnAttributes* const* attributes, char* const* arguments, char* const* environment) {
    (void)processId;
    (void)file;
    (void)actions;
    (void)attributes;
    (void)arguments;
    (void)environment;
    return 78;
}

}
