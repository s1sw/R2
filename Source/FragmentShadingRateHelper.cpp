#include <volk.h>
#include <R2/VK.hpp>
#include <R2/FragmentShadingRateHelper.hpp>
#include <vector>

namespace R2
{
    inline uint32_t ceilDivide(uint32_t numerator, uint32_t denominator)
    {
        return (numerator + denominator - 1) / denominator;
    }

	FragmentShadingRateHelper::FragmentShadingRateHelper(VK::Core* core, uint32_t width, uint32_t height, uint32_t layers)
		: core(core)
		, width(width)
		, height(height)
		, layers(layers)
	{
		const VK::Handles* handles = core->GetHandles();

		VkPhysicalDeviceFragmentShadingRatePropertiesKHR fsrProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR };
		VkPhysicalDeviceProperties2 deviceProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		deviceProps.pNext = &fsrProps;

		vkGetPhysicalDeviceProperties2(handles->PhysicalDevice, &deviceProps);

		attachmentWidth = ceilDivide(width, fsrProps.maxFragmentShadingRateAttachmentTexelSize.width);
		attachmentHeight = ceilDivide(height, fsrProps.maxFragmentShadingRateAttachmentTexelSize.height);
	}

	uint32_t FragmentShadingRateHelper::GetWidth() const
	{
		return attachmentWidth;
	}

	uint32_t FragmentShadingRateHelper::GetHeight() const
	{
		return attachmentHeight;
	}

	uint32_t FragmentShadingRateHelper::GetTextureByteSize() const
	{
		return attachmentWidth * attachmentHeight * layers * sizeof(uint8_t);
	}

	struct Vec2
	{
		float x;
		float y;

		Vec2 operator*(float b)
		{
			return Vec2(x * b, y * b);
		}

		Vec2 operator*(const Vec2& b)
		{
			return Vec2(x * b.x, y * b.y);
		}

		Vec2 operator/(const Vec2& b)
		{
			return Vec2(x / b.x, y / b.y);
		}

		Vec2 operator-(float b)
		{
			return Vec2(x - b, y - b);
		}
	};

	Vec2 operator*(const float a, const Vec2& b)
	{
		return Vec2(b.x * a, b.y * a);
	}

	struct Ellipse
	{
		Vec2 radius;

		bool contains(Vec2 point)
		{
			return ((point.x * point.x) / (radius.x * radius.x) + (point.y * point.y) / (radius.y * radius.y)) < 1.0f;
		}
	};

	void FragmentShadingRateHelper::GenerateTexture(uint8_t* outBuffer, ShadingRateSettings settings)
	{
		const VK::Handles* handles = core->GetHandles();

		// get the available shading rates to find out what's the lowest
		uint32_t shadingRateCount = 0;
		VKCHECK(vkGetPhysicalDeviceFragmentShadingRatesKHR(handles->PhysicalDevice, &shadingRateCount, nullptr));

		std::vector<VkPhysicalDeviceFragmentShadingRateKHR> shadingRates;
		shadingRates.resize(shadingRateCount);

		for (uint32_t i = 0; i < shadingRateCount; i++)
		{
			shadingRates[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR;
		}

		VKCHECK(vkGetPhysicalDeviceFragmentShadingRatesKHR(handles->PhysicalDevice, &shadingRateCount, shadingRates.data()));

		for (uint32_t layer = 0; layer < layers; layer++)
		for (uint32_t x = 0; x < attachmentWidth; x++)
		for (uint32_t y = 0; y < attachmentHeight; y++)
		{
			float xOffset = 0.01f * (layer == 0 ? -1.0f : 1.0f);
			Vec2 uv{ (float)x / attachmentWidth, (float)y / attachmentHeight };
			uv = 2.0f * (uv - 0.5f);

			if (layers == 2)
				uv.x += xOffset;

			uint32_t rateX = 1;
			uint32_t rateY = 1;
			
			// The VRS regions consist of a central high detail ellipse....
			Ellipse centralEllipse{ Vec2(settings.centerXRadius, settings.centerYRadius) };
			
			if (!centralEllipse.contains(uv))
			{
				// ...with two outer ellipses stretched along the respective axis.
				Ellipse xEllipse{ settings.onAxisRadius, settings.offAxisRadius };
				Ellipse yEllipse{ settings.offAxisRadius, settings.onAxisRadius };
				
				if (xEllipse.contains(uv))
					rateX = 2;

				if (yEllipse.contains(uv))
					rateY = 2;

				if (!xEllipse.contains(uv) && !yEllipse.contains(uv))
				{
					rateX = 4;
					rateY = 4;
				}
			}

			uint8_t xBits = rateX == 1 ? 0 : (rateX >> 1u);
			uint8_t yBits = rateY == 1 ? 0 : (rateY << 1u);
			uint32_t idx = (layer * attachmentWidth * attachmentHeight) + (y * attachmentWidth) + x;
			outBuffer[idx] = xBits | yBits;
		}
	}
}