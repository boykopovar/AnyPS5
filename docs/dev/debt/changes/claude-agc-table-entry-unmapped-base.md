# libSceAgcDriver

## add Functional

- [Image table entries with an unmapped base](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp) (libSceAgcDriver) - what the console returns when a shader selects a table entry whose base address is not mapped but whose surface has committed pages further on (a partially resident texture whose first page is not committed) is not measured. Such an entry is refused before its surface is read, and a shader that selects it faults with `is not mapped at its base address`; an unselected one changes nothing. Before, only an entry with no committed page anywhere in its surface was refused, so a record holding other data could describe a view of hundreds of GiB over a mostly reserved range and be uploaded from whatever pages were committed in it. Directly bound T#s keep reading uncommitted pages as zeros.
