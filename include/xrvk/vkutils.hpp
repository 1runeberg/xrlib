/* 
 * Copyright 2024-26 Rune Berg
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

#include <xrlib/vulkan.hpp>
#include <span>

namespace vkutils
{
	VkCommandBuffer BeginSingleTimeCommands( VkDevice device, VkCommandPool commandPool );

	void EndSingleTimeCommands( VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer commandBuffer );

	uint32_t FindMemoryType( VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties );

	uint32_t FindMemoryTypeWithFallback( VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties );

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
		VkMemoryPropertyFlags properties, VkImageCreateFlags flags = 0, uint32_t mipLevels = 1 );

	VkResult CreateImageView( VkImageView &outImageView, VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels = 1 );

	VkResult CreateSampler( VkSampler &outSampler, VkDevice device, VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode, VkSamplerAddressMode addressMode );

	void UploadTextureDataToImage( VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, VkImage image, const std::vector< uint8_t > &imageData, uint32_t width, uint32_t height, VkFormat format );

	/// <summary>Texel block footprint of an upload format (uncompressed formats use 1x1 blocks)</summary>
	struct SFormatBlock
	{
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t bytes = 0; // Zero when the format isn't supported for uploads
	};

	SFormatBlock GetFormatBlock( VkFormat format );

	/// <summary>sRGB view format for a UNORM image (undefined when there isn't one)</summary>
	VkFormat GetSrgbFormat( VkFormat format );

	/// <summary>Byte range for one precomputed mip level in an image's pixel data</summary>
	struct SImageMip
	{
		VkDeviceSize offset;
		VkDeviceSize size;
	};

	/// <summary>Borrowed pixels and optional mip ranges for a newly created image</summary>
	struct SImageUpload
	{
		VkImage image;
		std::span< const uint8_t > data;
		uint32_t width;
		uint32_t height;
		VkFormat format;
		std::span< const SImageMip > mips {}; // Empty for a single-level image
		uint32_t layers = 1;				  // Each mip contains tightly packed array layers
	};


	// Prepare on a worker with its own pool, then hand off before Submit/Poll
	// Prepare copies pixels. Keep destination images and the pool alive until destruction
	// Serialize queue access for Submit. Destruction waits for pending uploads
	class CImageUpload
	{
	  public:
		CImageUpload() = default;
		~CImageUpload();
		CImageUpload( const CImageUpload & ) = delete;
		CImageUpload &operator=( const CImageUpload & ) = delete;

		VkResult Prepare( VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, std::span< const SImageUpload > images );
		VkResult Submit( VkQueue queue );
		VkResult Poll();
		VkResult Wait();

	  private:
		VkDevice m_device = VK_NULL_HANDLE;
		VkCommandPool m_pool = VK_NULL_HANDLE;
		VkBuffer m_buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_memory = VK_NULL_HANDLE;
		VkCommandBuffer m_commands = VK_NULL_HANDLE;
		VkFence m_fence = VK_NULL_HANDLE;
		bool m_prepared = false;
		bool m_submitted = false;
		VkResult m_result = VK_NOT_READY;
	};

	/// <summary>Uploads 8/16-bit UNORM or ASTC images with one staging allocation, submission and fence wait</summary>
	/// <param name="images">Images in UNDEFINED layout; pixels must remain valid until this call returns</param>
	/// <returns>Vulkan result. Success leaves all supplied mip levels ready for fragment shader reads</returns>
	/// <remarks>The caller must serialize access to the command pool and graphics queue.</remarks>
	VkResult UploadTextureDataToImages( VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, std::span< const SImageUpload > images );

	void TransitionImageLayout(
		VkDevice device,
		VkCommandPool commandPool,
		VkQueue graphicsQueue,
		VkImage image,
		VkFormat format,
		VkImageLayout oldLayout,
		VkImageLayout newLayout );

	void TransitionImageLayout(
		VkDevice device,
		VkCommandPool commandPool,
		VkQueue graphicsQueue,
		VkImage image,
		VkFormat format,
		VkImageLayout oldLayout,
		VkImageLayout newLayout,
		VkImageAspectFlags aspectMask,
		uint32_t baseArrayLayer = 0,
		uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS );

	void CopyBufferToImage( 
		VkDevice device, 
		VkCommandPool commandPool, 
		VkQueue graphicsQueue, 
		VkBuffer buffer, 
		VkImage image, 
		uint32_t width, 
		uint32_t height );

} // namespace vkutils