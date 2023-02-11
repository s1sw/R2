#include <volk.h>
#include <R2/VK.hpp>
#include <RenderPassCache.hpp>

namespace R2::VK
{
    VkAttachmentDescription getAttachmentDesc(RenderPassAttachment attachment, bool isColor)
    {
        VkImageLayout layout;
        if (isColor)
        {
            layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        else
        {
            layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        
        return VkAttachmentDescription {
            .flags = 0,
            .format = attachment.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = attachment.loadOp,
            .storeOp = attachment.storeOp,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = layout,
            .finalLayout = layout
        };
    }
    
    RenderPassCache::RenderPassCache(Core* core)
        : core(core)
    {
    }

    VkRenderPass RenderPassCache::GetPass(RenderPassKey key)
    {
        auto pos = passes.find(key);
        if (pos != passes.end())
        {
            return pos->second;
        }

        // now how do we create a render pass...
        VkAttachmentDescription depthDesc = getAttachmentDesc(key.depthAttachment, false);
        VkAttachmentDescription colorDesc = getAttachmentDesc(key.colorAttachment, true);

        uint32_t attachmentCount = 0;
        
        VkAttachmentReference depthRef
        {
            .attachment = attachmentCount,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL 
        };

        if (key.useDepth) attachmentCount++;

        VkAttachmentReference colorRef
        {
            .attachment = attachmentCount,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        if (key.useColor) attachmentCount++;

        VkSubpassDescription subpassDesc
        {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = key.useColor ? 1u : 0u,
            .pColorAttachments = key.useColor ? &colorRef : nullptr,
            .pDepthStencilAttachment = key.useDepth ? &depthRef : nullptr,
        };

        std::vector<VkAttachmentDescription> attachments;

        if (key.useDepth) attachments.push_back(depthDesc);
        if (key.useColor) attachments.push_back(colorDesc);

        VkRenderPassMultiviewCreateInfo multiviewCi
        {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
            .subpassCount = 1,
            .pViewMasks = &key.viewMask,
            .dependencyCount = 0,
            .pViewOffsets = nullptr,
            .correlationMaskCount = (key.viewMask != 0) ? 1u : 0u,
            .pCorrelationMasks = &key.viewMask
        };

        VkRenderPassCreateInfo createInfo
        {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = &multiviewCi,
            .attachmentCount = (uint32_t)attachments.size(),
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpassDesc,
            .dependencyCount = 0,
            .pDependencies = nullptr
        };

        const Handles* handles = core->GetHandles();

        VkRenderPass renderPass;
        VKCHECK(vkCreateRenderPass(handles->Device, &createInfo, handles->AllocCallbacks, &renderPass));

        // leak!!! be evil!!! never destroy!!!!!
        passes.insert({ key, renderPass });
        return renderPass;
    }

    bool isAttachmentDepth(VkFormat format)
    {
        // FIXME: there are so many more depth formats!!!
        return format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM;
    }

    VkFramebuffer RenderPassCache::GetFramebuffer(FramebufferKey key)
    {
        if (framebuffers.contains(key))
        {
            return framebuffers.at(key);
        }

        // otherwise create a framebuffer :(
        std::vector<VkFramebufferAttachmentImageInfo> imageInfos;

        for (int i = 0; i < key.numTextures; i++)
        {
            VkFramebufferAttachmentImageInfo imageInfo
            {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,
                .flags = key.textureFlags[i],
                .usage = key.textureUsages[i],
                .width = key.width,
                .height = key.height,
                .layerCount = key.layerCount,
                .viewFormatCount = 1,
                .pViewFormats = &key.textureFormats[i]
            };
            
            imageInfos.push_back(imageInfo);
        }

        VkFramebufferAttachmentsCreateInfo faci
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,
            .attachmentImageInfoCount = (uint32_t)imageInfos.size(),
            .pAttachmentImageInfos = imageInfos.data()
        };

        VkFramebufferCreateInfo fci
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = &faci,
            .flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
            .renderPass = key.renderPass,
            .attachmentCount = key.numTextures,
            .pAttachments = nullptr,
            .width = key.width,
            .height = key.height,
            .layers = 1
        };

        const Handles* handles = core->GetHandles();
        VkFramebuffer framebuffer;
        VKCHECK(vkCreateFramebuffer(handles->Device, &fci, handles->AllocCallbacks, &framebuffer));
        framebuffers.insert({ key, framebuffer });

        return framebuffer;
    }
}