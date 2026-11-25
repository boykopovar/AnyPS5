# libSceJson2

## add Unknown 3.20 import declarations

- [Parser::parse guest pointer validation](../../../core/libs/prx/libSceJson2/Export.cpp) - console behaviour on null, inaccessible, read-only, truncated or aliased `Parser::parse` destination, text or path pointers is unmeasured. Closes when: console behaviour on invalid `Parser::parse` pointers is measured.
