# libSceAgcDriver

## add Functional

- [Storage image stores to pages that cannot keep them](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp) (libSceAgcDriver) - a storage image unit written while part of its guest range is uncommitted or read-only is stored and uploaded again at its next lookup, which waits for both, so later dispatches read zeros or the guest bytes there, as under Vulkan's `residencyNonResidentStrict`. Within one dispatch a load after such a store still returns the stored value. Commitment is checked when the store is recorded: a page decommitted later keeps the stored value in the image, and one committed before the next lookup receives it. A write-back over unreadable pages stores the readable ones only when an unreadable page's allocation is readable elsewhere in it; otherwise it drops all its pending results. What the console returns for a page decommitted outside a `sceKernelSetPrtAperture` aperture is not measured.
