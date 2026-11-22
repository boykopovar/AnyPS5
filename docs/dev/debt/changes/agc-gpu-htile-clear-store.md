# libSceAgcDriver

## add Functional

- [GPU stores of clear HTILE words over native multisample depth](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - a GPU store with no clear note over a native multisample depth target's depth-only HTILE plane clears the native depth to the target's clear value when, once it lands, every HTILE word proves that clear (`htileProvesClear`, depth 0.0 or 1.0); the stencil plane is left as it is. Resident Evil Village (PPSA01556) clears depth to 0.0 this way. HTILE that also holds stencil state, and stores whose words prove neither a clear nor an expanded surface, still throw.
