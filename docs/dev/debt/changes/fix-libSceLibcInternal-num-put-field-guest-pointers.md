# libSceLibcInternal

## add Unknown function info

- [NumPutField.cpp](../../../core/libs/prx/libSceLibcInternal/NumPutField.cpp): `num_put` field exports and facet reference-count vtable callbacks validate guest memory readability, writability, alignment, and range overflow and throw `std::invalid_argument` on invalid guest pointers because PS5 hardware fault behavior is unverified.
