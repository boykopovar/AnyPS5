#include "prx/libSceAgcDriver/Execution/include/ReadTrapsOverHostPointerImports.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/udmabuf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t Pages = 64;
constexpr std::size_t TrappedPage = 8;
constexpr std::size_t WrittenPage = 32;

struct Skip : std::runtime_error {
    using std::runtime_error::runtime_error;
};

#ifdef _WIN32
constexpr const char* VulkanLibrary = "vulkan-1.dll";

void* OpenVulkan() {
    return reinterpret_cast<void*>(LoadLibraryA(VulkanLibrary));
}

void* VulkanSymbol(void* library, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library), name));
}

void CloseVulkan(void* library) {
    FreeLibrary(static_cast<HMODULE>(library));
}

std::size_t PageBytes() {
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    return system.dwPageSize;
}

void* MapPrivate(std::size_t bytes) {
    return VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void UnmapPrivate(void* mapping, std::size_t) {
    VirtualFree(mapping, 0, MEM_RELEASE);
}

bool SetAccessible(void* pointer, std::size_t bytes, bool accessible) {
    DWORD previous = 0;
    return VirtualProtect(pointer, bytes, accessible ? PAGE_READWRITE : PAGE_NOACCESS, &previous) != 0;
}
#else
constexpr const char* VulkanLibrary = "libvulkan.so.1";

void* OpenVulkan() {
    return ::dlopen(VulkanLibrary, RTLD_NOW | RTLD_LOCAL);
}

void* VulkanSymbol(void* library, const char* name) {
    return ::dlsym(library, name);
}

void CloseVulkan(void* library) {
    ::dlclose(library);
}

std::size_t PageBytes() {
    return static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
}

void* MapPrivate(std::size_t bytes) {
    void* mapping = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return mapping == MAP_FAILED ? nullptr : mapping;
}

void UnmapPrivate(void* mapping, std::size_t bytes) {
    ::munmap(mapping, bytes);
}

bool SetAccessible(void* pointer, std::size_t bytes, bool accessible) {
    return ::mprotect(pointer, bytes, accessible ? PROT_READ | PROT_WRITE : PROT_NONE) == 0;
}
#endif

void Check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(what) + " returned " + std::to_string(static_cast<int>(result)));
}

class Gpu {
public:
    Gpu() {
        library = OpenVulkan();
        if (library == nullptr) throw Skip(std::string("cannot load ") + VulkanLibrary);
        instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(VulkanSymbol(library, "vkGetInstanceProcAddr"));
        if (instanceProc == nullptr) throw Skip(std::string(VulkanLibrary) + " has no vkGetInstanceProcAddr");
        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.pApplicationInfo = &application;
        if (instanceFunction<PFN_vkCreateInstance>("vkCreateInstance")(&info, nullptr, &instance) != VK_SUCCESS) throw Skip("vkCreateInstance failed");
        std::uint32_t count = 0;
        const auto enumerate = instanceFunction<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
        Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
        std::vector<VkPhysicalDevice> devices(count);
        Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
        const char* requested = std::getenv("ANYPS5_GPU");
        const std::string_view request = requested != nullptr ? requested : "";
        const auto properties = instanceFunction<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
        std::string chosen;
        bool chosenCpu = false;
        for (const auto candidate : devices) {
            VkPhysicalDeviceProperties described{};
            properties(candidate, &described);
            if (described.apiVersion < VK_API_VERSION_1_2) continue;
            if (!request.empty() && std::string_view(described.deviceName).find(request) == std::string_view::npos) continue;
            const bool cpu = described.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
            if (physical != VK_NULL_HANDLE && (cpu || !chosenCpu)) continue;
            physical = candidate;
            chosen = described.deviceName;
            chosenCpu = cpu;
        }
        if (physical == VK_NULL_HANDLE) throw Skip(request.empty() ? "no usable Vulkan device" : "no usable Vulkan device matches ANYPS5_GPU");
        std::printf("device: %s\n", chosen.c_str());
        VkPhysicalDeviceDriverProperties driverProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT, &driverProperties};
        VkPhysicalDeviceProperties2 extended{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &hostProperties};
        instanceFunction<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(physical, &extended);
        driver = driverProperties.driverID;
        const auto extensionList = instanceFunction<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
        Check(extensionList(physical, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties");
        std::vector<VkExtensionProperties> available(count);
        Check(extensionList(physical, nullptr, &count, available.data()), "vkEnumerateDeviceExtensionProperties");
        const auto has = [&](const char* name) { return std::any_of(available.begin(), available.end(), [&](const VkExtensionProperties& item) { return std::strcmp(item.extensionName, name) == 0; }); };
        hostPointer = has(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
        dmaBuf = has(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME) && has(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
        hostAlignment = hostPointer ? hostProperties.minImportedHostPointerAlignment : 0;
        std::vector<const char*> enabled;
        if (hostPointer) enabled.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
        if (dmaBuf) {
            enabled.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
            enabled.push_back(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
        }
        const auto queues = instanceFunction<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        queues(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        queues(physical, &count, families.data());
        while (family < count && (families[family].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)) == 0) ++family;
        if (family == count) throw Skip("no queue that can fill a buffer");
        const float priority = 1;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabled.size());
        deviceInfo.ppEnabledExtensionNames = enabled.data();
        Check(instanceFunction<PFN_vkCreateDevice>("vkCreateDevice")(physical, &deviceInfo, nullptr, &device), "vkCreateDevice");
        deviceProc = instanceFunction<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
        deviceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(device, family, 0, &queue);
        instanceFunction<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(physical, &memory);
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = family;
        Check(deviceFunction<PFN_vkCreateCommandPool>("vkCreateCommandPool")(device, &poolInfo, nullptr, &pool), "vkCreateCommandPool");
    }

    ~Gpu() {
        if (pool != VK_NULL_HANDLE) deviceFunction<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(device, pool, nullptr);
        if (device != VK_NULL_HANDLE) instanceFunction<PFN_vkDestroyDevice>("vkDestroyDevice")(device, nullptr);
        if (instance != VK_NULL_HANDLE) instanceFunction<PFN_vkDestroyInstance>("vkDestroyInstance")(instance, nullptr);
        if (library != nullptr) CloseVulkan(library);
    }

    Gpu(const Gpu&) = delete;
    Gpu& operator=(const Gpu&) = delete;

    template <typename TFunction>
    TFunction instanceFunction(const char* name) const {
        const auto found = reinterpret_cast<TFunction>(instanceProc(instance, name));
        if (found == nullptr) throw std::runtime_error(std::string("missing ") + name);
        return found;
    }

    template <typename TFunction>
    TFunction deviceFunction(const char* name) const {
        const auto found = reinterpret_cast<TFunction>(deviceProc(device, name));
        if (found == nullptr) throw std::runtime_error(std::string("missing ") + name);
        return found;
    }

    std::uint32_t MemoryType(std::uint32_t allowed, VkMemoryPropertyFlags wanted) const {
        for (std::uint32_t index = 0; index < memory.memoryTypeCount; ++index) {
            if ((allowed & (1u << index)) != 0 && (memory.memoryTypes[index].propertyFlags & wanted) == wanted) return index;
        }
        for (std::uint32_t index = 0; index < memory.memoryTypeCount; ++index) {
            if ((allowed & (1u << index)) != 0) return index;
        }
        throw std::runtime_error("no memory type for the buffer");
    }

    VkResult Fill(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, std::uint32_t value) const {
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        Check(deviceFunction<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(device, &allocate, &commands), "vkAllocateCommandBuffers");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        Check(deviceFunction<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands, &begin), "vkBeginCommandBuffer");
        deviceFunction<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, buffer, offset, size, value);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        deviceFunction<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        Check(deviceFunction<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands), "vkEndCommandBuffer");
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fence = VK_NULL_HANDLE;
        Check(deviceFunction<PFN_vkCreateFence>("vkCreateFence")(device, &fenceInfo, nullptr, &fence), "vkCreateFence");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commands;
        VkResult result = deviceFunction<PFN_vkQueueSubmit>("vkQueueSubmit")(queue, 1, &submit, fence);
        if (result == VK_SUCCESS) result = deviceFunction<PFN_vkWaitForFences>("vkWaitForFences")(device, 1, &fence, VK_TRUE, 5'000'000'000ull);
        deviceFunction<PFN_vkDestroyFence>("vkDestroyFence")(device, fence, nullptr);
        deviceFunction<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(device, pool, 1, &commands);
        return result;
    }

    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    PFN_vkGetDeviceProcAddr deviceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memory{};
    VkDriverId driver{};
    VkDeviceSize hostAlignment = 0;
    std::uint32_t family = 0;
    bool hostPointer = false;
    bool dmaBuf = false;
};

struct BoundBuffer {
    const Gpu& gpu;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    explicit BoundBuffer(const Gpu& owner) : gpu(owner) {}
    BoundBuffer(const BoundBuffer&) = delete;
    BoundBuffer& operator=(const BoundBuffer&) = delete;
    ~BoundBuffer() {
        if (buffer != VK_NULL_HANDLE) gpu.deviceFunction<PFN_vkDestroyBuffer>("vkDestroyBuffer")(gpu.device, buffer, nullptr);
        if (memory != VK_NULL_HANDLE) gpu.deviceFunction<PFN_vkFreeMemory>("vkFreeMemory")(gpu.device, memory, nullptr);
    }

    void Create(VkDeviceSize bytes, const void* external) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, external};
        info.size = bytes;
        info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Check(gpu.deviceFunction<PFN_vkCreateBuffer>("vkCreateBuffer")(gpu.device, &info, nullptr, &buffer), "vkCreateBuffer");
    }

    VkResult Allocate(const VkMemoryAllocateInfo& allocate) {
        return gpu.deviceFunction<PFN_vkAllocateMemory>("vkAllocateMemory")(gpu.device, &allocate, nullptr, &memory);
    }

    void Bind(const VkMemoryAllocateInfo& allocate) {
        Check(Allocate(allocate), "vkAllocateMemory");
        Check(gpu.deviceFunction<PFN_vkBindBufferMemory>("vkBindBufferMemory")(gpu.device, buffer, memory, 0), "vkBindBufferMemory");
    }
};

void Require(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error(what);
}

void HeldTrapKeepsSubmitting(const Gpu& gpu, std::uint8_t* bytes, std::size_t page, BoundBuffer& imported, const char* kind) {
    const auto size = static_cast<VkDeviceSize>(Pages * page);
    Check(gpu.Fill(imported.buffer, 0, size, 0x22222222u), (std::string(kind) + ": fill of the whole import").c_str());
    Require(bytes[0] == 0x22 && bytes[TrappedPage * page] == 0x22, std::string(kind) + ": the import does not show the GPU's fill");
    BoundBuffer other(gpu);
    other.Create(page, nullptr);
    VkMemoryRequirements requirements{};
    gpu.deviceFunction<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(gpu.device, other.buffer, &requirements);
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = gpu.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    other.Bind(allocate);
    Require(SetAccessible(bytes + TrappedPage * page, page, false), std::string(kind) + ": cannot make a page of the import inaccessible");
    const VkResult unrelated = gpu.Fill(other.buffer, 0, page, 0x77777777u);
    const VkResult inside = unrelated == VK_SUCCESS ? gpu.Fill(imported.buffer, WrittenPage * page, page, 0x88888888u) : unrelated;
    Require(SetAccessible(bytes + TrappedPage * page, page, true), std::string(kind) + ": cannot restore the inaccessible page");
    Require(unrelated == VK_SUCCESS, std::string(kind) + ": a submission that does not touch the import failed with " + std::to_string(static_cast<int>(unrelated)) + " while a page of the import was inaccessible");
    Require(inside == VK_SUCCESS, std::string(kind) + ": a fill of another page of the import failed with " + std::to_string(static_cast<int>(inside)) + " while a page of the import was inaccessible");
    Require(bytes[WrittenPage * page] == 0x88 && bytes[TrappedPage * page] == 0x22, std::string(kind) + ": the fill while a page was inaccessible did not land where it was sent");
    std::printf("%s: submissions kept working while a page of the import was inaccessible\n", kind);
}

bool HostPointerImport(const Gpu& gpu) {
    const auto page = PageBytes();
    const auto alignment = std::max<std::size_t>(page, static_cast<std::size_t>(gpu.hostAlignment));
    const auto bytes = Pages * page;
    if (bytes % alignment != 0) return false;
    const auto reserved = bytes + alignment;
    void* mapping = MapPrivate(reserved);
    Require(mapping != nullptr, "cannot map memory for the host pointer import");
    auto* base = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(mapping) + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1));
    try {
        std::memset(base, 0x11, bytes);
        VkMemoryHostPointerPropertiesEXT pointer{VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
        Check(gpu.deviceFunction<PFN_vkGetMemoryHostPointerPropertiesEXT>("vkGetMemoryHostPointerPropertiesEXT")(gpu.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, base, &pointer), "vkGetMemoryHostPointerPropertiesEXT");
        BoundBuffer imported(gpu);
        const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT};
        imported.Create(bytes, &external);
        VkMemoryRequirements requirements{};
        gpu.deviceFunction<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(gpu.device, imported.buffer, &requirements);
        VkImportMemoryHostPointerInfoEXT import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT};
        import.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
        import.pHostPointer = base;
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &import};
        allocate.allocationSize = bytes;
        allocate.memoryTypeIndex = gpu.MemoryType(requirements.memoryTypeBits & pointer.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        imported.Bind(allocate);
        HeldTrapKeepsSubmitting(gpu, base, page, imported, "host pointer import");
    } catch (...) {
        UnmapPrivate(mapping, reserved);
        throw;
    }
    UnmapPrivate(mapping, reserved);
    return true;
}

#ifndef _WIN32
class Descriptor {
public:
    explicit Descriptor(int value) : file(value) {}
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    ~Descriptor() {
        if (file >= 0) ::close(file);
    }
    int Get() const { return file; }
    int Release() { return std::exchange(file, -1); }

private:
    int file;
};

bool DmaBufImport(const Gpu& gpu) {
    const auto page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
    const auto bytes = Pages * page;
    Descriptor device(::open("/dev/udmabuf", O_RDWR | O_CLOEXEC));
    if (device.Get() < 0) return false;
    Descriptor memfd(::memfd_create("read trap import", MFD_ALLOW_SEALING | MFD_CLOEXEC));
    Require(memfd.Get() >= 0, "memfd_create failed");
    Require(::ftruncate(memfd.Get(), static_cast<off_t>(bytes)) == 0, "cannot size the memfd");
    Require(::fcntl(memfd.Get(), F_ADD_SEALS, F_SEAL_SHRINK) == 0, "cannot seal the memfd");
    void* mapping = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, memfd.Get(), 0);
    Require(mapping != MAP_FAILED, "cannot map the memfd");
    auto* base = static_cast<std::uint8_t*>(mapping);
    try {
        std::memset(base, 0x11, bytes);
        udmabuf_create create{};
        create.memfd = static_cast<std::uint32_t>(memfd.Get());
        create.flags = UDMABUF_FLAGS_CLOEXEC;
        create.offset = 0;
        create.size = bytes;
        Descriptor exported(::ioctl(device.Get(), UDMABUF_CREATE, &create));
        Require(exported.Get() >= 0, "UDMABUF_CREATE failed");
        VkMemoryFdPropertiesKHR fdProperties{VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR};
        if (gpu.deviceFunction<PFN_vkGetMemoryFdPropertiesKHR>("vkGetMemoryFdPropertiesKHR")(gpu.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, exported.Get(), &fdProperties) != VK_SUCCESS || fdProperties.memoryTypeBits == 0) {
            ::munmap(mapping, bytes);
            return false;
        }
        BoundBuffer imported(gpu);
        const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
        imported.Create(bytes, &external);
        VkMemoryRequirements requirements{};
        gpu.deviceFunction<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(gpu.device, imported.buffer, &requirements);
        if ((requirements.memoryTypeBits & fdProperties.memoryTypeBits) == 0) {
            ::munmap(mapping, bytes);
            return false;
        }
        VkImportMemoryFdInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR};
        import.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        import.fd = exported.Get();
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &import};
        allocate.allocationSize = bytes;
        allocate.memoryTypeIndex = gpu.MemoryType(requirements.memoryTypeBits & fdProperties.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (imported.Allocate(allocate) != VK_SUCCESS) {
            imported.memory = VK_NULL_HANDLE;
            ::munmap(mapping, bytes);
            return false;
        }
        exported.Release();
        Check(gpu.deviceFunction<PFN_vkBindBufferMemory>("vkBindBufferMemory")(gpu.device, imported.buffer, imported.memory, 0), "vkBindBufferMemory");
        HeldTrapKeepsSubmitting(gpu, base, page, imported, "dma-buf import");
    } catch (...) {
        ::munmap(mapping, bytes);
        throw;
    }
    ::munmap(mapping, bytes);
    return true;
}
#endif

}

int main() {
    const char* required = std::getenv("ANYPS5_REQUIRE_VULKAN");
    const bool mustRun = required != nullptr && std::string_view(required) == "1";
    try {
        Gpu gpu;
        int ran = 0;
        if (!gpu.hostPointer || !AgcDriver::ReadTrapsOverHostPointerImportsUsable(gpu.driver)) {
            std::printf("host pointer import: not run, this driver keeps label read traps off host pointer imports\n");
        } else if (HostPointerImport(gpu)) {
            ++ran;
        } else {
            std::printf("host pointer import: not run, the device reports a host pointer import alignment larger than the test range\n");
        }
#ifdef _WIN32
        std::printf("dma-buf import: not run, Windows has no dma-buf imports\n");
#else
        if (gpu.dmaBuf && DmaBufImport(gpu)) {
            ++ran;
        } else {
            std::printf("dma-buf import: not run, no dma-buf import support, no access to /dev/udmabuf, or the device cannot import a udmabuf\n");
        }
#endif
        if (ran == 0) {
            std::printf("skipped, the device lacks an import kind that label read traps are set over\n");
            return 77;
        }
        return 0;
    } catch (const Skip& skip) {
        std::printf("%s, %s\n", mustRun ? "failed" : "skipped", skip.what());
        return mustRun ? 1 : 77;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "read trap import submit: %s\n", error.what());
        return 1;
    }
}
