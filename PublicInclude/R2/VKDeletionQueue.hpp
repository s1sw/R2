#pragma once
#include <stdint.h>
#include <vector>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)
VK_DEFINE_HANDLE(VkDevice)
VK_DEFINE_HANDLE(VkDescriptorPool)
VK_DEFINE_HANDLE(VkDescriptorSet)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    struct Handles;

    class DeletionQueue
    {
    public:
        DeletionQueue(const Handles* handles);
#ifdef DQ_TRACK_SOURCE
        void QueueObjectDeletion(void* object, uint32_t type, int line, const char* file);
        void QueueMemoryFree(VmaAllocation allocation, int line, const char* file);
        void QueueDescriptorSetFree(VkDescriptorPool dPool, VkDescriptorSet ds, int line, const char* file);
#else
        void QueueObjectDeletion(void* object, uint32_t type);
        void QueueMemoryFree(VmaAllocation allocation);
        void QueueDescriptorSetFree(VkDescriptorPool dPool, VkDescriptorSet ds);
#endif
        void Cleanup();
    private:
        const Handles* handles;

        struct ObjectDeletion
        {
            void* object;
            uint32_t type;
#ifdef DQ_TRACK_SOURCE
            int line;
            const char* file;
#endif
        };

        struct MemoryFree
        {
            VmaAllocation allocation;
#ifdef DQ_TRACK_SOURCE
            int line;
            const char* file;
#endif
        };

        struct DescriptorSetFree
        {
            VkDescriptorPool desciptorPool;
            VkDescriptorSet descriptorSet;
#ifdef DQ_TRACK_SOURCE
            int line;
            const char* file;
#endif
        };

        std::vector<ObjectDeletion> objectDeletions;
        std::vector<MemoryFree> memoryFrees;
        std::vector<DescriptorSetFree> dsFrees;

        void processObjectDeletion(const ObjectDeletion& od);
        void processMemoryFree(const MemoryFree& mf);
    };

#ifdef DQ_TRACK_SOURCE
    #define DQ_QueueObjectDeletion(queue, object, type) queue->QueueObjectDeletion(object, type, __LINE__, __FILE__)
    #define DQ_QueueMemoryFree(queue, allocation) queue->QueueMemoryFree(allocation, __LINE__, __FILE__)
    #define DQ_QueueDescriptorSetFree(queue, pool, set) queue->QueueDescriptorSetFree(pool, set, __LINE__, __FILE__)
#else
    #define DQ_QueueObjectDeletion(queue, object, type) queue->QueueObjectDeletion(object, type)
    #define DQ_QueueMemoryFree(queue, allocation) queue->QueueMemoryFree(allocation)
    #define DQ_QueueDescriptorSetFree(queue, pool, set) queue->QueueDescriptorSetFree(pool, set)
#endif
}