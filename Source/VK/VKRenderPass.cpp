#include <cassert>
#include <R2/VKRenderPass.hpp>
#include <R2/VKCommandBuffer.hpp>
#include <R2/VKEnums.hpp>
#include <R2/VKTexture.hpp>
#include <volk.h>
#include <malloc.h>
#include <RenderPassCache.hpp>
#ifdef __linux__
#include <alloca.h>
#endif

namespace R2::VK
{
    VkAttachmentStoreOp convertStoreOp(StoreOp op)
    {
        switch (op)
        {
        case StoreOp::Store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case StoreOp::DontCare:
        default:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
    }

    VkAttachmentLoadOp convertLoadOp(LoadOp op)
    {
        switch (op)
        {
        case LoadOp::Load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOp::Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::DontCare:
        default:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
    }

    RenderPass::RenderPass()
        : numColorAttachments(0)
        , viewMask(0)
        , useFragmentShadingRateAttachment(false)
    {
        depthAttachment.Texture = nullptr;
    }

    RenderPass& RenderPass::RenderArea(uint32_t width, uint32_t height)
    {
        this->width = width;
        this->height = height;

        return *this;
    }

    RenderPass& RenderPass::ColorAttachment(Texture* tex, LoadOp loadOp, StoreOp storeOp)
    {
        AttachmentInfo ai{};
        ai.Texture = tex;
        ai.LoadOp = loadOp;
        ai.StoreOp = storeOp;

        colorAttachments[numColorAttachments] = ai;
        numColorAttachments++;

        return *this;
    }

    RenderPass& RenderPass::ColorAttachmentClearValue(ClearValue cv)
    {
        colorAttachments[numColorAttachments - 1].ClearValue = cv;

        return *this;
    }

    RenderPass& RenderPass::DepthAttachment(Texture* tex, LoadOp loadOp, StoreOp storeOp)
    {
        AttachmentInfo ai{};
        ai.Texture = tex;
        ai.LoadOp = loadOp;
        ai.StoreOp = storeOp;
        depthAttachment = ai;

        return *this;
    }

    RenderPass& RenderPass::DepthAttachmentClearValue(ClearValue cv)
    {
        depthAttachment.ClearValue = cv;

        return *this;
    }

    RenderPass& RenderPass::FragmentShadingRateAttachment(Texture* tex, uint32_t texelWidth, uint32_t texelHeight)
    {
        useFragmentShadingRateAttachment = true;
        fragmentShadingRateAttachment = FragmentShadingRateAttachmentInfo{
            .tex = tex,
            .texelWidth = texelWidth,
            .texelHeight = texelHeight
        };

        return *this;
    }

    RenderPass& RenderPass::ViewMask(uint32_t viewMask)
    {
        this->viewMask = viewMask;
        return *this;
    }

    void RenderPass::Begin(CommandBuffer cb)
    {
        for (int i = 0; i < numColorAttachments; i++)
        {
            colorAttachments[i].Texture->Acquire(cb, ImageLayout::AttachmentOptimal, AccessFlags::ColorAttachmentReadWrite, PipelineStageFlags::ColorAttachmentOutput);
        }

        if (depthAttachment.Texture)
            depthAttachment.Texture->Acquire(cb, ImageLayout::AttachmentOptimal, AccessFlags::DepthStencilAttachmentReadWrite, PipelineStageFlags::LateFragmentTests);

        if (g_renderPassCache == nullptr)
        {
            VkRenderingInfo renderInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
            renderInfo.renderArea = VkRect2D{ { 0, 0 }, { width, height }, };
            renderInfo.layerCount = 1;
            renderInfo.colorAttachmentCount = numColorAttachments;
            renderInfo.viewMask = viewMask;

            VkRenderingAttachmentInfo depthAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            if (depthAttachment.Texture)
            {
                depthAttachmentInfo.clearValue.depthStencil.depth = depthAttachment.ClearValue.DepthStencil.Depth;
                depthAttachmentInfo.imageView = depthAttachment.Texture->GetView();
                depthAttachmentInfo.storeOp = convertStoreOp(depthAttachment.StoreOp);
                depthAttachmentInfo.loadOp = convertLoadOp(depthAttachment.LoadOp);
                depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                renderInfo.pDepthAttachment = &depthAttachmentInfo;
            }

            // Is alloca the right choice here? We certainly don't want to heap allocate.
            VkRenderingAttachmentInfo* colorAttachmentInfos =
                static_cast<VkRenderingAttachmentInfo*>(alloca(sizeof(VkRenderingAttachmentInfo) * numColorAttachments));

            for (int i = 0; i < numColorAttachments; i++)
            {
                const AttachmentInfo& colorAttachment = colorAttachments[i];
                VkRenderingAttachmentInfo& colorAttachmentInfo = colorAttachmentInfos[i];
                colorAttachmentInfo = VkRenderingAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };

                for (int j = 0; j < 4; j++)
                    colorAttachmentInfo.clearValue.color.uint32[j] = colorAttachment.ClearValue.Color.Uint32[j];

                colorAttachmentInfo.imageView = colorAttachment.Texture->GetView();
                colorAttachmentInfo.storeOp = convertStoreOp(colorAttachment.StoreOp);
                colorAttachmentInfo.loadOp = convertLoadOp(colorAttachment.LoadOp);
                colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            }

            renderInfo.pColorAttachments = colorAttachmentInfos;

            VkRenderingFragmentShadingRateAttachmentInfoKHR fsrAttachmentInfo{ VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR };
            if (useFragmentShadingRateAttachment)
            {
                fragmentShadingRateAttachment.tex->Acquire(cb, VK::ImageLayout::FragmentShadingRateOptimal,
                    VK::AccessFlags::FragmentShadingRateAttachmentRead, VK::PipelineStageFlags::FragmentShadingRateAttachment);

                fsrAttachmentInfo.imageView = fragmentShadingRateAttachment.tex->GetView();
                fsrAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
                fsrAttachmentInfo.shadingRateAttachmentTexelSize.width = fragmentShadingRateAttachment.texelWidth;
                fsrAttachmentInfo.shadingRateAttachmentTexelSize.height = fragmentShadingRateAttachment.texelHeight;

                renderInfo.pNext = &fsrAttachmentInfo;
            }

            vkCmdBeginRendering(cb.GetNativeHandle(), &renderInfo);
        }
        else
        {
            // For now we only support a max of 1 color attachment
            assert(numColorAttachments <= 1);

            const AttachmentInfo& colorAttachment = colorAttachments[0];

            RenderPassKey key
            {
                .viewMask = viewMask
            };

            if (depthAttachment.Texture)
            {
                key.depthAttachment = RenderPassAttachment
                {
                    .format = (VkFormat)depthAttachment.Texture->GetFormat(),
                    .loadOp = convertLoadOp(depthAttachment.LoadOp),
                    .storeOp = convertStoreOp(depthAttachment.StoreOp),
                    .samples = (VkSampleCountFlagBits)depthAttachment.Texture->GetSamples()
                };
                key.useDepth = true;
            }

            if (numColorAttachments > 0)
            {
                key.colorAttachment = RenderPassAttachment
                {
                    .format = (VkFormat)colorAttachment.Texture->GetFormat(),
                    .loadOp = convertLoadOp(colorAttachment.LoadOp),
                    .storeOp = convertStoreOp(colorAttachment.StoreOp),
                    .samples = (VkSampleCountFlagBits)colorAttachment.Texture->GetSamples()
                };
                key.useColor = true;
            }

            VkRenderPass renderPass = g_renderPassCache->GetPass(key);
            FramebufferKey framebufferKey
            {
                .width = width,
                .height = height,
                .renderPass = renderPass,
                .numTextures = 0,
                .layerCount = 1
            };
            VkImageView attachmentViews[2];
            VkClearValue clearVals[2];

            if (depthAttachment.Texture)
            {
                framebufferKey.textureFormats[framebufferKey.numTextures] =
                    (VkFormat)depthAttachment.Texture->GetFormat();

                framebufferKey.textureUsages[framebufferKey.numTextures] =
                    depthAttachment.Texture->GetUsageFlags();

                framebufferKey.textureFlags[framebufferKey.numTextures] =
                    depthAttachment.Texture->GetImageFlags();

                attachmentViews[framebufferKey.numTextures] = depthAttachment.Texture->GetView();
                framebufferKey.layerCount = depthAttachment.Texture->GetLayerCount();
                clearVals[framebufferKey.numTextures].depthStencil.depth = depthAttachment.ClearValue.DepthStencil.Depth;
                framebufferKey.numTextures++;
            }

            if (numColorAttachments > 0)
            {
                framebufferKey.textureFormats[framebufferKey.numTextures] =
                    (VkFormat)colorAttachment.Texture->GetFormat();

                framebufferKey.textureUsages[framebufferKey.numTextures] =
                    colorAttachment.Texture->GetUsageFlags();

                framebufferKey.textureFlags[framebufferKey.numTextures] =
                    colorAttachment.Texture->GetImageFlags();

                attachmentViews[framebufferKey.numTextures] = colorAttachment.Texture->GetView();
                framebufferKey.layerCount = colorAttachment.Texture->GetLayerCount();
                for (int j = 0; j < 4; j++)
                    clearVals[framebufferKey.numTextures].color.uint32[j] = colorAttachment.ClearValue.Color.Uint32[j];

                framebufferKey.numTextures++;
            }

            VkFramebuffer framebuffer = g_renderPassCache->GetFramebuffer(framebufferKey);

            VkRenderPassAttachmentBeginInfo attachBeginInfo
            {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,
                .attachmentCount = framebufferKey.numTextures,
                .pAttachments = attachmentViews
            };

            VkRenderPassBeginInfo beginInfo
            {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext = &attachBeginInfo,
                .renderPass = renderPass,
                .framebuffer = framebuffer,
                .renderArea = VkRect2D { 0, 0, width, height },
                .clearValueCount = framebufferKey.numTextures,
                .pClearValues = clearVals
            };

            vkCmdBeginRenderPass(cb.GetNativeHandle(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        }
    }

    void RenderPass::End(CommandBuffer cb)
    {
        if (g_renderPassCache == nullptr)
        {
            vkCmdEndRendering(cb.GetNativeHandle());
        }
        else
        {
            vkCmdEndRenderPass(cb.GetNativeHandle());
        }
    }
}
