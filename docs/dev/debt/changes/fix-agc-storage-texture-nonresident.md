# libSceAgcDriver

## change [Storage images over uncommitted guest memory]

- [Storage images over uncommitted guest memory](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp) (libSceAgcDriver) - a storage image whose guest range has no committed page is treated as non-resident memory, as its uncommitted pages already were when some other page was committed: reads return zeros and stores to it are discarded, as for an unmapped AMD PRT page and Vulkan's `residencyNonResidentStrict`. Whether the console GPU reads zeros and drops stores for a page decommitted outside a PRT aperture set with `sceKernelSetPrtAperture`, or faults, is not public and not measured. The host image itself is resident, so a load after a store to the same texel inside one dispatch returns the stored value, not zeros (not tested). A range with committed pages none of which is writable still throws `storage texture has no writable guest pages`.
