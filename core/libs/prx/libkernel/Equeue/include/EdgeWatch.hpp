#ifndef CORE_LIBS_PRX_LIBKERNEL_EQUEUE_EDGEWATCH_HPP
#define CORE_LIBS_PRX_LIBKERNEL_EQUEUE_EDGEWATCH_HPP

#include <cstdint>
#include <memory>

namespace EdgeWatch {

class Watch {
public:
    static std::shared_ptr<Watch> Open(std::intptr_t hostDescriptor, bool write);

    explicit Watch(int descriptor) : m_descriptor(descriptor) {}
    ~Watch();
    Watch(const Watch&) = delete;
    Watch& operator=(const Watch&) = delete;

    bool TakeEdge();
    int Descriptor() const { return m_descriptor; }

    bool armed = true;

private:
    int m_descriptor;
};

}

#endif
