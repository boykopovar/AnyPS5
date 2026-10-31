# libSceConnectAccountDialog technical debt

### Silent stubs

- [libSceConnectAccountDialog](../../../core/libs/prx/libSceConnectAccountDialog/Export.cpp) - lifecycle simulated without UI. `Close` from initialized state returning 0 and going to finished is emulator policy, unmeasured for this dialog. `Open` and `GetResult` remain throwing stubs.

[Pinned export metadata](https://gist.github.com/kyon-eth/3fbb1a4a6f95a936b4cd58e9c3d8c50c/5e3af709224ed8235af11337309af0725e5685cb) (extract hash `02ab27f12bcd0cc9fa03fa33f55fc201110f0d54a86bbbaa093d55d0a8cf262d`):

| Name | NID |
| --- | --- |
| `sceConnectAccountDialogClose` | `9ShAQi6BXhg` |
| `sceConnectAccountDialogGetResult` | `T+wthkoDDFE` |
| `sceConnectAccountDialogGetStatus` | `BsY6ldG11CU` |
| `sceConnectAccountDialogInitialize` | `41VeaFH-BlE` |
| `sceConnectAccountDialogOpen` | `5dDAY+FchRM` |
| `sceConnectAccountDialogTerminate` | `pr2u71StfRo` |
| `sceConnectAccountDialogUpdateStatus` | `B24oCJO6Awg` |
