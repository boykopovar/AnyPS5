#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libScePosixForWebKit/GuestTimes.hpp"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <thread>

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    errno = E2BIG;
    PosixForWebKitTms first{};
    const auto before = times_nid_postfix(&first);
    Require(first.tms_utime >= 0 && first.tms_stime >= 0);
    Require(first.tms_cutime == 0 && first.tms_cstime == 0);
    Require(errno == E2BIG);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    PosixForWebKitTms second{};
    const auto after = times_nid_postfix(&second);
    Require(after > before);
    Require(second.tms_utime >= first.tms_utime && second.tms_stime >= first.tms_stime);
    Require(second.tms_cutime == 0 && second.tms_cstime == 0 && errno == E2BIG);

    try {
        times_nid_postfix(nullptr);
        std::abort();
    } catch (const std::invalid_argument&) { Require(errno == E2BIG); }

    try {
        times_nid_postfix(reinterpret_cast<PosixForWebKitTms*>(0x1000));
        std::abort();
    } catch (const std::invalid_argument&) { Require(errno == E2BIG); }
}
