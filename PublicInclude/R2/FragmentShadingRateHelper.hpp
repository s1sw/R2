#pragma once
#include <stdint.h>
#include <stddef.h>

namespace R2
{
    namespace VK
    {
        class Core;
        class Texture;
    }

    struct ShadingRateSettings
    {
        // Radius of the central high-detail ellipse
        float centerXRadius = 0.25f;
        float centerYRadius = 0.4f;

        // Radius of the anisotropic ellipses
        // (i.e. 2x1 shading rate on the X, 2x2 shading rate on the Y)
        float onAxisRadius = 0.85f;
        float offAxisRadius = 0.7f;
    };

    class FragmentShadingRateHelper
    {
    public:
        FragmentShadingRateHelper(VK::Core* core, uint32_t width, uint32_t height, uint32_t layers);
        uint32_t GetWidth() const;
        uint32_t GetHeight() const;
		uint32_t GetTextureByteSize() const;
        void GenerateTexture(uint8_t* outBuffer, ShadingRateSettings shadingRateSettings);
    private:
        VK::Core* core;
        uint32_t width;
        uint32_t height;
        uint32_t attachmentWidth;
        uint32_t attachmentHeight;
        uint32_t layers;
    };
}