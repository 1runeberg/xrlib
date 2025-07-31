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


#ifdef XR_USE_PLATFORM_ANDROID
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif

#include <tinygltf/tiny_gltf.h>
#include <xrvk/gltf.hpp>
#include <xrvk/texture.hpp>

#include <filesystem>

namespace fs = std::filesystem;
namespace xrlib
{
	CGltf::CGltf( CSession *pSession )
		: m_pSession( pSession )
	{
		assert( pSession );
	}

	CGltf::~CGltf()
	{
	}

	bool CGltf::LoadAndParse( CRenderModel *outRenderModel, VkCommandPool commandPool, const std::string &sFilename, XrVector3f scale )
	{
		std::unique_ptr< TinyGLTF > pGltfLoader = std::make_unique< TinyGLTF >();
		std::unique_ptr< Model > pModel = std::make_unique< Model >();

		// Check file validity
		bool bResult = false;
		std::string sError, sWarn;

		fs::path file( sFilename );

        #ifndef XR_USE_PLATFORM_ANDROID
            if ( file.empty() )
            {
				LogError( XRLIB_NAME, "Error: Attempted to load an empty file!" );
                return false;
            }

            if ( !fs::exists( file ) )
            {
				LogError( XRLIB_NAME, "Error: Attempted to load a non-existent file: %s", sFilename.c_str() );
                return false;
            }
        #endif

		// Load gltf file based on extension
		if ( file.extension() == ".glb" )
		{
			bResult = pGltfLoader->LoadBinaryFromFile( pModel.get(), &sError, &sWarn, sFilename );
		}
		else if ( file.extension() == ".gltf" )
		{
			bResult = pGltfLoader->LoadASCIIFromFile( pModel.get(), &sError, &sWarn, sFilename );
		}
		else
		{
			LogError( XRLIB_NAME, "Error: Invalid file format: %s", sFilename.c_str() );
			return false;
		}

		// Show warnings and errors
		if ( !sWarn.empty() )
			LogWarning( XRLIB_NAME, "Warning loading %s: %s", sFilename.c_str(), sWarn.c_str() );

		if ( !sError.empty() )
			LogError( XRLIB_NAME, "Error loading %s: %s", sFilename.c_str(), sError.c_str() );
        

		if ( !bResult )
		{
			LogError( XRLIB_NAME, "Failed to parse gltf file: %s", sFilename.c_str() );
			return false;
		}

		// Parse Textures
		ParseTextures( outRenderModel, commandPool, *pModel );

		// Parse Materials
		ParseMaterials( outRenderModel, *pModel );

		// Parse Skins
		ParseSkins( outRenderModel, *pModel );

		// Process all nodes in the scene
		for ( size_t i = 0; i < pModel->scenes[ 0 ].nodes.size(); i++ )
		{
			const tinygltf::Node &node = pModel->nodes[ pModel->scenes[ 0 ].nodes[ i ] ];
			ProcessNode( *pModel, node, outRenderModel->vertices, outRenderModel->indices, outRenderModel->materialSections );
		}

		// Set scale
		for ( size_t i = 0; i < outRenderModel->GetInstanceCount(); i++ )
		{
			outRenderModel->instances[ i ].scale = scale;
		}
		
		return true;
	}

	bool CGltf::LoadFromDisk( CRenderModel *outRenderModel, tinygltf::Model *outModel, const std::string &sFilename, XrVector3f scale ) 
	{ 
		std::unique_ptr< TinyGLTF > pGltfLoader = std::make_unique< TinyGLTF >();

		// Check file validity
		bool bResult = false;
		std::string sError, sWarn;

		fs::path file( sFilename );

		#ifndef XR_USE_PLATFORM_ANDROID
		if ( file.empty() )
		{
			LogError( XRLIB_NAME, "Error: Attempted to load an empty file!" );
			return false;
		}

		if ( !fs::exists( file ) )
		{
			LogError( XRLIB_NAME, "Error: Attempted to load a non-existent file: %s", sFilename.c_str() );
			return false;
		}
		#endif

		// Debug: Log disk read performance
		//std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
		//LogDebug( "CGltf::LoadFromDisk", "Started loading gltf file: %s", sFilename.c_str() );

		// Load gltf file based on extension
		if ( file.extension() == ".glb" )
		{
			bResult = pGltfLoader->LoadBinaryFromFile( outModel, &sError, &sWarn, sFilename );
		}
		else if ( file.extension() == ".gltf" )
		{
			bResult = pGltfLoader->LoadASCIIFromFile( outModel, &sError, &sWarn, sFilename );
		}
		else
		{
			LogError( XRLIB_NAME, "Error: Invalid file format: %s", sFilename.c_str() );
			return false;
		}

		// Show warnings and errors
		if ( !sWarn.empty() )
			LogWarning( XRLIB_NAME, "Warning loading %s: %s", sFilename.c_str(), sWarn.c_str() );

		if ( !sError.empty() )
			LogError( XRLIB_NAME, "Error loading %s: %s", sFilename.c_str(), sError.c_str() );

		if ( !bResult )
		{
			LogError( XRLIB_NAME, "Failed to parse gltf file: %s", sFilename.c_str() );
			return false;
		}

		// Set scale
		for ( size_t i = 0; i < outRenderModel->GetInstanceCount(); i++ )
		{
			outRenderModel->instances[ i ].scale = scale;
		}

		// Debug: Log disk read performance
		//std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		//std::chrono::duration< double > duration = end - start;
		//LogDebug( "CGltf::LoadFromDisk", "Finished loading gltf file: %s in %.4f seconds", sFilename.c_str(), duration.count() );

		return true; 
	}

	void CGltf::ParseModel( CRenderModel *outRenderModel, tinygltf::Model *pModel, VkCommandPool commandPool ) 
	{ 
		// Parse Textures
		ParseTextures( outRenderModel, commandPool, *pModel );

		// Parse Materials
		ParseMaterials( outRenderModel, *pModel );

		// Parse Skins
		ParseSkins( outRenderModel, *pModel );

		// Process all nodes in the scene
		for ( size_t i = 0; i < pModel->scenes[ 0 ].nodes.size(); i++ )
		{
			const tinygltf::Node &node = pModel->nodes[ pModel->scenes[ 0 ].nodes[ i ] ];
			ProcessNode( *pModel, node, outRenderModel->vertices, outRenderModel->indices, outRenderModel->materialSections );
		}
	}

	void CGltf::ProcessNode( 
		const tinygltf::Model &model, 
		const tinygltf::Node &node, 
		std::vector< SMeshVertex > &vertices, 
		std::vector< uint32_t > &indices, 
		std::vector< SMeshSection > &materialSections )
	{
		// Process mesh if present
		if ( node.mesh >= 0 )
		{
			ProcessMesh( model, model.meshes[ node.mesh ], vertices, indices, materialSections );
		}

		// Process child nodes
		for ( size_t i = 0; i < node.children.size(); i++ )
		{
			ProcessNode( model, model.nodes[ node.children[ i ] ], vertices, indices, materialSections );
		}
	}

	void CGltf::ProcessMesh( 
		const tinygltf::Model &model, 
		const tinygltf::Mesh &mesh, 
		std::vector< SMeshVertex > &vertices, 
		std::vector< uint32_t > &indices, 
		std::vector< SMeshSection > &materialSections )
	{
		// Set material mesh section tracking
		uint32_t currentFirstIndex = indices.empty() ? 0 : indices.size() - 1;
		uint32_t currentIndexCount = 0;
		uint32_t currentMaterialIndex = 0;

		// Process each primitive in the mesh
		for ( const auto &primitive : mesh.primitives )
		{
			uint32_t vertexBase = vertices.size();

			// Get accessor for vertex positions (required)
			const tinygltf::Accessor &posAccessor = model.accessors[ primitive.attributes.at( "POSITION" ) ];
			const tinygltf::BufferView &posView = model.bufferViews[ posAccessor.bufferView ];
			const float *positions = reinterpret_cast< const float * >( &( model.buffers[ posView.buffer ].data[ posView.byteOffset + posAccessor.byteOffset ] ) );

			// Process each vertex
			for ( size_t i = 0; i < posAccessor.count; i++ )
			{
				SMeshVertex vertex {};

				// Position (required)
				vertex.position = { positions[ i * 3 ], positions[ i * 3 + 1 ], positions[ i * 3 + 2 ] };

				// Normal (optional)
				if ( primitive.attributes.find( "NORMAL" ) != primitive.attributes.end() )
				{
					const tinygltf::Accessor &normalAccessor = model.accessors[ primitive.attributes.at( "NORMAL" ) ];
					const tinygltf::BufferView &normalView = model.bufferViews[ normalAccessor.bufferView ];
					const float *normals = reinterpret_cast< const float * >( &( model.buffers[ normalView.buffer ].data[ normalView.byteOffset + normalAccessor.byteOffset ] ) );

					vertex.normal = { normals[ i * 3 ], normals[ i * 3 + 1 ], normals[ i * 3 + 2 ] };
				}

				// Texture coordinates (optional)
				if ( primitive.attributes.find( "TEXCOORD_0" ) != primitive.attributes.end() )
				{
					const tinygltf::Accessor &uvAccessor = model.accessors[ primitive.attributes.at( "TEXCOORD_0" ) ];
					const tinygltf::BufferView &uvView = model.bufferViews[ uvAccessor.bufferView ];
					const float *uvs = reinterpret_cast< const float * >( &( model.buffers[ uvView.buffer ].data[ uvView.byteOffset + uvAccessor.byteOffset ] ) );

					vertex.uv0 = { uvs[ i * 2 ], uvs[ i * 2 + 1 ] };
				}

				// Second set of texture coordinates (optional)
				if ( primitive.attributes.find( "TEXCOORD_1" ) != primitive.attributes.end() )
				{
					const tinygltf::Accessor &uvAccessor = model.accessors[ primitive.attributes.at( "TEXCOORD_1" ) ];
					const tinygltf::BufferView &uvView = model.bufferViews[ uvAccessor.bufferView ];
					const float *uvs = reinterpret_cast< const float * >( &( model.buffers[ uvView.buffer ].data[ uvView.byteOffset + uvAccessor.byteOffset ] ) );

					vertex.uv1 = { uvs[ i * 2 ], uvs[ i * 2 + 1 ] };
				}

				// Tangents (optional but important for normal mapping)
				if ( primitive.attributes.find( "TANGENT" ) != primitive.attributes.end() )
				{
					const tinygltf::Accessor &tangentAccessor = model.accessors[ primitive.attributes.at( "TANGENT" ) ];
					const tinygltf::BufferView &tangentView = model.bufferViews[ tangentAccessor.bufferView ];
					const float *tangents = reinterpret_cast< const float * >( &( model.buffers[ tangentView.buffer ].data[ tangentView.byteOffset + tangentAccessor.byteOffset ] ) );

					vertex.tangent = { tangents[ i * 4 ], tangents[ i * 4 + 1 ], tangents[ i * 4 + 2 ], tangents[ i * 4 + 3 ] };
				}
				else if ( primitive.attributes.find( "NORMAL" ) != primitive.attributes.end() )
				{
					vertex.tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
				}

				// Color (optional)
				if ( primitive.attributes.find( "COLOR_0" ) != primitive.attributes.end() )
				{
					const tinygltf::Accessor &colorAccessor = model.accessors[ primitive.attributes.at( "COLOR_0" ) ];
					const tinygltf::BufferView &colorView = model.bufferViews[ colorAccessor.bufferView ];
					const float *colors = reinterpret_cast< const float * >( &( model.buffers[ colorView.buffer ].data[ colorView.byteOffset + colorAccessor.byteOffset ] ) );

					vertex.color0 = { colors[ i * 3 ], colors[ i * 3 + 1 ], colors[ i * 3 + 2 ] };
				}
				else
				{
					vertex.color0 = { 1.0f, 1.0f, 1.0f };
				}

				// Joint indices (optional)
				if ( primitive.attributes.find( "JOINTS_0" ) != primitive.attributes.end() )
				{
					const tinygltf::Accessor &jointAccessor = model.accessors[ primitive.attributes.at( "JOINTS_0" ) ];
					const tinygltf::BufferView &jointView = model.bufferViews[ jointAccessor.bufferView ];

					// Handle different component types for joints
					if ( jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE )
					{
						const uint8_t *joints = reinterpret_cast< const uint8_t * >( &( model.buffers[ jointView.buffer ].data[ jointView.byteOffset + jointAccessor.byteOffset ] ) );
						for ( int j = 0; j < JOINT_INFLUENCE_COUNT; j++ )
						{
							vertex.joints[ j ] = static_cast< uint32_t >( joints[ i * 4 + j ] );
						}
					}
					else if ( jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT )
					{
						const uint16_t *joints = reinterpret_cast< const uint16_t * >( &( model.buffers[ jointView.buffer ].data[ jointView.byteOffset + jointAccessor.byteOffset ] ) );
						for ( int j = 0; j < JOINT_INFLUENCE_COUNT; j++ )
						{
							vertex.joints[ j ] = static_cast< uint32_t >( joints[ i * 4 + j ] );
						}
					}
				}

				// Joint weights (optional)
				if ( primitive.attributes.find( "WEIGHTS_0" ) != primitive.attributes.end() )
				{
					const tinygltf::Accessor &weightAccessor = model.accessors[ primitive.attributes.at( "WEIGHTS_0" ) ];
					const tinygltf::BufferView &weightView = model.bufferViews[ weightAccessor.bufferView ];
					const float *weights = reinterpret_cast< const float * >( &( model.buffers[ weightView.buffer ].data[ weightView.byteOffset + weightAccessor.byteOffset ] ) );

					for ( int j = 0; j < JOINT_INFLUENCE_COUNT; j++ )
					{
						vertex.weights[ j ] = weights[ i * 4 + j ];
					}

					// Normalize weights (in case file isn't up to spec)
					float weightSum = 0.0f;
					for ( int j = 0; j < JOINT_INFLUENCE_COUNT; ++j )
					{
						weightSum += vertex.weights[ j ];
					}

					if ( weightSum > 1.0f )
					{
						for ( int j = 0; j < JOINT_INFLUENCE_COUNT; ++j )
						{
							vertex.weights[ j ] /= weightSum;
						}
					}
				}
				else if ( primitive.attributes.find( "JOINTS_0" ) != primitive.attributes.end() )
				{
					// If we have joints but no weights, rest weights to 0
					for ( int j = 1; j < JOINT_INFLUENCE_COUNT; ++j )
					{
						vertex.weights[ j ] = 0.0f;
					}

					// Then assign full weight to the first joint
					vertex.weights[ 0 ] = 1.0f;
				}

				vertices.push_back( vertex );
			}

			// Process indices
			if ( primitive.indices >= 0 )
			{
				const tinygltf::Accessor &accessor = model.accessors[ primitive.indices ];
				const uint32_t numIndices = accessor.count;
				const tinygltf::BufferView &indexView = model.bufferViews[ accessor.bufferView ];

				// Check component type to determine index size
				switch ( accessor.componentType )
				{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					{
						// 16-bit indices
						const uint16_t *indicesArray = reinterpret_cast< const uint16_t * >( &( model.buffers[ indexView.buffer ].data[ indexView.byteOffset + accessor.byteOffset ] ) );
						for ( size_t i = 0; i < numIndices; i++ )
						{
							indices.push_back( static_cast< uint32_t >( indicesArray[ i ] ) + vertexBase );
						}
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					{
						// 32-bit indices
						const uint32_t *indicesArray = reinterpret_cast< const uint32_t * >( &( model.buffers[ indexView.buffer ].data[ indexView.byteOffset + accessor.byteOffset ] ) );
						for ( size_t i = 0; i < numIndices; i++ )
						{
							indices.push_back( indicesArray[ i ] + vertexBase );
						}
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					{
						// 8-bit indices
						const uint8_t *indicesArray = reinterpret_cast< const uint8_t * >( &( model.buffers[ indexView.buffer ].data[ indexView.byteOffset + accessor.byteOffset ] ) );
						for ( size_t i = 0; i < numIndices; i++ )
						{
							indices.push_back( static_cast< uint32_t >( indicesArray[ i ] ) + vertexBase );
						}
						break;
					}
					default:
						throw std::runtime_error( "Unsupported index component type" );
				}

				// Material section
				uint32_t materialIndex = primitive.material >= 0 ? primitive.material : 0;
				if ( primitive.material >= 0 )
				{
					// If material changes, add the last section and create a new one
					if ( materialIndex != currentMaterialIndex )
					{
						// Add new mesh section
						SMeshSection newSection { .firstIndex = currentFirstIndex, .indexCount = currentIndexCount, .materialIndex = materialIndex };
						materialSections.push_back( newSection );

						// Set for new section
						currentFirstIndex = currentFirstIndex + currentIndexCount;
						currentIndexCount = 0;
						currentMaterialIndex = materialIndex;
					}

					currentIndexCount += numIndices;
				}
				else
				{
					// No material - add last mesh section
					if ( currentIndexCount > 0 )
					{
						// Add new mesh section
						SMeshSection newSection { .firstIndex = currentFirstIndex, .indexCount = currentIndexCount, .materialIndex = materialIndex };
						materialSections.push_back( newSection );

						// Set for new section
						currentFirstIndex = currentFirstIndex + currentIndexCount;
						currentIndexCount = 0;
					}
				}
			}
			else
			{
				// Handle non-indexed geometry
				// When no indices are specified, vertices are used in order as triangles
				const size_t numVertices = posAccessor.count;
				for ( size_t i = 0; i < numVertices; i++ )
				{
					indices.push_back( static_cast< uint32_t >( vertices.size() - numVertices + i ) );
				}

				// Update material section tracking
				uint32_t materialIndex = primitive.material >= 0 ? primitive.material : 0;
				if ( primitive.material >= 0 )
				{
					if ( materialIndex != currentMaterialIndex )
					{
						// Add new mesh section
						SMeshSection newSection { .firstIndex = currentFirstIndex, .indexCount = currentIndexCount, .materialIndex = materialIndex };
						materialSections.push_back( newSection );

						// Set for new section
						currentFirstIndex = currentFirstIndex + currentIndexCount;
						currentIndexCount = 0;
						currentMaterialIndex = materialIndex;
					}

					currentIndexCount += numVertices;
				}
				else if ( currentIndexCount > 0 )
				{
					// No material - add last mesh section
					SMeshSection newSection { .firstIndex = currentFirstIndex, .indexCount = currentIndexCount, .materialIndex = materialIndex };
					materialSections.push_back( newSection );

					// Set for new section
					currentFirstIndex = currentFirstIndex + currentIndexCount;
					currentIndexCount = 0;
				}
			}
		

			// Check for single material
			if ( materialSections.empty() && currentIndexCount > 0 )
			{
				// Add new mesh section
				SMeshSection newSection { .firstIndex = 0, .indexCount = static_cast< uint32_t >( indices.size() ), .materialIndex = 0 };
				materialSections.push_back( newSection );
			}
			// Check for last material section
			else if ( !materialSections.empty() && currentIndexCount > 0 )
			{

				// Add new mesh section
				SMeshSection newSection { .firstIndex = currentFirstIndex, .indexCount = currentIndexCount, .materialIndex = currentMaterialIndex };
				materialSections.push_back( newSection );
			}

		}

	}

	void CGltf::ParseTextures( CRenderModel *outRenderModel, VkCommandPool commandPool, const tinygltf::Model &model ) 
	{
		if ( model.textures.size() < 1 )
			return;

		outRenderModel->textures.reserve( model.textures.size() );
		for ( const auto &gltfTexture : model.textures )
		{
			STexture texture;
			ParseTexture( &texture, commandPool, model, gltfTexture );
			outRenderModel->textures.push_back( texture );
		}
	}

	void CGltf::ParseTexture( STexture *outTexture, VkCommandPool commandPool, const tinygltf::Model &model, const tinygltf::Texture &gltfTexture ) 
	{
		// Get the image data
		if ( gltfTexture.source < 0 || gltfTexture.source >= static_cast< int >( model.images.size() ) )
		{
			LogError( "CGltf::ParseTexture", "Invalid texture source index: %d", gltfTexture.source );
			return;
		}

		const tinygltf::Image &image = model.images[ gltfTexture.source ];

		// Basic texture properties
		outTexture->name = gltfTexture.name.empty() ? image.name : gltfTexture.name;
		outTexture->uri = image.uri;
		outTexture->width = image.width;
		outTexture->height = image.height;
		outTexture->channels = image.component;
		outTexture->bitsPerChannel = image.bits;

		// Determine format based on components and bits
		if ( image.bits == 8 )
		{
			switch ( image.component )
			{
				case 1:
					outTexture->format = VK_FORMAT_R8_UNORM;
					break;
				case 2:
					outTexture->format = VK_FORMAT_R8G8_UNORM;
					break;
				case 3:
					outTexture->format = VK_FORMAT_R8G8B8_UNORM;
					break;
				case 4:
					outTexture->format = VK_FORMAT_R8G8B8A8_UNORM;
					break;
				default:
					LogWarning( XRLIB_NAME, "Unsupported component count: %d, defaulting to RGBA8", image.component );
					outTexture->format = VK_FORMAT_R8G8B8A8_UNORM;
			}
		}
		else if ( image.bits == 16 )
		{
			switch ( image.component )
			{
				case 1:
					outTexture->format = VK_FORMAT_R16_UNORM;
					break;
				case 2:
					outTexture->format = VK_FORMAT_R16G16_UNORM;
					break;
				case 3:
					outTexture->format = VK_FORMAT_R16G16B16_UNORM;
					break;
				case 4:
					outTexture->format = VK_FORMAT_R16G16B16A16_UNORM;
					break;
				default:
					LogWarning( XRLIB_NAME, "Unsupported component count: %d, defaulting to RGBA16", image.component );
					outTexture->format = VK_FORMAT_R16G16B16A16_UNORM;
			}
		}
		else
		{
			LogWarning( XRLIB_NAME, "Unsupported bits per channel: %d, defaulting to 8-bit RGBA", image.bits );
			outTexture->format = VK_FORMAT_R8G8B8A8_UNORM;
		}

		// Parse sampler if present
		if ( gltfTexture.sampler >= 0 && gltfTexture.sampler < static_cast< int >( model.samplers.size() ) )
		{
			const tinygltf::Sampler &sampler = model.samplers[ gltfTexture.sampler ];
			
			// Set sampler properties
			outTexture->samplerConfig.minFilter = ConvertMinFilter( sampler.minFilter );
			outTexture->samplerConfig.magFilter = ConvertMagFilter( sampler.magFilter );
			outTexture->samplerConfig.addressModeU = ConvertWrappingMode( sampler.wrapS );
			outTexture->samplerConfig.addressModeV = ConvertWrappingMode( sampler.wrapT );
			outTexture->samplerConfig.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT; // Default for 2D textures

			// Handle mipmapping
			if ( sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST || sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR || sampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST ||
				 sampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR )
			{
				// Calculate max mip levels based on texture dimensions
				outTexture->samplerConfig.mipLevels = static_cast< uint32_t >( std::floor( std::log2( std::max( image.width, image.height ) ) ) ) + 1;
				outTexture->samplerConfig.minLod = 0.0f;
				outTexture->samplerConfig.maxLod = static_cast< float >( outTexture->samplerConfig.mipLevels );
			}
			else
			{
				outTexture->samplerConfig.mipLevels = 1;
				outTexture->samplerConfig.minLod = 0.0f;
				outTexture->samplerConfig.maxLod = 0.0f;
			}
		}
		else
		{
			// Set default sampler properties
			outTexture->samplerConfig.minFilter = VK_FILTER_LINEAR;
			outTexture->samplerConfig.magFilter = VK_FILTER_LINEAR;
			outTexture->samplerConfig.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			outTexture->samplerConfig.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			outTexture->samplerConfig.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			outTexture->samplerConfig.mipLevels = 1;
			outTexture->samplerConfig.minLod = 0.0f;
			outTexture->samplerConfig.maxLod = 0.0f;
		}

		// Copy image data
		if ( !image.image.empty() )
		{
			outTexture->data = image.image;

			// Create image
			vkutils::CreateImage(
				outTexture->image,
				outTexture->memory,
				m_pSession->GetVulkan()->GetVkLogicalDevice(),
				m_pSession->GetVulkan()->GetVkPhysicalDevice(),
				outTexture->width,
				outTexture->height,
				outTexture->format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

			// Create image view
			vkutils::CreateImageView( outTexture->view, m_pSession->GetVulkan()->GetVkLogicalDevice(), outTexture->image, outTexture->format, VK_IMAGE_ASPECT_COLOR_BIT );

			// Upload image to gpu buffer and transition image for shader reads
			vkutils::UploadTextureDataToImage(
				m_pSession->GetVulkan()->GetVkLogicalDevice(),
				m_pSession->GetVulkan()->GetVkPhysicalDevice(),
				commandPool,
				m_pSession->GetVulkan()->GetVkQueue_Graphics(),
				outTexture->image,
				outTexture->data,
				outTexture->width,
				outTexture->height,
				outTexture->format );

			// Create texture sampler
			VkSamplerCreateInfo samplerInfo {};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = outTexture->samplerConfig.magFilter;
			samplerInfo.minFilter = outTexture->samplerConfig.minFilter;

			if ( outTexture->samplerConfig.minFilter == VK_FILTER_LINEAR )
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			else if ( outTexture->samplerConfig.minFilter == VK_FILTER_NEAREST )
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			else
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

			samplerInfo.addressModeU = outTexture->samplerConfig.addressModeU;
			samplerInfo.addressModeV = outTexture->samplerConfig.addressModeV;
			samplerInfo.addressModeW = outTexture->samplerConfig.addressModeW;
			samplerInfo.anisotropyEnable = outTexture->samplerConfig.anisotropyEnable ? VK_TRUE : VK_FALSE;
			samplerInfo.maxAnisotropy = outTexture->samplerConfig.maxAnisotropy;
			samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = outTexture->samplerConfig.minLod;
			samplerInfo.maxLod = outTexture->samplerConfig.maxLod;

			vkCreateSampler( m_pSession->GetVulkan()->GetVkLogicalDevice(), &samplerInfo, nullptr, &outTexture->sampler );
		}
	}

	void CGltf::IdentifyTextureTypes( std::vector< STexture > &textures, const tinygltf::Model &model )
	{
		// Go through each material to identify texture types
		for ( const auto &material : model.materials )
		{
			// Base color texture
			if ( material.pbrMetallicRoughness.baseColorTexture.index >= 0 )
			{
				int texIndex = model.textures[ material.pbrMetallicRoughness.baseColorTexture.index ].source;
				if ( texIndex >= 0 && texIndex < textures.size() )
				{
					textures[ texIndex ].type = ETextureType::BaseColor;
				}
			}

			// Metallic roughness texture
			if ( material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0 )
			{
				int texIndex = model.textures[ material.pbrMetallicRoughness.metallicRoughnessTexture.index ].source;
				if ( texIndex >= 0 && texIndex < textures.size() )
				{
					textures[ texIndex ].type = ETextureType::MetallicRoughness;
				}
			}

			// Normal map
			if ( material.normalTexture.index >= 0 )
			{
				int texIndex = model.textures[ material.normalTexture.index ].source;
				if ( texIndex >= 0 && texIndex < textures.size() )
				{
					textures[ texIndex ].type = ETextureType::Normal;
				}
			}

			// Emissive map
			if ( material.emissiveTexture.index >= 0 )
			{
				int texIndex = model.textures[ material.emissiveTexture.index ].source;
				if ( texIndex >= 0 && texIndex < textures.size() )
				{
					textures[ texIndex ].type = ETextureType::Emissive;
				}
			}

			// Occlusion map
			if ( material.occlusionTexture.index >= 0 )
			{
				int texIndex = model.textures[ material.occlusionTexture.index ].source;
				if ( texIndex >= 0 && texIndex < textures.size() )
				{
					textures[ texIndex ].type = ETextureType::Occlusion;
				}
			}
		}
	}

	void CGltf::ParseMaterials( CRenderModel *outRenderModel, const tinygltf::Model &model ) 
	{
		if ( model.materials.size() < 1 )
			return;

		outRenderModel->materials.reserve( model.materials.size() );
		for ( const auto &gltfMaterial : model.materials )
		{
			SMaterial material;
			ParseMaterial( &material, model, gltfMaterial );
			outRenderModel->materials.push_back( material );

			// Parse texture type referenced by this material
			IdentifyTextureTypes( outRenderModel->textures, model );
		}
	}

	void CGltf::ParseMaterial( SMaterial *outMaterial, const tinygltf::Model &model, const tinygltf::Material &gltfMaterial ) 
	{
		// Parse PBR Metallic Roughness properties
		if ( gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() == 4 )
		{
			for ( int i = 0; i < 4; i++ )
			{
				outMaterial->baseColorFactor[ i ] = static_cast< float >( gltfMaterial.pbrMetallicRoughness.baseColorFactor[ i ] );
			}
		}

		// Parse texture indices
		outMaterial->baseColorTexture = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
		outMaterial->metallicRoughnessTexture = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
		outMaterial->normalTexture = gltfMaterial.normalTexture.index;
		outMaterial->occlusionTexture = gltfMaterial.occlusionTexture.index;
		outMaterial->emissiveTexture = gltfMaterial.emissiveTexture.index;

		// Parse emissive factor
		if ( gltfMaterial.emissiveFactor.size() == 3 )
		{
			for ( int i = 0; i < 3; i++ )
			{
				outMaterial->emissiveFactor[ i ] = static_cast< float >( gltfMaterial.emissiveFactor[ i ] );
			}
		}

		// Parse alpha mode
		if ( gltfMaterial.alphaMode == "MASK" )
		{
			outMaterial->setAlphaMode( EAlphaMode::Mask );
		}
		else if ( gltfMaterial.alphaMode == "BLEND" )
		{
			outMaterial->setAlphaMode( EAlphaMode::Blend );
		}
		else // "OPAQUE" or any other value
		{
			outMaterial->setAlphaMode( EAlphaMode::Opaque );
		}

		// Parse alpha cutoff
		outMaterial->alphaCutoff = static_cast< float >( gltfMaterial.alphaCutoff );

		// Parse double sided flag
		outMaterial->doubleSided = gltfMaterial.doubleSided;
	}

	void CGltf::ParseSkins( CRenderModel *outRenderModel, const tinygltf::Model &model ) 
	{
		if ( model.skins.size() < 1 )
			return;

		outRenderModel->skins.resize( model.skins.size() );
		for ( uint32_t i = 0; i < model.skins.size(); i++ )
		{
			ParseSkin( &outRenderModel->skins[ i ], model, model.skins[ i ] );
		}

		if ( model.skins.size() > MAX_JOINT_COUNT )
		{
			LogError( XRLIB_NAME, "Max joint count (%i) execeeded in gltf file (%i).\nLoad skeletal meshes as single files if possible.", 
				MAX_JOINT_COUNT, model.skins.size() );
			throw std::runtime_error( "Maximum joints supported by the renderer reached in glTF file" );
		}
			
	}

	void CGltf::ParseSkin( SSkin *outSkin, const tinygltf::Model &model, const tinygltf::Skin &gltfSkin ) 
	{
		outSkin->name = gltfSkin.name;
		outSkin->skeleton = gltfSkin.skeleton;

		// Parse joints with validation
		outSkin->joints.reserve( gltfSkin.joints.size() );
		for ( const int jointIndex : gltfSkin.joints )
		{
			if ( jointIndex < 0 )
			{
				throw std::runtime_error( "Invalid negative joint index in glTF skin" );
			}
			if ( jointIndex >= static_cast< int >( model.nodes.size() ) )
			{
				throw std::runtime_error( "Joint index exceeds number of nodes in glTF model" );
			}

			outSkin->joints.push_back( static_cast< uint32_t > ( jointIndex ) );
		}

		// Build parent-child hierarchy
		outSkin->hierarchy.clear();

		// Create mapping from node index to joint index for quick lookups
		std::unordered_map< uint32_t, uint32_t > nodeToJointIndex;
		for ( size_t i = 0; i < gltfSkin.joints.size(); ++i )
		{
			nodeToJointIndex[ gltfSkin.joints[ i ] ] = i;
		}

		// Iterate over joints and process their children
		for ( size_t i = 0; i < gltfSkin.joints.size(); ++i )
		{
			uint32_t jointNodeIndex = gltfSkin.joints[ i ];
			const tinygltf::Node &node = model.nodes[ jointNodeIndex ];

			// Process each child of this node
			for ( int childNodeIndex : node.children )
			{
				// Check if the child is also a joint in this skin
				auto childJointIt = nodeToJointIndex.find( childNodeIndex );
				if ( childJointIt != nodeToJointIndex.end() )
				{
					// Add child joint index to parent's hierarchy entry
					outSkin->hierarchy[ i ].push_back( childJointIt->second );
				}
			}
		}

		// Parse inverse bind matrices if available
		if ( gltfSkin.inverseBindMatrices >= 0 )
		{
			const tinygltf::Accessor &accessor = model.accessors[ gltfSkin.inverseBindMatrices ];
			const tinygltf::BufferView &bufferView = model.bufferViews[ accessor.bufferView ];
			const tinygltf::Buffer &buffer = model.buffers[ bufferView.buffer ];
			const float *matrices = reinterpret_cast< const float * >( &buffer.data[ bufferView.byteOffset + accessor.byteOffset ] );

			size_t numMatrices = accessor.count;
			outSkin->inverseBindMatrices.resize( numMatrices );

			// Create a mapping from node index to joint index
			std::unordered_map< uint32_t, uint32_t > nodeToJointIndex;
			for ( size_t i = 0; i < gltfSkin.joints.size(); ++i )
			{
				nodeToJointIndex[ gltfSkin.joints[ i ] ] = i;
			}

			// Process each joint node
			for ( size_t i = 0; i < numMatrices; i++ )
			{
				uint32_t nodeIndex = gltfSkin.joints[ i ];			 // Get the node index from joints array
				uint32_t jointIndex = nodeToJointIndex[ nodeIndex ]; // Convert to sequential joint index

				// Get the inverse bind matrix for this joint
				float mat[ 16 ];
				memcpy( &mat, matrices + ( i * 16 ), sizeof( mat ) );

				// Manually transpose the matrix (glTF is column-major, we need row-major)
				XrMatrix4x4f transposed;
				transposed.m[ 0 ] = mat[ 0 ];
				transposed.m[ 1 ] = mat[ 4 ];
				transposed.m[ 2 ] = mat[ 8 ];
				transposed.m[ 3 ] = mat[ 12 ];
				transposed.m[ 4 ] = mat[ 1 ];
				transposed.m[ 5 ] = mat[ 5 ];
				transposed.m[ 6 ] = mat[ 9 ];
				transposed.m[ 7 ] = mat[ 13 ];
				transposed.m[ 8 ] = mat[ 2 ];
				transposed.m[ 9 ] = mat[ 6 ];
				transposed.m[ 10 ] = mat[ 10 ];
				transposed.m[ 11 ] = mat[ 14 ];
				transposed.m[ 12 ] = mat[ 3 ];
				transposed.m[ 13 ] = mat[ 7 ];
				transposed.m[ 14 ] = mat[ 11 ];
				transposed.m[ 15 ] = mat[ 15 ];

				float pitch, yaw, roll;
				ExtractEulerAngles( transposed, pitch, yaw, roll );

				// Add correction for coordinate systems
				XrQuaternionf xRotation {
					-0.7071067811865476f, // sin(90/2) for X rotation
					0.0f,
					0.0f,
					0.7071067811865476f // cos(90/2)
				};
				XrQuaternionf yRotation {
					0.0f,
					0.0f,
					0.7071067811865476f, // sin(90/2) for Y rotation
					0.7071067811865476f	 // cos(90/2)
				};

				// Convert matrix to TRS components
				XrVector3f position, scale;
				XrQuaternionf rotation;

				XrMatrix4x4f_GetTranslation( &position, &transposed );
				XrMatrix4x4f_GetRotation( &rotation, &transposed );
				XrMatrix4x4f_GetScale( &scale, &transposed );

				// Apply correction
				XrQuaternionf tempRotation;
				XrQuaternionf_Multiply( &tempRotation, &xRotation, &rotation );
				XrQuaternionf_Multiply( &rotation, &yRotation, &tempRotation );

				// Rebuild matrix with corrected rotation
				XrMatrix4x4f_CreateTranslationRotationScale( &outSkin->inverseBindMatrices[ jointIndex ], &position, &rotation, &scale );

				ExtractEulerAngles( outSkin->inverseBindMatrices[ jointIndex ], pitch, yaw, roll );

				// Normalize the matrix (removes additional scaling from gltf file)
				NormalizeMatrix( outSkin->inverseBindMatrices[ jointIndex ] );
			}
		}

	}

	VkFilter CGltf::ConvertMagFilter( int gltfFilter )
	{
		switch ( gltfFilter )
		{
			case TINYGLTF_TEXTURE_FILTER_NEAREST:
				return VK_FILTER_NEAREST;
			case TINYGLTF_TEXTURE_FILTER_LINEAR:
				return VK_FILTER_LINEAR;
			default:
				return VK_FILTER_LINEAR; // Default to linear filtering
		}
	}

	VkFilter CGltf::ConvertMinFilter( int gltfFilter )
	{
		switch ( gltfFilter )
		{
			case TINYGLTF_TEXTURE_FILTER_NEAREST:
			case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
			case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
				return VK_FILTER_NEAREST;

			case TINYGLTF_TEXTURE_FILTER_LINEAR:
			case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
			case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
				return VK_FILTER_LINEAR;

			default:
				return VK_FILTER_LINEAR; // Default to linear filtering
		}
	}

	VkSamplerAddressMode CGltf::ConvertWrappingMode( int gltfWrap )
	{
		switch ( gltfWrap )
		{
			case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
				return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

			case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
				return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

			case TINYGLTF_TEXTURE_WRAP_REPEAT:
				return VK_SAMPLER_ADDRESS_MODE_REPEAT;

			default:
				return VK_SAMPLER_ADDRESS_MODE_REPEAT; // Default to repeat mode
		}
	}

} // namespace xrlib
