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


#include <xrvk/buffer.hpp>

namespace xrlib
{
	CDeviceBuffer::CDeviceBuffer( CSession *pSession ) : m_pSession( pSession )
	{ 
		assert( pSession );
		assert( pSession->GetVulkan()->GetVkPhysicalDevice() != VK_NULL_HANDLE );
		assert( pSession->GetVulkan()->GetVkLogicalDevice() != VK_NULL_HANDLE );
	}

	CDeviceBuffer::~CDeviceBuffer() 
	{ 
		UnmapMemory();

		if ( m_vkBufferInfo.buffer != VK_NULL_HANDLE )
			vkDestroyBuffer( GetLogicalDevice(), m_vkBufferInfo.buffer, nullptr );

		if ( m_vkDeviceMemory != VK_NULL_HANDLE )
			vkFreeMemory( GetLogicalDevice(), m_vkDeviceMemory, nullptr );
	}

	VkResult CDeviceBuffer::Init( 
		VkBufferUsageFlags usageFlags, 
		VkMemoryPropertyFlags memPropFlags, 
		VkDeviceSize unSize, 
		void *pData, 
		bool bUnmap,
		VkAllocationCallbacks *pCallbacks ) 
	{ 
		assert( usageFlags != 0 );
		assert( memPropFlags != 0 );

		// Create vulkan buffer
		VkBufferCreateInfo bufferCI { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCI.usage = usageFlags;
		bufferCI.size = unSize;
		
		VkResult result = vkCreateBuffer( GetLogicalDevice(), &bufferCI, pCallbacks, &m_vkBufferInfo.buffer );
		if ( result != VK_SUCCESS )
			return result;

		// Get buffer memory requirements
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAllocInfo { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		vkGetBufferMemoryRequirements( GetLogicalDevice(), m_vkBufferInfo.buffer, &memReqs );

		// Find memory type matching memory requirement for the buffer and provided memory property(ies)
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = FindMemoryType( memPropFlags, memReqs.memoryTypeBits );

		// If buffer is for shader use, allocate required flag
		VkMemoryAllocateFlagsInfoKHR memAllocFlags {};
		if ( usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT )
		{
			memAllocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
			memAllocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
			memAllocInfo.pNext = &memAllocFlags;
		}

		result = vkAllocateMemory( GetLogicalDevice(), &memAllocInfo, pCallbacks, &m_vkDeviceMemory );
		if ( result != VK_SUCCESS )
			return result;

		m_vkMemoryAlignment = memReqs.alignment;
		m_vkMemorySize = memReqs.size;

		// If buffer data's provided, copy to internal cache
		if ( pData )
		{
			result = vkMapMemory( GetLogicalDevice(), m_vkDeviceMemory, 0, m_vkMemorySize, 0, &m_pData ); 
			if ( result != VK_SUCCESS )
				return result;


			memcpy( m_pData, pData, unSize );
			if ( ( memPropFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) == 0 )
			{
				result = FlushMemory();
				if ( result != VK_SUCCESS )
					return result;
			}
			
			if ( bUnmap )
				UnmapMemory();
		}

		// Bind buffer to device memory
		return vkBindBufferMemory( GetLogicalDevice(), m_vkBufferInfo.buffer, m_vkDeviceMemory, 0 );
	}

	uint32_t CDeviceBuffer::FindMemoryType( VkMemoryPropertyFlags memPropFlags, uint32_t unBits ) 
	{ 
		VkPhysicalDeviceMemoryProperties memProps;
		vkGetPhysicalDeviceMemoryProperties( GetPhysicalDevice(), &memProps );

		for ( uint32_t i = 0; i < memProps.memoryTypeCount; i++ )
			if ( ( unBits & ( 1 << i ) ) && ( memProps.memoryTypes[ i ].propertyFlags & memPropFlags ) == memPropFlags )
				return i;

		throw std::runtime_error( "FATAL: Unable to find requested memory type for device's physical device." );
	}

	VkResult CDeviceBuffer::FlushMemory( VkDeviceSize unSize, VkDeviceSize unOffset, uint32_t unRangeCount ) 
	{ 
		VkMappedMemoryRange mappedRange { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		mappedRange.memory = m_vkDeviceMemory;
		mappedRange.offset = unOffset;
		mappedRange.size = unSize;
		return vkFlushMappedMemoryRanges( GetLogicalDevice(), unRangeCount, &mappedRange );
	}

	VkResult CDeviceBuffer::MapMemory() 
	{ 
		return vkMapMemory( GetLogicalDevice(), m_vkDeviceMemory, 0, m_vkMemorySize, 0, &m_pData ); 
	}

	void CDeviceBuffer::UnmapMemory() 
	{
		if ( !m_pData )
			return;

		vkUnmapMemory( GetLogicalDevice(), m_vkDeviceMemory );
		m_pData = nullptr;
	}

	VkPhysicalDevice CDeviceBuffer::GetPhysicalDevice() 
	{ 
		return m_pSession->GetVulkan()->GetVkPhysicalDevice(); 
	}

	VkDevice CDeviceBuffer::GetLogicalDevice() 
	{ 
		return m_pSession->GetVulkan()->GetVkLogicalDevice();
	}

} // namespace xrlib
