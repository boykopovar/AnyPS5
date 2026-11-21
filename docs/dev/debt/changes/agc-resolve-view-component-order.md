# libSceAgcDriver

## add Functional

- [CB resolve into a view of another component order](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) (libSceAgcDriver) - a CB resolve whose destination color buffer format is neither the resident image's format nor its sRGB twin (a `B8G8R8A8_SRGB` color buffer over an `R8G8B8A8_UNORM` image) goes through a single-sample image of the source's format and a copy on devices whose render-pass resolve writes the image's component order instead of the view's: AMD's Windows driver 24.30.36 on a Radeon iGPU (`1002:13c0`) and, per issue #14380, AMD 32.0.13058.8001 and NVIDIA 572.16. A probe at the first such resolve of each format pair and sample count decides, once per physical device and process; a probe that fails for another reason also selects the copy. The cost of the copy on those devices is not measured.
