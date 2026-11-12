#include "prx/libSceAgcDriver/Graphics/include/QueueSubmitter.hpp"

namespace AgcDriver::Graphics {

QueueAccess::QueueAccess(VkQueue) : submitter(nullptr) {}

QueueAccess::~QueueAccess() = default;

}
