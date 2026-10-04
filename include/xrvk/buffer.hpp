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

#include <xrlib/session.hpp>
#include <xrlib/vulkan.hpp>

namespace xrlib
{
	class CDeviceBuffer
	{
	  public:


		CDeviceBuffer( CSession *pSession );
		~CDeviceBuffer();

		VkResult Init( VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memPropFlags, VkDeviceSize unSize, void *pData = nullptr, bool bUnmap = true, VkAllocationCallbacks *pCallbacks = nullptr );
		uint32_t FindMemoryType( VkMemoryPropertyFlags memPropFlags, uint32_t unBits );
		VkResult FlushMemory( VkDeviceSize unSize = VK_WHOLE_SIZE, VkDeviceSize unOffset = 0, uint32_t unRangeCount = 1 );

		VkResult MapMemory();
		void UnmapMemory();
		
		VkPhysicalDevice GetPhysicalDevice();
		VkDevice GetLogicalDevice();

		inline VkBuffer GetVkBuffer() { return m_vkBufferInfo.buffer; }
		inline VkBuffer *GetVkBufferPtr() { return &m_vkBufferInfo.buffer; };
		inline VkDescriptorBufferInfo *GetBufferInfo() { return &m_vkBufferInfo; }
		inline VkDeviceMemory GetDeviceMemory() { return m_vkDeviceMemory; }
		inline void *GetMappedData() { return m_pData; }
		inline VkDeviceSize GetBufferSize() const { return m_vkBufferSize; }
		inline VkMemoryPropertyFlags GetMemoryPropertyFlags() const { return m_memPropFlags; }

	  private:
		CSession *m_pSession = nullptr;

		VkDescriptorBufferInfo m_vkBufferInfo { VK_NULL_HANDLE, 0, 0 };
		VkDeviceMemory m_vkDeviceMemory = VK_NULL_HANDLE;
		VkDeviceSize m_vkMemoryAlignment = 0;
		VkDeviceSize m_vkMemorySize = VK_WHOLE_SIZE;
		VkDeviceSize m_vkBufferSize = 0;
		VkMemoryPropertyFlags m_memPropFlags = 0;

		VkAllocationCallbacks m_callbacks {};
		const VkAllocationCallbacks *m_pCallbacks = nullptr;

		void *m_pData = nullptr;
	};

}
