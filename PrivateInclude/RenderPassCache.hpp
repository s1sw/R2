#pragma once
#include <stdint.h>
#include <unordered_map>
#include <volk.h>

namespace R2::VK
{
    struct RenderPassAttachment
    {
        VkFormat format;
        VkAttachmentLoadOp loadOp;
        VkAttachmentStoreOp storeOp;
        VkSampleCountFlagBits samples;

        bool operator==(const RenderPassAttachment& other) const
        {
            return
                format == other.format &&
                loadOp == other.loadOp &&
                storeOp == other.storeOp &&
                samples == other.samples;
        }
    };
    
    struct RenderPassKey
    {
        uint32_t viewMask;
        bool useDepth : 1;
        bool useColor : 1;
        RenderPassAttachment depthAttachment;
        RenderPassAttachment colorAttachment;

        bool operator==(const RenderPassKey& other) const
        {
            return
                viewMask == other.viewMask &&
                useDepth == other.useDepth &&
                useColor == other.useColor &&
                depthAttachment == other.depthAttachment &&
                colorAttachment == other.colorAttachment;
        }
    };

    struct FramebufferKey
    {
        uint32_t width, height;
        VkRenderPass renderPass;
        VkFormat textureFormats[2];
        VkImageUsageFlags textureUsages[2];
        VkImageCreateFlags textureFlags[2];
        uint8_t numTextures;
        uint8_t layerCount;

        bool operator==(const FramebufferKey& other) const
        {
            if (numTextures != other.numTextures) return false;
            bool formatsMatch = true;
            
            for (int i = 0; i < numTextures; i++)
            {
                formatsMatch &= textureFormats[i] == other.textureFormats[i];
                formatsMatch &= textureUsages[i] == other.textureUsages[i];
                formatsMatch &= textureFlags[i] == other.textureFlags[i];
            }
            
            return
                formatsMatch &&
                width == other.width &&
                height == other.height &&
                renderPass == other.renderPass &&
                layerCount == other.layerCount;
        }
    };
}

template <class T>
inline void hash_combine(std::size_t & s, const T & v)
{
    std::hash<T> h;
    s^= h(v) + 0x9e3779b9 + (s<< 6) + (s>> 2);
}

namespace std
{
    template<>
    struct hash<R2::VK::RenderPassAttachment>
    {
        std::size_t operator()(const R2::VK::RenderPassAttachment& c) const
        {
            std::size_t result = 0;
            hash_combine(result, c.format);
            hash_combine(result, c.loadOp);
            hash_combine(result, c.storeOp);
            hash_combine(result, c.samples);
            return result;
        }
    };
    
    template<>
    struct hash<R2::VK::RenderPassKey>
    {
        std::size_t operator()(const R2::VK::RenderPassKey& c) const
        {
            std::size_t result = 0;
            hash_combine(result, c.viewMask);
            hash_combine(result, c.useColor);
            hash_combine(result, c.useDepth);
            hash_combine(result, c.colorAttachment);
            hash_combine(result, c.depthAttachment);
            return result;
        }
    };
    
    template<>
    struct hash<R2::VK::FramebufferKey>
    {
        std::size_t operator()(const R2::VK::FramebufferKey& c) const
        {
            std::size_t result = 0;
            hash_combine(result, c.width);
            hash_combine(result, c.height);
            hash_combine(result, c.renderPass);
            hash_combine(result, c.numTextures);
            hash_combine(result, c.layerCount);

            for (int i = 0; i < c.numTextures; i++)
            {
                hash_combine(result, c.textureFormats[i]);
                hash_combine(result, c.textureUsages[i]);
                hash_combine(result, c.textureFlags[i]);
            }
            return result;
        }
    };
}

namespace R2::VK
{
    class Core;
    class RenderPassCache
    {
    public:
        RenderPassCache(Core* core);
        VkRenderPass GetPass(RenderPassKey key);
        VkFramebuffer GetFramebuffer(FramebufferKey key);
    private:
        std::unordered_map<RenderPassKey, VkRenderPass> passes;
        std::unordered_map<FramebufferKey, VkFramebuffer> framebuffers;
        Core* core;
    };
    extern RenderPassCache* g_renderPassCache;
}
