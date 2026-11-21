#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t NotTerminatedWarning = -124;
constexpr std::int32_t BufferOverflowError = 15;

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": unreadable status");
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* function, const char* what) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": unwritable " + what);
}

void WriteStatus(std::int32_t* status, std::int32_t value, const char* function) {
    CopyOut(status, &value, sizeof(value), function, "status");
}

template<typename TUnit>
std::int32_t Terminate(TUnit* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status,
                       const char* function) {
    if (!status) return length;
    std::int32_t current = 0;
    CopyIn(&current, status, sizeof(current), function);
    if (current > 0 || length < 0) return length;
    if (length < capacity) {
        if (!destination) throw std::invalid_argument("ICU string termination: null destination");
        const TUnit terminator{};
        CopyOut(destination + length, &terminator, sizeof(terminator), function, "destination");
        if (current == NotTerminatedWarning) WriteStatus(status, 0, function);
    } else if (length == capacity) {
        WriteStatus(status, NotTerminatedWarning, function);
    } else {
        WriteStatus(status, BufferOverflowError, function);
    }
    return length;
}

}

extern "C" {

std::int32_t APS5_VABI u_terminateChars_nid_postfix(char* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    return Terminate(destination, capacity, length, status, __func__);
}

std::int32_t APS5_VABI u_terminateUChars_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    return Terminate(destination, capacity, length, status, __func__);
}

std::int32_t APS5_VABI u_terminateUChar32s_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    return Terminate(destination, capacity, length, status, __func__);
}

std::int32_t APS5_VABI u_terminateWChars_nid_postfix(std::uint16_t* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    return Terminate(destination, capacity, length, status, __func__);
}

}
