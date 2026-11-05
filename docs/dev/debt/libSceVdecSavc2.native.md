# libSceVdecSavc2.native technical debt

### Inventory exports declared elsewhere

The complete PS5 3.20 export table of this module per the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceVdecSavc2.native.c) (4 names) is already declared under the sibling libSceVdecSavc2 in this tree; the duplicate-export rule keeps each name in a single library, so this module keeps its `APS5_DUMMY_FUN` skeleton. Imports resolve through the sibling declarations.
