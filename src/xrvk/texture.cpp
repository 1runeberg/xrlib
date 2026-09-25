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


#include <xrvk/texture.hpp>

namespace xrlib
{
	CTextureManager::CTextureManager( CSession *pSession, VkCommandPool pool )
		: m_pSession( pSession )
		, m_pool( pool )
	{
		// Create default linear sampler
		vkutils::CreateSampler( 
			m_defaultSampler, 
			m_pSession->GetVulkan()->GetVkLogicalDevice(),
			VK_FILTER_LINEAR, 
			VK_FILTER_LINEAR, 
			VK_SAMPLER_MIPMAP_MODE_LINEAR, 
			VK_SAMPLER_ADDRESS_MODE_REPEAT );
	}

	CTextureManager::~CTextureManager() 
	{ 
		Cleanup(); 
	}

	const STexture &CTextureManager::GetDefaultTexture()
	{
		if ( !m_defaultTexture.image )
			VK_CHECK_RESULT( CreateDefaultTexture( m_defaultTexture ) );
		return m_defaultTexture;
	}

	VkResult CTextureManager::CreateDefaultTexture( STexture &outTexture ) 
	{
		// Initialize with 1x1 white pixel
		uint32_t whitePixel = 0xFFFFFFFF;
		return CreateTextureFromData( outTexture, VK_FORMAT_R8G8B8A8_UNORM, &whitePixel, 1, 1 );
	}

	VkResult CTextureManager::CreateTextureFromData( 
		STexture &outTexture, 
		VkFormat format, 
		void *data, 
		uint32_t width, 
		uint32_t height )
	{
		outTexture.width = width;
		outTexture.height = height;
		outTexture.format = format;

		// Calculate data size
		VkDeviceSize imageSize = width * height * BYTES_PER_PIXEL; 

		// Create staging buffer using CDeviceBuffer
		CDeviceBuffer stagingBuffer( m_pSession );
		VK_CHECK_RESULT( stagingBuffer.Init( VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, imageSize, data, true ) );

		// Create image
		VK_CHECK_RESULT( vkutils::CreateImage( 
			outTexture.image, 
			outTexture.memory, 
			GetDevice(), 
			GetPhysicalDevice(),
			width, 
			height, 
			format, 
			VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) );

		// Copy data to image
		vkutils::TransitionImageLayout( 
			GetDevice(), 
			m_pool, 
			GetGraphicsQueue(),
			outTexture.image, 
			format, 
			VK_IMAGE_LAYOUT_UNDEFINED, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

		vkutils::CopyBufferToImage( 
			GetDevice(), 
			m_pool, 
			GetGraphicsQueue(),
			stagingBuffer.GetVkBuffer(), 
			outTexture.image, 
			width, 
			height );

		vkutils::TransitionImageLayout( 
			GetDevice(), 
			m_pool,
			GetGraphicsQueue(),
			outTexture.image, 
			format,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

		// Create image view
		VK_CHECK_RESULT( vkutils::CreateImageView( outTexture.view, GetDevice(), outTexture.image, format, VK_IMAGE_ASPECT_COLOR_BIT ) );

		// Use default sampler if none specified
		if ( outTexture.sampler == VK_NULL_HANDLE )
		{
			outTexture.sampler = m_defaultSampler;
		}

		return VK_SUCCESS;
	}

	VkResult CTextureManager::CreateSampler( VkSampler &outSampler, VkDevice device, const STextureSamplerConfig &config ) 
	{ 
		VkSamplerCreateInfo samplerInfo {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = config.magFilter;
		samplerInfo.minFilter = config.minFilter;

		if ( config.minFilter == VK_FILTER_LINEAR )
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		else if ( config.minFilter == VK_FILTER_NEAREST )
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		else
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

		samplerInfo.addressModeU = config.addressModeU;
		samplerInfo.addressModeV = config.addressModeV;
		samplerInfo.addressModeW = config.addressModeW;
		samplerInfo.anisotropyEnable = config.anisotropyEnable ? VK_TRUE : VK_FALSE;
		samplerInfo.maxAnisotropy = config.maxAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = config.minLod;
		samplerInfo.maxLod = config.maxLod;

		return vkCreateSampler( device, &samplerInfo, nullptr, &outSampler );
	}

	void CTextureManager::DestroyTexture( STexture &texture )
	{
		if ( texture.srgbView != VK_NULL_HANDLE )
			vkDestroyImageView( GetDevice(), texture.srgbView, nullptr );
		if ( texture.view != VK_NULL_HANDLE )
			vkDestroyImageView( GetDevice(), texture.view, nullptr );
		if ( texture.image != VK_NULL_HANDLE )
			vkDestroyImage( GetDevice(), texture.image, nullptr );
		if ( texture.memory != VK_NULL_HANDLE )
			vkFreeMemory( GetDevice(), texture.memory, nullptr );
		if ( texture.sampler != VK_NULL_HANDLE && texture.sampler != m_defaultSampler )
			vkDestroySampler( GetDevice(), texture.sampler, nullptr );

		texture = {}; // Reset to default values
	}

	void CTextureManager::Cleanup()
	{
		DestroyTexture( m_defaultTexture );
		if ( m_defaultSampler != VK_NULL_HANDLE )
		{
			vkDestroySampler( GetDevice(), m_defaultSampler, nullptr );
			m_defaultSampler = VK_NULL_HANDLE;
		}
	}

} // namespace xrlib
