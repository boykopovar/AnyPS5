#include "prx/libSceFios2/include/Api.hpp"
#include "Runtime.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <cstring>
#include <vector>

namespace {

constexpr std::size_t DirentHeaderBytes = 8;
constexpr int DirentBufferBytes = 8192;
constexpr std::uint64_t ListingOverheadBytes = 16;

std::int32_t ListNames(const std::string& path, std::vector<std::string>& names) {
    const int descriptor = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    if (descriptor < 0) return Fios::FromKernelError(descriptor);
    std::vector<char> buffer(DirentBufferBytes);
    std::int32_t error = SCE_FIOS_OK;
    for (;;) {
        const int count = sceKernelGetdirentries(descriptor, buffer.data(), static_cast<int>(buffer.size()), nullptr);
        if (count < 0) {
            error = Fios::FromKernelError(count);
            break;
        }
        if (count == 0) break;
        for (int offset = 0; offset + static_cast<int>(DirentHeaderBytes) <= count;) {
            std::uint16_t length = 0;
            std::memcpy(&length, buffer.data() + offset + 4, sizeof(length));
            const auto nameLength = static_cast<unsigned char>(buffer[static_cast<std::size_t>(offset) + 7]);
            if (length < DirentHeaderBytes) break;
            std::string name(buffer.data() + offset + DirentHeaderBytes, nameLength);
            if (name != "." && name != "..") names.push_back(std::move(name));
            offset += length;
        }
    }
    sceKernelClose(descriptor);
    return error;
}

std::uint64_t RequiredBytes(const std::vector<std::string>& names) {
    std::uint64_t bytes = ListingOverheadBytes;
    for (const std::string& name : names) bytes += name.size() + 1;
    return bytes;
}

SceFiosOp StartOpen(const SceFiosOpAttr* attr, SceFiosDH* outDH, const char* path, SceFiosBuffer buffer) {
    Fios::OpInfo info;
    info.buffer = buffer.pPtr;
    info.request = static_cast<std::int64_t>(buffer.length);
    const std::int32_t invalid = outDH == nullptr ? SCE_FIOS_ERROR_BAD_PTR : Fios::CopyPath(path, info.path);
    const std::string target = info.path;
    return Fios::Submit(attr, std::move(info), [=](Fios::Completion& completion) {
        if (invalid != SCE_FIOS_OK) {
            completion.error = invalid;
            return;
        }
        SceFiosStat stat{};
        if (const std::int32_t error = Fios::StatPath(target, stat)) {
            completion.error = error;
            return;
        }
        if ((stat.statFlags & SCE_FIOS_STAT_DIRECTORY) == 0) {
            completion.error = SCE_FIOS_ERROR_NOT_A_DIRECTORY;
            return;
        }
        auto directory = std::make_shared<Fios::DirectoryHandle>();
        directory->path = target;
        if (const std::int32_t error = ListNames(target, directory->names)) {
            completion.error = error;
            return;
        }
        const std::uint64_t required = RequiredBytes(directory->names);
        completion.result = static_cast<std::int64_t>(required);
        if (buffer.pPtr == nullptr || buffer.length < required) {
            completion.error = SCE_FIOS_ERROR_BAD_SIZE;
            return;
        }
        const SceFiosDH handle = Fios::AddDirectory(directory);
        if (handle < 0) {
            completion.error = handle;
            return;
        }
        if (!Fios::CopyOut(outDH, &handle, sizeof(handle))) {
            Fios::RemoveDirectory(handle);
            completion.error = SCE_FIOS_ERROR_BAD_PTR;
        }
    });
}

SceFiosOp StartRead(const SceFiosOpAttr* attr, SceFiosDH handle, SceFiosDirEntry* outEntry) {
    Fios::OpInfo info;
    info.buffer = outEntry;
    if (const auto directory = Fios::FindDirectory(handle)) info.path = directory->path;
    return Fios::Submit(attr, std::move(info), [handle, outEntry](Fios::Completion& completion) {
        const auto directory = Fios::FindDirectory(handle);
        if (!directory) {
            completion.error = SCE_FIOS_ERROR_BAD_DH;
            return;
        }
        if (outEntry == nullptr) {
            completion.error = SCE_FIOS_ERROR_BAD_PTR;
            return;
        }
        const std::lock_guard lock(directory->mutex);
        if (directory->next >= directory->names.size()) {
            completion.error = SCE_FIOS_ERROR_EOF;
            return;
        }
        std::string prefix = directory->path;
        while (prefix.size() > 1 && prefix.back() == '/') prefix.pop_back();
        if (prefix.back() != '/') prefix += '/';
        const std::string& name = directory->names[directory->next];
        const std::string full = prefix + name;
        if (full.size() >= SCE_FIOS_PATH_MAX) {
            completion.error = SCE_FIOS_ERROR_PATH_TOO_LONG;
            return;
        }
        SceFiosStat stat{};
        Fios::StatPath(full, stat);
        SceFiosDirEntry entry{};
        entry.fileSize = stat.fileSize;
        entry.statFlags = stat.statFlags;
        entry.nameLength = static_cast<std::uint16_t>(name.size());
        entry.fullPathLength = static_cast<std::uint16_t>(full.size());
        entry.offsetToName = static_cast<std::uint16_t>(prefix.size());
        std::memcpy(entry.fullPath, full.c_str(), full.size());
        if (!Fios::CopyOut(outEntry, &entry, sizeof(entry))) {
            completion.error = SCE_FIOS_ERROR_BAD_PTR;
            return;
        }
        ++directory->next;
    });
}

SceFiosOp StartClose(const SceFiosOpAttr* attr, SceFiosDH handle) {
    Fios::OpInfo info;
    if (const auto directory = Fios::FindDirectory(handle)) info.path = directory->path;
    return Fios::Submit(attr, std::move(info), [handle](Fios::Completion& completion) {
        if (!Fios::RemoveDirectory(handle)) completion.error = SCE_FIOS_ERROR_BAD_DH;
    });
}

int StatusSync(SceFiosOp op) {
    return Fios::SyncStatus(op);
}

}

extern "C" {

SceFiosOp APS5_VABI sceFiosDHOpen(const SceFiosOpAttr* attr, SceFiosDH* outDH, const char* path, SceFiosBuffer buffer) {
    return StartOpen(attr, outDH, path, buffer);
}

int APS5_VABI sceFiosDHOpenSync(const SceFiosOpAttr* attr, SceFiosDH* outDH, const char* path, SceFiosBuffer buffer) {
    return StatusSync(StartOpen(attr, outDH, path, buffer));
}

SceFiosOp APS5_VABI sceFiosDHRead(const SceFiosOpAttr* attr, SceFiosDH dh, SceFiosDirEntry* outEntry) {
    return StartRead(attr, dh, outEntry);
}

int APS5_VABI sceFiosDHReadSync(const SceFiosOpAttr* attr, SceFiosDH dh, SceFiosDirEntry* outEntry) {
    return StatusSync(StartRead(attr, dh, outEntry));
}

SceFiosOp APS5_VABI sceFiosDHClose(const SceFiosOpAttr* attr, SceFiosDH dh) {
    return StartClose(attr, dh);
}

int APS5_VABI sceFiosDHCloseSync(const SceFiosOpAttr* attr, SceFiosDH dh) {
    return StatusSync(StartClose(attr, dh));
}

const char* APS5_VABI sceFiosDHGetPath(SceFiosDH dh) {
    const auto directory = Fios::FindDirectory(dh);
    return directory ? directory->path.c_str() : nullptr;
}

}
