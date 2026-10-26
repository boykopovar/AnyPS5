# libSceVrServiceDialog technical debt

### Silent stubs

- [libSceVrServiceDialog](../../../core/libs/prx/libSceVrServiceDialog/Export.cpp) - the VR service dialog runs the common-dialog state machine but shows nothing and reports the user as having cancelled, as no headset is supported (see libSceHmd2). The signatures are from [Orbital's type database](https://github.com/AlexAltea/orbital/blob/b321fcdbd700cc02da1cb1ad25412781d5ec73c6/tools/ida/db_types.json); the result layout beyond the leading `int32_t` result code is not known, so nothing else is written.
