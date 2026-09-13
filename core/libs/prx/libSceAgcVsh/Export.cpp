#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int DummyFunction_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
