#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <system_error>

extern "C" {

const std::error_category* _ZSt17iostream_categoryv_nid_postfix();

void APS5_VABI sceLibcInternalSizedNothrowDelete_nid_no_patch(void* pointer, std::size_t size, const void*) noexcept {
    _ZdlPvm_nid_postfix(pointer, size);
}
APS5_EXPORT("7VPIYFpwU2A", sceLibcInternalSizedNothrowDelete_nid_no_patch);

void APS5_VABI sceLibcInternalSizedNothrowArrayDelete_nid_no_patch(void* pointer, std::size_t size, const void*) noexcept {
    _ZdaPvm_nid_postfix(pointer, size);
}
APS5_EXPORT("7lCihI18N9I", sceLibcInternalSizedNothrowArrayDelete_nid_no_patch);

const std::error_category* APS5_VABI sceLibcInternalIostreamCategory_nid_no_patch() {
    return _ZSt17iostream_categoryv_nid_postfix();
}
APS5_EXPORT("V23qt24VPVs", sceLibcInternalIostreamCategory_nid_no_patch);

}
