#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_FAULTSIGNALS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_FAULTSIGNALS_HPP

#include <cstdint>

bool SiginfoFaultHandlerInstalled(int guest);
void RunSiginfoFaultHandler(int guest, std::int32_t code, std::uintptr_t address, std::uint32_t trap, void* ucontext);
void InstallSiginfoFaultDelivery();

#endif
