# libSceIcu

## add Functional

- [Hash table exports](../../../core/libs/prx/libSceIcu/UHash.cpp) (libSceIcu) - thirty-eight exports ported from ICU 68.2's `uhash.cpp`: open, init, close, the put/get/remove families, enumeration, the resize policy, `uhash_equals` and the built-in hashes and comparators. `UHashtable` and `UHashElement` are asserted at compile time (80 and 24 bytes, at the header's offsets), and guest hasher, comparator and deleter pointers are called through the `APS5_VABI` types the other callbacks use. A lookup that finds neither key nor a free slot returns a null element, and a null key hashes to zero. The four `UnicodeString` hash and compare entries stay stubs for lack of that model. The table storage comes from the ICU allocator, so the test initializes the application heap through libc first. Comparators are equality predicates: a nonzero result means equal in the probe loop and in `uhash_equals`; tests force one bucket to pin collisions and value equality.
