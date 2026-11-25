# libSceAgcDriver

## add Functional

- [DlssUpscaler](../../../core/libs/prx/libSceAgcDriver/Execution/src/DlssUpscaler.cpp) (libSceAgcDriver) - DLSS Super Resolution presentation upscaler under `ANYPS5_UPSCALE=dlss`. Requires NVIDIA RTX hardware and dynamic loading of `libnvidia-ngx.so`; on non-NVIDIA GPUs or when NGX is absent, presentation falls back cleanly to the default scaler.
