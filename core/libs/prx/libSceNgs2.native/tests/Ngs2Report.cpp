#include "Ngs2Test.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

template <typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

int callbacks = 0;
void APS5_VABI Counted(const Ngs2ReportDataHeader*, std::uintptr_t) {
    callbacks++;
}

}

int main() {
    Ngs2Handle handle = 0x1234;
    Require(Throws([&] { sceNgs2ReportRegisterHandler(5, nullptr, 0, &handle); }));
    Require(handle == 0x1234);
    Require(sceNgs2ReportRegisterHandler(5, Counted, 0, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);

    Require(sceNgs2ReportRegisterHandler(5, Counted, 42, &handle) == SCE_NGS2_OK);
    Require(handle != 0);
    Ngs2Handle second = 0;
    Require(sceNgs2ReportRegisterHandler(7, Counted, 43, &second) == SCE_NGS2_OK);
    Require(second != 0 && second != handle);

    Require(sceNgs2ReportUnregisterHandler(handle) == SCE_NGS2_OK);
    Require(Throws([&] { sceNgs2ReportUnregisterHandler(handle); }));
    Require(Throws([] { sceNgs2ReportUnregisterHandler(0x1234); }));
    Require(sceNgs2ReportUnregisterHandler(second) == SCE_NGS2_OK);
    Require(callbacks == 0);
    return 0;
}
