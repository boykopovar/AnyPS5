#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstddef>

extern "C" {

int APS5_VABI __fflush_nid_postfix(FileStream* stream) {
    if (stream != nullptr) {
        alignas(FileStream) std::byte raw[sizeof(FileStream)];
        if (!GuestCopyIn_nid_no_patch(raw, stream, sizeof(raw))) {
            *__error_nid_postfix() = 14;
            return -1;
        }
        if (!reinterpret_cast<const FileStream*>(raw)->IsOpen()) return 0;
    }
    return fflush_nid_postfix(stream);
}

}
