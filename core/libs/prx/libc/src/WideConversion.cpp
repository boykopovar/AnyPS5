#include "prx/libc/include/WindowsFormatting.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>

extern "C" void APS5_VABI ignore_handler_s_nid_postfix(const char* message, void* pointer, int error) {
    SonyIgnoreHandlerS_nid_no_patch(message, pointer, error);
}

extern "C" void* APS5_VABI set_constraint_handler_s_nid_postfix(void* handler) {
    return SonySetConstraintHandlerS_nid_no_patch(handler);
}

extern "C" std::size_t APS5_VABI wcstombs_nid_postfix(char* destination, const std::uint16_t* source, std::size_t capacity) {
    return wcsrtombs_nid_postfix(destination, &source, capacity, nullptr);
}

extern "C" int APS5_VABI wcscpy_s_nid_postfix(std::uint16_t* dest, std::size_t size, const std::uint16_t* src) {
    return SonyWcscpyS(dest, size, src);
}

namespace {

std::size_t ReadErrorMessage(int error, char (&message)[128]) {
    strerror_r_nid_postfix(error, message, sizeof(message));
    return std::strlen(message);
}

void ErrorMessageConstraint() {
    SonyConstraintHandler handler;
    {
        std::lock_guard<std::mutex> lock(SonyConstraintMutex());
        handler = SonyConstraintHandlerSlot();
    }
    if (handler == SonyDefaultConstraintHandler) {
        SonyDefaultConstraintHandler("strerror_s: bad argument", nullptr, 22);
    } else {
        using GuestHandler = void (APS5_VABI *)(const char*, void*, int);
        reinterpret_cast<GuestHandler>(handler)("strerror_s: bad argument", nullptr, 22);
    }
}

}

extern "C" std::size_t APS5_VABI strerrorlen_s_nid_postfix(int error) {
    char message[128];
    return ReadErrorMessage(error, message);
}

extern "C" int APS5_VABI strerror_s_nid_postfix(char* buffer, std::size_t capacity, int error) {
    if (!buffer || capacity == 0 || capacity > (std::numeric_limits<std::size_t>::max() >> 3)) {
        ErrorMessageConstraint();
        return 22;
    }
    char message[128];
    const auto length = ReadErrorMessage(error, message);
    std::size_t count = length + 1;
    if (length >= capacity) {
        count = capacity;
        message[capacity - 1] = '\0';
        if (capacity > 3) std::memcpy(message + capacity - 4, "...", 3);
    }
    if (!GuestCopyOut_nid_no_patch(buffer, message, count)) {
        ErrorMessageConstraint();
        return 22;
    }
    return 0;
}
