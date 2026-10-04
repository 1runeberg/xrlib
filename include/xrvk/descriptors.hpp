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

#include <unordered_map>
#include <vector>
#include <xrvk/buffer.hpp>

namespace xrlib
{
	struct SDescriptorBinding
	{
		uint32_t binding;
		VkDescriptorType type;
		uint32_t count;
		VkShaderStageFlags stageFlags;
		std::vector< VkSampler > immutableSamplers;
	};

	class CSession;
	class CDescriptorManager
	{
	  public:
		CDescriptorManager( CSession* pSession );
		~CDescriptorManager();

		// Helper functions to create common descriptor bindings
		static SDescriptorBinding CreateUniformBinding( uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count = 1 );
		static SDescriptorBinding CreateStorageBinding( uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count = 1 );
		static SDescriptorBinding CreateSamplerBinding( uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count = 1, const std::vector< VkSampler > &immutableSamplers = {} );
		static SDescriptorBinding CreateStorageImageBinding( uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count = 1 );

		// Creation methods with output IDs
		VkResult CreateDescriptorSetLayout( uint32_t &outLayoutId, const std::vector< SDescriptorBinding > &bindings, VkAllocationCallbacks *pCallbacks = nullptr );
		VkResult CreateDescriptorPool( uint32_t &outPoolId, uint32_t layoutId, uint32_t unSetCount = 1, VkAllocationCallbacks *pCallbacks = nullptr );
		VkResult CreateDescriptorPool( uint32_t &outPoolId, const VkDescriptorPoolCreateInfo &poolInfo, VkAllocationCallbacks *pCallbacks = nullptr );
		VkResult CreateDescriptorSets( std::vector< VkDescriptorSet > &outDescriptorSets, uint32_t layoutId, uint32_t poolId, uint32_t unSetCount = 1, VkAllocationCallbacks *pCallbacks = nullptr );
		VkResult CreateDescriptorSets( uint32_t &outPoolId, uint32_t layoutId, uint32_t unSetCount, VkAllocationCallbacks *pCallbacks = nullptr );
		VkResult CreateBuffer( uint32_t &outBufferId, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memPropFlags, VkDeviceSize unSize, void *pData = nullptr, bool bUnmap = true, VkAllocationCallbacks *pCallbacks = nullptr );
		CDeviceBuffer* CreateBuffer( VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memPropFlags, VkDeviceSize unSize, void *pData = nullptr, bool bUnmap = true, VkAllocationCallbacks *pCallbacks = nullptr );

		// Update methods
		void UpdateUniformBuffer( uint32_t layoutId, uint32_t binding, VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE );
		void UpdateUniformBuffer( std::vector< VkDescriptorSet > &descriptors, uint32_t binding, VkBuffer buffer, VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE );

		void UpdateStorageBuffer( uint32_t layoutId, uint32_t binding, VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE );

		void UpdateImageDescriptor( uint32_t layoutId, uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
		void UpdateImageDescriptor( std::vector< VkDescriptorSet > &descriptors, uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

		void UpdateStorageImage( uint32_t layoutId, uint32_t binding, VkImageView imageView, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_GENERAL );

		// Deletion methods
		void DeleteLayout( uint32_t layoutId );
		void DeletePool( uint32_t poolId );
		void DeleteDescriptorSet( uint32_t layoutId, const VkDescriptorSet &descriptorSet );
		void DeleteDescriptorSets( uint32_t layoutId, const std::vector< VkDescriptorSet > &descriptorSets );
		void DeleteBuffer( uint32_t bufferId );
		void DeleteAll();

		// Resource getters
		CDeviceBuffer *GetBuffer( uint32_t bufferId );
		VkDescriptorSetLayout GetDescriptorSetLayout( uint32_t layoutId ) const;
		VkDescriptorPool GetDescriptorPool( uint32_t poolId ) const;
		const std::vector< VkDescriptorSet > &GetDescriptorSets( uint32_t layoutId ) const;
		const std::vector< SDescriptorBinding > &GetLayoutBindings( uint32_t layoutId ) const;
		const std::unordered_map< uint32_t, std::unique_ptr< CDeviceBuffer > > &GetBuffers() const;

		// ID getters
		inline uint32_t GetNextLayoutId() const { return m_nextLayoutId; }
		inline uint32_t GetNextPoolId() const { return m_nextPoolId; }
		inline uint32_t GetNextBufferId() const { return m_nextBufferId; }

		// Existence checks
		inline bool HasLayout( uint32_t layoutId ) const { return m_descriptorSetLayouts.find( layoutId ) != m_descriptorSetLayouts.end(); }
		inline bool HasPool( uint32_t poolId ) const { return m_descriptorPools.find( poolId ) != m_descriptorPools.end(); }
		inline bool HasDescriptorSets( uint32_t layoutId ) const { return m_descriptorSets.find( layoutId ) != m_descriptorSets.end(); }
		inline bool HasLayoutBindings( uint32_t layoutId ) const { return m_layoutBindings.find( layoutId ) != m_layoutBindings.end(); }
		inline bool HasBuffer( uint32_t bufferId ) const { return m_buffers.find( bufferId ) != m_buffers.end(); }

	  private:
		CSession *m_pSession = nullptr;
		uint32_t m_nextLayoutId = 0;
		uint32_t m_nextPoolId = 0;
		uint32_t m_nextBufferId = 0;

		std::unordered_map< uint32_t, VkDescriptorSetLayout > m_descriptorSetLayouts;
		std::unordered_map< uint32_t, VkDescriptorPool > m_descriptorPools;
		std::unordered_map< uint32_t, std::vector< VkDescriptorSet > > m_descriptorSets;
		std::unordered_map< uint32_t, std::vector< SDescriptorBinding > > m_layoutBindings;
		std::unordered_map< uint32_t, std::unique_ptr< CDeviceBuffer > > m_buffers;

		void UpdateBufferDescriptor( uint32_t layoutId, uint32_t binding, VkDescriptorType descriptorType, const VkDescriptorBufferInfo &bufferInfo );

	};

} // namespace xrlib
