/* 
 * Copyright 2024,2025 Copyright Rune Berg 
 * https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This work is the next iteration of OpenXRProvider (v1, v2)
 * OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
 * OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
 * v1 & v2 licensed under MIT: https://opensource.org/license/mit
*/


#pragma once

#include <xrlib/session.hpp>
#include <xrlib/vulkan.hpp>

#include <xrvk/vkutils.hpp>
#include <xrvk/buffer.hpp>

#define BYTES_PER_PIXEL 4

namespace xrlib
{
	struct STextureSamplerConfig
	{
		VkFilter minFilter = VK_FILTER_LINEAR;
		VkFilter magFilter = VK_FILTER_LINEAR;
		VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		float maxLod = 0.0f;
		float minLod = 0.0f;
		uint32_t mipLevels = 1;
		bool anisotropyEnable = true;
		float maxAnisotropy = 16.0f;
	};

	enum class ETextureType
	{
		Unknown = 0,
		BaseColor = 1,
		MetallicRoughness = 2,
		Normal = 3,
		Emissive = 4,
		Occlusion = 5
	};

	struct STexture
	{
		std::string name;
		std::string uri;								// URI/path to the image file
		ETextureType type = ETextureType::Unknown;
		
		std::vector< uint8_t > data;					// Raw image data
		int32_t width = 1;
		int32_t height = 1;
		int32_t channels = 4;							// Number of color channels (if 4, alpha is supported)
		int32_t bitsPerChannel = 8;
		VkFormat format = VK_FORMAT_UNDEFINED;

		// Sampler configuration
		STextureSamplerConfig samplerConfig;

		// Vulkan resources
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkImageView srgbView = VK_NULL_HANDLE; // Optional color view sharing the same image allocation
		VkSampler sampler = VK_NULL_HANDLE;
	};


	class CTextureManager
	{
	  public:
		CTextureManager( CSession *pSession, VkCommandPool pool );

		~CTextureManager();

		// Create a default 1x1 white texture
		VkResult CreateDefaultTexture( STexture &outTexture );
		/// <summary>Returns a shared white texture owned by this manager</summary>
		const STexture &GetDefaultTexture();

		// Create a texture from raw data
		VkResult CreateTextureFromData( STexture &outTexture, VkFormat format, void *data, uint32_t width, uint32_t height );

		VkResult CreateSampler( VkSampler &outSampler, VkDevice device, const STextureSamplerConfig &config );
		void DestroyTexture( STexture &texture );

	  private:
		CSession *m_pSession;
		VkCommandPool m_pool = VK_NULL_HANDLE; 
		VkSampler m_defaultSampler = VK_NULL_HANDLE;
		STexture m_defaultTexture;

		VkDevice GetDevice() { return m_pSession->GetVulkan()->GetVkLogicalDevice(); }
		VkPhysicalDevice GetPhysicalDevice() { return m_pSession->GetVulkan()->GetVkPhysicalDevice(); }
		VkQueue GetGraphicsQueue() { return m_pSession->GetVulkan()->GetVkQueue_Graphics(); }

		void Cleanup();
	};
} // namespace xrlib
