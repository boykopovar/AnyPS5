#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct FileHandle {
    WtfAbi::String path;
    int mode;
    int fileHandle;
    unsigned lockMode;
    bool shouldLock;
};

static_assert(sizeof(FileHandle) == 24 && alignof(FileHandle) == 8);
static_assert(offsetof(FileHandle, mode) == 0x08);
static_assert(offsetof(FileHandle, fileHandle) == 0x0c);
static_assert(offsetof(FileHandle, lockMode) == 0x10);
static_assert(offsetof(FileHandle, shouldLock) == 0x14);
static_assert(std::is_standard_layout_v<FileHandle>);

}  // namespace NKWebKit::WebCoreAbi
