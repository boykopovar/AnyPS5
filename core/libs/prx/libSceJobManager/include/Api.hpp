#pragma once

#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

struct MemorySizeQueryParams {
    std::uint32_t version;
    std::uint32_t workers;
    std::uint32_t sequences;
    std::uint32_t invocations;
    std::uint32_t dispatches;
    std::uint32_t syncPoints;
    std::uint32_t waits;
    std::uint32_t computeSequences;
    std::uint64_t computeQueueMask;
};

struct ConfigParams {
    std::uint32_t version;
    std::uint32_t threadFlags;
    std::uint32_t stackSize;
    std::uint32_t priority;
    const char* threadName;
    std::uint32_t spinMicroseconds;
    std::uint32_t computeAffinity;
    std::uint32_t computePriority;
    std::uint32_t reserved;
};

using JobEntry = void (APS5_VABI*)(void* argument, std::uint32_t index);

class ISequence {
public:
    APS5_VABI virtual int Dispatch(JobEntry entry, void* argument, std::uint32_t count, const char* name) = 0;
    APS5_VABI virtual int SyncPoint() = 0;
    APS5_VABI virtual int Wait() = 0;
    APS5_VABI virtual void CompleteDestructor() {}
    APS5_VABI virtual void DeletingDestructor() { throw std::runtime_error("libSceJobManager: a sequence was deleted by its caller"); }
    APS5_VABI virtual int DispatchCompute(const void* shader, void* userData, std::int32_t x, std::int32_t y, std::int32_t z, const char* name) = 0;
    APS5_VABI virtual int DispatchComputeIndirect(const void* shader, void* userData, const void* indirectArguments, const char* name) = 0;
};

class ISequenceFactory {
public:
    APS5_VABI virtual int Create(ISequence** out, std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits, const char* name) = 0;
    APS5_VABI virtual int Destroy(ISequence* sequence) = 0;
    APS5_VABI virtual void CompleteDestructor() {}
    APS5_VABI virtual void DeletingDestructor() { throw std::runtime_error("libSceJobManager: a sequence factory was deleted by its caller"); }
    APS5_VABI virtual int CreateCompute(ISequence** out, std::uint32_t invocations, std::uint32_t dispatches, std::uint32_t syncPoints, std::uint32_t waits, const char* name) = 0;
    APS5_VABI virtual int DestroyCompute(ISequence* sequence) = 0;
};

extern "C" {

void APS5_VABI _ZN3sce3Job10JobManagerC1Ev(void* self);
void APS5_VABI _ZN3sce3Job10JobManagerC2Ev(void* self);
void APS5_VABI _ZN3sce3Job10JobManagerD1Ev(void* self);
void APS5_VABI _ZN3sce3Job10JobManagerD2Ev(void* self);
std::uint64_t APS5_VABI _ZN3sce3Job10JobManager27calculateRequiredMemorySizeEPKNS1_21MemorySizeQueryParamsE(const MemorySizeQueryParams* params);
int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(void* self, void* memory, std::uint64_t memorySize, std::uint32_t workers, std::uint32_t sequences, std::uint32_t computeSequences, std::uint64_t computeQueueMask, const ConfigParams* config);
int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjPKNS1_12ConfigParamsE(void* self, void* memory, std::uint64_t memorySize, std::uint32_t workers, std::uint32_t sequences, const ConfigParams* config);
int APS5_VABI _ZN3sce3Job10JobManager8shutdownEv(void* self);
ISequenceFactory* APS5_VABI _ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE(void* self, std::uint32_t priority);
ISequenceFactory* APS5_VABI _ZN3sce3Job10JobManager34getUnifiedSequenceFactoryInterfaceENS1_8PriorityE(void* self, std::uint32_t priority);
int APS5_VABI _ZN3sce3Job10JobManager24setPersistentThreadCountEj(void* self, std::uint32_t count);

}
