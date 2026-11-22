# libSceAppInstUtil

## change [sceAppInstUtilAppGetAddcontNum]

- [sceAppInstUtilAppGetAddcontNum](../../../core/libs/prx/libSceAppInstUtil/Export.cpp) (libSceAppInstUtil) - unknown signature. `dlopen` of libSceAppInstUtil.sprx does not return in a PS5 Pro payload process (13.60, `kernel_get_fw_version` = `0x13600007`): two sessions stopped inside the call and were ended by the deployer timeout (60 and 75 s), so no name of this module can be measured from a payload through `dlopen` and everything below stays unverified; the load must not be repeated casually. A title process may differ.
