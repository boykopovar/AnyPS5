#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

template<typename TException, typename TCall>
static void RequireThrows(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return;
    }
    std::abort();
}

constexpr int success = 0;

int main() {
    void* condition = nullptr;
    Require(_Cnd_init_with_default_name_override_nid_postfix(&condition, "GameCond") == success);
    Require(condition != nullptr);
    Require(_Cnd_signal_nid_postfix(&condition) == success);
    _Cnd_destroy_nid_postfix(&condition);

    Require(condition == nullptr);

    void* unnamed = nullptr;
    Require(_Cnd_init_with_default_name_override_nid_postfix(&unnamed, nullptr) == success);
    Require(unnamed != nullptr);
    _Cnd_destroy_nid_postfix(&unnamed);

    RequireThrows<std::invalid_argument>([] {
        _Cnd_init_with_default_name_override_nid_postfix(nullptr, "NullHandle");

    });
    return 0;
}
