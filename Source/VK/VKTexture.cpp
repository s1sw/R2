#include <R2/VKTexture.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKDeletionQueue.hpp>
#include <R2/VKEnums.hpp>
#include <R2/VKUtil.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>
#include <assert.h>
#include <math.h>
#ifndef __ANDROID__
#define USE_SYNC_2
#endif

namespace R2::VK
{
    TextureBlockInfo GetTextureBlockInfo(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::BC1_RGB_UNORM_BLOCK:
        case TextureFormat::BC1_RGB_SRGB_BLOCK:
        case TextureFormat::BC1_RGBA_UNORM_BLOCK:
        case TextureFormat::BC1_RGBA_SRGB_BLOCK:
            return TextureBlockInfo{4, 4, 8};
        case TextureFormat::BC3_SRGB_BLOCK:
        case TextureFormat::BC3_UNORM_BLOCK:
        case TextureFormat::BC5_UNORM_BLOCK:
        case TextureFormat::BC5_SNORM_BLOCK:
            return TextureBlockInfo{4, 4, 16};
        case TextureFormat::ASTC_6x6_SRGB_BLOCK:
        case TextureFormat::ASTC_6x6_UNORM_BLOCK:
            return TextureBlockInfo{6, 6, 16};
        case TextureFormat::B10G11R11_UFLOAT_PACK32:
            return TextureBlockInfo{1, 1, 4};
        case TextureFormat::R16G16B16A16_SFLOAT:
            return TextureBlockInfo{1, 1, 8};
        case TextureFormat::R32G32B32A32_SFLOAT:
            return TextureBlockInfo{1, 1, 16}; // chonky!
        default:
            return TextureBlockInfo{1, 1, 1};
        }
    }

    uint64_t CalculateTextureByteSize(TextureFormat format, uint32_t width, uint32_t height)
    {
        TextureBlockInfo blockInfo = GetTextureBlockInfo(format);

        uint32_t blocksX = (width + blockInfo.BlockWidth - 1) / blockInfo.BlockWidth;
        uint32_t blocksY = (height + blockInfo.BlockHeight - 1) / blockInfo.BlockHeight;
        return blockInfo.BytesPerBlock * blocksX * blocksY;
    }
    
    VkImageType convertType(TextureDimension dim)
    {
        switch (dim)
        {
        // "Cube" textures are just a special case of 2D textures
        // in Vulkan
        case TextureDimension::Cube:
        case TextureDimension::ArrayCube:
        case TextureDimension::Array2D:
        case TextureDimension::Dim2D:
        default:
            return VK_IMAGE_TYPE_2D;
        case TextureDimension::Dim3D:
            return VK_IMAGE_TYPE_3D;
        }
    }

    VkImageViewType convertViewType(TextureDimension dim)
    {
        switch (dim)
        {
        case TextureDimension::Cube:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        case TextureDimension::ArrayCube:
            return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        case TextureDimension::Dim2D:
        default:
            return VK_IMAGE_VIEW_TYPE_2D;
        case TextureDimension::Array2D:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureDimension::Dim3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        }
    }

    bool supportsStorage(VkPhysicalDevice physicalDevice, TextureFormat format)
    {
        VkImageFormatProperties formatProps;
        VkResult result = vkGetPhysicalDeviceImageFormatProperties(
            physicalDevice, (VkFormat)format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT, 0, &formatProps
        );

        return result != VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    bool isDimensionCube(TextureDimension dim)
    {
        return dim == TextureDimension::Cube || dim == TextureDimension::ArrayCube;
    }

    bool isFormatDepth(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::D32_SFLOAT:
        case TextureFormat::D16_UNORM:
            return true;
        }

        return false;
    }

    void TextureCreateInfo::SetFullMipChain()
    {
        int biggerDimension = Width > Height ? Width : Height;
        NumMips = ceil(log2(biggerDimension)) + 1;
    }

    Texture::Texture(Core* core, const TextureCreateInfo& createInfo)
        : core(core)
        , lastLayout(ImageLayout::Undefined)
        , lastAccess(AccessFlags::None)
        , lastPipelineStage(PipelineStageFlags::AllCommands)
    {
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.extent.width = createInfo.Width;
        ici.extent.height = createInfo.Height;
        ici.extent.depth = createInfo.Depth;
        ici.arrayLayers = createInfo.Layers;
        ici.mipLevels = createInfo.NumMips;
        ici.samples = static_cast<VkSampleCountFlagBits>(createInfo.Samples);

        if (isDimensionCube(createInfo.Dimension))
        {
            ici.arrayLayers *= 6;
            ici.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        ici.imageType = convertType(createInfo.Dimension);
        ici.format = static_cast<VkFormat>(createInfo.Format);
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;

        if (createInfo.CanSample)
        {
            ici.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }

        if (createInfo.CanTransfer)
        {
            ici.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        if (createInfo.IsTransient)
        {
            ici.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        }

        if (supportsStorage(core->GetHandles()->PhysicalDevice, createInfo.Format) && createInfo.CanUseAsStorage)
            ici.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

        bool forceSRGBView = false;
        if (createInfo.Format == TextureFormat::R8G8B8A8_SRGB && createInfo.CanUseAsStorage)
        {
            ici.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            ici.flags |= VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
            ici.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
            ici.format = (VkFormat)TextureFormat::R8G8B8A8_UNORM;
            forceSRGBView = true;
        }

        if (createInfo.IsRenderTarget)
        {
            if (isFormatDepth(createInfo.Format))
            {
                ici.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
            else
            {
                ici.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            }
        }

        usageFlags = ici.usage;
        imageFlags = ici.flags;

        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        const Handles* handles = core->GetHandles();

        VmaAllocationCreateInfo vaci{};
#ifdef __ANDROID__
        vaci.usage = createInfo.IsTransient ? VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED : VMA_MEMORY_USAGE_AUTO;
#else
        vaci.usage = VMA_MEMORY_USAGE_AUTO;
#endif
        VKCHECK(vmaCreateImage(handles->Allocator, &ici, &vaci, &image, &allocation, nullptr));

        // Now copy everything...
        width = createInfo.Width;
        height = createInfo.Height;
        depth = createInfo.Depth;
        layers = ici.arrayLayers;
        numMips = createInfo.NumMips;
        format = createInfo.Format;
        dimension = createInfo.Dimension;
        samples = createInfo.Samples;

        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = image;
        ivci.viewType = convertViewType(createInfo.Dimension);
        ivci.format = static_cast<VkFormat>(createInfo.Format);
        ivci.subresourceRange.aspectMask = getAspectFlags();
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.layerCount = layers;
        ivci.subresourceRange.levelCount = numMips;

        VkImageViewUsageCreateInfo usageCI{ VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
        usageCI.usage = ici.usage & ~VK_IMAGE_USAGE_STORAGE_BIT;
        if (!supportsStorage(core->GetHandles()->PhysicalDevice, createInfo.Format))
        {
            ivci.pNext = &usageCI;
        }

        if (forceSRGBView)
        {
            ivci.format = VK_FORMAT_R8G8B8A8_SRGB;
            usageFlags = usageCI.usage;
        }

        VKCHECK(vkCreateImageView(handles->Device, &ivci, handles->AllocCallbacks, &imageView));
    }

    Texture::Texture(Core* core, VkImage image, ImageLayout layout, const TextureCreateInfo& createInfo, uint32_t usageFlags)
        : image(image)
        , allocation(nullptr)
        , core(core)
        , lastLayout(layout)
        , lastAccess(AccessFlags::MemoryRead | AccessFlags::MemoryWrite)
        , lastPipelineStage(PipelineStageFlags::AllCommands)
        , usageFlags(usageFlags)
        , imageFlags(0)
    {
        // Now copy everything...
        width = createInfo.Width;
        height = createInfo.Height;
        depth = createInfo.Depth;
        layers = createInfo.Layers;
        numMips = createInfo.NumMips;
        format = createInfo.Format;
        dimension = createInfo.Dimension;
        samples = createInfo.Samples;

        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = image;
        ivci.viewType = convertViewType(createInfo.Dimension);
        ivci.format = static_cast<VkFormat>(createInfo.Format);
        ivci.subresourceRange.aspectMask = getAspectFlags();
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.layerCount = layers;
        ivci.subresourceRange.levelCount = numMips;

        const Handles* handles = core->GetHandles();
        VKCHECK(vkCreateImageView(handles->Device, &ivci, handles->AllocCallbacks, &imageView));
    }

    VkImage Texture::GetNativeHandle()
    {
        return image;
    }

    VkImage Texture::ReleaseHandle()
    {
        assert(allocation == nullptr);
        const Handles* handles = core->GetHandles();
        vkDestroyImageView(handles->Device, imageView, handles->AllocCallbacks);
        VkImage tmp = image;
        image = VK_NULL_HANDLE;
        return tmp;
    }

    VkImageView Texture::GetView()
    {
        return imageView;
    }

    void Texture::SetDebugName(const char* name)
    {
        // If there isn't a layer present that uses these names,
        // the function will be null.
        if (vkSetDebugUtilsObjectNameEXT != nullptr)
        {
            VkDebugUtilsObjectNameInfoEXT nameInfo;
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.pObjectName = name;
            nameInfo.objectHandle = (uint64_t)image;
            nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
            nameInfo.pNext = nullptr;
            vkSetDebugUtilsObjectNameEXT(core->GetHandles()->Device, &nameInfo);

            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.pObjectName = name;
            nameInfo.objectHandle = (uint64_t)imageView;
            nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
            nameInfo.pNext = nullptr;
            vkSetDebugUtilsObjectNameEXT(core->GetHandles()->Device, &nameInfo);
        }
    }

    int Texture::GetWidth()
    {
        return width;
    }

    int Texture::GetHeight()
    {
        return height;
    }

    int Texture::GetNumMips()
    {
        return numMips;
    }

    int Texture::GetLayerCount()
    {
        return layers;
    }

    int Texture::GetSamples()
    {
        return samples;
    }

    TextureFormat Texture::GetFormat()
    {
        return format;
    }

    bool hasAccessBit(AccessFlags flags, AccessFlags test)
    {
        return ((uint64_t)flags & (uint64_t)test) == (uint64_t)test;
    }

    bool hasStageBit(PipelineStageFlags flags, PipelineStageFlags test)
    {
        return ((uint64_t)flags & (uint64_t)test) == (uint64_t)test;
    }

    VkAccessFlags getOldAccessFlags(AccessFlags access)
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

    VkPipelineStageFlags getOldPipelineStageFlags(PipelineStageFlags flags)
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

    void Texture::Acquire(CommandBuffer cb, ImageLayout layout, AccessFlags access, PipelineStageFlags stage)
    {
#ifdef USE_SYNC_2
        VkImageMemoryBarrier2 imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        imb.oldLayout = (VkImageLayout)lastLayout;
        imb.newLayout = (VkImageLayout)layout;
        imb.subresourceRange = VkImageSubresourceRange{ getAspectFlags(), 0, (uint32_t)numMips, 0, (uint32_t)layers };
        imb.image = image;
        imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.srcAccessMask = (VkAccessFlags2)lastAccess;
        imb.dstAccessMask = (VkAccessFlags2)access;

        imb.srcStageMask = (VkPipelineStageFlags2)lastPipelineStage;
        imb.dstStageMask = (VkPipelineStageFlags2)stage;

        VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imb;
        depInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vkCmdPipelineBarrier2(cb.GetNativeHandle(), &depInfo);
#else
        VkImageMemoryBarrier imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

        if (layout == ImageLayout::AttachmentOptimal)
        {
            VkImageAspectFlags aspectFlags = getAspectFlags();

            if (aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT)
            {
                layout = ImageLayout::DepthStencilAttachmentOptimal;
            }
            else
            {
                layout = ImageLayout::ColorAttachmentOptimal;
            }
        }
        
        imb.oldLayout = (VkImageLayout)lastLayout;
        imb.newLayout = (VkImageLayout)layout;
        imb.subresourceRange = VkImageSubresourceRange{ getAspectFlags(), 0, (uint32_t)numMips, 0, (uint32_t)layers };
        imb.image = image;
        imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.srcAccessMask = getOldAccessFlags(lastAccess);
        imb.dstAccessMask = getOldAccessFlags(access);

        vkCmdPipelineBarrier(
            cb.GetNativeHandle(),
            getOldPipelineStageFlags(lastPipelineStage),
            getOldPipelineStageFlags(stage),
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &imb
        );
#endif

        lastLayout = layout;
        lastAccess = access;
        lastPipelineStage = stage;
    }

    void Texture::WriteLayoutTransition(CommandBuffer cb, ImageLayout layout)
    {
#ifdef USE_SYNC_2
        VkImageMemoryBarrier2 imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        imb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imb.newLayout = (VkImageLayout)layout;
        imb.subresourceRange = VkImageSubresourceRange{ getAspectFlags(), 0, (uint32_t)numMips, 0, (uint32_t)layers };
        imb.image = image;
        imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        imb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        
        VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imb;
        depInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vkCmdPipelineBarrier2(cb.GetNativeHandle(), &depInfo);
#else
        VkImageMemoryBarrier imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb.oldLayout = (VkImageLayout)lastLayout;
        imb.newLayout = (VkImageLayout)layout;
        imb.subresourceRange = VkImageSubresourceRange{ getAspectFlags(), 0, (uint32_t)numMips, 0, (uint32_t)layers };
        imb.image = image;
        imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.srcAccessMask = getOldAccessFlags(lastAccess);
        imb.dstAccessMask = getOldAccessFlags(lastAccess);

        vkCmdPipelineBarrier(
            cb.GetNativeHandle(),
            getOldPipelineStageFlags(lastPipelineStage),
            getOldPipelineStageFlags(lastPipelineStage),
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &imb
        );
#endif
    }

    void Texture::WriteLayoutTransition(CommandBuffer cb, ImageLayout oldLayout, ImageLayout newLayout)
    {
#ifdef USE_SYNC_2
        VkImageMemoryBarrier2 imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        imb.oldLayout = (VkImageLayout)oldLayout;
        imb.newLayout = (VkImageLayout)newLayout;
        imb.subresourceRange = VkImageSubresourceRange{ getAspectFlags(), 0, (uint32_t)numMips, 0, (uint32_t)layers };
        imb.image = image;
        imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        imb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

        VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imb;
        depInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        vkCmdPipelineBarrier2(cb.GetNativeHandle(), &depInfo);
#else
        VkImageMemoryBarrier imb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imb.oldLayout = (VkImageLayout)oldLayout;
        imb.newLayout = (VkImageLayout)newLayout;
        imb.subresourceRange = VkImageSubresourceRange{ getAspectFlags(), 0, (uint32_t)numMips, 0, (uint32_t)layers };
        imb.image = image;
        imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imb.srcAccessMask = getOldAccessFlags(lastAccess);
        imb.dstAccessMask = getOldAccessFlags(lastAccess);

        vkCmdPipelineBarrier(
                cb.GetNativeHandle(),
                getOldPipelineStageFlags(lastPipelineStage),
                getOldPipelineStageFlags(lastPipelineStage),
                VK_DEPENDENCY_BY_REGION_BIT,
                0, nullptr,
                0, nullptr,
                1, &imb
        );
#endif
    }

    Texture::~Texture()
    {
        const Handles* handles = core->GetHandles();
        DeletionQueue* dq;

        dq = core->perFrameResources[core->frameIndex].DeletionQueue;

        if (allocation)
        {
            DQ_QueueMemoryFree(dq, allocation);
        }

        if (image)
        {
            DQ_QueueObjectDeletion(dq, image, VK_OBJECT_TYPE_IMAGE);
            DQ_QueueObjectDeletion(dq, imageView, VK_OBJECT_TYPE_IMAGE_VIEW);
        }
    }

    VkImageAspectFlags Texture::getAspectFlags() const
    {
        if (format == TextureFormat::D32_SFLOAT || format == TextureFormat::D16_UNORM)
        {
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

    uint32_t Texture::GetUsageFlags()
    {
        return usageFlags;
    }

    uint32_t Texture::GetImageFlags()
    {
        return imageFlags;
    }

    TextureView::TextureView(Core* core, Texture* texture, TextureSubset subset)
        : core(core)
        , subset(subset)
    {
        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = texture->GetNativeHandle();
        ivci.viewType = convertViewType(subset.Dimension);
        ivci.format = static_cast<VkFormat>(texture->GetFormat());

        if (texture->GetFormat() == TextureFormat::R8G8B8A8_SRGB)
            ivci.format = VK_FORMAT_R8G8B8A8_UNORM;

        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; 
        ivci.subresourceRange.baseArrayLayer = subset.LayerStart;
        ivci.subresourceRange.baseMipLevel = subset.MipStart;
        ivci.subresourceRange.layerCount = subset.LayerCount;
        ivci.subresourceRange.levelCount = subset.MipCount;

        VKCHECK(vkCreateImageView(core->GetHandles()->Device, &ivci, core->GetHandles()->AllocCallbacks, &imageView));
    }

    TextureView::~TextureView()
    {
        DeletionQueue* dq;

        dq = core->perFrameResources[core->frameIndex].DeletionQueue;
        DQ_QueueObjectDeletion(dq, imageView, VK_OBJECT_TYPE_IMAGE_VIEW);
    }

    VkImageView TextureView::GetNativeHandle()
    {
        return imageView;
    }
}