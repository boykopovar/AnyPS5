#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/PkeyContext.hpp"

extern "C" int APS5_VABI RSA_pkey_ctx_ctrl_nid_postfix(void* pointer, int operationType, int command, int argument, void* value) {
    auto* context = static_cast<PkeyContext*>(pointer);
    if (context && context->method && context->method->pkeyId != 6 && context->method->pkeyId != 9) return -1;
    return PkeyContextControl_nid_no_patch(context, -1, operationType, command, argument, value);
}
