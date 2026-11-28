# libSceGameUpdate

## add Functional

- [sceGameUpdateCheck result range](../../../core/libs/prx/libSceGameUpdate/Export.cpp) - the host checks that the full result range is writable before publishing the no-update result. The console response to a partly writable result is unmeasured. A concurrent mapping change after the check may still cause a partial write.
