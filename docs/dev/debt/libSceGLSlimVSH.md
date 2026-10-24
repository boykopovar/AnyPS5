# libSceGLSlimVSH technical debt

### Unknown function info

- The 23 known-name imports in [Export.cpp](../../../core/libs/prx/libSceGLSlimVSH/Export.cpp) are listed in the PS5 3.20 [export inventory](https://github.com/DNNDHH/PS5-3.20_Libs). Their exact prototypes and behavior are unverified; each uses a no-parameter throwing import-resolution placeholder.
