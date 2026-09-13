#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP

#include <cstdio>
#include <stdexcept>
#include <utility>

static constexpr const char* FOPEN_EXT_VERT = ".vert";
static constexpr const char* FOPEN_MSG_NULL_ARG = "null argument";
static constexpr const char* FOPEN_MSG_NOT_FOUND = "file not found";
static constexpr const char* FOPEN_MSG_OPEN_FAILED = "open failed";

class FileStream {
    std::FILE* _handle;
    bool _dynamic;

public:
    explicit FileStream(std::FILE* handle, bool dynamic = false) : _handle(handle), _dynamic(dynamic) {
        if (!_handle) throw std::runtime_error("FileStream: null handle");
    }

    FileStream(const FileStream&) = delete;
    FileStream& operator=(const FileStream&) = delete;

    std::FILE* GetHandle() const {
        if (!_handle) throw std::runtime_error("FileStream: closed stream");
        return _handle;
    }

    bool IsDynamic() const {
        return _dynamic;
    }

    void Close() {
        GetHandle();
        if (std::fclose(std::exchange(_handle, nullptr)) != 0) throw std::runtime_error("FileStream: close failed");
    }
};

inline std::FILE* GetNativeStream(FileStream* stream) {
    if (!stream) throw std::runtime_error("FileStream: null stream");
    return stream->GetHandle();
}

extern "C" {
extern FileStream _Stdout_nid_postfix;
extern FileStream _Stderr_nid_postfix;
}

#endif
