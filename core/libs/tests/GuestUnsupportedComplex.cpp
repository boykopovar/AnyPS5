#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>

namespace {

using Unsupported = void (APS5_VABI *)(...);

void Require(bool condition) {
    if (!condition) std::abort();
}

}

int main() {
    const Unsupported functions[] = {
        cacos_nid_postfix,
        cacosh_nid_postfix,
        casin_nid_postfix,
        casinh_nid_postfix,
        catan_nid_postfix,
        catanh_nid_postfix,
        ccos_nid_postfix,
        ccosh_nid_postfix,
        cexp_nid_postfix,
        clog_nid_postfix,
        cpow_nid_postfix,
        csin_nid_postfix,
        csinh_nid_postfix,
        csqrt_nid_postfix,
        ctan_nid_postfix,
        ctanh_nid_postfix,
        cacosf_nid_postfix,
        cacoshf_nid_postfix,
        casinf_nid_postfix,
        casinhf_nid_postfix,
        catanf_nid_postfix,
        catanhf_nid_postfix,
        ccosf_nid_postfix,
        ccoshf_nid_postfix,
        cexpf_nid_postfix,
        clogf_nid_postfix,
        cpowf_nid_postfix,
        csinf_nid_postfix,
        csinhf_nid_postfix,
        csqrtf_nid_postfix,
        ctanf_nid_postfix,
        ctanhf_nid_postfix,
        cacosl_nid_postfix,
        cacoshl_nid_postfix,
        casinl_nid_postfix,
        casinhl_nid_postfix,
        catanl_nid_postfix,
        catanhl_nid_postfix,
        ccosl_nid_postfix,
        ccoshl_nid_postfix,
        cexpl_nid_postfix,
        clogl_nid_postfix,
        cpowl_nid_postfix,
        csinl_nid_postfix,
        csinhl_nid_postfix,
        csqrtl_nid_postfix,
        ctanl_nid_postfix,
        ctanhl_nid_postfix,
    };
    for (const Unsupported function : functions) {
        bool thrown = false;
        try {
            function();
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        Require(thrown);
    }
    return 0;
}
