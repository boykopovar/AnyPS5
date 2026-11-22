# libSceAgcDriver

## add Functional

- [Staged copy-back stores](../../../core/libs/prx/libSceAgcDriver/Graphics/shaders/StagingCopyBack.comp) (libSceAgcDriver) - the changed-byte copy-back kernels (`StagingCopyBack.comp`, `StagingCopyBackBound.comp`) store every word of an eight-word group in which the shader changed a byte, also a word the shader left alone and a word whose changed bytes the CPU had changed too, with the value they loaded from the host import a moment before. A CPU store to such a word between that load and the store is lost; before, only words with a byte to store were written. The window was measured once, with a diagnostic that swaps instead of storing (one PPSA21564 level, NVIDIA, Windows, the bound kernel; details in the pull request): no swap failed; other titles, GPUs and the address-based kernel are unmeasured. Closes when: the kernels write a group without a load-and-store of its untouched words
