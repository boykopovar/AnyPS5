# libSceJson2

## add Unknown 3.20 import declarations

- [Object, Pair and Object::iterator guest pointer validation](../../../core/libs/prx/libSceJson2/Export.cpp) - console behaviour on null, inaccessible, read-only or aliased `Object`, `Object::Pair` and `Object::iterator` guest pointers is unmeasured; internal non-null `Object::items` and `ObjectIteratorState::position` node pointers are not separately probed. Closes when: console behaviour on invalid `Object`, `Object::Pair` and `Object::iterator` pointers is measured.
