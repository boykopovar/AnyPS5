# libSceIcu

## add Functional

- [UnicodeSet span and applyIntPropertyValue](../../../core/libs/prx/libSceIcu/Set.cpp) are unverified on a console. `UnicodeSet::span`, `spanBack`, `spanUTF8`, `spanBackUTF8` and `applyIntPropertyValue` take `this` as a set from this library's `uset_*` exports or `u_getBinaryPropertySet`, as ICU's `USet` and `UnicodeSet` are the same object; a `UnicodeSet` that title code built in its own memory is not such a set and throws. They call ICU 78.1 (Unicode 17; the console's ICU 68 has Unicode 13), which reads the text directly, as the other set exports do. `applyIntPropertyValue` leaves a property set from `u_getBinaryPropertySet` unchanged, as ICU does for a frozen set. A null set, null text with a non-zero length, or a null or inaccessible status throws where the console would fault. Closes when: compared with the console.
