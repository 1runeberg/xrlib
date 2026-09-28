/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <span>
#include <vector>
#include <xrvk/vkutils.hpp>

namespace xrlib
{
	struct SEnvironmentBakeConfig
	{
		uint32_t diffuseSize;
		uint32_t specularSize;
		uint32_t lutSize;
		uint32_t sampleCount;
	};

	// Linear RGBA16F, mip-major then Vulkan cube face order (+X, -X, +Y, -Y, +Z, -Z)
	// Diffuse stores irradiance divided by pi, specular mips span perceptual roughness 0 to 1
	struct SEnvironmentImage
	{
		uint32_t size = 0;
		uint32_t levels = 0;
		std::vector< uint16_t > pixels;
	};

	struct SEnvironmentData
	{
		SEnvironmentImage diffuse;
		SEnvironmentImage specular;
		SEnvironmentImage brdf;
	};

	// CPU preparation can run on a worker or in an offline asset tool
	// Input is linear RGB in equirectangular order, north at the first row, +X at u=.5
	// Invalid input throws std::invalid_argument, allocation failures propagate
	SEnvironmentData BakeEnvironment( std::span< const float > rgb, uint32_t width, uint32_t height, const SEnvironmentBakeConfig &config );
	SEnvironmentData BakeEnvironmentHDR( std::span< const uint8_t > encodedHDR, const SEnvironmentBakeConfig &config );

	// Portable little-endian baked payload for bundling prepared lighting with an app
	std::vector< uint8_t > EncodeEnvironment( const SEnvironmentData &data );
	SEnvironmentData DecodeEnvironment( std::span< const uint8_t > bytes );

	// Checks image sizes, mip levels and pixel counts before encoding or upload
	bool IsValidEnvironment( const SEnvironmentData &data );

	class CEnvironmentLighting
	{
	  public:
		CEnvironmentLighting() = default;
		~CEnvironmentLighting();
		CEnvironmentLighting( const CEnvironmentLighting & ) = delete;
		CEnvironmentLighting &operator=( const CEnvironmentLighting & ) = delete;

		// Initialise once, serialize queue/pool access and keep the device alive until destruction
		// Upload completes before return, earlier shader reads must finish before destruction
		VkResult Init( VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool pool, VkQueue queue, const SEnvironmentData &data );
		const std::array< VkDescriptorImageInfo, 3 > &GetDescriptors() const { return m_descriptors; }
		float GetMaxLod() const { return m_fMaxLod; }
		bool IsReady() const { return m_bReady; }
		VkDevice GetDevice() const { return m_device; }
		static SEnvironmentData DisabledData();

	  private:
		VkDevice m_device = VK_NULL_HANDLE;
		std::array< VkImage, 3 > m_images {};
		std::array< VkDeviceMemory, 3 > m_memory {};
		std::array< VkDescriptorImageInfo, 3 > m_descriptors {};
		float m_fMaxLod = 0.f;
		bool m_bReady = false;
	};
} // namespace xrlib
