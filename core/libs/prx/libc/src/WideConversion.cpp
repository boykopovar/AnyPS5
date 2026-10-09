#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/SonyCrt.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>

extern "C" void APS5_VABI ignore_handler_s_nid_postfix(const char* message, void* pointer, int error) {
    SonyIgnoreHandlerS(message, pointer, error);
}

extern "C" void* APS5_VABI set_constraint_handler_s_nid_postfix(void* handler) {
    return SonySetConstraintHandlerS(handler);
}

extern "C" std::size_t APS5_VABI wcstombs_nid_postfix(char* destination, const std::uint16_t* source, std::size_t capacity) {
    std::size_t count = 0;
    while (!destination || count < capacity) {
        const auto value = source[count];
        if (value > 255) {
            errno = 86;
            return static_cast<std::size_t>(-1);
        }
        if (destination) destination[count] = static_cast<char>(value);
        if (value == 0) break;
        ++count;
    }
    return count;
}

extern "C" int APS5_VABI wcscpy_s_nid_postfix(std::uint16_t* dest, std::size_t size, const std::uint16_t* src) {
    return SonyWcscpyS(dest, size, src);
}
