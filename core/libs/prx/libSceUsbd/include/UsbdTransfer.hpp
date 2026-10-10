#ifndef CORE_LIBS_PRX_LIBSCEUSBD_INCLUDE_USBDTRANSFER_HPP
#define CORE_LIBS_PRX_LIBSCEUSBD_INCLUDE_USBDTRANSFER_HPP

#include "prx/libSceUsbd/include/Usbd.hpp"

namespace Usbd {

void RegisterTransferCallback(UsbdTransfer* transfer, UsbdTransferCallback callback, void* userData);
void UnregisterTransferCallback(UsbdTransfer* transfer);

}

#endif
