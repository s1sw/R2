#pragma once
#include <R2/VKEnums.hpp>

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
VK_DEFINE_HANDLE(VkSampler)
#undef VK_DEFINE_HANDLE

namespace R2::VK
{
    class Core;
    struct Handles;
    typedef unsigned int Bool32;

    class Sampler
    {
    public:
        Sampler(Core* core, VkSampler sampler);
        VkSampler GetNativeHandle();
        ~Sampler();
    private:
        VkSampler sampler;
        Core* core;
    };

    enum class Filter : unsigned int
    {
        Nearest = 0,
        Linear = 1,
    };

    enum class SamplerMipmapMode : unsigned int
    {
        Nearest = 0,
        Linear = 1
    };

    enum class SamplerAddressMode : unsigned int
    {
        Repeat = 0,
        MirroredRepeat = 1,
        ClampToEdge = 2,
        ClampToBorder = 3,
        MirrorClampToEdge = 4
    };

    enum class BorderColor : unsigned int
    {
        FloatTransparentBlack = 0,
        IntTransparentBlack = 1,
        FloatOpaqueBlack = 2,
        IntOpaqueBlack = 3,
        FloatOpaqueWhite = 4,
        IntOpaqueWhite = 5
    };

    class SamplerBuilder
    {
    public:
        SamplerBuilder(Core* core);

        SamplerBuilder& MagFilter(Filter filt);
        SamplerBuilder& MinFilter(Filter filt);
        SamplerBuilder& MipmapMode(SamplerMipmapMode mode);
        SamplerBuilder& AddressMode(SamplerAddressMode mode);
        SamplerBuilder& EnableCompare(bool enableCompare);
        SamplerBuilder& CompareOp(CompareOp compareOp);

        Sampler* Build();
    private:
        struct SamplerCreateInfo
        {
            Filter                magFilter;
            Filter                minFilter;
            SamplerMipmapMode     mipmapMode;
            SamplerAddressMode    addressModeU;
            SamplerAddressMode    addressModeV;
            SamplerAddressMode    addressModeW;
            float                 mipLodBias;
            Bool32                anisotropyEnable;
            float                 maxAnisotropy;
            Bool32                compareEnable;
            VK::CompareOp         compareOp;
            float                 minLod;
            float                 maxLod;
            BorderColor           borderColor;
            Bool32                unnormalizedCoordinates;
        };

        SamplerCreateInfo ci;
        Core* core;
    };
}