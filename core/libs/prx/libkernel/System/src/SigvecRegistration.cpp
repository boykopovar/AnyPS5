#include "prx/libc/include/LegacySignals.hpp"

namespace {

struct RegisterSigvecOnLoad {
    RegisterSigvecOnLoad() {
        RegisterSigvec_nid_no_patch(sigaction_nid_postfix);
    }
};

const RegisterSigvecOnLoad registerSigvec;

}
