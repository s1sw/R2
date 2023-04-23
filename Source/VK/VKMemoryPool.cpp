#include <R2/VKMemoryPool.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <R2/VK.hpp>

#include "R2/VKDeletionQueue.hpp"

namespace R2::VK
{
    MemoryPool::MemoryPool(Core* core, const MemoryPoolCreateInfo& createInfo)
        : core(core)
    {
        VmaPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.blockSize = createInfo.BlockSize;
        poolCreateInfo.minBlockCount = createInfo.MinBlockCount;
        poolCreateInfo.maxBlockCount = createInfo.MaxBlockCount;
        
        if (createInfo.IgnoreBufferImageGranularity)
        {
            poolCreateInfo.flags = VMA_POOL_CREATE_IGNORE_BUFFER_IMAGE_GRANULARITY_BIT;
        }
        
        VKCHECK(vmaCreatePool(core->GetHandles()->Allocator, &poolCreateInfo, &pool));
    }

    MemoryPoolStats MemoryPool::GetStats() const
    {
        VmaStatistics stats{};
        vmaGetPoolStatistics(core->GetHandles()->Allocator, pool, &stats);
        return {
            .BlockCount = stats.blockCount,
            .AllocationCount = stats.allocationCount,
            .BlockBytes = stats.blockBytes,
            .AllocationBytes = stats.allocationBytes
        };
    }

    MemoryPool::~MemoryPool()
    {
        const Handles* handles = core->GetHandles();
        DeletionQueue* dq;
        dq = core->perFrameResources[core->GetFrameIndex()].DeletionQueue;
        DQ_QueuePoolDeletion(dq, pool);
    }
}
