/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <xrvk/environment.hpp>

namespace xrlib
{
	CEnvironmentLighting::~CEnvironmentLighting()
	{
		for ( size_t i = 0; i < m_images.size(); ++i )
		{
			if ( m_descriptors[ i ].sampler )
				vkDestroySampler( m_device, m_descriptors[ i ].sampler, nullptr );
			if ( m_descriptors[ i ].imageView )
				vkDestroyImageView( m_device, m_descriptors[ i ].imageView, nullptr );
			if ( m_images[ i ] )
				vkDestroyImage( m_device, m_images[ i ], nullptr );
			if ( m_memory[ i ] )
				vkFreeMemory( m_device, m_memory[ i ], nullptr );
		}
	}

	VkResult CEnvironmentLighting::Init( VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool pool, VkQueue queue, const SEnvironmentData &data )
	{
		if ( m_device || !device || !physicalDevice || !pool || !queue || !IsValidEnvironment( data ) )
			return VK_ERROR_INITIALIZATION_FAILED;

		constexpr VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties( physicalDevice, format, &properties );
		const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
		if ( ( properties.optimalTilingFeatures & required ) != required )
			return VK_ERROR_FORMAT_NOT_SUPPORTED;

		m_device = device;
		const std::array< const SEnvironmentImage *, 3 > images { &data.diffuse, &data.specular, &data.brdf };
		std::array< std::vector< vkutils::SImageMip >, 3 > mips;
		std::vector< vkutils::SImageUpload > uploads;
		for ( size_t i = 0; i < images.size(); ++i )
		{
			const auto &source = *images[ i ];
			const uint32_t layers = i == 2 ? 1 : 6;
			VkImageCreateInfo image { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			image.flags = i == 2 ? 0 : VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			image.imageType = VK_IMAGE_TYPE_2D;
			image.format = format;
			image.extent = { source.size, source.size, 1 };
			image.mipLevels = source.levels;
			image.arrayLayers = layers;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			VK_CHECK_RETURN( vkCreateImage( device, &image, nullptr, &m_images[ i ] ) );

			VkMemoryRequirements requirements;
			vkGetImageMemoryRequirements( device, m_images[ i ], &requirements );
			VkMemoryAllocateInfo allocation { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			allocation.allocationSize = requirements.size;
			allocation.memoryTypeIndex = vkutils::FindMemoryType( physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
			VK_CHECK_RETURN( vkAllocateMemory( device, &allocation, nullptr, &m_memory[ i ] ) );
			VK_CHECK_RETURN( vkBindImageMemory( device, m_images[ i ], m_memory[ i ], 0 ) );

			VkImageViewCreateInfo view { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			view.image = m_images[ i ];
			view.viewType = i == 2 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_CUBE;
			view.format = format;
			view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, source.levels, 0, layers };
			VK_CHECK_RETURN( vkCreateImageView( device, &view, nullptr, &m_descriptors[ i ].imageView ) );

			VkSamplerCreateInfo sampler { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
			sampler.minFilter = sampler.magFilter = VK_FILTER_LINEAR;
			sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler.maxLod = float( source.levels - 1 );
			VK_CHECK_RETURN( vkCreateSampler( device, &sampler, nullptr, &m_descriptors[ i ].sampler ) );
			m_descriptors[ i ].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkDeviceSize offset = 0;
			for ( uint32_t level = 0, size = source.size; level < source.levels; ++level, size = std::max( 1u, size / 2 ) )
			{
				const VkDeviceSize bytes = VkDeviceSize( size ) * size * layers * 8;
				mips[ i ].push_back( { offset, bytes } );
				offset += bytes;
			}
			uploads.push_back( { m_images[ i ], { reinterpret_cast< const uint8_t * >( source.pixels.data() ), source.pixels.size() * sizeof( uint16_t ) }, source.size, source.size, format, mips[ i ], layers } );
		}

		VK_CHECK_RETURN( vkutils::UploadTextureDataToImages( device, physicalDevice, pool, queue, uploads ) );
		m_fMaxLod = float( data.specular.levels - 1 );
		m_bReady = true;
		return VK_SUCCESS;
	}
} // namespace xrlib
