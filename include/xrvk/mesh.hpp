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

#define MAX_JOINT_COUNT 64
#define JOINT_INFLUENCE_COUNT 4

#include <vector>
#include <array>
#include <filesystem>
#include <fstream>
#include <cfloat>
#include <functional>

#include <xrvk/renderables.hpp>
#include <xrvk/texture.hpp>

namespace xrlib
{
	// Material texture flags
	static constexpr uint32_t TEXTURE_BASE_COLOR_BIT = 0x01u;
	static constexpr uint32_t TEXTURE_METALLIC_ROUGH_BIT = 0x02u;
	static constexpr uint32_t TEXTURE_NORMAL_BIT = 0x04u;
	static constexpr uint32_t TEXTURE_EMISSIVE_BIT = 0x08u;
	static constexpr uint32_t TEXTURE_OCCLUSION_BIT = 0x10u;

	static constexpr uint32_t TEXTURE_UV1_SHIFT = 8u;   // Bits 8-12 select TEXCOORD_1 for each texture slot
	static constexpr uint32_t TEXTURE_BASE_COLOR_SRGB_BIT = 1u << 16;
	static constexpr uint32_t TEXTURE_EMISSIVE_SRGB_BIT = 1u << 17;

	// Alpha modes packed into texture flags (using bits 24-25)
	static constexpr uint32_t ALPHA_MODE_SHIFT = 24u;
	static constexpr uint32_t ALPHA_MODE_MASK = 0x03u << ALPHA_MODE_SHIFT;

	struct SMeshVertex
	{
		XrVector3f position;
		XrVector3f normal;
		XrVector4f tangent; // W component is the handedness/bitangent sign
		XrVector2f uv0;		// Base color, metallic-roughness, normal maps
		XrVector2f uv1;		// Additional texture coordinates if needed
		XrVector3f color0;
		uint32_t joints[ JOINT_INFLUENCE_COUNT ] = { 0, 0, 0, 0 };
		float weights[ JOINT_INFLUENCE_COUNT ] = { 0.0f, 0.0f, 0.0f, 0.0f };
	};

	enum class EAlphaMode : uint8_t
	{
		Opaque, // Default, fully opaque
		Mask,	// Alpha testing/cutout using alphaCutoff
		Blend	// Traditional transparency
	};

	struct alignas( 16 ) SMaterialUBO
	{
		alignas( 16 ) float baseColorFactor[ 4 ] = { 1.0f, 1.0f, 1.0f, 1.0f };
		alignas( 16 ) float emissiveFactor[ 4 ] = { 0.0f, 0.0f, 0.0f }; // last value is for padding only
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		float alphaCutoff = 0.5f;
		float normalScale = 1.0f;
		uint32_t textureFlags = 0; // Includes alpha mode in bits 24-25
		float occlusionStrength = 1.0f;
		float padding[ 2 ] = { 0.0f, 0.0f };

	    void setTextureFlag( uint32_t flag, bool present ) 
		{ 
			textureFlags = present ? ( textureFlags | flag ) : ( textureFlags & ~flag ); 
		}

		void setAlphaMode( EAlphaMode mode )
		{
			// Clear existing alpha mode bits
			textureFlags &= ~ALPHA_MODE_MASK;

			// Set new alpha mode bits
			textureFlags |= ( static_cast< uint32_t >( mode ) ) << ALPHA_MODE_SHIFT;
		}

		EAlphaMode getAlphaMode() const { return static_cast< EAlphaMode >( ( textureFlags & ALPHA_MODE_MASK ) >> ALPHA_MODE_SHIFT ); }
	};

	static_assert( sizeof( SMaterialUBO ) == 64 );
	static_assert( offsetof( SMaterialUBO, textureFlags ) == 48 );
	static_assert( offsetof( SMaterialUBO, occlusionStrength ) == 52 );

	struct SMaterial : public SMaterialUBO
	{
		// CPU only properties
		int baseColorTexture = -1;
		int metallicRoughnessTexture = -1;
		int normalTexture = -1;
		int occlusionTexture = -1;
		int emissiveTexture = -1;
		bool doubleSided = false;

		uint32_t descriptorsBufferIndex = 0;
		std::vector< VkDescriptorSet > descriptors;

		void resetPadding() 
		{ 
			emissiveFactor[ 3 ] = 0.0f;
			padding[ 0 ] = 0.0f;
			padding[ 1 ] = 0.0f;
		}

		void updateTextureFlags()
		{
			setTextureFlag( TEXTURE_BASE_COLOR_BIT, baseColorTexture >= 0 );
			setTextureFlag( TEXTURE_METALLIC_ROUGH_BIT, metallicRoughnessTexture >= 0 );
			setTextureFlag( TEXTURE_NORMAL_BIT, normalTexture >= 0 );
			setTextureFlag( TEXTURE_EMISSIVE_BIT, emissiveTexture >= 0 );
			setTextureFlag( TEXTURE_OCCLUSION_BIT, occlusionTexture >= 0 );
		}
	};

	struct SMeshSection
	{
		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t materialIndex;
	};

	struct SSkin
	{
		std::string name;
		std::vector< uint32_t > joints;
		std::vector< XrMatrix4x4f > inverseBindMatrices;
		std::unordered_map< uint32_t, std::vector< uint32_t > > hierarchy;
		std::vector< XrMatrix4x4f > matrices;
		int32_t skeleton = -1; // Index of the root node (if defined)

		void UpdateMatrices( const std::vector< XrQuaternionf > &orientation, const std::vector< XrVector3f > &position, XrVector3f scale = { 1.0f, 1.0f, 1.0f } )
		{
			matrices.resize( joints.size() );

			std::function< void( uint32_t jointIndex, const XrMatrix4x4f &parentWorldMatrix ) > updateJointHierarchy;
			updateJointHierarchy = [ & ]( uint32_t jointIndex, const XrMatrix4x4f &parentWorldMatrix )
			{
				// Get local transform using the helper function
				XrMatrix4x4f localMatrix;
				XrMatrix4x4f_CreateTranslationRotationScale( &localMatrix, &position[ jointIndex ], &orientation[ jointIndex ], &scale );

				// Calculate world matrix
				XrMatrix4x4f worldMatrix;
				XrMatrix4x4f_Multiply( &worldMatrix, &parentWorldMatrix, &localMatrix );

				// Multiply with inverse bind matrix if it exists
				if ( jointIndex < inverseBindMatrices.size() )
				{
					XrMatrix4x4f finalMatrix;
					XrMatrix4x4f_Multiply( &finalMatrix, &worldMatrix, &inverseBindMatrices[ jointIndex ] );
					matrices[ jointIndex ] = finalMatrix;
				}
				else
				{
					matrices[ jointIndex ] = worldMatrix;
				}

				// Store the result
				matrices[ jointIndex ] = worldMatrix;

				// Process children
				auto childrenIt = hierarchy.find( jointIndex );
				if ( childrenIt != hierarchy.end() )
				{
					for ( uint32_t childIndex : childrenIt->second )
					{
						updateJointHierarchy( childIndex, worldMatrix );
					}
				}
			};

			// Start recursion from root(s)
			if ( skeleton != -1 )
			{
				XrMatrix4x4f identityMatrix;
				XrMatrix4x4f_CreateIdentity( &identityMatrix );
				updateJointHierarchy( skeleton, identityMatrix );
			}
			else
			{
				std::vector< bool > isChild( joints.size(), false );
				for ( const auto &[ parentIdx, children ] : hierarchy )
				{
					for ( uint32_t childIdx : children )
					{
						isChild[ childIdx ] = true;
					}
				}

				XrMatrix4x4f identityMatrix;
				XrMatrix4x4f_CreateIdentity( &identityMatrix );
				for ( size_t i = 0; i < joints.size(); ++i )
				{
					if ( !isChild[ i ] )
					{
						updateJointHierarchy( i, identityMatrix );
					}
				}
			}
		}

		void UpdateMatrices( const std::vector< XrPosef > &newJointPoses, XrVector3f scale = { 1.0f, 1.0f, 1.0f } )
		{
			std::vector< XrQuaternionf > orientation;
			std::vector< XrVector3f > position;

			for ( auto &pose : newJointPoses )
			{
				orientation.push_back( pose.orientation );
				position.push_back( pose.position );
			}

			UpdateMatrices( orientation, position, scale );
		}

		void UpdateMatrices( XrMatrix4x4f *localMatrices )
		{
			matrices.resize( joints.size() );

			// Recursive lambda that uses the existing TRS matrices in localMatrices
			std::function< void( uint32_t jointIndex, const XrMatrix4x4f &parentWorldMatrix ) > updateJointHierarchy;
			updateJointHierarchy = [ & ]( uint32_t jointIndex, const XrMatrix4x4f &parentWorldMatrix )
			{
				// The local transform is already in localMatrices[jointIndex]
				const XrMatrix4x4f &localMatrix = localMatrices[ jointIndex ];

				// Calculate world matrix
				XrMatrix4x4f worldMatrix;
				XrMatrix4x4f_Multiply( &worldMatrix, &parentWorldMatrix, &localMatrix );

				// Multiply with inverse bind matrix if it exists
				if ( jointIndex < inverseBindMatrices.size() )
				{
					XrMatrix4x4f_Multiply( &matrices[ jointIndex ], &worldMatrix, &inverseBindMatrices[ jointIndex ] );
				}
				else
				{
					matrices[ jointIndex ] = worldMatrix;
				}

				// Process children using the calculated world matrix
				auto childrenIt = hierarchy.find( jointIndex );
				if ( childrenIt != hierarchy.end() )
				{
					for ( uint32_t childIndex : childrenIt->second )
					{
						updateJointHierarchy( childIndex, worldMatrix );
					}
				}
			};

			// Start recursion from root with identity matrix
			XrMatrix4x4f identityMatrix;
			XrMatrix4x4f_CreateIdentity( &identityMatrix );
			updateJointHierarchy( 0, identityMatrix );
		}
	};


	class CRenderModel : public CRenderable
	{
	  public:

		CRenderModel( 
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			uint16_t pipelineLayoutIdx,
			uint16_t graphicsPipelineIdx,
			uint32_t descriptorLayoutIdx = ( std::numeric_limits< uint32_t >::max )(),
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		CRenderModel(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		~CRenderModel();

		// Interfaces
		void Reset() override;
		VkResult InitBuffers( bool bReset = false ) override;
		void Draw( const VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo ) override;

		uint32_t LoadMaterial( CRenderInfo *pRenderInfo, uint32_t layoutId, uint32_t poolId, CTextureManager* pTextureManager );
		uint32_t LoadMaterial( std::vector< SMaterialUBO* > &outMaterialData, CRenderInfo *pRenderInfo, uint32_t layoutId, uint32_t poolId, CTextureManager *pTextureManager );

		// Mesh data
		const VkDeviceSize vertexOffsets[ 1 ] = { 0 };

		std::vector< SMeshVertex > vertices;
		std::vector< uint32_t > indices;

		std::vector< STexture > textures;
		std::vector< SMaterial > materials;
		std::vector< SSkin > skins;
		std::vector< SMeshSection > materialSections;

	  private:

		// Interfaces
		void DeleteBuffers() override;

	};

}
