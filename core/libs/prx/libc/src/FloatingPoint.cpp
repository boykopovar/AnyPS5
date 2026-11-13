#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>

#include "prx/libc/include/GuestCpu.hpp"

namespace {

constexpr unsigned Exceptions = 0x3f;
constexpr unsigned Rounding = 0xc00;

void ReplaceFlags(unsigned mask, unsigned flags) {
    GuestCpu::SetX87Status(static_cast<std::uint16_t>((GuestCpu::X87Status() & ~mask) | (flags & mask)));
    GuestCpu::SetMxcsr((GuestCpu::Mxcsr() & ~mask) | (flags & mask));
}

int ChangeTraps(int exceptions, bool enable) {
    const auto mask = static_cast<unsigned>(exceptions) & Exceptions;
    const std::uint16_t control = GuestCpu::X87Control();
    const unsigned csr = GuestCpu::Mxcsr();
    GuestCpu::SetX87Control(static_cast<std::uint16_t>(enable ? (control & ~mask) : (control | mask)));
    GuestCpu::SetMxcsr(enable ? (csr & ~(mask << 7)) : (csr | (mask << 7)));
    return static_cast<int>(~((csr >> 7) | control) | ~Exceptions);
}

}

extern "C" {

int APS5_VABI fegetround_nid_postfix() {
    return GuestCpu::X87Control() & Rounding;
}

int APS5_VABI fesetround_nid_postfix(int rounding) {
    const auto mode = static_cast<unsigned>(rounding);
    if ((mode & ~Rounding) != 0) return -1;
    GuestCpu::SetX87Control(static_cast<std::uint16_t>((GuestCpu::X87Control() & ~Rounding) | mode));
    GuestCpu::SetMxcsr((GuestCpu::Mxcsr() & ~(Rounding << 3)) | (mode << 3));
    return 0;
}

int APS5_VABI fetestexcept_nid_postfix(int exceptions) {
    return static_cast<int>((GuestCpu::X87Status() | GuestCpu::Mxcsr()) & static_cast<unsigned>(exceptions));
}

int APS5_VABI fegetexceptflag_nid_postfix(std::uint16_t* flags, int exceptions) {
    if (flags == nullptr) throw std::invalid_argument("fegetexceptflag: null flags");
    *flags = static_cast<std::uint16_t>(fetestexcept_nid_postfix(exceptions));
    return 0;
}

void APS5_VABI _Feraise_nid_postfix(int exceptions) {
    constexpr unsigned Invalid = 0x01, Ranges = 0x1c, Overflows = 0x18, Inexact = 0x20;
    constexpr int GuestEdom = 33, GuestErange = 34;
    auto raised = static_cast<unsigned>(exceptions);
    if ((raised & Overflows) != 0) raised |= Inexact;
    *__error_nid_postfix() = GuestEdom;
    GuestCpu::SetX87Status(static_cast<std::uint16_t>((GuestCpu::X87Status() & ~raised) | (raised & 0xffff)));
    GuestCpu::SetMxcsr((GuestCpu::Mxcsr() & ~raised) | (raised & 0xffff));
    GuestCpu::X87Wait();
    if ((raised & Invalid) != 0) *__error_nid_postfix() = GuestEdom;
    else if ((raised & Ranges) != 0) *__error_nid_postfix() = GuestErange;
}

int APS5_VABI fesetexceptflag_nid_postfix(const std::uint16_t* flags, int exceptions) {
    if (flags == nullptr) throw std::invalid_argument("fesetexceptflag: null flags");
    ReplaceFlags(static_cast<unsigned>(exceptions) & Exceptions, *flags);
    return 0;
}

int APS5_VABI feraiseexcept_nid_postfix(int exceptions) {
    const auto raised = static_cast<unsigned>(exceptions);
    if ((raised & ~Exceptions) != 0) throw std::invalid_argument("feraiseexcept: unsupported exception bits");
    ReplaceFlags(raised, raised);
    GuestCpu::X87Wait();
    return 0;
}

int APS5_VABI feclearexcept_nid_postfix(int exceptions) {
    const auto mask = static_cast<unsigned>(exceptions) & Exceptions;
    if (mask == Exceptions) {
        GuestCpu::X87ClearExceptions();
        GuestCpu::SetMxcsr(GuestCpu::Mxcsr() & ~Exceptions);
    } else if (mask != 0) {
        ReplaceFlags(mask, 0);
    }
    return 0;
}

int APS5_VABI feenableexcept_nid_postfix(int exceptions) {
    return ChangeTraps(exceptions, true);
}

int APS5_VABI fedisableexcept_nid_postfix(int exceptions) {
    return ChangeTraps(exceptions, false);
}

int APS5_VABI __feenableexcept_nid_postfix(int exceptions) {
    return ChangeTraps(exceptions, true);
}

int APS5_VABI __fedisableexcept_nid_postfix(int exceptions) {
    return ChangeTraps(exceptions, false);
}

int APS5_VABI fegettrapenable_nid_postfix() {
    return ~GuestCpu::X87Control() & Exceptions;
}

int APS5_VABI fegetexcept_nid_postfix() {
    return fegettrapenable_nid_postfix();
}

int APS5_VABI fesettrapenable_nid_postfix(int exceptions) {
    const auto mask = static_cast<std::uint16_t>(exceptions);
    if (mask > Exceptions) return 1;
    GuestCpu::SetX87Control(static_cast<std::uint16_t>(GuestCpu::X87Control() & ~mask));
    GuestCpu::SetMxcsr(GuestCpu::Mxcsr() & ~(static_cast<unsigned>(mask) << 7));
    return 0;
}

}
