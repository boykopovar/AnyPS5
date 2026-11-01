#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VK_ENABLE_BETA_EXTENSIONS 1
#include <vulkan/vulkan.h>

static int fail(VkResult result, const char* operation) {
    if (result == VK_SUCCESS) return 0;
    fprintf(stderr, "%s failed with VkResult %d\n", operation, result);
    return 1;
}

static int has_instance_extension(const char* name) {
    uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(NULL, &count, NULL) != VK_SUCCESS) return 0;
    VkExtensionProperties* extensions = calloc(count, sizeof(*extensions));
    if (extensions == NULL) return 0;
    int found = 0;
    if (vkEnumerateInstanceExtensionProperties(NULL, &count, extensions) == VK_SUCCESS) {
        for (uint32_t i = 0; i < count; ++i) found |= strcmp(name, extensions[i].extensionName) == 0;
    }
    free(extensions);
    return found;
}

static int has_device_extension(VkPhysicalDevice device, const char* name) {
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL) != VK_SUCCESS) return 0;
    VkExtensionProperties* extensions = calloc(count, sizeof(*extensions));
    if (extensions == NULL) return 0;
    int found = 0;
    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, extensions) == VK_SUCCESS) {
        for (uint32_t i = 0; i < count; ++i) found |= strcmp(name, extensions[i].extensionName) == 0;
    }
    free(extensions);
    return found;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: subgroup_barrier_pipeline <shader.spv>\n");
        return 2;
    }

    FILE* input = fopen(argv[1], "rb");
    if (input == NULL || fseek(input, 0, SEEK_END) != 0) {
        fprintf(stderr, "cannot open SPIR-V input\n");
        return 2;
    }
    long file_size = ftell(input);
    if (file_size < 20 || (file_size % 4) != 0 || fseek(input, 0, SEEK_SET) != 0) {
        fprintf(stderr, "invalid SPIR-V file size\n");
        fclose(input);
        return 2;
    }
    uint32_t* code = malloc((size_t)file_size);
    if (code == NULL || fread(code, 1, (size_t)file_size, input) != (size_t)file_size) {
        fprintf(stderr, "cannot read SPIR-V input\n");
        free(code);
        fclose(input);
        return 2;
    }
    fclose(input);
    if (code[0] != 0x07230203u) {
        fprintf(stderr, "unexpected SPIR-V magic 0x%08x\n", code[0]);
        free(code);
        return 2;
    }

    const char* instance_extensions[1];
    uint32_t instance_extension_count = 0;
    VkInstanceCreateFlags flags = 0;
    if (has_instance_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        instance_extensions[instance_extension_count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    VkApplicationInfo application = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "AnyPS5 subgroup-barrier MoltenVK smoke test",
        .apiVersion = VK_API_VERSION_1_1,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = flags,
        .pApplicationInfo = &application,
        .enabledExtensionCount = instance_extension_count,
        .ppEnabledExtensionNames = instance_extensions,
    };
    VkInstance instance = VK_NULL_HANDLE;
    if (fail(vkCreateInstance(&instance_info, NULL, &instance), "vkCreateInstance")) return 1;

    uint32_t device_count = 0;
    if (fail(vkEnumeratePhysicalDevices(instance, &device_count, NULL), "vkEnumeratePhysicalDevices(count)")) return 1;
    if (device_count == 0) {
        fprintf(stderr, "no Vulkan physical device found\n");
        return 1;
    }
    VkPhysicalDevice* devices = calloc(device_count, sizeof(*devices));
    if (devices == NULL || fail(vkEnumeratePhysicalDevices(instance, &device_count, devices), "vkEnumeratePhysicalDevices")) return 1;
    VkPhysicalDevice physical_device = devices[0];
    free(devices);

    const char* device_extensions[1];
    uint32_t device_extension_count = 0;
    if (has_device_extension(physical_device, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
        device_extensions[device_extension_count++] = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
    }
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_families = calloc(queue_family_count, sizeof(*queue_families));
    if (queue_families == NULL || queue_family_count == 0) {
        fprintf(stderr, "no Vulkan queue family found\n");
        return 1;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);
    uint32_t compute_family = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if ((queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
            compute_family = i;
            break;
        }
    }
    free(queue_families);
    if (compute_family == UINT32_MAX) {
        fprintf(stderr, "no Vulkan compute queue family found\n");
        return 1;
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = compute_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = device_extension_count,
        .ppEnabledExtensionNames = device_extensions,
    };
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    VkDevice device = VK_NULL_HANDLE;
    if (fail(vkCreateDevice(physical_device, &device_info, NULL, &device), "vkCreateDevice")) {
        fprintf(stderr, "device: %s\n", properties.deviceName);
        return 1;
    }

    VkShaderModuleCreateInfo shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = (size_t)file_size,
        .pCode = code,
    };
    VkShaderModule shader = VK_NULL_HANDLE;
    VkResult shader_result = vkCreateShaderModule(device, &shader_info, NULL, &shader);
    free(code);
    if (fail(shader_result, "vkCreateShaderModule")) return 1;

    VkPipelineLayoutCreateInfo layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (fail(vkCreatePipelineLayout(device, &layout_info, NULL, &layout), "vkCreatePipelineLayout")) return 1;

    VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main",
        },
        .layout = layout,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (fail(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline), "vkCreateComputePipelines")) {
        fprintf(stderr, "device: %s\n", properties.deviceName);
        return 1;
    }
    printf("subgroup barrier pipeline compiled on %s\n", properties.deviceName);

    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, layout, NULL);
    vkDestroyShaderModule(device, shader, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    return 0;
}
