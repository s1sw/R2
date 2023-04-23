#pragma once
#include <stdint.h>
#include <stddef.h>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VmaPool)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    class Core;
    
    struct MemoryPoolCreateInfo
    {
        size_t BlockSize;
        size_t MinBlockCount;
        size_t MaxBlockCount;
        bool IgnoreBufferImageGranularity;
    };

    struct MemoryPoolStats
    {
        uint32_t BlockCount;
        uint32_t AllocationCount;
        size_t BlockBytes;
        size_t AllocationBytes;
    };
    
    class MemoryPool
    {
    public:
        MemoryPool(Core* core, const MemoryPoolCreateInfo& createInfo);
        [[nodiscard]] VmaPool GetNativeHandle() const { return pool; }
        MemoryPoolStats GetStats() const;
        ~MemoryPool();
    private:
        Core* core;
        VmaPool pool;
    };
}