#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

namespace {

struct GuestQueueElement {
    GuestQueueElement* q_forw;
    GuestQueueElement* q_back;
};

}

extern "C" void APS5_VABI insque_nid_postfix(void* element, void* pred) {
    if (element == nullptr) {
        NotImplemented_nid_no_patch(__func__);
    }
    auto* const elem = static_cast<GuestQueueElement*>(element);
    if (pred == nullptr) {
        elem->q_forw = nullptr;
        elem->q_back = nullptr;
        return;
    }
    auto* const p = static_cast<GuestQueueElement*>(pred);
    auto* const next = p->q_forw;
    elem->q_forw = next;
    elem->q_back = p;
    if (next != nullptr) {
        next->q_back = elem;
    }
    p->q_forw = elem;
}

extern "C" void APS5_VABI remque_nid_postfix(void* element) {
    if (element == nullptr) {
        NotImplemented_nid_no_patch(__func__);
    }
    auto* const elem = static_cast<GuestQueueElement*>(element);
    if (elem->q_forw != nullptr) {
        elem->q_forw->q_back = elem->q_back;
    }
    if (elem->q_back != nullptr) {
        elem->q_back->q_forw = elem->q_forw;
    }
}
