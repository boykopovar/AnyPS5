#include <hsa/hsa.h>
#include <hsa/hsa_ext_amd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { hsa_status_t s_ = (x); if (s_ != HSA_STATUS_SUCCESS && s_ != HSA_STATUS_INFO_BREAK) { const char* m_; hsa_status_string(s_, &m_); fprintf(stderr, "%s: %s\n", #x, m_); exit(1); } } while (0)

static hsa_agent_t gpu;
static int haveGpu;
static hsa_amd_memory_pool_t kernargPool, finePool;
static int haveKernarg, haveFine;
static hsa_amd_memory_pool_t coarsePool;
static int haveCoarse;

static hsa_status_t findCoarse(hsa_amd_memory_pool_t pool, void* data) {
    hsa_amd_segment_t segment;
    hsa_amd_memory_pool_get_info(pool, HSA_AMD_MEMORY_POOL_INFO_SEGMENT, &segment);
    if (segment != HSA_AMD_SEGMENT_GLOBAL) return HSA_STATUS_SUCCESS;
    uint32_t flags;
    hsa_amd_memory_pool_get_info(pool, HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS, &flags);
    if ((flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED) && !haveCoarse) { coarsePool = pool; haveCoarse = 1; }
    return HSA_STATUS_SUCCESS;
}

static hsa_status_t findGpu(hsa_agent_t agent, void* data) {
    hsa_device_type_t type;
    hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &type);
    if (type == HSA_DEVICE_TYPE_GPU) { gpu = agent; haveGpu = 1; return HSA_STATUS_INFO_BREAK; }
    return HSA_STATUS_SUCCESS;
}

static hsa_status_t findPools(hsa_amd_memory_pool_t pool, void* data) {
    hsa_amd_segment_t segment;
    hsa_amd_memory_pool_get_info(pool, HSA_AMD_MEMORY_POOL_INFO_SEGMENT, &segment);
    if (segment != HSA_AMD_SEGMENT_GLOBAL) return HSA_STATUS_SUCCESS;
    uint32_t flags;
    hsa_amd_memory_pool_get_info(pool, HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS, &flags);
    if ((flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_KERNARG_INIT) && !haveKernarg) { kernargPool = pool; haveKernarg = 1; }
    if ((flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED) && !haveFine) { finePool = pool; haveFine = 1; }
    return HSA_STATUS_SUCCESS;
}

static hsa_status_t findCpu(hsa_agent_t agent, void* data) {
    hsa_device_type_t type;
    hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &type);
    if (type == HSA_DEVICE_TYPE_CPU) { hsa_amd_agent_iterate_memory_pools(agent, findPools, NULL); }
    return HSA_STATUS_SUCCESS;
}

static void* readFile(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END); *size = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    void* data = malloc(*size);
    if (fread(data, 1, *size, f) != *size) { perror(path); exit(1); }
    fclose(f);
    return data;
}

int main(int argc, char** argv) {
    if (argc == 2 && strcmp(argv[1], "--target") == 0) {
        CHECK(hsa_init());
        CHECK(hsa_iterate_agents(findGpu, NULL));
        if (!haveGpu) { fprintf(stderr, "no GPU agent\n"); return 1; }
        char name[64] = {0};
        CHECK(hsa_agent_get_info(gpu, HSA_AGENT_INFO_NAME, name));
        printf("%s\n", name);
        return 0;
    }
    if (argc != 6) { fprintf(stderr, "usage: oracle --target | oracle code.o threads in.bin out.bin out_dwords\n"); return 2; }
    const uint32_t threads = (uint32_t)atoi(argv[2]);
    const uint32_t outDwords = (uint32_t)atoi(argv[5]);
    CHECK(hsa_init());
    CHECK(hsa_iterate_agents(findGpu, NULL));
    if (!haveGpu) { fprintf(stderr, "no GPU agent\n"); return 1; }
    CHECK(hsa_iterate_agents(findCpu, NULL));
    if (!haveKernarg || !haveFine) { fprintf(stderr, "no system memory pools\n"); return 1; }

    size_t codeSize; void* code = readFile(argv[1], &codeSize);
    hsa_code_object_reader_t reader;
    CHECK(hsa_code_object_reader_create_from_memory(code, codeSize, &reader));
    hsa_executable_t exe;
    CHECK(hsa_executable_create_alt(HSA_PROFILE_FULL, HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT, NULL, &exe));
    CHECK(hsa_executable_load_agent_code_object(exe, gpu, reader, NULL, NULL));
    CHECK(hsa_executable_freeze(exe, NULL));
    hsa_executable_symbol_t sym;
    CHECK(hsa_executable_get_symbol_by_name(exe, "probe.kd", &gpu, &sym));
    uint64_t kernelObject; uint32_t kernargSize, groupSize, privateSize;
    hsa_executable_symbol_get_info(sym, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &kernelObject);
    hsa_executable_symbol_get_info(sym, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_SIZE, &kernargSize);
    hsa_executable_symbol_get_info(sym, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE, &groupSize);
    hsa_executable_symbol_get_info(sym, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE, &privateSize);

    size_t inSize; void* inHost = readFile(argv[3], &inSize);
    void *in, *out, *args;
    const size_t outSize = (size_t)threads * outDwords * 4u;
    const int coarse = getenv("COARSE") != NULL;
    if (coarse) {
        CHECK(hsa_amd_agent_iterate_memory_pools(gpu, findCoarse, NULL));
        if (!haveCoarse) { fprintf(stderr, "no coarse-grained GPU pool\n"); return 1; }
    }
    void *inStage = NULL, *outStage = NULL;
    CHECK(hsa_amd_memory_pool_allocate(coarse ? coarsePool : finePool, inSize, 0, &in));
    CHECK(hsa_amd_memory_pool_allocate(coarse ? coarsePool : finePool, outSize, 0, &out));
    if (coarse) {
        CHECK(hsa_amd_memory_pool_allocate(finePool, inSize, 0, &inStage));
        CHECK(hsa_amd_memory_pool_allocate(finePool, outSize, 0, &outStage));
        CHECK(hsa_amd_agents_allow_access(1, &gpu, NULL, inStage));
        CHECK(hsa_amd_agents_allow_access(1, &gpu, NULL, outStage));
    }
    CHECK(hsa_amd_memory_pool_allocate(kernargPool, 16, 0, &args));
    if (!coarse) {
        CHECK(hsa_amd_agents_allow_access(1, &gpu, NULL, in));
        CHECK(hsa_amd_agents_allow_access(1, &gpu, NULL, out));
    }
    CHECK(hsa_amd_agents_allow_access(1, &gpu, NULL, args));
    if (coarse) {
        memcpy(inStage, inHost, inSize);
        memset(outStage, 0xee, outSize);
        CHECK(hsa_memory_copy(in, inStage, inSize));
        CHECK(hsa_memory_copy(out, outStage, outSize));
    } else {
        memcpy(in, inHost, inSize);
        memset(out, 0xee, outSize);
    }
    ((void**)args)[0] = in;
    ((void**)args)[1] = out;

    hsa_queue_t* queue;
    CHECK(hsa_queue_create(gpu, 64, HSA_QUEUE_TYPE_SINGLE, NULL, NULL, UINT32_MAX, UINT32_MAX, &queue));
    hsa_signal_t done;
    CHECK(hsa_signal_create(1, 0, NULL, &done));
    const uint64_t index = hsa_queue_add_write_index_relaxed(queue, 1);
    hsa_kernel_dispatch_packet_t* packet = (hsa_kernel_dispatch_packet_t*)queue->base_address + (index & (queue->size - 1));
    memset((uint8_t*)packet + 4, 0, sizeof(*packet) - 4);
    packet->setup = 1u << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
    packet->workgroup_size_x = (uint16_t)(threads < 1024 ? threads : 1024);
    packet->workgroup_size_y = 1;
    packet->workgroup_size_z = 1;
    packet->grid_size_x = threads;
    packet->grid_size_y = 1;
    packet->grid_size_z = 1;
    packet->kernel_object = kernelObject;
    packet->kernarg_address = args;
    packet->private_segment_size = privateSize;
    packet->group_segment_size = groupSize;
    packet->completion_signal = done;
    const uint16_t header = (HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE) |
        (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) | (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);
    __atomic_store_n((uint32_t*)packet, header | ((uint32_t)packet->setup << 16), __ATOMIC_RELEASE);
    hsa_signal_store_screlease(queue->doorbell_signal, (hsa_signal_value_t)index);
    if (hsa_signal_wait_scacquire(done, HSA_SIGNAL_CONDITION_LT, 1, 5000000000ull, HSA_WAIT_STATE_BLOCKED) != 0) {
        fprintf(stderr, "kernel timed out\n");
        return 1;
    }
    if (coarse) {
        CHECK(hsa_memory_copy(outStage, out, outSize));
        out = outStage;
    }
    FILE* f = fopen(argv[4], "wb");
    if (!f || fwrite(out, 1, outSize, f) != outSize) { perror(argv[4]); return 1; }
    fclose(f);
    return 0;
}
