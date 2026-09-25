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


#include <xrvk/vkutils.hpp>
#include <numeric>

namespace vkutils
{
	VkCommandBuffer BeginSingleTimeCommands( VkDevice device, VkCommandPool commandPool ) 
	{
		VkCommandBufferAllocateInfo allocInfo {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers( device, &allocInfo, &commandBuffer );

		VkCommandBufferBeginInfo beginInfo {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer( commandBuffer, &beginInfo );

		return commandBuffer;
	}

	void EndSingleTimeCommands( VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer commandBuffer )
	{
		vkEndCommandBuffer( commandBuffer );

		VkSubmitInfo submitInfo {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
		vkQueueWaitIdle( graphicsQueue );

		vkFreeCommandBuffers( device, commandPool, 1, &commandBuffer );
	}

	uint32_t FindMemoryType( VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties )
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties( physicalDevice, &memProperties );

		for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ )
		{
			if ( ( typeFilter & ( 1 << i ) ) && ( memProperties.memoryTypes[ i ].propertyFlags & properties ) == properties )
			{
				return i;
			}
		}

		throw std::runtime_error( "Failed to find suitable memory type!" );
	}

	uint32_t FindMemoryTypeWithFallback( VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties ) 
	{ 
		try
		{
			// First, attempt to find memory with the requested properties
			return FindMemoryType( physicalDevice, typeFilter, properties );
		}
		catch ( const std::runtime_error & )
		{
			// If lazily allocated memory isn't available, fall back
			VkMemoryPropertyFlags fallbackProperties = properties & ~VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
			return FindMemoryType( physicalDevice, typeFilter, fallbackProperties );
		}
	}

	VkResult CreateImage( 
		VkImage &outImage,
		VkDeviceMemory &outImageMemory,
		VkDevice device, 
		VkPhysicalDevice physicalDevice, 
		uint32_t width, 
		uint32_t height, 
		VkFormat format, 
		VkImageTiling tiling, 
		VkImageUsageFlags usage, 
		VkMemoryPropertyFlags properties )
	{
		VkImageCreateInfo imageInfo {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT( vkCreateImage( device, &imageInfo, nullptr, &outImage ) );

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements( device, outImage, &memRequirements );

		VkMemoryAllocateInfo allocInfo {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType( physicalDevice, memRequirements.memoryTypeBits, properties );

		VK_CHECK_RESULT( vkAllocateMemory( device, &allocInfo, nullptr, &outImageMemory ) );
		VK_CHECK_RESULT( vkBindImageMemory( device, outImage, outImageMemory, 0 ) );

		return VK_SUCCESS;
	}

	VkResult CreateImageView( VkImageView &outImageView, VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags )
	{
		VkImageViewCreateInfo viewInfo {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		return vkCreateImageView( device, &viewInfo, nullptr, &outImageView );
	}

	VkResult CreateSampler( VkSampler &outSampler, VkDevice device, VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode )
	{
		VkSamplerCreateInfo samplerInfo {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = magFilter;
		samplerInfo.minFilter = minFilter;
		samplerInfo.mipmapMode = mipmapMode;
		samplerInfo.addressModeU = addressMode;
		samplerInfo.addressModeV = addressMode;
		samplerInfo.addressModeW = addressMode;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		return vkCreateSampler( device, &samplerInfo, nullptr, &outSampler );
	}

	void UploadTextureDataToImage( VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, const std::vector< uint8_t > &imageData, uint32_t width, uint32_t height, VkFormat format )
	{
		// Create staging buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		VkBufferCreateInfo bufferInfo {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = imageData.size();
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT( vkCreateBuffer( device, &bufferInfo, nullptr, &stagingBuffer ) );

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements( device, stagingBuffer, &memRequirements );

		VkMemoryAllocateInfo allocInfo {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType( physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		VK_CHECK_RESULT( vkAllocateMemory( device, &allocInfo, nullptr, &stagingBufferMemory ) );
		VK_CHECK_RESULT( vkBindBufferMemory( device, stagingBuffer, stagingBufferMemory, 0 ) );

		// Copy data to staging buffer
		void *data;
		vkMapMemory( device, stagingBufferMemory, 0, imageData.size(), 0, &data );
		memcpy( data, imageData.data(), imageData.size() );
		vkUnmapMemory( device, stagingBufferMemory );

		// Transition image to transfer destination layout
		TransitionImageLayout( device, commandPool, graphicsQueue, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );

		// Copy buffer to image
		CopyBufferToImage( device, commandPool, graphicsQueue, stagingBuffer, image, width, height );

		// Transition image to shader read layout
		TransitionImageLayout( device, commandPool, graphicsQueue, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

		// Cleanup staging buffer
		vkDestroyBuffer( device, stagingBuffer, nullptr );
		vkFreeMemory( device, stagingBufferMemory, nullptr );
	}

	VkResult UploadTextureDataToImages( VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, std::span< const SImageUpload > images )
	{
		if ( images.empty() )
			return VK_SUCCESS;

		struct SUploadResources
		{
			VkDevice device;
			VkCommandPool pool;
			VkBuffer buffer = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			VkCommandBuffer commands = VK_NULL_HANDLE;
			VkFence fence = VK_NULL_HANDLE;
			bool submitted = false;
			~SUploadResources()
			{
				if ( submitted )
					vkWaitForFences( device, 1, &fence, VK_TRUE, UINT64_MAX );
				if ( fence )
					vkDestroyFence( device, fence, nullptr );
				if ( commands )
					vkFreeCommandBuffers( device, pool, 1, &commands );
				if ( buffer )
					vkDestroyBuffer( device, buffer, nullptr );
				if ( memory )
					vkFreeMemory( device, memory, nullptr );
			}
		} resources { device, commandPool };

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties( physicalDevice, &properties );
		// Align to both Vulkan's preferred copy offset and all supported texel sizes
		const VkDeviceSize alignment = std::lcm( VkDeviceSize( 24 ), std::max( VkDeviceSize( 4 ), properties.limits.optimalBufferCopyOffsetAlignment ) );
		std::vector< VkDeviceSize > offsets;
		offsets.reserve( images.size() );
		VkDeviceSize size = 0;
		for ( const auto &image : images )
		{
			uint32_t texelBytes = 0;
			switch ( image.format )
			{
				case VK_FORMAT_R8_UNORM:
					texelBytes = 1;
					break;
				case VK_FORMAT_R8G8_UNORM:
					texelBytes = 2;
					break;
				case VK_FORMAT_R8G8B8_UNORM:
					texelBytes = 3;
					break;
				case VK_FORMAT_R8G8B8A8_UNORM:
					texelBytes = 4;
					break;
				case VK_FORMAT_R16_UNORM:
					texelBytes = 2;
					break;
				case VK_FORMAT_R16G16_UNORM:
					texelBytes = 4;
					break;
				case VK_FORMAT_R16G16B16_UNORM:
					texelBytes = 6;
					break;
				case VK_FORMAT_R16G16B16A16_UNORM:
					texelBytes = 8;
					break;
				default:
					return VK_ERROR_FORMAT_NOT_SUPPORTED;
			}
			if ( !image.image || !image.width || !image.height || image.data.size() / texelBytes / image.width != image.height || image.data.size() % ( VkDeviceSize( image.width ) * texelBytes ) != 0 )
				return VK_ERROR_INITIALIZATION_FAILED;
			if ( size > UINT64_MAX - alignment || image.data.size() > UINT64_MAX - size - alignment )
				return VK_ERROR_OUT_OF_HOST_MEMORY;
			size = ( size + alignment - 1 ) / alignment * alignment;
			offsets.push_back( size );
			size += image.data.size();
		}

		VkBufferCreateInfo bufferInfo { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.size = size;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RETURN( vkCreateBuffer( device, &bufferInfo, nullptr, &resources.buffer ) );
		VkMemoryRequirements requirements;
		vkGetBufferMemoryRequirements( device, resources.buffer, &requirements );
		VkMemoryAllocateInfo allocation { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocation.allocationSize = requirements.size;
		allocation.memoryTypeIndex = FindMemoryType( physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
		VK_CHECK_RETURN( vkAllocateMemory( device, &allocation, nullptr, &resources.memory ) );
		VK_CHECK_RETURN( vkBindBufferMemory( device, resources.buffer, resources.memory, 0 ) );
		void *mapped = nullptr;
		VK_CHECK_RETURN( vkMapMemory( device, resources.memory, 0, size, 0, &mapped ) );
		for ( size_t i = 0; i < images.size(); ++i )
			std::memcpy( static_cast< uint8_t * >( mapped ) + offsets[ i ], images[ i ].data.data(), images[ i ].data.size() );
		vkUnmapMemory( device, resources.memory );

		VkCommandBufferAllocateInfo commandInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		commandInfo.commandPool = commandPool;
		commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandInfo.commandBufferCount = 1;
		VK_CHECK_RETURN( vkAllocateCommandBuffers( device, &commandInfo, &resources.commands ) );
		VkCommandBufferBeginInfo begin { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK_RETURN( vkBeginCommandBuffer( resources.commands, &begin ) );
		for ( size_t i = 0; i < images.size(); ++i )
		{
			VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = images[ i ].image;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			vkCmdPipelineBarrier( resources.commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
			VkBufferImageCopy copy {};
			copy.bufferOffset = offsets[ i ];
			copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			copy.imageExtent = { images[ i ].width, images[ i ].height, 1 };
			vkCmdCopyBufferToImage( resources.commands, resources.buffer, images[ i ].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy );
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier( resources.commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
		}
		VK_CHECK_RETURN( vkEndCommandBuffer( resources.commands ) );
		VkFenceCreateInfo fenceInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		VK_CHECK_RETURN( vkCreateFence( device, &fenceInfo, nullptr, &resources.fence ) );
		VkSubmitInfo submit { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &resources.commands;
		VK_CHECK_RETURN( vkQueueSubmit( graphicsQueue, 1, &submit, resources.fence ) );
		resources.submitted = true;
		const auto result = vkWaitForFences( device, 1, &resources.fence, VK_TRUE, UINT64_MAX );
		resources.submitted = false;
		return result;
	}

	void TransitionImageLayout( 
		VkDevice device, 
		VkCommandPool commandPool, 
		VkQueue graphicsQueue, 
		VkImage image, 
		VkFormat format, 
		VkImageLayout oldLayout, 
		VkImageLayout newLayout )
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands( device, commandPool );

		VkImageMemoryBarrier barrier {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL )
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if ( oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}

		vkCmdPipelineBarrier( commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );

		EndSingleTimeCommands( device, commandPool, graphicsQueue, commandBuffer );
	}

	void TransitionImageLayout(
		VkDevice device,
		VkCommandPool commandPool,
		VkQueue graphicsQueue,
		VkImage image,
		VkFormat format,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
		VkImageAspectFlags aspectMask,
		uint32_t baseArrayLayer,
		uint32_t layerCount )
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands( device, commandPool );

		VkImageMemoryBarrier barrier {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = aspectMask;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
		barrier.subresourceRange.layerCount = layerCount;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if ( oldLayout == VK_IMAGE_LAYOUT_UNDEFINED )
		{
			barrier.srcAccessMask = 0;
			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

			if ( newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL )
			{
				barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			}
			else if ( newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL )
			{
				barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			}
			else
			{
				barrier.dstAccessMask = 0;
				destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			}
		}
		else
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = 0;
			sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}

		vkCmdPipelineBarrier( commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );

		EndSingleTimeCommands( device, commandPool, graphicsQueue, commandBuffer );
	}

	void CopyBufferToImage( 
		VkDevice device, 
		VkCommandPool commandPool, 
		VkQueue graphicsQueue, 
		VkBuffer buffer, 
		VkImage image, 
		uint32_t width, 
		uint32_t height )
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands( device, commandPool );

		VkBufferImageCopy region {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage( commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

		EndSingleTimeCommands( device, commandPool, graphicsQueue, commandBuffer );
	}

} // namespace vkutils
