# libSceAgcDriver

## add Functional

- [Image table entries with an unmapped base](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp) (libSceAgcDriver) - a sampled image-table entry whose base address is not mapped is refused before its surface is read, and a shader that selects it faults with `is not mapped at its base address`, even when later pages of its surface are committed. Before, only an entry with no committed page anywhere in its surface was refused, so a record that holds other data could describe a view of hundreds of GiB over a mostly reserved range and be uploaded from whatever pages were committed in it. A partially resident texture whose first page is not committed, read through a table, would fault the same way; what the console returns for such a read is not measured. Directly bound T#s keep reading uncommitted pages as zeros.
