# libSceAgcDriver

## add Functional

- [Layer borrow at a storage refresh covers linear arrays only](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Texture.cpp) (libSceAgcDriver) - a storage image takes the pending results of an image that is whole array layers of it with a device copy only when both images are tracked per array layer, which today means linear arrays. A tiled array, an image based inside a layer and an image of another format, extent, mip count or sample count still have the pending results stored to guest memory and the layer uploaded again at the refresh. The borrow also needs a host import under the array, so a platform without host imports always takes the store. Closes when: tiled arrays are tracked per array layer and borrow the same way.
