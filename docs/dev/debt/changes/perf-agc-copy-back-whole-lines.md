# libSceAgcDriver

## add Functional

- [Staged copy-back stores](../../../core/libs/prx/libSceAgcDriver/Graphics/shaders/StagingCopyBack.comp) (libSceAgcDriver) - the changed-byte copy-back kernels (`StagingCopyBack.comp`, `StagingCopyBackBound.comp`) store every word of an eight-word group in which the shader changed a byte, also a word the shader left alone and a word whose changed bytes the CPU had changed too, with the value they loaded from the host import a moment before. A CPU store to such a word between that load and the store is lost; before, only words with a byte to store were written. The window and how often a title writes into it were not measured. Closes when: the kernels write a group without a load-and-store of its untouched words, or a title's writes into the window are shown not to occur
