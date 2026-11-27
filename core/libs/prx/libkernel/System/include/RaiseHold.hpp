#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_RAISEHOLD_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_RAISEHOLD_HPP

#include <cstdint>

bool HoldRaisedException(std::int64_t thread, int guest);
bool HoldBlockedDelivery(int guest);
void DeliverHeldException(int guest);

#endif
