#include <R2/BindlessTextureManager.hpp>
#include <R2/VKCore.hpp>
#include <R2/VKDescriptorSet.hpp>
#include <R2/VKSampler.hpp>
#include <assert.h>

namespace R2
{
    BindlessTextureManager::BindlessTextureManager(VK::Core* core)
        : core(core)
    {
        VK::DescriptorSetLayoutBuilder dslb{core};

        dslb.Binding(0, VK::DescriptorType::Sampler, 1, VK::ShaderStage::Vertex | VK::ShaderStage::Fragment | VK::ShaderStage::Compute);
        dslb.Binding(1, VK::DescriptorType::SampledImage, NUM_TEXTURES, 
            VK::ShaderStage::Vertex | VK::ShaderStage::Fragment | VK::ShaderStage::Compute)
            .PartiallyBound()
            .UpdateAfterBind();

        textureDescriptorSetLayout = dslb.Build();

        textureDescriptors = core->CreateDescriptorSet(textureDescriptorSetLayout);

        VK::SamplerBuilder sb{core};
        sampler = sb
            .AddressMode(VK::SamplerAddressMode::Repeat)
            .MinFilter(VK::Filter::Linear)
            .MagFilter(VK::Filter::Linear)
            .MipmapMode(VK::SamplerMipmapMode::Linear)
            .Build();

        for (int i = 0; i < NUM_TEXTURES; i++)
        {
            presentTextures[i] = false;
            useView[i] = false;
        }

        VK::DescriptorSetUpdater dsu{core, textureDescriptors};
        dsu.AddSampler(0, 0, VK::DescriptorType::Sampler, sampler);
        dsu.Update();
    }

    BindlessTextureManager::~BindlessTextureManager()
    {
    }

    uint32_t BindlessTextureManager::FindFreeSlot()
    {
        for (uint32_t i = 0; i < NUM_TEXTURES; i++)
        {
            if (!presentTextures[i])
                return i;
        }

        return ~0u;
    }

    uint32_t BindlessTextureManager::AllocateTextureHandle(VK::Texture* tex)
    {
        uint32_t freeSlot = FindFreeSlot();

        std::lock_guard lock{texturesMutex};
        assert(freeSlot != ~0u);
        textures[freeSlot] = tex;
        presentTextures[freeSlot] = true;
        descriptorsNeedUpdate = true;
        return freeSlot;
    }

    void BindlessTextureManager::SetTextureAt(uint32_t handle, VK::Texture* tex)
    {
        std::lock_guard lock{texturesMutex};
        assert(presentTextures[handle]);
        textures[handle] = tex;
        descriptorsNeedUpdate = true;
    }

    void BindlessTextureManager::SetViewAt(uint32_t handle, VK::TextureView* texView)
    {
        std::lock_guard lock{texturesMutex};
        assert(presentTextures[handle]);
        textureViews[handle] = texView;
        useView[handle] = texView != nullptr;
        descriptorsNeedUpdate = true;
    }


    VK::Texture* BindlessTextureManager::GetTextureAt(uint32_t handle)
    {
        std::lock_guard lock{texturesMutex};
        assert(presentTextures[handle]);
        return textures[handle];
    }

    void BindlessTextureManager::FreeTextureHandle(uint32_t handle)
    {
        std::lock_guard lock{texturesMutex};
        textures[handle] = nullptr;
        presentTextures[handle] = false;
        useView[handle] = false;
        textureViews[handle] = nullptr;
        descriptorsNeedUpdate = true;
    }

    VK::DescriptorSet& BindlessTextureManager::GetTextureDescriptorSet()
    {
        return *textureDescriptors;
    }

    VK::DescriptorSetLayout& BindlessTextureManager::GetTextureDescriptorSetLayout()
    {
        return *textureDescriptorSetLayout;
    }

    void BindlessTextureManager::UpdateDescriptorsIfNecessary()
    {
        if (descriptorsNeedUpdate)
        {
            VK::DescriptorSetUpdater dsu{core, textureDescriptors};

            for (int i = 0; i < NUM_TEXTURES; i++)
            {
                if (!presentTextures[i]) continue;

                if (!useView[i])
                {
                    dsu.AddTexture(1, i, VK::DescriptorType::SampledImage, textures[i]);
                }
                else
                {
                    dsu.AddTextureView(1, i, VK::DescriptorType::SampledImage, textureViews[i]);
                }
            }

            dsu.Update();
            descriptorsNeedUpdate = false;
        }
    }
}