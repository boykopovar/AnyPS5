#include "SceTypes.hpp"

#include <cstdlib>

extern "C" int APS5_VABI scePadUnknown_zBsgF0q8DIM();

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    Require(scePadUnknown_zBsgF0q8DIM() == 0);
    Require(scePadUnknown_zBsgF0q8DIM() == 0);
}
