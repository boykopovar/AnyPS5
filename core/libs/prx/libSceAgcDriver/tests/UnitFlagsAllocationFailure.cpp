#include "AllocationFailure.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "Testing.hpp"
#include <cstdio>
#include <new>
#include <stdexcept>
#include <string_view>

namespace {

void failAssign(AgcDriver::Graphics::UnitFlags& flags, std::size_t size) {
    bool failed = false;
    AgcDriver::Tests::SetAllocationFailureCountdown(0);
    try {
        flags.assign(size, true);
    } catch (const std::bad_alloc&) {
        failed = true;
    } catch (...) {
        AgcDriver::Tests::SetAllocationFailureCountdown(-1);
        throw;
    }
    AgcDriver::Tests::SetAllocationFailureCountdown(-1);
    Require(failed, "unit flags assignment did not exercise allocation failure");
}

void emptySet() {
    AgcDriver::Graphics::UnitFlags flags;
    failAssign(flags, 65);
    Require(flags.empty() && flags.size() == 0 && !flags.Any(), "failed first allocation changed the unit flags size");
    flags.assign(65, true);
    Require(flags.size() == 65 && flags.Count() == 65 && flags.AllIn(0, 65), "unit flags did not recover after a failed first allocation");
    flags.SetRange(64, 1, false);
    Require(flags.Count() == 64 && !flags.AnyIn(64, 1), "recovered unit flags lost range access");
}

void populatedSet() {
    AgcDriver::Graphics::UnitFlags flags;
    flags.assign(65, false);
    flags[3] = true;
    flags[64] = true;
    failAssign(flags, 129);
    Require(flags.size() == 65 && flags.Count() == 2 && flags[3] && flags[64], "failed growth changed the existing unit flags");
    flags.assign(130, true);
    Require(flags.size() == 130 && flags.Count() == 130 && flags.AllIn(0, 130), "unit flags did not recover after failed growth");
    flags.SetRange(0, 130, false);
    Require(!flags.Any(), "recovered unit flags retained bits after clearing");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected empty or populated case");
        const std::string_view mode(argv[1]);
        if (mode == "empty") emptySet();
        else if (mode == "populated") populatedSet();
        else throw std::runtime_error("unknown unit flags allocation-failure case");
        return 0;
    } catch (const std::exception& error) {
        AgcDriver::Tests::SetAllocationFailureCountdown(-1);
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
