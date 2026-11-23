#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libwebrtc/include/GuestString.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {

using Type = WebRtcCheckArgType;

struct GuestStringView {
    const char* data;
    std::size_t size;
};

void FatalLog(const char* message, const Type* format, auto... values) {
    _ZN3rtc18webrtc_checks_impl8FatalLogEPKciS2_PKNS0_12CheckArgTypeEz_nid_postfix("rtc_base/example.cc", 42, message, format, values...);
}

void CheckOp() {
    WebRtcGuestString string {};
    std::memcpy(string.buffer, "guest", 5);
    string.size = 5;
    string.capacity = WebRtcGuestStringInlineCapacity;
    const GuestStringView view {"view!", 4};
    const Type format[] = {Type::CheckOp, Type::Int, Type::LongLong, Type::CharP, Type::StdString, Type::StringView, Type::End};
    *__error_nid_postfix() = 2;
    FatalLog("a == b", format, -1, std::int64_t {1} << 40, "<", &string, &view);
}

void Numbers() {
    const Type format[] = {Type::Long, Type::UInt, Type::ULong, Type::ULongLong, Type::CharP, Type::Double, Type::CharP, Type::LongDouble, Type::CharP, Type::VoidP, Type::End};
    *__error_nid_postfix() = 0;
    FatalLog("ready", format, std::int64_t {-5000000000}, 7u, std::uint64_t {1} << 63, std::uint64_t {9}, " ", 1.5, " ", 2.25L, " ",
             reinterpret_cast<const void*>(std::uintptr_t {0xbeef}));
}

void Invalid() {
    const Type format[] = {Type::Int, static_cast<Type>(99), Type::Int, Type::End};
    *__error_nid_postfix() = 0;
    FatalLog("bad", format, 5, 6);
}

void CheckOpWithoutOperands() {
    const Type format[] = {Type::CheckOp, Type::End};
    *__error_nid_postfix() = 0;
    FatalLog("x != y", format);
}

}

int main(int argc, char** argv) {
    std::signal(SIGABRT, [](int) { std::_Exit(0); });
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (mode == "check_op") CheckOp();
    if (mode == "numbers") Numbers();
    if (mode == "invalid") Invalid();
    if (mode == "no_operands") CheckOpWithoutOperands();
    return 1;
}
