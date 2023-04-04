#pragma once
#include <R2/VKEnums.hpp>
#include <volk.h>

namespace R2::VK
{
    inline bool hasAccessBit(AccessFlags flags, AccessFlags test)
    {
        return ((uint64_t)flags & (uint64_t)test) == (uint64_t)test;
    }

    inline bool hasStageBit(PipelineStageFlags flags, PipelineStageFlags test)
    {
        return ((uint64_t)flags & (uint64_t)test) == (uint64_t)test;
    }

    inline VkAccessFlags getOldAccessFlags(AccessFlags access)
    {
        VkAccessFlags flagBits = 0;
        flagBits = (VkAccessFlagBits)((uint64_t)(access) & 0xFFFFFFFF);

        if (hasAccessBit(access, AccessFlags::ShaderSampledRead)  ||
            hasAccessBit(access, AccessFlags::ShaderStorageRead))
        {
            flagBits |= VK_ACCESS_SHADER_READ_BIT;
        }

        if (hasAccessBit(access, AccessFlags::ShaderStorageWrite))
        {
            flagBits |= VK_ACCESS_SHADER_WRITE_BIT;
        }

        return flagBits;
    }

    inline VkPipelineStageFlags getOldPipelineStageFlags(PipelineStageFlags flags)
    {
        VkPipelineStageFlags oldFlags = 0;
        oldFlags = (VkPipelineStageFlags)((uint64_t)(flags) & 0xFFFFFFFF);

        if (hasStageBit(flags, PipelineStageFlags::Copy) ||
            hasStageBit(flags, PipelineStageFlags::Resolve) ||
            hasStageBit(flags, PipelineStageFlags::Blit) ||
            hasStageBit(flags, PipelineStageFlags::Clear))
        {
            oldFlags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        }

        if (hasStageBit(flags, PipelineStageFlags::IndexInput) ||
            hasStageBit(flags, PipelineStageFlags::VertexAttributeInput))
        {
            oldFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        }

        return oldFlags;
    }
}