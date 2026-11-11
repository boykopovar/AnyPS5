#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int64_t Success = 0;
constexpr std::int64_t FileError = -102;
constexpr int FromStart = 0;

FileStream* File(Jxr::Stream* stream, const char* function) {
    if (!stream) throw std::invalid_argument(std::string(function) + " requires a stream");
    return reinterpret_cast<FileStream*>(stream->state[0]);
}

}

extern "C" {

std::int64_t APS5_VABI WriteWS_File_nid_postfix(Jxr::Stream* stream, const void* data, std::size_t size) {
    auto* file = File(stream, __func__);
    if (size != 0 && fwrite_nid_postfix(data, size, 1, file) != 1) return FileError;
    return Success;
}

std::int64_t APS5_VABI ReadWS_File_nid_postfix(Jxr::Stream* stream, void* data, std::size_t size) {
    return fread_nid_postfix(data, size, 1, File(stream, __func__)) == 1 ? Success : FileError;
}

std::int64_t APS5_VABI SetPosWS_File_nid_postfix(Jxr::Stream* stream, std::size_t position) {
    auto* file = File(stream, __func__);
    if (fseek_nid_postfix(file, static_cast<std::int64_t>(position), FromStart) != 0) return FileError;
    return Success;
}

std::int64_t APS5_VABI GetPosWS_File_nid_postfix(Jxr::Stream* stream, std::size_t* position) {
    const std::int64_t offset = ftell_nid_postfix(File(stream, __func__));
    if (offset == -1) return FileError;
    if (!position) throw std::invalid_argument("GetPosWS_File requires an output");
    *position = static_cast<std::size_t>(offset);
    return Success;
}

int APS5_VABI EOSWS_File_nid_postfix(Jxr::Stream* stream) {
    return feof_nid_postfix(File(stream, __func__));
}

}
