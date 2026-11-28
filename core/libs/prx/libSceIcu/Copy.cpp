#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

namespace {

std::size_t Bytes(std::size_t units, const char* function) {
    if (units > std::numeric_limits<std::size_t>::max() / sizeof(char16_t)) NotImplemented_nid_no_patch(function);
    return units * sizeof(char16_t);
}

std::size_t TerminatedUnits(const char16_t* source, const char* function) {
    const auto length = std::char_traits<char16_t>::length(source);
    if (length >= std::numeric_limits<std::size_t>::max() / sizeof(char16_t)) NotImplemented_nid_no_patch(function);
    return length + 1;
}

std::size_t BoundedLength(const char16_t* source, std::size_t count) {
    std::size_t length = 0;
    while (length < count && source[length] != 0) ++length;
    return length;
}

void Copy(char16_t* destination, const char16_t* source, std::size_t units) {
    for (std::size_t index = 0; index < units; ++index) destination[index] = source[index];
}

} // namespace

extern "C" {

char16_t* APS5_VABI u_strcpy_nid_postfix(char16_t* destination, const char16_t* source) {
    if (!destination || !source) NotImplemented_nid_no_patch(__func__);
    const auto units = TerminatedUnits(source, __func__);
    GuestArena::HostWrite access(destination, Bytes(units, __func__));
    if (!access.Open()) NotImplemented_nid_no_patch(__func__);
    Copy(destination, source, units);
    return destination;
}

char16_t* APS5_VABI u_strncpy_nid_postfix(char16_t* destination, const char16_t* source, std::int32_t count) {
    if (count <= 0) return destination;
    if (!destination || !source) NotImplemented_nid_no_patch(__func__);
    auto units = BoundedLength(source, static_cast<std::size_t>(count));
    if (units < static_cast<std::size_t>(count)) ++units;
    GuestArena::HostWrite access(destination, Bytes(units, __func__));
    if (!access.Open()) NotImplemented_nid_no_patch(__func__);
    Copy(destination, source, units);
    return destination;
}

char16_t* APS5_VABI u_strcat_nid_postfix(char16_t* destination, const char16_t* source) {
    if (!destination || !source) NotImplemented_nid_no_patch(__func__);
    char16_t* end = destination;
    while (*end != 0) ++end;
    const auto units = TerminatedUnits(source, __func__);
    GuestArena::HostWrite access(end, Bytes(units, __func__));
    if (!access.Open()) NotImplemented_nid_no_patch(__func__);
    Copy(end, source, units);
    return destination;
}

char16_t* APS5_VABI u_strncat_nid_postfix(char16_t* destination, const char16_t* source, std::int32_t count) {
    if (count <= 0) return destination;
    if (!destination || !source) NotImplemented_nid_no_patch(__func__);
    char16_t* end = destination;
    while (*end != 0) ++end;
    const auto units = BoundedLength(source, static_cast<std::size_t>(count));
    GuestArena::HostWrite access(end, Bytes(units + 1, __func__));
    if (!access.Open()) NotImplemented_nid_no_patch(__func__);
    Copy(end, source, units);
    end[units] = 0;
    return destination;
}

char16_t* APS5_VABI u_memcpy_nid_postfix(char16_t* destination, const char16_t* source, std::int32_t count) {
    if (count > 0) {
        if (!destination || !source) NotImplemented_nid_no_patch(__func__);
        const auto bytes = static_cast<std::size_t>(count) * sizeof(char16_t);
        GuestArena::HostWrite access(destination, bytes);
        if (!access.Open()) NotImplemented_nid_no_patch(__func__);
        std::memcpy(destination, source, bytes);
    }
    return destination;
}

}
