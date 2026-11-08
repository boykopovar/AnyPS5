#pragma once

#include "SceTypes.hpp"
#include "general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

struct FilesystemForwards_nid_no_patch {
    int (APS5_VABI* stat)(const char*, FileStat*);
    int (APS5_VABI* fstat)(int, FileStat*);
    int (APS5_VABI* open)(const char*, int, std::uint16_t);
    std::int64_t (APS5_VABI* read)(int, void*, std::size_t);
    std::int64_t (APS5_VABI* write)(int, const void*, std::size_t);
    int (APS5_VABI* close)(int);
    int (APS5_VABI* mkdir)(const char*, std::uint16_t);
    int (APS5_VABI* unlink)(const char*);
    int (APS5_VABI* rename)(const char*, const char*);
    int (APS5_VABI* truncate)(const char*, std::int64_t);
    int (APS5_VABI* chmod)(const char*, std::uint16_t);
    int (APS5_VABI* fchmod)(int, std::uint16_t);
    int (APS5_VABI* utimes)(const char*, const KernelTimeval*);
    int (APS5_VABI* posixStat)(const char*, FileStat*);
    int (APS5_VABI* posixRmdir)(const char*);
};

extern "C" void RegisterFilesystemForwards_nid_no_patch(const FilesystemForwards_nid_no_patch* forwards);
