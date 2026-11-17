# libScePad

## change scePadGetControllerInformation reports connectedCount

- [scePadGetControllerInformation](../../../core/libs/prx/libScePad/Export.cpp) (libScePad) - behaviour across multiple open controller handles is not measured on hardware; connected and connectedCount (1 or 0) follow the connection state of the handle's own port
