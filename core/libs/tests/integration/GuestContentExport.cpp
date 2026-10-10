#include "SceTypes.hpp"
#include <cstdlib>
#include <stdexcept>

extern "C" int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param);
extern "C" int APS5_VABI sceContentExportTerm(void);

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

int allocator = 0;

}

int main() {
    ContentExportInitParam2 param{&allocator, &allocator, nullptr, 0x10000, 0, 0};
    Require(Throws([] { sceContentExportInit2(nullptr); }));
    Require(Throws([] { sceContentExportTerm(); }));
    ContentExportInitParam2 missing = param;
    missing.free_func = nullptr;
    Require(Throws([&] { sceContentExportInit2(&missing); }));
    ContentExportInitParam2 reserved = param;
    reserved.reserved1 = 1;
    Require(Throws([&] { sceContentExportInit2(&reserved); }));
    Require(sceContentExportInit2(&param) == 0);
    Require(Throws([&] { sceContentExportInit2(&param); }));
    Require(sceContentExportTerm() == 0);
    Require(Throws([] { sceContentExportTerm(); }));
    Require(sceContentExportInit2(&param) == 0);
    Require(sceContentExportTerm() == 0);
}
