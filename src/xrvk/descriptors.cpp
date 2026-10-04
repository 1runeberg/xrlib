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


#include <xrvk/descriptors.hpp>
#include <xrlib/session.hpp>
#include <algorithm>


namespace xrlib
{

	CDescriptorManager::CDescriptorManager( CSession *pSession )
		: m_pSession( pSession )
	{
		assert( pSession );
	}

	CDescriptorManager::~CDescriptorManager() { DeleteAll(); }

	SDescriptorBinding CDescriptorManager::CreateUniformBinding( uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count ) { return { binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, count, stageFlags, {} }; }

	SDescriptorBinding CDescriptorManager::CreateStorageBinding( uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count ) { return { binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, count, stageFlags, {} }; }

	SDescriptorBinding CDescriptorManager::CreateSamplerBinding( uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count, const std::vector< VkSampler > &immutableSamplers )
	{
		return { binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, count, stageFlags, immutableSamplers };
	}

	SDescriptorBinding CDescriptorManager::CreateStorageImageBinding( uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count ) { return { binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, count, stageFlags, {} }; }

	VkResult CDescriptorManager::CreateDescriptorSetLayout( uint32_t &outLayoutId, const std::vector< SDescriptorBinding > &bindings, VkAllocationCallbacks *pCallbacks )
	{
		std::vector< VkDescriptorSetLayoutBinding > vkBindings;
		vkBindings.reserve( bindings.size() );

		for ( const auto &binding : bindings )
		{
			VkDescriptorSetLayoutBinding vkBinding {};
			vkBinding.binding = binding.binding;
			vkBinding.descriptorType = binding.type;
			vkBinding.descriptorCount = binding.count;
			vkBinding.stageFlags = binding.stageFlags;
			vkBinding.pImmutableSamplers = binding.immutableSamplers.empty() ? nullptr : binding.immutableSamplers.data();
			vkBindings.push_back( vkBinding );
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast< uint32_t >( vkBindings.size() );
		layoutInfo.pBindings = vkBindings.data();

		VkDescriptorSetLayout layout;
		VkResult result = vkCreateDescriptorSetLayout( m_pSession->GetVulkan()->GetVkLogicalDevice(), &layoutInfo, pCallbacks, &layout );

		if ( result == VK_SUCCESS )
		{
			outLayoutId = m_nextLayoutId++;
			m_descriptorSetLayouts[ outLayoutId ] = layout;
			m_layoutBindings[ outLayoutId ] = bindings;
		}

		return result;
	}

	VkResult CDescriptorManager::CreateDescriptorPool( uint32_t &outPoolId, uint32_t layoutId, uint32_t unSetCount, VkAllocationCallbacks *pCallbacks ) 
	{ 
		auto layoutIt = m_descriptorSetLayouts.find( layoutId );
		if ( layoutIt == m_descriptorSetLayouts.end() )
		{
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		auto bindingsIt = m_layoutBindings.find( layoutId );
		if ( bindingsIt == m_layoutBindings.end() )
		{
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		std::vector< VkDescriptorPoolSize > poolSizes;
		for ( const auto &binding : bindingsIt->second )
		{
			VkDescriptorPoolSize poolSize {};
			poolSize.type = binding.type;
			poolSize.descriptorCount = binding.count * unSetCount;
			poolSizes.push_back( poolSize );
		}

		VkDescriptorPoolCreateInfo poolInfo {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = unSetCount;

		VkDescriptorPool descriptorPool;
		VkResult result = vkCreateDescriptorPool( m_pSession->GetVulkan()->GetVkLogicalDevice(), &poolInfo, pCallbacks, &descriptorPool );
		if ( result != VK_SUCCESS )
			return result;

		outPoolId = m_nextPoolId++;
		m_descriptorPools[ outPoolId ] = descriptorPool;

		return VK_SUCCESS;
	}

	VkResult CDescriptorManager::CreateDescriptorPool( uint32_t &outPoolId, const VkDescriptorPoolCreateInfo &poolInfo, VkAllocationCallbacks *pCallbacks ) 
	{ 
		VkDescriptorPool descriptorPool;
		VkResult result = vkCreateDescriptorPool( m_pSession->GetVulkan()->GetVkLogicalDevice(), &poolInfo, pCallbacks, &descriptorPool );
		if ( result != VK_SUCCESS )
			return result;

		outPoolId = m_nextPoolId++;
		m_descriptorPools[ outPoolId ] = descriptorPool;

		return VK_SUCCESS; 
	}

	VkResult CDescriptorManager::CreateDescriptorSets( 
		std::vector< VkDescriptorSet > &outDescriptorSets, 
		uint32_t layoutId, 
		uint32_t poolId, 
		uint32_t unSetCount, 
		VkAllocationCallbacks *pCallbacks ) 
	{ 
		auto layoutIt = m_descriptorSetLayouts.find( layoutId );
		if ( layoutIt == m_descriptorSetLayouts.end() )
		{
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		auto poolIt = m_descriptorPools.find( poolId );
		if ( poolIt == m_descriptorPools.end() )
		{
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		std::vector< VkDescriptorSetLayout > layouts( unSetCount, layoutIt->second );
		VkDescriptorSetAllocateInfo allocInfo {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = poolIt->second;
		allocInfo.descriptorSetCount = unSetCount;
		allocInfo.pSetLayouts = layouts.data();

		outDescriptorSets.assign( unSetCount, VK_NULL_HANDLE );
		return vkAllocateDescriptorSets( m_pSession->GetVulkan()->GetVkLogicalDevice(), &allocInfo, outDescriptorSets.data() );
	}

	VkResult CDescriptorManager::CreateDescriptorSets( uint32_t &outPoolId, uint32_t layoutId, uint32_t unSetCount, VkAllocationCallbacks *pCallbacks )
	{
		auto layoutIt = m_descriptorSetLayouts.find( layoutId );
		if ( layoutIt == m_descriptorSetLayouts.end() )
		{
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		auto bindingsIt = m_layoutBindings.find( layoutId );
		if ( bindingsIt == m_layoutBindings.end() )
		{
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		std::vector< VkDescriptorPoolSize > poolSizes;
		for ( const auto &binding : bindingsIt->second )
		{
			VkDescriptorPoolSize poolSize {};
			poolSize.type = binding.type;
			poolSize.descriptorCount = binding.count * unSetCount;
			poolSizes.push_back( poolSize );
		}

		VkDescriptorPoolCreateInfo poolInfo {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = unSetCount;

		VkDescriptorPool descriptorPool;
		VkResult result = vkCreateDescriptorPool( m_pSession->GetVulkan()->GetVkLogicalDevice(), &poolInfo, pCallbacks, &descriptorPool );
		if ( result != VK_SUCCESS )
		{
			return result;
		}

		std::vector< VkDescriptorSetLayout > layouts( unSetCount, layoutIt->second );
		VkDescriptorSetAllocateInfo allocInfo {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = unSetCount;
		allocInfo.pSetLayouts = layouts.data();

		std::vector< VkDescriptorSet > descriptorSets( unSetCount );
		result = vkAllocateDescriptorSets( m_pSession->GetVulkan()->GetVkLogicalDevice(), &allocInfo, descriptorSets.data() );

		if ( result == VK_SUCCESS )
		{
			outPoolId = m_nextPoolId++;
			m_descriptorPools[ outPoolId ] = descriptorPool;
			m_descriptorSets[ layoutId ] = descriptorSets;
		}
		else
		{
			vkDestroyDescriptorPool( m_pSession->GetVulkan()->GetVkLogicalDevice(), descriptorPool, pCallbacks );
		}

		return result;
	}

	VkResult CDescriptorManager::CreateBuffer( uint32_t &outBufferId, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memPropFlags, VkDeviceSize unSize, void *pData, bool bUnmap, VkAllocationCallbacks *pCallbacks )
	{
		auto buffer = std::make_unique< CDeviceBuffer >( m_pSession );
		VkResult result = buffer->Init( usageFlags, memPropFlags, unSize, pData, bUnmap, pCallbacks );

		if ( result == VK_SUCCESS )
		{
			outBufferId = m_nextBufferId++;
			m_buffers[ outBufferId ] = std::move( buffer );
		}

		return result;
	}

	CDeviceBuffer* CDescriptorManager::CreateBuffer( VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memPropFlags, VkDeviceSize unSize, void *pData, bool bUnmap, VkAllocationCallbacks *pCallbacks ) 
	{ 
		CDeviceBuffer *outBuffer = new CDeviceBuffer( m_pSession );
		VkResult result = outBuffer->Init( usageFlags, memPropFlags, unSize, pData, bUnmap, pCallbacks );

		if ( result != VK_SUCCESS )
		{
			delete outBuffer;
			outBuffer = nullptr;
			throw std::runtime_error( "Unable to create descriptor buffer in CDescriptorManager::CreateBuffer!" );
		}

		return outBuffer;
	}

	void CDescriptorManager::UpdateUniformBuffer( uint32_t layoutId, uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range )
	{
		VkDescriptorBufferInfo bufferInfo {};
		bufferInfo.buffer = buffer;
		bufferInfo.offset = offset;
		bufferInfo.range = range;

		UpdateBufferDescriptor( layoutId, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfo );
	}

	void CDescriptorManager::UpdateUniformBuffer( std::vector< VkDescriptorSet > &descriptors, uint32_t binding, VkBuffer buffer, VkDescriptorType type, VkDeviceSize offset, VkDeviceSize range ) 
	{
		VkDescriptorBufferInfo bufferInfo {};
		bufferInfo.buffer = buffer;
		bufferInfo.offset = offset;
		bufferInfo.range = range;

		VkWriteDescriptorSet descriptorWrite {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.descriptorType = type;
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorCount = descriptors.size();
		descriptorWrite.pBufferInfo = &bufferInfo;

		for ( auto &descriptorSet : descriptors )
		{
			descriptorWrite.dstSet = descriptorSet;
			vkUpdateDescriptorSets( m_pSession->GetVulkan()->GetVkLogicalDevice(), 1, &descriptorWrite, 0, nullptr );
		}
	}

	void CDescriptorManager::UpdateStorageBuffer( uint32_t layoutId, uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range )
	{
		VkDescriptorBufferInfo bufferInfo {};
		bufferInfo.buffer = buffer;
		bufferInfo.offset = offset;
		bufferInfo.range = range;

		UpdateBufferDescriptor( layoutId, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfo );
	}

	void CDescriptorManager::UpdateImageDescriptor( uint32_t layoutId, uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout )
	{
		auto setIt = m_descriptorSets.find( layoutId );
		if ( setIt == m_descriptorSets.end() )
			return;

		VkDescriptorImageInfo imageInfo {};
		imageInfo.imageLayout = imageLayout;
		imageInfo.imageView = imageView;
		imageInfo.sampler = sampler;

		VkWriteDescriptorSet descriptorWrite {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		for ( auto &descriptorSet : setIt->second )
		{
			descriptorWrite.dstSet = descriptorSet;
			vkUpdateDescriptorSets( m_pSession->GetVulkan()->GetVkLogicalDevice(), 1, &descriptorWrite, 0, nullptr );
		}
	}

	void CDescriptorManager::UpdateImageDescriptor( std::vector< VkDescriptorSet > &descriptors, uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout ) 
	{
		VkDescriptorImageInfo imageInfo {};
		imageInfo.imageLayout = imageLayout;
		imageInfo.imageView = imageView;
		imageInfo.sampler = sampler;

		VkWriteDescriptorSet descriptorWrite {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		for ( auto &descriptorSet : descriptors )
		{
			descriptorWrite.dstSet = descriptorSet;
			vkUpdateDescriptorSets( m_pSession->GetVulkan()->GetVkLogicalDevice(), 1, &descriptorWrite, 0, nullptr );
		}
	}

	void CDescriptorManager::UpdateStorageImage( uint32_t layoutId, uint32_t binding, VkImageView imageView, VkImageLayout imageLayout )
	{
		auto setIt = m_descriptorSets.find( layoutId );
		if ( setIt == m_descriptorSets.end() )
			return;

		VkDescriptorImageInfo imageInfo {};
		imageInfo.imageLayout = imageLayout;
		imageInfo.imageView = imageView;
		imageInfo.sampler = VK_NULL_HANDLE;

		VkWriteDescriptorSet descriptorWrite {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		for ( auto &descriptorSet : setIt->second )
		{
			descriptorWrite.dstSet = descriptorSet;
			vkUpdateDescriptorSets( m_pSession->GetVulkan()->GetVkLogicalDevice(), 1, &descriptorWrite, 0, nullptr );
		}
	}

	void CDescriptorManager::DeleteLayout( uint32_t layoutId )
	{
		auto layoutIt = m_descriptorSetLayouts.find( layoutId );
		if ( layoutIt != m_descriptorSetLayouts.end() )
		{
			vkDestroyDescriptorSetLayout( m_pSession->GetVulkan()->GetVkLogicalDevice(), layoutIt->second, nullptr );
			m_descriptorSetLayouts.erase( layoutIt );
			m_layoutBindings.erase( layoutId );
		}
	}

	void CDescriptorManager::DeletePool( uint32_t poolId )
	{
		auto poolIt = m_descriptorPools.find( poolId );
		if ( poolIt != m_descriptorPools.end() )
		{
			vkDestroyDescriptorPool( m_pSession->GetVulkan()->GetVkLogicalDevice(), poolIt->second, nullptr );
			m_descriptorPools.erase( poolIt );
			m_descriptorSets.clear();
		}
	}

	void CDescriptorManager::DeleteDescriptorSet( uint32_t layoutId, const VkDescriptorSet &descriptorSet )
	{
		auto setIt = m_descriptorSets.find( layoutId );
		if ( setIt != m_descriptorSets.end() )
		{
			auto &sets = setIt->second;
			sets.erase( std::remove( sets.begin(), sets.end(), descriptorSet ), sets.end() );
		}
	}

	void CDescriptorManager::DeleteDescriptorSets( uint32_t layoutId, const std::vector< VkDescriptorSet > &descriptorSets )
	{
		auto setIt = m_descriptorSets.find( layoutId );
		if ( setIt != m_descriptorSets.end() )
		{
			auto &sets = setIt->second;
			for ( const auto &descriptorSet : descriptorSets )
			{
				sets.erase( std::remove( sets.begin(), sets.end(), descriptorSet ), sets.end() );
			}
		}
	}

	void CDescriptorManager::DeleteBuffer( uint32_t bufferId ) { m_buffers.erase( bufferId ); }

	void CDescriptorManager::DeleteAll()
	{
		for ( const auto &pool : m_descriptorPools )
		{
			vkDestroyDescriptorPool( m_pSession->GetVulkan()->GetVkLogicalDevice(), pool.second, nullptr );
		}

		for ( const auto &layout : m_descriptorSetLayouts )
		{
			vkDestroyDescriptorSetLayout( m_pSession->GetVulkan()->GetVkLogicalDevice(), layout.second, nullptr );
		}

		m_descriptorPools.clear();
		m_descriptorSetLayouts.clear();
		m_descriptorSets.clear();
		m_layoutBindings.clear();
		m_buffers.clear();

		// Reset IDs
		m_nextLayoutId = 0;
		m_nextPoolId = 0;
		m_nextBufferId = 0;
	}

	CDeviceBuffer *CDescriptorManager::GetBuffer( uint32_t bufferId )
	{
		auto it = m_buffers.find( bufferId );
		return it != m_buffers.end() ? it->second.get() : nullptr;
	}

	void CDescriptorManager::UpdateBufferDescriptor( uint32_t layoutId, uint32_t binding, VkDescriptorType descriptorType, const VkDescriptorBufferInfo &bufferInfo )
	{
		auto setIt = m_descriptorSets.find( layoutId );
		if ( setIt == m_descriptorSets.end() )
			return;

		VkWriteDescriptorSet descriptorWrite {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.descriptorType = descriptorType;
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		for ( auto &descriptorSet : setIt->second )
		{
			descriptorWrite.dstSet = descriptorSet;
			vkUpdateDescriptorSets( m_pSession->GetVulkan()->GetVkLogicalDevice(), 1, &descriptorWrite, 0, nullptr );
		}
	}


	VkDescriptorSetLayout CDescriptorManager::GetDescriptorSetLayout( uint32_t layoutId ) const
	{
		auto it = m_descriptorSetLayouts.find( layoutId );
		return it != m_descriptorSetLayouts.end() ? it->second : VK_NULL_HANDLE;
	}

	VkDescriptorPool CDescriptorManager::GetDescriptorPool( uint32_t poolId ) const
	{
		auto it = m_descriptorPools.find( poolId );
		return it != m_descriptorPools.end() ? it->second : VK_NULL_HANDLE;
	}

	const std::vector< VkDescriptorSet > &CDescriptorManager::GetDescriptorSets( uint32_t layoutId ) const
	{
		static const std::vector< VkDescriptorSet > emptySet;
		auto it = m_descriptorSets.find( layoutId );
		return it != m_descriptorSets.end() ? it->second : emptySet;
	}

	const std::vector< SDescriptorBinding > &CDescriptorManager::GetLayoutBindings( uint32_t layoutId ) const
	{
		static const std::vector< SDescriptorBinding > emptyBindings;
		auto it = m_layoutBindings.find( layoutId );
		return it != m_layoutBindings.end() ? it->second : emptyBindings;
	}

	const std::unordered_map< uint32_t, std::unique_ptr< CDeviceBuffer > > &CDescriptorManager::GetBuffers() const { return m_buffers; }
} // namespace xrlib