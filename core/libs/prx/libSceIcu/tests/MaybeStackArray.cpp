#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

int main() {
    const auto check = [](bool value) { if (!value) std::abort(); };
    using Array = Icu::Abi::MaybeStackArray40;
    for (auto construct : {_ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix, _ZN6icu_6815MaybeStackArrayIcLi40EEC2Ev_nid_postfix}) {
        Array array{};
        for (auto& byte : array.stack) byte = '!';
        construct(&array);
        check(array.pointer == array.stack && array.capacity == 40 && !array.needToRelease);
        check(_ZNK6icu_6815MaybeStackArrayIcLi40EE11getCapacityEv_nid_postfix(&array) == 40);
        check(_ZNK6icu_6815MaybeStackArrayIcLi40EE8getAliasEv_nid_postfix(&array) == array.stack);
        check(_ZNK6icu_6815MaybeStackArrayIcLi40EE13getArrayLimitEv_nid_postfix(&array) == array.stack + 40);
        for (int index = 0; index < 40; ++index) {
            auto* item = _ZN6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(&array, index);
            check(item == array.stack + index && *item == '!');
            *item = static_cast<char>(index);
            check(*_ZNK6icu_6815MaybeStackArrayIcLi40EEixEl_nid_postfix(&array, index) == index);
        }
        char external[3]{};
        array.pointer = external;
        array.capacity = 3;
        check(_ZNK6icu_6815MaybeStackArrayIcLi40EE13getArrayLimitEv_nid_postfix(&array) == external + 3);
        _ZN6icu_6815MaybeStackArrayIcLi40EE17resetToStackArrayEv_nid_postfix(&array);
        check(array.pointer == array.stack && array.capacity == 40 && !array.needToRelease);
        for (int index = 0; index < 40; ++index) check(array.stack[index] == index);
    }
    bool threw = false;
    try { _ZN6icu_6815MaybeStackArrayIcLi40EEC1Ev_nid_postfix(nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw);
    Array empty{};
    check(_ZNK6icu_6815MaybeStackArrayIcLi40EE8getAliasEv_nid_postfix(&empty) == nullptr);
    threw = false;
    try { _ZNK6icu_6815MaybeStackArrayIcLi40EE13getArrayLimitEv_nid_postfix(&empty); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw);
    std::puts("ICU MaybeStackArray access tests passed");
}
