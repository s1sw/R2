#pragma once
#include <stdint.h>
#include <array>
#include <bitset>
#include <mutex>

namespace R2
{
    namespace VK
    {
        class Core;
        class Texture;
        class TextureView;
        class DescriptorSet;
        class DescriptorSetLayout;
        class Sampler;
    }

    class BindlessTextureManager
    {
        static const uint32_t NUM_TEXTURES = 1024;

        std::mutex texturesMutex;
        std::array<VK::Texture*, NUM_TEXTURES> textures;
        std::array<VK::TextureView*, NUM_TEXTURES> textureViews;
        std::bitset<NUM_TEXTURES> presentTextures;
        std::bitset<NUM_TEXTURES> useView;

        VK::DescriptorSet* textureDescriptors;
        VK::DescriptorSetLayout* textureDescriptorSetLayout;
        VK::Core* core;
        VK::Sampler* sampler;
        bool descriptorsNeedUpdate = false;

        uint32_t FindFreeSlot();
    public:
        BindlessTextureManager(VK::Core* core);
        ~BindlessTextureManager();

        uint32_t AllocateTextureHandle(VK::Texture* tex);
        void SetTextureAt(uint32_t handle, VK::Texture* tex);
        void SetViewAt(uint32_t handle, VK::TextureView* texView);
        VK::Texture* GetTextureAt(uint32_t handle);
        void FreeTextureHandle(uint32_t handle);

        VK::DescriptorSet& GetTextureDescriptorSet();
        VK::DescriptorSetLayout& GetTextureDescriptorSetLayout();
        void UpdateDescriptorsIfNecessary();
    };
}