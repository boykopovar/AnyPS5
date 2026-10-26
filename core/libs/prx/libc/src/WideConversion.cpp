#include "prx/libc/include/WindowsFormatting.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>

extern "C" void APS5_VABI ignore_handler_s_nid_postfix(const char* message, void* pointer, int error) {
    SonyIgnoreHandlerS(message, pointer, error);
}

extern "C" void* APS5_VABI set_constraint_handler_s_nid_postfix(void* handler) {
    return SonySetConstraintHandlerS(handler);
}

extern "C" std::size_t APS5_VABI wcstombs_nid_postfix(char* destination, const std::uint16_t* source, std::size_t capacity) {
    std::size_t count = 0;
    for (;; ++source) {
        char bytes[MB_LEN_MAX];
        const std::size_t length = wcrtomb_nid_postfix(bytes, *source, nullptr);
        for (std::size_t index = 0; index < length; ++index) {
            if (destination && count == capacity) return count;
            if (destination) destination[count] = bytes[index];
            if (bytes[index] == '\0') return count;
            ++count;
        }
    }
}

extern "C" int APS5_VABI wcscpy_s_nid_postfix(std::uint16_t* dest, std::size_t size, const std::uint16_t* src) {
    return SonyWcscpyS(dest, size, src);
}

namespace {

std::size_t ReadErrorMessage(int error, char (&message)[128]) {
    const int result = strerror_r_nid_postfix(error, message, sizeof(message));
    if (result != 0 && result != 22) throw std::runtime_error("strerror_s: error message exceeds internal buffer");
    return std::strlen(message);
}

void ErrorMessageConstraint(char* buffer) {
    SonyConstraintHandler handler;
    {
        std::lock_guard<std::mutex> lock(SonyConstraintMutex());
        handler = SonyConstraintHandlerSlot();
    }
    if (handler == SonyDefaultConstraintHandler) {
        SonyDefaultConstraintHandler("strerror_s", buffer, 22);
    } else {
        using GuestHandler = void (APS5_VABI *)(const char*, void*, int);
        reinterpret_cast<GuestHandler>(handler)("strerror_s", buffer, 22);
    }
}

}

extern "C" std::size_t APS5_VABI strerrorlen_s_nid_postfix(int error) {
    char message[128];
    return ReadErrorMessage(error, message);
}

extern "C" int APS5_VABI strerror_s_nid_postfix(char* buffer, std::size_t capacity, int error) {
    if (!buffer || capacity == 0 || capacity > (std::numeric_limits<std::size_t>::max() >> 1)) {
        ErrorMessageConstraint(buffer);
        return 22;
    }
    char message[128];
    const auto length = ReadErrorMessage(error, message);
    if (length < capacity) {
        std::memcpy(buffer, message, length + 1);
        return 0;
    }
    std::memcpy(buffer, message, capacity - 1);
    buffer[capacity - 1] = '\0';
    if (capacity > 3) std::memcpy(buffer + capacity - 4, "...", 3);
    return 34;
}
