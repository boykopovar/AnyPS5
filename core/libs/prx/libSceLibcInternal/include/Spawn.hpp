#ifndef CORE_LIBS_PRX_LIBSCELIBCINTERNAL_INCLUDE_SPAWN_HPP
#define CORE_LIBS_PRX_LIBSCELIBCINTERNAL_INCLUDE_SPAWN_HPP

#include <cstdint>
#include <string>
#include <vector>

struct GuestSignalSet {
    std::uint32_t bits[4];
};
static_assert(sizeof(GuestSignalSet) == 16);

struct PosixSpawnAttributes {
    std::int16_t flags;
    std::int32_t processGroup;
    std::int32_t schedulingPriority;
    std::int32_t schedulingPolicy;
    GuestSignalSet signalDefault;
    GuestSignalSet signalMask;
};

enum class PosixSpawnFileActionKind { Open, Duplicate, Close };

struct PosixSpawnFileAction {
    PosixSpawnFileActionKind kind;
    int descriptor;
    std::string path;
    int openFlags;
    std::uint16_t mode;
    int newDescriptor;
};

struct PosixSpawnFileActions {
    std::vector<PosixSpawnFileAction> actions;
};

#endif
