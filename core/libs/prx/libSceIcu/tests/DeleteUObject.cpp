#include "prx/libSceIcu/libSceIcu.hpp"
#include "unicode/uobject.h"
#include <cstdlib>
#include <type_traits>

static_assert(std::is_same_v<decltype(&uprv_deleteUObject_nid_postfix), void (APS5_VABI*)(void*)>);

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

class ProbeObject final : public icu::UObject {
public:
    explicit ProbeObject(int* destroyed) : destroyed_(destroyed) {}
    ~ProbeObject() override { ++(*destroyed_); }

private:
    int* destroyed_;
};

}

int main() {
    uprv_deleteUObject_nid_postfix(nullptr);

    int destroyed = 0;
    auto* object = new ProbeObject(&destroyed);
    uprv_deleteUObject_nid_postfix(object);
    Require(destroyed == 1);
    return 0;
}
