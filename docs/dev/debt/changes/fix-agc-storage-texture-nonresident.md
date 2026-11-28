# libSceAgcDriver

## change [Storage images over uncommitted guest memory]

- [Storage images over uncommitted guest memory](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp) (libSceAgcDriver) - the uncommitted pages of a storage image, also when none is committed, upload as zeros and drop stores, as for an unmapped AMD PRT page under Vulkan's `residencyNonResidentStrict`. Whether the console GPU does this for a page decommitted outside a `sceKernelSetPrtAperture` aperture, or faults, is not public and not measured. Until the image's pending results are flushed, later dispatches read the values stored into the resident host image instead of zeros. Madden NFL 27 (PPSA32904) and Biped (PPSA24589) stopped on the old throw. A range whose committed pages are all read-only still throws `storage texture has no writable guest pages`.
